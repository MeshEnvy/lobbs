#include "LoFSQspiNorBlockDevice.h"
#include <Arduino.h>

#if LOFS_BOARD_HAS_QSPI
#include "variant.h"
#include "nrf.h"
#include <cstring>
#include <nrfx_qspi.h>

extern const uint32_t g_ADigitalPinMap[];

#include <Adafruit_LittleFS.h>

static constexpr uint32_t LOFS_QSPI_BLOCK = 4096;
static constexpr uint32_t LOFS_QSPI_PAGE = 256;
static constexpr uint32_t LOFS_QSPI_BLOCK_COUNT = 512;

static uint8_t lofsQspiScratch[LOFS_QSPI_PAGE] __attribute__((aligned(4)));
static bool lofsQspiHwReady = false;

static uint8_t lofsQspiPin(uint32_t dPin)
{
    return (uint8_t)g_ADigitalPinMap[dPin];
}

static nrfx_err_t lofsQspiCinstr(uint8_t op, nrf_qspi_cinstr_len_t len, void *rx)
{
    nrf_qspi_cinstr_conf_t c = NRFX_QSPI_DEFAULT_CINSTR(op, len);
    c.io2_level = true;
    c.io3_level = true;
    return nrfx_qspi_cinstr_xfer(&c, NULL, rx);
}

static bool lofsQspiWaitReady()
{
    uint8_t sr[4];
    do {
        if (lofsQspiCinstr(0x05, NRF_QSPI_CINSTR_LEN_2B, sr) != NRFX_SUCCESS)
            return false;
    } while (sr[0] & 0x01);
    return true;
}

static bool lofsQspiReadJedec(uint8_t out[3])
{
    const uint8_t wakeOps[] = {0xAB, 0x66, 0x99};
    for (size_t i = 0; i < sizeof(wakeOps); i++) {
        lofsQspiCinstr(wakeOps[i], NRF_QSPI_CINSTR_LEN_1B, NULL);
        delayMicroseconds(50);
    }
    nrfx_err_t err = lofsQspiCinstr(0x9F, NRF_QSPI_CINSTR_LEN_4B, out);
    if (err != NRFX_SUCCESS) {
        Serial.printf("LoFS QSPI JEDEC cinstr err %d\n", (int)err);
        return false;
    }
    return true;
}

static bool lofsQspiInitHw()
{
    if (lofsQspiHwReady)
        return true;

#ifndef NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY 6
#endif

    nrfx_qspi_config_t cfg =
        NRFX_QSPI_DEFAULT_CONFIG(lofsQspiPin(PIN_QSPI_SCK), lofsQspiPin(PIN_QSPI_CS), lofsQspiPin(PIN_QSPI_IO0),
                                 lofsQspiPin(PIN_QSPI_IO1), lofsQspiPin(PIN_QSPI_IO2), lofsQspiPin(PIN_QSPI_IO3));
    cfg.phy_if.sck_freq = NRF_QSPI_FREQ_DIV8;

    nrfx_err_t err = nrfx_qspi_init(&cfg, NULL, NULL);
    if (err == NRFX_ERROR_INVALID_STATE) {
        lofsQspiHwReady = true;
    } else if (err != NRFX_SUCCESS) {
        Serial.printf("LoFS QSPI init failed: %d\n", (int)err);
        return false;
    } else {
        lofsQspiHwReady = true;
    }

    uint8_t jedec[3];
    if (!lofsQspiReadJedec(jedec)) {
        Serial.printf("LoFS QSPI JEDEC read failed\n");
        nrfx_qspi_uninit();
        lofsQspiHwReady = false;
        return false;
    }
    Serial.printf("LoFS QSPI JEDEC %02x %02x %02x\n", jedec[0], jedec[1], jedec[2]);
    if (jedec[0] != 0x85 && jedec[0] != 0x9D) {
        Serial.printf("LoFS QSPI unexpected manufacturer 0x%02x\n", jedec[0]);
        nrfx_qspi_uninit();
        lofsQspiHwReady = false;
        return false;
    }
    return true;
}

static int lofsQspiRead(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;
    uint32_t addr = block * LOFS_QSPI_BLOCK + off;
    if (((uintptr_t)buffer & 3) == 0 && (size & 3) == 0) {
        nrfx_err_t err = nrfx_qspi_read(buffer, size, addr);
        return err == NRFX_SUCCESS ? 0 : -1;
    }
    uint8_t *dst = (uint8_t *)buffer;
    while (size > 0) {
        uint32_t chunk = size > LOFS_QSPI_PAGE ? LOFS_QSPI_PAGE : size;
        uint32_t qchunk = (chunk + 3) & ~3u;
        nrfx_err_t err = nrfx_qspi_read(lofsQspiScratch, qchunk, addr);
        if (err != NRFX_SUCCESS)
            return -1;
        memcpy(dst, lofsQspiScratch, chunk);
        dst += chunk;
        addr += chunk;
        size -= chunk;
    }
    return 0;
}

static int lofsQspiProg(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                        lfs_size_t size)
{
    (void)c;
    uint32_t addr = block * LOFS_QSPI_BLOCK + off;
    if (((uintptr_t)buffer & 3) == 0 && (size & 3) == 0) {
        nrfx_err_t err = nrfx_qspi_write(buffer, size, addr);
        lofsQspiWaitReady();
        return err == NRFX_SUCCESS ? 0 : -1;
    }
    const uint8_t *src = (const uint8_t *)buffer;
    while (size > 0) {
        uint32_t chunk = size > LOFS_QSPI_PAGE ? LOFS_QSPI_PAGE : size;
        uint32_t qchunk = (chunk + 3) & ~3u;
        memcpy(lofsQspiScratch, src, chunk);
        for (uint32_t i = chunk; i < qchunk; i++)
            lofsQspiScratch[i] = 0xFF;
        nrfx_err_t err = nrfx_qspi_write(lofsQspiScratch, qchunk, addr);
        if (err != NRFX_SUCCESS)
            return -1;
        src += chunk;
        addr += chunk;
        size -= chunk;
    }
    lofsQspiWaitReady();
    return 0;
}

static int lofsQspiErase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    uint32_t addr = block * LOFS_QSPI_BLOCK;
    nrfx_err_t err = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, addr);
    lofsQspiWaitReady();
    return err == NRFX_SUCCESS ? 0 : -1;
}

static int lofsQspiSync(const struct lfs_config *c)
{
    (void)c;
    lofsQspiWaitReady();
    return 0;
}

bool LoFSQspiNorBlockDevice::fill(lfs_config &cfg)
{
    if (!lofsQspiInitHw())
        return false;
    cfg.context = NULL;
    cfg.read = lofsQspiRead;
    cfg.prog = lofsQspiProg;
    cfg.erase = lofsQspiErase;
    cfg.sync = lofsQspiSync;
    cfg.read_size = LOFS_QSPI_PAGE;
    cfg.prog_size = LOFS_QSPI_PAGE;
    cfg.block_size = LOFS_QSPI_BLOCK;
    cfg.block_count = LOFS_QSPI_BLOCK_COUNT;
    cfg.lookahead = 512;
    return true;
}

#else

bool LoFSQspiNorBlockDevice::fill(lfs_config &cfg)
{
    (void)cfg;
    return false;
}

#endif

#include "core/LoBBSStackGuard.h"
