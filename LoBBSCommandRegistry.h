#pragma once
#include "LoBBSCommandCtx.h"
#include "LoBBSHooks.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

static constexpr uint8_t LOBBS_V_LOGIN = 1;
static constexpr uint8_t LOBBS_V_SYSOP = 2;

struct LoBBSVerb {
    const char *verb;
    void (*fn)(LoBBSCommandCtx &);
    uint8_t flags;
    const char *help;
};

void lobbsCommandReply(LoBBSCommandCtx &ctx, const char *body);
void lobbsCommandReplyError(LoBBSCommandCtx &ctx, const char *message);
bool lobbsCtxLoggedIn(const LoBBSCommandCtx &ctx);
bool lobbsCtxIsSysop(const LoBBSCommandCtx &ctx);
uint64_t lobbsCtxUserUuid(const LoBBSCommandCtx &ctx);
bool lobbsCtxUsername(const LoBBSCommandCtx &ctx, char *buf, size_t bufCap);
bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx);
bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx);

const char *lobbsArgShift(LoBBSCommandCtx &ctx);
const char *lobbsArgPeek(const LoBBSCommandCtx &ctx);
const char *lobbsArgRest(LoBBSCommandCtx &ctx);
bool lobbsArgHasMore(const LoBBSCommandCtx &ctx);
bool lobbsArgPeekIsUint(const LoBBSCommandCtx &ctx);
bool lobbsArgShiftUint(LoBBSCommandCtx &ctx, uint32_t &out);

bool lobbsSlashVerbIs(const LoScalar &args, const char *verb);
/** help_for_topic helper: sets value's description when args' query is `topic` or `topic <verb>`. LOBBS_V_SYSOP rows are
 * hidden unless ctx is a sysop. Rows with fn == nullptr are help-only. */
void lobbsHelpForTable(const LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args, const char *topic,
                       const LoBBSVerb *table, size_t count);
bool lobbsDispatchSub(LoBBSCommandCtx &ctx, const LoBBSVerb *table, size_t count);
int lobbsArgShiftMany(LoBBSCommandCtx &ctx, const char *out[], int maxOut);

void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, const LoBBSSession &session, char *line);

