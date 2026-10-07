#pragma once
#include "../../core/LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>

bool lobbsMsgShiftIndex(LoBBSCommandCtx &ctx, size_t count, const char *usage, const char *badNumMsg, uint32_t &idxOut);
void lobbsMsgRegisterDisplay();

