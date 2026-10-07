#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

void lobbsConfigRegisterCommands();
uint32_t lobbsReplyCacheTtlSec();

#endif
