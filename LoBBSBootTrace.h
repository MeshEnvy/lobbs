#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "configuration.h"

/** Log immediately before a boot step; last line wins when diagnosing hangs. */
#define LOBBS_BOOT_STEP(msg) LOG_INFO("LoBBS> %s", (msg))

#endif
