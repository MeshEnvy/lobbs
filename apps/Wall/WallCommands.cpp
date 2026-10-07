#include "WallCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "../Config/ConfigCommon.h"
#include "WallDal.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <strings.h>

#include "LoBBSStackGuard.h"

static void replyGrid(LoBBSCommandCtx &ctx, bool markSeen)
{
    WallDal &wall = ctx.mod->wall().dal();
    char buf[LOBBS_REPLY_BYTES + 1];
    if (!wall.formatGridLines(buf, sizeof(buf))) {
        lobbsCommandReplyError(ctx, "Canvas error.");
        return;
    }
    if (markSeen && lobbsCtxLoggedIn(ctx))
        wall.markSeen(lobbsCtxUserUuid(ctx), wall.canvasCrc32());
    lobbsCommandReply(ctx, buf);
}

static const LoBBSVerb wallVerbs[] = {
    {"view", nullptr, 0, "view — /wall shows 12x12 grid (no login)"},
    {"paint", nullptr, LOBBS_V_LOGIN, "paint — a4x set; -a4 blank; 1/cycle default"},
};

static void handleWall(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        replyGrid(ctx, true);
        return;
    }

    if (lobbsDispatchSub(ctx, wallVerbs, sizeof(wallVerbs) / sizeof(wallVerbs[0])))
        return;

    if (!lobbsCommandRequireLogin(ctx))
        return;

    const char *toks[48];
    int n = lobbsArgShiftMany(ctx, toks, 48);
    if (n <= 0) {
        replyGrid(ctx, true);
        return;
    }

    const uint32_t period = lobbsConfigGet(ctx, "wall.period");
    const uint32_t cells = lobbsConfigGet(ctx, "wall.cells");
    if (const char *err =
            ctx.mod->wall().dal().applyPaintTokens(lobbsCtxUserUuid(ctx), ctx.session.isSysop, toks, n, period, cells)) {
        lobbsCommandReplyError(ctx, err);
        return;
    }
    replyGrid(ctx, true);
}

static void slashWall(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "wall"))
        return;
    handleWall(*ctx);
}

static void filterWallHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "wall", "shared 12x12 paint grid");
}

static void filterWallHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "wall", wallVerbs, sizeof(wallVerbs) / sizeof(wallVerbs[0]));
}

static void filterWallStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod || !lobbsCtxLoggedIn(*ctx))
        return;
    bool dirty = ctx->mod->wall().dal().isDirtyForUser(lobbsCtxUserUuid(*ctx));
    lobbsRecordPush(lines, "Wall", dirty ? "new" : "seen");
}

#if LOBBS_SEED
#include "WallSeed.h"
static void actionWallSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->mod)
        lobbsSeedWall(*ctx->mod);
}
#endif

static void filterWallConfigKeys(LoBBSCommandCtx *ctx, std::vector<LoScalar> &keys, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsConfigPushKey(keys, "wall.period", LOBBS_WALL_DEFAULT_PERIOD_SEC, 60, 86400, "Wall paint quota period (seconds)");
    lobbsConfigPushKey(keys, "wall.cells", LOBBS_WALL_DEFAULT_MAX_CELLS, 1, LOBBS_WALL_CELLS, "Max cells painted per period");
}

void lobbsWallRegisterCommands()
{
    lobbsAddFilter("config_keys", filterWallConfigKeys, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddAction("slash_cmd", slashWall, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterWallHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterWallHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterWallStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
#if LOBBS_SEED
    lobbsAddAction("seed", actionWallSeed, LOBBS_HOOK_PRIORITY_FEATURE);
#endif
}

