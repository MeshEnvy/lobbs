#if !MESHTASTIC_EXCLUDE_LOBBS

#include "TimeCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "gps/RTC.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <sys/time.h>

#include "LoBBSStackGuard.h"

static void handleTime(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        char buf[LOBBS_REPLY_BYTES + 1];
        snprintf(buf, sizeof(buf), "Time: %u (%s)", (unsigned)getTime(), RtcName(getRTCQuality()));
        lobbsCommandReply(ctx, buf);
        return;
    }
    if (!lobbsCommandRequireSysop(ctx))
        return;
    uint32_t sec = 0;
    if (!lobbsArgShiftUint(ctx, sec)) {
        lobbsCommandReplyError(ctx, "Invalid time.");
        return;
    }
    if (lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /time\nUsage: /time unix (sysop)");
        return;
    }
    struct timeval tv;
    tv.tv_sec = (time_t)sec;
    tv.tv_usec = 0;
    RTCSetResult r = perhapsSetRTC(RTCQualityNTP, &tv, true);
    if (r == RTCSetResultSuccess)
        lobbsCommandReply(ctx, "Time set.");
    else if (r == RTCSetResultInvalidTime)
        lobbsCommandReplyError(ctx, "Invalid time.");
    else
        lobbsCommandReplyError(ctx, "Could not set time.");
}

static const LoBBSVerb timeHelpVerbs[] = {
    {"time", nullptr, 0, "time — show Unix time and source"},
    {"time", nullptr, LOBBS_V_SYSOP, "time unix — sysop: set clock to Unix epoch"},
};

static void slashTime(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "time"))
        return;
    handleTime(*ctx);
}

static void filterTimeHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "time", "show the clock");
}

static void filterTimeHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "time", timeHelpVerbs, sizeof(timeHelpVerbs) / sizeof(timeHelpVerbs[0]));
}

void lobbsTimeRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashTime, LOBBS_HOOK_PRIORITY_TIME);
    lobbsAddFilter("help_topics", filterTimeHelpTopics, LOBBS_HOOK_PRIORITY_TIME);
    lobbsAddFilter("help_for_topic", filterTimeHelpForTopic, LOBBS_HOOK_PRIORITY_TIME);
}

#endif
