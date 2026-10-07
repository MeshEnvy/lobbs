#include "LoFSNrfFlashRawDevice.h"
#include "lofs/lolog/LoLogConfig.h"
#include <cstring>

#if defined(LOFS_NRF52)

#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "flash/flash_nrf5x.h"
#include <InternalFileSystem.h>
#elif defined(LOBBS_PLATFORM_MESHCORE)
extern "C" int flash_nrf5x_read(void *buffer, uint32_t address, uint32_t len);
extern "C" int flash_nrf5x_write(uint32_t address, const void *data, uint32_t len);
extern "C" void flash_nrf5x_flush(void);
extern "C" void flash_nrf5x_erase(uint32_t address);
#endif

LoFSNrfFlashRawDevice::LoFSNrfFlashRawDevice(void *startSym, void *endSym, bool lockWithInternalFs,
                                             uint32_t flashPageSize)
    : startSym_(startSym), endSym_(endSym), lockWithInternalFs_(lockWithInternalFs), flashPageSize_(flashPageSize)
{
}

void LoFSNrfFlashRawDevice::lockFs()
{
#if defined(LOBBS_PLATFORM_MESHTASTIC)
    if (lockWithInternalFs_)
        InternalFS._lockFS();
#else
    (void)lockWithInternalFs_;
#endif
}

void LoFSNrfFlashRawDevice::unlockFs()
{
#if defined(LOBBS_PLATFORM_MESHTASTIC)
    if (lockWithInternalFs_)
        InternalFS._unlockFS();
#endif
}

bool LoFSNrfFlashRawDevice::begin()
{
    const uint32_t start = (uint32_t)(uintptr_t)startSym_;
    const uint32_t end = (uint32_t)(uintptr_t)endSym_;
    if (!start || end <= start)
        return false;
    base_ = start;
    size_ = end - start;
    if (size_ < lolog::kSectorSize)
        return false;
    return true;
}

uint32_t LoFSNrfFlashRawDevice::sectorSize() const
{
    return lolog::kSectorSize;
}

uint32_t LoFSNrfFlashRawDevice::pageSize() const
{
    return lolog::kPageSize;
}

uint32_t LoFSNrfFlashRawDevice::sectorCount() const
{
    return size_ / lolog::kSectorSize;
}

bool LoFSNrfFlashRawDevice::partialPageProgram() const
{
    return true;
}

bool LoFSNrfFlashRawDevice::read(uint32_t addr, void *buf, size_t len)
{
    if (!buf || addr + len > size_)
        return false;
    lockFs();
    int n = flash_nrf5x_read(buf, base_ + addr, (uint32_t)len);
    unlockFs();
    return n > 0;
}

bool LoFSNrfFlashRawDevice::prog(uint32_t addr, const void *buf, size_t len)
{
    if (!buf || addr + len > size_ || (len & 3) != 0 || (addr & 3) != 0)
        return false;
    lockFs();
    int n = flash_nrf5x_write(base_ + addr, buf, (uint32_t)len);
    unlockFs();
    return n > 0;
}

bool LoFSNrfFlashRawDevice::eraseSector(uint32_t sectorIndex)
{
    if (sectorIndex >= sectorCount())
        return false;
    const uint32_t addr = base_ + sectorIndex * lolog::kSectorSize;
    lockFs();
    flash_nrf5x_erase(addr);
    unlockFs();
    if (sectorIndex < kMaxSectors)
        eraseCounts_[sectorIndex]++;
    return true;
}

bool LoFSNrfFlashRawDevice::sync()
{
    lockFs();
    flash_nrf5x_flush();
    unlockFs();
    return true;
}

#else

LoFSNrfFlashRawDevice::LoFSNrfFlashRawDevice(void *, void *, bool, uint32_t) {}

bool LoFSNrfFlashRawDevice::begin()
{
    return false;
}

uint32_t LoFSNrfFlashRawDevice::sectorSize() const
{
    return 4096;
}

uint32_t LoFSNrfFlashRawDevice::pageSize() const
{
    return 256;
}

uint32_t LoFSNrfFlashRawDevice::sectorCount() const
{
    return 0;
}

bool LoFSNrfFlashRawDevice::partialPageProgram() const
{
    return true;
}

bool LoFSNrfFlashRawDevice::read(uint32_t, void *, size_t)
{
    return false;
}

bool LoFSNrfFlashRawDevice::prog(uint32_t, const void *, size_t)
{
    return false;
}

bool LoFSNrfFlashRawDevice::eraseSector(uint32_t)
{
    return false;
}

bool LoFSNrfFlashRawDevice::sync()
{
    return true;
}

#endif

#include "core/LoBBSStackGuard.h"
