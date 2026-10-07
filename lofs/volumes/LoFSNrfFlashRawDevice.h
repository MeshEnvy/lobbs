#pragma once

#include "LoFSRawDevice.h"

/** nRF52840 internal flash window for LoLog (not LittleFS). */
class LoFSNrfFlashRawDevice : public LoFSRawDevice {
  public:
    LoFSNrfFlashRawDevice(void *startSym, void *endSym, bool lockWithInternalFs = true,
                          uint32_t flashPageSize = 4096);

    bool begin() override;
    uint32_t sectorSize() const override;
    uint32_t pageSize() const override;
    uint32_t sectorCount() const override;
    bool partialPageProgram() const override;

    bool read(uint32_t addr, void *buf, size_t len) override;
    bool prog(uint32_t addr, const void *buf, size_t len) override;
    bool eraseSector(uint32_t sectorIndex) override;
    bool sync() override;

  private:
    void *startSym_;
    void *endSym_;
    bool lockWithInternalFs_;
    uint32_t flashPageSize_;
    uint32_t base_ = 0;
    uint32_t size_ = 0;

    void lockFs();
    void unlockFs();
};
