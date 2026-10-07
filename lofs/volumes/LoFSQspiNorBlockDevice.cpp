#include "LoFSQspiNorBlockDevice.h"
#include <Arduino.h>
#include <cstring>

#if LOFS_BOARD_HAS_QSPI
#include "variant.h"
#include "nrf.h"
#include <nrfx_qspi.h>

extern const uint32_t g_ADigitalPinMap[];

static constexpr uint32_t LOFS_QSPI_SECTOR = 4096;
static constexpr uint32_t LOFS_QSPI_PAGE = 256;
static constexpr uint32_t LOFS_QSPI_SECTOR_COUNT = 512;

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

static bool lofsQspiReadAddr(uint32_t addr, void *buffer, size_t size)
{
    if (((uintptr_t)buffer & 3) == 0 && (size & 3) == 0) {
        nrfx_err_t err = nrfx_qspi_read(buffer, size, addr);
        return err == NRFX_SUCCESS;
    }
    uint8_t *dst = (uint8_t *)buffer;
    while (size > 0) {
        uint32_t chunk = size > LOFS_QSPI_PAGE ? LOFS_QSPI_PAGE : (uint32_t)size;
        uint32_t qchunk = (chunk + 3) & ~3u;
        nrfx_err_t err = nrfx_qspi_read(lofsQspiScratch, qchunk, addr);
        if (err != NRFX_SUCCESS)
            return false;
        memcpy(dst, lofsQspiScratch, chunk);
        dst += chunk;
        addr += chunk;
        size -= chunk;
    }
    return true;
}

static bool lofsQspiProgAddr(uint32_t addr, const void *buffer, size_t size)
{
    if (((uintptr_t)buffer & 3) == 0 && (size & 3) == 0) {
        nrfx_err_t err = nrfx_qspi_write(buffer, size, addr);
        lofsQspiWaitReady();
        return err == NRFX_SUCCESS;
    }
    const uint8_t *src = (const uint8_t *)buffer;
    while (size > 0) {
        uint32_t chunk = size > LOFS_QSPI_PAGE ? LOFS_QSPI_PAGE : (uint32_t)size;
        uint32_t qchunk = (chunk + 3) & ~3u;
        memcpy(lofsQspiScratch, src, chunk);
        for (uint32_t i = chunk; i < qchunk; i++)
            lofsQspiScratch[i] = 0xFF;
        nrfx_err_t err = nrfx_qspi_write(lofsQspiScratch, qchunk, addr);
        if (err != NRFX_SUCCESS)
            return false;
        src += chunk;
        addr += chunk;
        size -= chunk;
    }
    lofsQspiWaitReady();
    return true;
}

bool LoFSQspiNorBlockDevice::begin()
{
    return lofsQspiInitHw();
}

uint32_t LoFSQspiNorBlockDevice::sectorSize() const
{
    return LOFS_QSPI_SECTOR;
}

uint32_t LoFSQspiNorBlockDevice::pageSize() const
{
    return LOFS_QSPI_PAGE;
}

uint32_t LoFSQspiNorBlockDevice::sectorCount() const
{
    return LOFS_QSPI_SECTOR_COUNT;
}

bool LoFSQspiNorBlockDevice::partialPageProgram() const
{
    return true;
}

bool LoFSQspiNorBlockDevice::read(uint32_t addr, void *buf, size_t len)
{
    return lofsQspiReadAddr(addr, buf, len);
}

bool LoFSQspiNorBlockDevice::prog(uint32_t addr, const void *buf, size_t len)
{
    return lofsQspiProgAddr(addr, buf, len);
}

bool LoFSQspiNorBlockDevice::eraseSector(uint32_t sectorIndex)
{
    if (sectorIndex >= LOFS_QSPI_SECTOR_COUNT)
        return false;
    uint32_t addr = sectorIndex * LOFS_QSPI_SECTOR;
    nrfx_err_t err = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, addr);
    lofsQspiWaitReady();
    if (err == NRFX_SUCCESS && sectorIndex < kMaxSectors)
        eraseCounts_[sectorIndex]++;
    return err == NRFX_SUCCESS;
}

bool LoFSQspiNorBlockDevice::sync()
{
    lofsQspiWaitReady();
    return true;
}

#else

bool LoFSQspiNorBlockDevice::begin()
{
    return false;
}

uint32_t LoFSQspiNorBlockDevice::sectorSize() const
{
    return 4096;
}

uint32_t LoFSQspiNorBlockDevice::pageSize() const
{
    return 256;
}

uint32_t LoFSQspiNorBlockDevice::sectorCount() const
{
    return 0;
}

bool LoFSQspiNorBlockDevice::partialPageProgram() const
{
    return true;
}

bool LoFSQspiNorBlockDevice::read(uint32_t addr, void *buf, size_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return false;
}

bool LoFSQspiNorBlockDevice::prog(uint32_t addr, const void *buf, size_t len)
{
    (void)addr;
    (void)buf;
    (void)len;
    return false;
}

bool LoFSQspiNorBlockDevice::eraseSector(uint32_t sectorIndex)
{
    (void)sectorIndex;
    return false;
}

bool LoFSQspiNorBlockDevice::sync()
{
    return true;
}

#endif

#include "core/LoBBSStackGuard.h"
