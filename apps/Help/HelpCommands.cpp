#include "HelpCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSResponse.h"
#include "../../LoBBSVersion.h"
#include <cstring>
#include <lodb/LoDB.h>

#include "LoBBSStackGuard.h"

static void lobbsTrimRestInPlace(char *s)
{
    if (!s)
        return;
    char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
}

static void replyCatalog(LoBBSCommandCtx &ctx)
{
    std::vector<LoScalar> topics;
    lobbsRecordPush(topics, "help", "this list, or /help topic");
    lobbsRecordPush(topics, "hi", "welcome screen");
    lobbsRecordPush(topics, "pN", "page N of the last reply");
    lobbsApplyFilter("help_topics", ctx, topics, LoScalar());

    LoBBSResponse resp;
    lobbsRecordPush(resp.records, "LoBBS v" LOBBS_VERSION_SHORT " Help");
    resp.records.insert(resp.records.end(), topics.begin(), topics.end());
    lobbsCommandReplyResponse(ctx, resp);
}

static void replyTopicHelp(LoBBSCommandCtx &ctx, const char *query)
{
    LoScalar args;
    args.setString(LODB_F_TITLE, query);
    LoScalar topic;
    topic.setString(LODB_F_TITLE, query);
    lobbsApplyFilter("help_for_topic", ctx, topic, args);

    LoBBSResponse resp;
    std::string body;
    if (!topic.getString(LODB_F_DESCRIPTION, body) || body.empty()) {
        lobbsRecordPush(resp.records, (std::string("No help found for ") + query).c_str());
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    size_t start = 0;
    while (start <= body.size()) {
        size_t nl = body.find('\n', start);
        if (nl == std::string::npos)
            nl = body.size();
        lobbsRecordPush(resp.records, body.substr(start, nl - start).c_str());
        start = nl + 1;
    }
    lobbsCommandReplyResponse(ctx, resp);
}

static void replyHi(LoBBSCommandCtx &ctx)
{
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, "LoBBS v" LOBBS_VERSION_SHORT);
    char uname[LOBBS_USERNAME_BUFFER_SIZE];
    if (lobbsCtxLoggedIn(ctx) && lobbsCtxUsername(ctx, uname, sizeof(uname)))
        lobbsRecordPush(resp.records, (std::string("Welcome back, ") + uname + "!").c_str());
    else
        lobbsRecordPush(resp.records, "Welcome!");
    lobbsRecordPush(resp.records, LOBBS_HELP_HINT);
    lobbsRecordPush(resp.records, "Use /status to see what's happening");
    if (!lobbsCtxLoggedIn(ctx))
        lobbsRecordPush(resp.records, "Use /login to sign in");
    lobbsCommandReplyResponse(ctx, resp);
}

static void slashHelp(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx)
        return;
    if (lobbsSlashVerbIs(args, "hi")) {
        replyHi(*ctx);
        return;
    }
    if (!lobbsSlashVerbIs(args, "help"))
        return;

    char work[128];
    work[0] = '\0';
    if (ctx->rest && ctx->rest[0])
        strncpy(work, ctx->rest, sizeof(work) - 1);

    lobbsTrimRestInPlace(work);
    if (!work[0]) {
        replyCatalog(*ctx);
        return;
    }

    replyTopicHelp(*ctx, work);
}

static const LoBBSVerb pagingHelpVerbs[] = {
    {"pN", nullptr, 0, "pN — next page of last reply (/p2, /43 p2 machine)"},
};

static void filterHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "pN", pagingHelpVerbs, sizeof(pagingHelpVerbs) / sizeof(pagingHelpVerbs[0]));
}

void lobbsHelpRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashHelp, LOBBS_HOOK_PRIORITY_HELP);
    lobbsAddFilter("help_for_topic", filterHelpForTopic, LOBBS_HOOK_PRIORITY_HELP);
}

