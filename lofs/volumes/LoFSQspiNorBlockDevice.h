#pragma once

#include "core/LoBBSConfig.h"
#include "LoFSRawDevice.h"

#if !defined(LOFS_BOARD_HAS_QSPI)
#if ((defined(LOFS_ENABLE_EXTRA_QSPI) && LOFS_ENABLE_EXTRA_QSPI) || defined(QSPIFLASH)) &&                          \
    defined(LOFS_NRF52)
#define LOFS_BOARD_HAS_QSPI 1
#else
#define LOFS_BOARD_HAS_QSPI 0
#endif
#endif

class LoFSQspiNorBlockDevice : public LoFSRawDevice {
  public:
    bool begin() override;
    uint32_t sectorSize() const override;
    uint32_t pageSize() const override;
    uint32_t sectorCount() const override;
    bool partialPageProgram() const override;

    bool read(uint32_t addr, void *buf, size_t len) override;
    bool prog(uint32_t addr, const void *buf, size_t len) override;
    bool eraseSector(uint32_t sectorIndex) override;
    bool sync() override;
};
