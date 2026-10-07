#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSResponse.h"
#include <stdint.h>

#ifndef LOBBS_REPLY_CACHE_TTL_SEC
#define LOBBS_REPLY_CACHE_TTL_SEC 300
#endif

#ifndef LOBBS_REPLY_CACHE_MAX_BYTES
#define LOBBS_REPLY_CACHE_MAX_BYTES 8192
#endif

#ifndef LOBBS_REPLY_CACHE_MAX_ENTRIES
#define LOBBS_REPLY_CACHE_MAX_ENTRIES 32
#endif

void lobbsReplyCacheGc(uint32_t nowSec);
void lobbsReplyCacheErase(uint32_t sessionNodeId);
bool lobbsReplyCacheStore(uint32_t sessionNodeId, const LoBBSResponse &resp);
bool lobbsReplyCacheLoad(uint32_t sessionNodeId, LoBBSResponse &out);

#endif
