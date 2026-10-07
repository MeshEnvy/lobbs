#include "LoFSQspi.h"
#include "configuration.h"

#if LOBBS_EXTRA_QSPI && defined(ARCH_NRF52)
#include "nrf.h"
#include "variant.h"
#include <Adafruit_LittleFS.h>
#include <Arduino.h>
#include <cstring>
#include <nrfx_qspi.h>

extern const uint32_t g_ADigitalPinMap[];

#include "../LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

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
    // IO2/IO3 are WP#/HOLD# when the quad-enable bit is clear; low HOLD# makes the flash ignore the command.
    c.io2_level = true;
    c.io3_level = true;
    return nrfx_qspi_cinstr_xfer(&c, NULL, rx);
}

// nrfx_qspi_mem_busy_check() sends RDSR with HOLD# low, so the status byte floats.
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
    // Flash state survives MCU resets on battery: release deep power-down, then software reset (66h/99h).
    for (uint8_t op : {0xAB, 0x66, 0x99}) {
        lofsQspiCinstr(op, NRF_QSPI_CINSTR_LEN_1B, NULL);
        delayMicroseconds(50);
    }

    LOBBS_BOOT_STEP("qspi: jedec cinstr xfer");
    nrfx_err_t err = lofsQspiCinstr(0x9F, NRF_QSPI_CINSTR_LEN_4B, out);
    if (err != NRFX_SUCCESS) {
        LOG_ERROR("LoFS QSPI JEDEC cinstr err %d", (int)err);
        return false;
    }
    return true;
}

static bool lofsQspiInitHw()
{
    if (lofsQspiHwReady)
        return true;

    LOBBS_BOOT_STEP("qspi: init hw enter");

#ifndef NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_QSPI_DEFAULT_CONFIG_IRQ_PRIORITY 6
#endif

    nrfx_qspi_config_t cfg =
        NRFX_QSPI_DEFAULT_CONFIG(lofsQspiPin(PIN_QSPI_SCK), lofsQspiPin(PIN_QSPI_CS), lofsQspiPin(PIN_QSPI_IO0),
                                 lofsQspiPin(PIN_QSPI_IO1), lofsQspiPin(PIN_QSPI_IO2), lofsQspiPin(PIN_QSPI_IO3));
    // FASTREAD default: works without QE bit; READ2IO needs status-register setup on P25Q16.
    cfg.phy_if.sck_freq = NRF_QSPI_FREQ_DIV8;

    LOBBS_BOOT_STEP("qspi: nrfx_qspi_init");
    nrfx_err_t err = nrfx_qspi_init(&cfg, NULL, NULL);
    if (err == NRFX_ERROR_INVALID_STATE) {
        lofsQspiHwReady = true;
    } else if (err != NRFX_SUCCESS) {
        LOG_ERROR("LoFS QSPI init failed: %d", (int)err);
        return false;
    } else {
        lofsQspiHwReady = true;
    }

    uint8_t jedec[3];
    if (!lofsQspiReadJedec(jedec)) {
        LOG_ERROR("LoFS QSPI JEDEC read failed");
        nrfx_qspi_uninit();
        lofsQspiHwReady = false;
        return false;
    }
    LOG_INFO("LoFS QSPI JEDEC %02x %02x %02x", jedec[0], jedec[1], jedec[2]);
    if (jedec[0] != 0x85) {
        LOG_ERROR("LoFS QSPI unexpected manufacturer 0x%02x", jedec[0]);
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

static int lofsQspiProg(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
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

bool lofsQspiConfig(struct lfs_config &cfg)
{
    LOBBS_BOOT_STEP("qspi: lofsQspiConfig");
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

#endif
