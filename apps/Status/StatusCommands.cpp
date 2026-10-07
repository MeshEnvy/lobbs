#include "StatusCommands.h"
#include "../../core/LoBBSCommandRegistry.h"
#include "../../core/LoBBSHooks.h"
#include "../../core/LoBBSResponse.h"

#include "core/LoBBSStackGuard.h"

static void handleStatus(LoBBSCommandCtx &ctx)
{
    LoBBSResponse resp;
    lobbsApplyFilter("status_lines", ctx, resp.records, LoScalar());
    if (resp.records.empty()) {
        lobbsResponseSetError(resp, "No status.");
    }
    lobbsCommandReplyResponse(ctx, resp);
}

static void slashStatus(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "status"))
        return;
    handleStatus(*ctx);
}

static void filterStatusHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "status", "what's happening");
}

void lobbsStatusRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashStatus, LOBBS_HOOK_PRIORITY_STATUS);
    lobbsAddFilter("help_topics", filterStatusHelpTopics, LOBBS_HOOK_PRIORITY_STATUS);
}

