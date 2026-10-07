#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandCtx.h"
#include <loscalar/LoScalar.h>
#include <string>
#include <vector>

struct LoBBSResponse {
    bool ok = true;
    std::string error;
    std::vector<LoScalar> records;
};

void lobbsResponseSetError(LoBBSResponse &resp, const char *message);
/** Push a `title` (+ optional `description`) record. Empty strings leave the field unset. */
void lobbsRecordPush(std::vector<LoScalar> &list, const char *title, const char *description = nullptr);
void lobbsResponseAppendRecord(LoBBSResponse &resp, const LoScalar &record);

void lobbsCommandReplyResponse(LoBBSCommandCtx &ctx, const LoBBSResponse &resp);
void lobbsCommandReplyError(LoBBSCommandCtx &ctx, const char *message);
void lobbsReplySendCachedPage(LoBBSCommandCtx &ctx);

#endif
