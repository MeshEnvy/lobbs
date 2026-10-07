#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "../AppUtil.h"
#include "../Msg/MsgCommon.h"
#include "MailDal.h"
#include "MailRecords.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>
#include <vector>

#include "LoBBSStackGuard.h"

static void mailAppendListRecord(LoBBSModule *mod, LoBBSResponse &resp, uint32_t oneBasedIndex, const LoScalar &mail)
{
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    // One char past the preview limit is enough for lobbsAppTruncMsg to add "...".
    char msg[LOBBS_LIST_LINE_TRUNC_MAX_CHARS + 2];
    char line[LOBBS_REPLY_BYTES + 1];
    lobbsAppUsernameForUuid(mod, MailDal::mailFromUuid(mail), name, sizeof(name));
    lobbsAppTimeAgo(MailDal::mailTimestamp(mail), when, sizeof(when));
    MailDal::mailMessage(mail, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
    snprintf(line, sizeof(line), "[%u]%s @%s: %s (%s)", (unsigned)oneBasedIndex, MailDal::mailRead(mail) ? "" : "*", name, trunc,
             when);

    LoScalar rec;
    rec.setUint64(LODB_F_ID, MailDal::mailUuid(mail));
    rec.setString(LODB_F_TITLE, line);
    rec.setString(LODB_F_DESCRIPTION, trunc);
    rec.setBool(MailField::FIELD_READ, MailDal::mailRead(mail));
    rec.setUint32(LODB_F_CREATED, MailDal::mailTimestamp(mail));
    lobbsResponseAppendRecord(resp, rec);
}

static void mailSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);

    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    const char *maybeUser = lobbsArgPeek(ctx);
    if (maybeUser) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        const char *user = lobbsArgShift(ctx);
        if (!lobbsAppResolveUsername(ctx, user, inboxUuid))
            return;
    }

    auto all = ctx.mod->mail().dal().getAllMailForUser(inboxUuid);
    LoBBSResponse resp;
    if (all.empty()) {
        lobbsResponseSetError(resp, "No mail");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (size_t i = 0; i < all.size(); i++)
        mailAppendListRecord(ctx.mod, resp, (uint32_t)(i + 1), all[i]);
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubRead(LoBBSCommandCtx &ctx, bool numericShorthand)
{
    MailDal &mail = ctx.mod->mail().dal();
    if (!numericShorthand) {
        const char *sub = lobbsArgShift(ctx);
        (void)sub;
    }

    uint32_t idx = 0;
    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    bool markRead = true;
    bool sysopRead = ctx.session.isSysop && lobbsArgPeek(ctx) && !lobbsArgPeekIsUint(ctx);

    if (sysopRead) {
        const char *user = lobbsArgShift(ctx);
        if (!user || !lobbsAppResolveUsername(ctx, user, inboxUuid))
            return;
        const char *beforeNum = lobbsArgPeek(ctx);
        if (!lobbsArgShiftUint(ctx, idx)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
        markRead = false;
    } else {
        const char *beforeNum = lobbsArgPeek(ctx);
        if (!lobbsArgShiftUint(ctx, idx)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
    }

    auto mailMessages = mail.getAllMailForUser(inboxUuid);
    if (idx == 0 || idx > mailMessages.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid message number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const LoScalar &m = mailMessages[idx - 1];
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
    char header[LOBBS_USERNAME_BUFFER_SIZE + LOBBS_TIME_AGO_BUFFER_SIZE + 16];
    lobbsAppUsernameForUuid(ctx.mod, MailDal::mailFromUuid(m), name, sizeof(name));
    MailDal::mailMessage(m, body, sizeof(body));
    lobbsAppTimeAgo(MailDal::mailTimestamp(m), when, sizeof(when));
    snprintf(header, sizeof(header), "From: @%s (%s)", name, when);

    LoScalar rec;
    rec.setUint64(LODB_F_ID, MailDal::mailUuid(m));
    rec.setString(LODB_F_TITLE, header);
    rec.setString(LODB_F_DESCRIPTION, body);
    rec.setUint32(LODB_F_CREATED, MailDal::mailTimestamp(m));
    if (markRead && inboxUuid == lobbsCtxUserUuid(ctx))
        mail.markMailAsRead(MailDal::mailUuid(m));

    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubReadCmd(LoBBSCommandCtx &ctx)
{
    mailSubRead(ctx, false);
}

static void mailSubUnread(LoBBSCommandCtx &ctx)
{
    uint32_t idx = 0;
    if (!lobbsMsgShiftIndex(ctx, 1, "Usage: /mail unread N", "Invalid message number.", idx))
        return;
    MailDal &mail = ctx.mod->mail().dal();
    auto mailMessages = mail.getAllMailForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > mailMessages.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid message number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    bool ok = mail.markMailAsUnread(MailDal::mailUuid(mailMessages[idx - 1]));
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, ok ? "Marked unread." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubDelete(LoBBSCommandCtx &ctx)
{
    uint32_t idx = 0;
    if (!lobbsMsgShiftIndex(ctx, 1, "Usage: /mail delete N", "Invalid message number.", idx))
        return;
    LoBBSResponse resp;
    lobbsRecordPush(resp.records,
                    ctx.mod->mail().dal().deleteMailInboxIndex(lobbsCtxUserUuid(ctx), idx) ? "Deleted." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubSend(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "send") != 0) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /mail send user message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const char *user = lobbsArgShift(ctx);
    const char *body = lobbsArgRest(ctx);
    if (!user || !body[0]) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /mail send user message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t toUuid = 0;
    if (!lobbsAppResolveUsername(ctx, user, toUuid))
        return;
    LoBBSResponse resp;
    LoDbError err = ctx.mod->mail().dal().sendMail(lobbsCtxUserUuid(ctx), toUuid, body);
    lobbsRecordPush(resp.records, err == LODB_OK ? "Mail sent." : lobbsDbErrorText(err, "Failed to send mail."));
    lobbsCommandReplyResponse(ctx, resp);
}

static const LoBBSVerb mailVerbs[] = {
    {"list", mailSubList, LOBBS_V_LOGIN, "list — inbox"},
    {"list", nullptr, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "list user — sysop: another inbox"},
    {"read", mailSubReadCmd, LOBBS_V_LOGIN, "read N — read message (/mail N)"},
    {"read", nullptr, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "read user N — sysop: read without marking read"},
    {"unread", mailSubUnread, LOBBS_V_LOGIN, "unread N — mark message unread"},
    {"delete", mailSubDelete, LOBBS_V_LOGIN, "delete N — delete message from inbox"},
    {"send", mailSubSend, LOBBS_V_LOGIN, "send user message... — send mail"},
};

static void handleMail(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;

    if (lobbsArgPeekIsUint(ctx)) {
        mailSubRead(ctx, true);
        return;
    }

    if (!lobbsDispatchSub(ctx, mailVerbs, sizeof(mailVerbs) / sizeof(mailVerbs[0]))) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Unknown command. Try /help mail");
        lobbsCommandReplyResponse(ctx, resp);
    }
}

static void slashMail(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "mail"))
        return;
    handleMail(*ctx);
}

static void filterMailHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRecordPush(topics, "mail", "read and send mail");
}

static void filterMailHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "mail", mailVerbs, sizeof(mailVerbs) / sizeof(mailVerbs[0]));
}

static void filterMailStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char title[64];
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->mail().dal().countUnreadMail(lobbsCtxUserUuid(*ctx));
        snprintf(title, sizeof(title), "Mail");
        snprintf(value, sizeof(value), "%u unread", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->mail().dal().countAllMail();
        snprintf(title, sizeof(title), "Mail");
        snprintf(value, sizeof(value), "%u total", (unsigned)n);
    }
    lobbsRecordPush(lines, title, value);
}

#if LOBBS_SEED
#include "MailSeed.h"
static void actionMailSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->mod)
        lobbsSeedMail(*ctx->mod);
}
#endif

void lobbsMailRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashMail, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterMailHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterMailHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterMailStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
#if LOBBS_SEED
    lobbsAddAction("seed", actionMailSeed, LOBBS_HOOK_PRIORITY_FEATURE);
#endif
}

#endif
