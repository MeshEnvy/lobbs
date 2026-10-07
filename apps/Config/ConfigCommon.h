#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSCommandCtx.h"
#include "ConfigRecords.h"
#include <lodb/LoDB.h>
#include <loscalar/LoScalar.h>
#include <vector>

void lobbsConfigPushKey(std::vector<LoScalar> &keys, const char *key, uint32_t def, uint32_t min, uint32_t max, const char *help);

uint32_t lobbsConfigGet(LoBBSCommandCtx &ctx, const char *key);

#endif
