#include "NewsCommands.h"
#include "../../core/LoBBSCommandRegistry.h"
#include "../../core/LoBBSConfig.h"
#include "../../core/LoBBSHooks.h"
#include "../../core/LoBBSKernel.h"
#include "../../core/LoBBSReply.h"
#include "../../core/LoBBSResponse.h"
#include "../AppUtil.h"
#include "../Msg/MsgCommon.h"
#include "NewsDal.h"
#include "NewsRecords.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>
#include <vector>

#include "core/LoBBSStackGuard.h"

static void newsAppendListRecord(LoBBSKernel *kernel, LoBBSResponse &resp, uint32_t oneBasedIndex, const LoBBSNewsEntry &entry)
{
    const LoScalar &news = entry.news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    // One char past the preview limit is enough for lobbsAppTruncMsg to add "...".
    char msg[LOBBS_LIST_LINE_TRUNC_MAX_CHARS + 2];
    char line[LOBBS_REPLY_BYTES + 1];
    lobbsAppUsernameForUuid(kernel, NewsDal::newsAuthorUuid(news), name, sizeof(name));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(news), when, sizeof(when));
    NewsDal::newsMessage(news, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
    snprintf(line, sizeof(line), "[%u]%s @%s: %s (%s)", (unsigned)oneBasedIndex, entry.isRead ? "" : "*", name, trunc, when);

    LoScalar rec;
    rec.setUint64(LODB_F_ID, NewsDal::newsUuid(news));
    rec.setString(LODB_F_TITLE, line);
    rec.setString(LODB_F_DESCRIPTION, trunc);
    rec.setBool(NewsField::FIELD_LIST_READ, entry.isRead);
    rec.setUint32(LODB_F_CREATED, NewsDal::newsTimestamp(news));
    lobbsResponseAppendRecord(resp, rec);
}

static void newsSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);

    auto all = ctx.kernel->news().dal().getAllNewsForUser(lobbsCtxUserUuid(ctx));
    LoBBSResponse resp;
    if (all.empty()) {
        lobbsResponseSetError(resp, "No news");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (size_t i = 0; i < all.size(); i++)
        newsAppendListRecord(ctx.kernel, resp, (uint32_t)(i + 1), all[i]);
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubRead(LoBBSCommandCtx &ctx, bool numericShorthand)
{
    if (!numericShorthand)
        lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid news number." : "Usage: /news read N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    NewsDal &news = ctx.kernel->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const LoScalar &item = newsItems[idx - 1].news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
    char header[LOBBS_USERNAME_BUFFER_SIZE + LOBBS_TIME_AGO_BUFFER_SIZE + 16];
    lobbsAppUsernameForUuid(ctx.kernel, NewsDal::newsAuthorUuid(item), name, sizeof(name));
    NewsDal::newsMessage(item, body, sizeof(body));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(item), when, sizeof(when));
    snprintf(header, sizeof(header), "From: @%s (%s)", name, when);

    LoScalar rec;
    rec.setUint64(LODB_F_ID, NewsDal::newsUuid(item));
    rec.setString(LODB_F_TITLE, header);
    rec.setString(LODB_F_DESCRIPTION, body);
    rec.setUint32(LODB_F_CREATED, NewsDal::newsTimestamp(item));
    news.markNewsAsRead(NewsDal::newsUuid(item), lobbsCtxUserUuid(ctx));

    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubUnread(LoBBSCommandCtx &ctx)
{
    uint32_t idx = 0;
    if (!lobbsMsgShiftIndex(ctx, 1, "Usage: /news unread N", "Invalid news number.", idx))
        return;
    NewsDal &news = ctx.kernel->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, news.markNewsAsUnread(uuid, lobbsCtxUserUuid(ctx)) ? "Marked unread." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubDelete(LoBBSCommandCtx &ctx)
{
    uint32_t idx = 0;
    if (!lobbsMsgShiftIndex(ctx, 1, "Usage: /news delete N", "Invalid news number.", idx))
        return;
    NewsDal &news = ctx.kernel->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, news.deleteNewsUuid(uuid) ? "Deleted." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubPost(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "post") != 0) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /news post message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const char *msgBody = lobbsArgRest(ctx);
    if (!msgBody[0]) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /news post message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (!isalpha((unsigned char)msgBody[0])) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Start with a letter.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoBBSResponse resp;
    LoDbError err = ctx.kernel->news().dal().postNews(lobbsCtxUserUuid(ctx), msgBody);
    lobbsRecordPush(resp.records, err == LODB_OK ? "News posted." : lobbsDbErrorText(err, "Failed to post news."));
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubReadCmd(LoBBSCommandCtx &ctx)
{
    newsSubRead(ctx, false);
}

static const LoBBSVerb newsVerbs[] = {
    {"list", newsSubList, LOBBS_V_LOGIN, "list — news index"},
    {"read", newsSubReadCmd, LOBBS_V_LOGIN, "read N — read and mark read"},
    {"unread", newsSubUnread, LOBBS_V_LOGIN, "unread N — mark unread"},
    {"delete", newsSubDelete, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "delete N — delete (sysop)"},
    {"post", newsSubPost, LOBBS_V_LOGIN, "post message... — post news"},
};

static void handleNews(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;

    if (lobbsArgPeekIsUint(ctx)) {
        newsSubRead(ctx, true);
        return;
    }

    if (!lobbsDispatchSub(ctx, newsVerbs, sizeof(newsVerbs) / sizeof(newsVerbs[0]))) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Unknown command. Try /help news");
        lobbsCommandReplyResponse(ctx, resp);
    }
}

static void slashNews(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "news"))
        return;
    handleNews(*ctx);
}

static void filterNewsHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRecordPush(topics, "news", "read and post news");
}

static void filterNewsHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "news", newsVerbs, sizeof(newsVerbs) / sizeof(newsVerbs[0]));
}

static void filterNewsStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->kernel)
        return;
    char title[64];
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->kernel->news().dal().countUnreadNews(lobbsCtxUserUuid(*ctx));
        snprintf(title, sizeof(title), "News");
        snprintf(value, sizeof(value), "%u unread", (unsigned)n);
    } else {
        uint32_t n = ctx->kernel->news().dal().countAllNews();
        snprintf(title, sizeof(title), "News");
        snprintf(value, sizeof(value), "%u total", (unsigned)n);
    }
    lobbsRecordPush(lines, title, value);
}

#if LOBBS_SEED
#include "NewsSeed.h"
static void actionNewsSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->kernel)
        lobbsSeedNews(*ctx->kernel);
}
#endif

void lobbsNewsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashNews, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterNewsHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterNewsHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterNewsStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
#if LOBBS_SEED
    lobbsAddAction("seed", actionNewsSeed, LOBBS_HOOK_PRIORITY_FEATURE);
#endif
}

