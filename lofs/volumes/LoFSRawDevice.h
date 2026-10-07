#pragma once

#include <stddef.h>
#include <stdint.h>

/** Raw NOR or RAM backing for LoLog (not LittleFS). */
class LoFSRawDevice {
  public:
    virtual ~LoFSRawDevice() = default;

    virtual bool begin() = 0;
    virtual uint32_t sectorSize() const = 0;
    virtual uint32_t pageSize() const = 0;
    virtual uint32_t sectorCount() const = 0;
    /** Multiple programs into the same erased page before erase. */
    virtual bool partialPageProgram() const = 0;

    virtual bool read(uint32_t addr, void *buf, size_t len) = 0;
    /** len 4-byte aligned; addr page-aligned offset within device. */
    virtual bool prog(uint32_t addr, const void *buf, size_t len) = 0;
    virtual bool eraseSector(uint32_t sectorIndex) = 0;
    virtual bool sync() = 0;

    uint32_t eraseCount(uint32_t sectorIndex) const;

  protected:
    static constexpr uint32_t kMaxSectors = 2048;
    uint32_t eraseCounts_[kMaxSectors] = {};
};
