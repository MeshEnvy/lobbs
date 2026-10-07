#include "LoFSRawDevice.h"

uint32_t LoFSRawDevice::eraseCount(uint32_t sectorIndex) const
{
    return sectorIndex < kMaxSectors ? eraseCounts_[sectorIndex] : 0;
}

#include "core/LoBBSStackGuard.h"
