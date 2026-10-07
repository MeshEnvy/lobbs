#pragma once
#include "platforms/LoPlatform.h"

/** Log immediately before a boot step; last line wins when diagnosing hangs. */
#define LOBBS_BOOT_STEP(msg) LOBBS_LOG_INFO("LoBBS> %s", (msg))
