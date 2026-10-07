#pragma once

#include "core/LoBBSConfig.h"
#include "LoFSBlockDevice.h"

#if !defined(LOFS_BOARD_HAS_QSPI)
#if ((defined(LOFS_ENABLE_EXTRA_QSPI) && LOFS_ENABLE_EXTRA_QSPI) || defined(QSPIFLASH)) &&                          \
    defined(LOFS_NRF52)
#define LOFS_BOARD_HAS_QSPI 1
#else
#define LOFS_BOARD_HAS_QSPI 0
#endif
#endif

class LoFSQspiNorBlockDevice : public LoFSBlockDevice {
  public:
    bool fill(lfs_config &cfg) override;
};
