#pragma once

#include "lofs/volumes/LoFSRawDevice.h"
#include <vector>

/** In-memory NOR model for host tests (1->0 program rules, fault injection). */
class LoLogRamDevice : public LoFSRawDevice {
  public:
    explicit LoLogRamDevice(uint32_t sectorCount = 64);

    bool begin() override;
    uint32_t sectorSize() const override;
    uint32_t pageSize() const override;
    uint32_t sectorCount() const override;
    bool partialPageProgram() const override;

    bool read(uint32_t addr, void *buf, size_t len) override;
    bool prog(uint32_t addr, const void *buf, size_t len) override;
    bool eraseSector(uint32_t sectorIndex) override;
    bool sync() override;

    void reset();
    /** Fail the next program after this many bytes have been programmed (0 = off). */
    void setFaultAfterProgBytes(uint32_t n);
    uint32_t faultAfterProgBytes() const { return faultAfter_; }
    /** Fail when starting prog call number n (1-based). 0 = off. */
    void setFaultOnProgCall(uint32_t n);
    uint32_t progCallCount() const { return progCallCount_; }
    void clearProgCallCount() { progCallCount_ = 0; }
    uint64_t progBytes() const { return progBytes_; }
    void clearProgBytes() { progBytes_ = 0; }
    /** Corrupt flash for torn-entry tests (must be 1->0 valid). */
    bool poke(uint32_t addr, uint8_t value);
    uint32_t sectorEraseCount(uint32_t sectorIndex) const { return eraseCount(sectorIndex); }

  private:
    std::vector<uint8_t> data_;
    uint32_t sectors_;
    uint32_t faultAfter_ = 0;
    uint32_t progSinceFault_ = 0;
    uint32_t faultOnProgCall_ = 0;
    uint32_t progCallCount_ = 0;
    uint64_t progBytes_ = 0;
};
