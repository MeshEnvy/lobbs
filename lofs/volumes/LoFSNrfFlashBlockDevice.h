#pragma once

#include "LoFSBlockDevice.h"
#include <cstdint>

class LoFSNrfFlashBlockDevice : public LoFSBlockDevice {
  public:
    LoFSNrfFlashBlockDevice(void *startSym, void *endSym, uint32_t blockSize, bool lockWithInternalFs,
                            bool pageEraseOnFormat, uint32_t flashPageSize = 4096);

    bool fill(lfs_config &cfg) override;
    bool prepareFormat() override;
    bool eraseAll();

    void lockFs();
    void unlockFs();

    struct BlockCtx {
        LoFSNrfFlashBlockDevice *owner;
        uint32_t base;
        uint32_t blockSize;
    };

  private:
    void *startSym_;
    void *endSym_;
    uint32_t blockSize_;
    bool lockWithInternalFs_;
    bool pageEraseOnFormat_;
    uint32_t flashPageSize_;
    uint32_t rangeStart_ = 0;
    uint32_t rangeEnd_ = 0;

    BlockCtx blockCtx_{};
};
