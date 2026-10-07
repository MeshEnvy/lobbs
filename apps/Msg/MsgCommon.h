#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>

bool lobbsMsgShiftIndex(LoBBSCommandCtx &ctx, size_t count, const char *usage, const char *badNumMsg, uint32_t &idxOut);
void lobbsMsgRegisterDisplay();

#endif
