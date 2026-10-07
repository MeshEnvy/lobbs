#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "../Config/ConfigCommon.h"
#include "YarnDal.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <strings.h>

#include "LoBBSStackGuard.h"

static void replyYarnView(LoBBSCommandCtx &ctx, bool markSeen)
{
    char buf[LOBBS_REPLY_BYTES + 1];
    if (!ctx.mod->yarn().dal().formatYarnView(buf, sizeof(buf))) {
        lobbsCommandReplyError(ctx, "Yarn error.");
        return;
    }
    if (markSeen && lobbsCtxLoggedIn(ctx))
        ctx.mod->yarn().dal().markYarnSeen(lobbsCtxUserUuid(ctx));
    lobbsCommandReply(ctx, buf);
}

static const LoBBSVerb yarnVerbs[] = {
    {"view", nullptr, 0, "view — /yarn shows the tail (no login)"},
    {"add", nullptr, LOBBS_V_LOGIN, "add — /yarn word … (login, quota)"},
};

static void handleYarn(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        replyYarnView(ctx, true);
        return;
    }

    if (lobbsDispatchSub(ctx, yarnVerbs, sizeof(yarnVerbs) / sizeof(yarnVerbs[0])))
        return;

    if (!lobbsCommandRequireLogin(ctx))
        return;

    const char *toks[32];
    int n = lobbsArgShiftMany(ctx, toks, 32);
    if (n <= 0) {
        replyYarnView(ctx, true);
        return;
    }

    const uint32_t period = lobbsConfigGet(ctx, "yarn.period");
    const uint32_t words = lobbsConfigGet(ctx, "yarn.words");
    const uint32_t chars = lobbsConfigGet(ctx, "yarn.chars");
    if (const char *err =
            ctx.mod->yarn().dal().appendWords(lobbsCtxUserUuid(ctx), ctx.session.isSysop, toks, n, period, words, chars)) {
        lobbsCommandReplyError(ctx, err);
        return;
    }
    replyYarnView(ctx, true);
}

static void slashYarn(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "yarn"))
        return;
    handleYarn(*ctx);
}

static void filterYarnHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "yarn", "shared story, a few words at a time");
}

static void filterYarnHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "yarn", yarnVerbs, sizeof(yarnVerbs) / sizeof(yarnVerbs[0]));
}

static void filterYarnStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->yarn().dal().newWordsForUser(lobbsCtxUserUuid(*ctx));
        snprintf(value, sizeof(value), "%u new words", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->yarn().dal().totalWordsAppended();
        snprintf(value, sizeof(value), "%u words total", (unsigned)n);
    }
    lobbsRecordPush(lines, "Yarn", value);
}

#if LOBBS_SEED
#include "YarnSeed.h"
static void actionYarnSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->mod)
        lobbsSeedYarn(*ctx->mod);
}
#endif

static void filterYarnConfigKeys(LoBBSCommandCtx *ctx, std::vector<LoScalar> &keys, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsConfigPushKey(keys, "yarn.period", LOBBS_YARN_DEFAULT_PERIOD_SEC, 60, 86400, "Yarn quota period (seconds)");
    lobbsConfigPushKey(keys, "yarn.words", LOBBS_YARN_DEFAULT_MAX_WORDS, 1, 1000, "Max words per period");
    lobbsConfigPushKey(keys, "yarn.chars", LOBBS_YARN_DEFAULT_MAX_CHARS, 1, LOBBS_YARN_BODY_MAX, "Max chars per period");
}

void lobbsYarnRegisterCommands()
{
    lobbsAddFilter("config_keys", filterYarnConfigKeys, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddAction("slash_cmd", slashYarn, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterYarnHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterYarnHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterYarnStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
#if LOBBS_SEED
    lobbsAddAction("seed", actionYarnSeed, LOBBS_HOOK_PRIORITY_FEATURE);
#endif
}

#endif
