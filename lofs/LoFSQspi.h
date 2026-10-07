#pragma once

#include "LoBBSConfig.h"

#if LOBBS_EXTRA_QSPI

struct lfs_config;

/** Init nrfx QSPI (once), verify JEDEC, fill LittleFS block callbacks. Returns false if hardware missing. */
bool lofsQspiConfig(struct lfs_config &cfg);

#endif
