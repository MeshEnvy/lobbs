#include "LoLogRamDevice.h"
#include "lofs/lolog/LoLogConfig.h"
#include <cstring>

LoLogRamDevice::LoLogRamDevice(uint32_t sectorCount) : sectors_(sectorCount)
{
    data_.assign((size_t)sectors_ * lolog::kSectorSize, 0xFF);
}

bool LoLogRamDevice::begin()
{
    return sectors_ > 0 && sectors_ <= kMaxSectors;
}

uint32_t LoLogRamDevice::sectorSize() const
{
    return lolog::kSectorSize;
}

uint32_t LoLogRamDevice::pageSize() const
{
    return lolog::kPageSize;
}

uint32_t LoLogRamDevice::sectorCount() const
{
    return sectors_;
}

bool LoLogRamDevice::partialPageProgram() const
{
    return true;
}

bool LoLogRamDevice::read(uint32_t addr, void *buf, size_t len)
{
    if (!buf || addr + len > data_.size())
        return false;
    memcpy(buf, data_.data() + addr, len);
    return true;
}

static bool canProgramByte(uint8_t oldB, uint8_t newB)
{
    return (oldB & newB) == newB;
}

bool LoLogRamDevice::prog(uint32_t addr, const void *buf, size_t len)
{
    if (!buf || addr + len > data_.size() || (len & 3) != 0 || (addr & 3) != 0)
        return false;
    progCallCount_++;
    if (faultOnProgCall_ > 0 && progCallCount_ >= faultOnProgCall_)
        return false;
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        if (faultAfter_ > 0) {
            if (progSinceFault_ >= faultAfter_)
                return false;
            progSinceFault_++;
        }
        uint8_t &cell = data_[addr + i];
        if (!canProgramByte(cell, src[i]))
            return false;
        cell = src[i];
        progBytes_++;
    }
    return true;
}

bool LoLogRamDevice::eraseSector(uint32_t sectorIndex)
{
    if (sectorIndex >= sectors_)
        return false;
    memset(data_.data() + (size_t)sectorIndex * lolog::kSectorSize, 0xFF, lolog::kSectorSize);
    if (sectorIndex < kMaxSectors)
        eraseCounts_[sectorIndex]++;
    return true;
}

bool LoLogRamDevice::sync()
{
    return true;
}

void LoLogRamDevice::reset()
{
    std::fill(data_.begin(), data_.end(), (uint8_t)0xFF);
    memset(eraseCounts_, 0, sizeof(eraseCounts_));
    faultAfter_ = 0;
    progSinceFault_ = 0;
    faultOnProgCall_ = 0;
    progCallCount_ = 0;
    progBytes_ = 0;
}

void LoLogRamDevice::setFaultAfterProgBytes(uint32_t n)
{
    faultAfter_ = n;
    progSinceFault_ = 0;
}

void LoLogRamDevice::setFaultOnProgCall(uint32_t n)
{
    faultOnProgCall_ = n;
    progCallCount_ = 0;
}

bool LoLogRamDevice::poke(uint32_t addr, uint8_t value)
{
    if (addr >= data_.size())
        return false;
    uint8_t &cell = data_[addr];
    if (!canProgramByte(cell, value))
        return false;
    cell = value;
    return true;
}

#include "core/LoBBSStackGuard.h"
