#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandRegistry.h"
#include "LoBBSHooks.h"
#include "LoBBSInstall.h"
#include "LoBBSModule.h"
#include "LoBBSReply.h"
#include "LoBBSReplyCache.h"
#include "LoBBSResponse.h"
#include "gps/RTC.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>

#include "LoBBSStackGuard.h"

static void lobbsSkipWs(char *&p)
{
    while (p && (*p == ' ' || *p == '\t'))
        p++;
}

static bool lobbsParsePageToken(const char *tok, uint32_t &pageOut)
{
    if (!tok || (tok[0] != 'p' && tok[0] != 'P') || !tok[1])
        return false;
    for (size_t i = 1; tok[i]; i++) {
        if (!isdigit((unsigned char)tok[i]))
            return false;
    }
    pageOut = (uint32_t)atoi(tok + 1);
    if (pageOut == 0)
        pageOut = 1;
    return true;
}

void lobbsCommandReply(LoBBSCommandCtx &ctx, const char *body)
{
    if (!body)
        body = "";
    LoBBSResponse resp;
    if (strchr(body, '\n')) {
        LoScalar rec;
        rec.setString(LODB_F_DESCRIPTION, body);
        lobbsResponseAppendRecord(resp, rec);
    } else {
        lobbsRecordPush(resp.records, body);
    }
    lobbsCommandReplyResponse(ctx, resp);
}

const char *lobbsArgShift(LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return nullptr;
    lobbsSkipWs(ctx.rest);
    if (!ctx.rest[0])
        return nullptr;
    char *start = ctx.rest;
    while (*ctx.rest && *ctx.rest != ' ' && *ctx.rest != '\t')
        ctx.rest++;
    if (*ctx.rest) {
        *ctx.rest++ = '\0';
        lobbsSkipWs(ctx.rest);
    }
    return start;
}

/** Next token without consuming it. Valid until the next peek; long tokens are truncated. */
const char *lobbsArgPeek(const LoBBSCommandCtx &ctx)
{
    static char tok[64];
    if (!ctx.rest)
        return nullptr;
    char *p = ctx.rest;
    lobbsSkipWs(p);
    if (!p[0])
        return nullptr;
    size_t n = 0;
    while (p[n] && p[n] != ' ' && p[n] != '\t' && n + 1 < sizeof(tok)) {
        tok[n] = p[n];
        n++;
    }
    tok[n] = '\0';
    return tok;
}

const char *lobbsArgRest(LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return "";
    lobbsSkipWs(ctx.rest);
    return ctx.rest;
}

bool lobbsArgHasMore(const LoBBSCommandCtx &ctx)
{
    return lobbsArgPeek(ctx) != nullptr;
}

static bool lobbsTokIsUint(const char *tok, uint32_t &out)
{
    if (!tok || !tok[0])
        return false;
    char *end = nullptr;
    unsigned long v = strtoul(tok, &end, 10);
    if (end == tok || (end && *end != '\0'))
        return false;
    if (v > UINT32_MAX)
        return false;
    out = (uint32_t)v;
    return true;
}

static bool lobbsArgPeekUint(const LoBBSCommandCtx &ctx, uint32_t &out)
{
    return lobbsTokIsUint(lobbsArgPeek(ctx), out);
}

static bool lobbsCommandIsPageOnly(const char *verb, const char *rest, uint32_t &pageOut)
{
    pageOut = 1;
    if (!verb || !verb[0])
        return false;
    if (lobbsParsePageToken(verb, pageOut))
        return !rest || !rest[0];
    if ((verb[0] == 'p' || verb[0] == 'P') && verb[1] == '\0' && lobbsTokIsUint(rest, pageOut))
        return true;
    return false;
}

bool lobbsArgPeekIsUint(const LoBBSCommandCtx &ctx)
{
    uint32_t dummy = 0;
    return lobbsArgPeekUint(ctx, dummy);
}

bool lobbsArgShiftUint(LoBBSCommandCtx &ctx, uint32_t &out)
{
    const char *tok = lobbsArgShift(ctx);
    return lobbsTokIsUint(tok, out);
}

int lobbsArgShiftMany(LoBBSCommandCtx &ctx, const char *out[], int maxOut)
{
    if (!out || maxOut <= 0)
        return 0;
    int n = 0;
    while (n < maxOut) {
        const char *t = lobbsArgShift(ctx);
        if (!t)
            break;
        out[n++] = t;
    }
    return n;
}

static bool lobbsHelpQueryMatches(const char *query, const char *prefix)
{
    if (!query || !prefix || !prefix[0])
        return false;
    if (strcasecmp(query, prefix) == 0)
        return true;
    size_t plen = strlen(prefix);
    if (strncasecmp(query, prefix, plen) != 0)
        return false;
    return query[plen] == ' ';
}

bool lobbsSlashVerbIs(const LoScalar &args, const char *verb)
{
    std::string v;
    return verb && args.getString(LOBBS_ARG_VERB, v) && strcasecmp(v.c_str(), verb) == 0;
}

static bool lobbsHelpVisible(const LoBBSCommandCtx *ctx, uint8_t flags)
{
    return !(flags & LOBBS_V_SYSOP) || (ctx && lobbsCtxIsSysop(*ctx));
}

void lobbsHelpForTable(const LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args, const char *topic,
                       const LoBBSVerb *table, size_t count)
{
    std::string query;
    if (!topic || !table || !args.getString(LODB_F_TITLE, query) || !lobbsHelpQueryMatches(query.c_str(), topic))
        return;

    std::string body;
    bool whole = strcasecmp(query.c_str(), topic) == 0;
    for (size_t i = 0; i < count; i++) {
        if (!table[i].help || !table[i].verb || !lobbsHelpVisible(ctx, table[i].flags))
            continue;
        if (!whole) {
            char path[80];
            snprintf(path, sizeof(path), "%s %s", topic, table[i].verb);
            if (strcasecmp(query.c_str(), path) != 0)
                continue;
        }
        if (!body.empty())
            body.push_back('\n');
        body += table[i].help;
        if (!whole)
            break;
    }
    if (!body.empty())
        value.setString(LODB_F_DESCRIPTION, body);
}

bool lobbsDispatchSub(LoBBSCommandCtx &ctx, const LoBBSVerb *table, size_t count)
{
    if (!table || count == 0)
        return false;

    const char *sub = lobbsArgPeek(ctx);
    if (!sub || strcasecmp(sub, "list") == 0) {
        for (size_t i = 0; i < count; i++) {
            if (!table[i].verb || !table[i].fn || strcasecmp(table[i].verb, "list") != 0)
                continue;
            if (sub && strcasecmp(sub, "list") == 0)
                lobbsArgShift(ctx);
            if ((table[i].flags & LOBBS_V_LOGIN) && !lobbsCommandRequireLogin(ctx))
                return true;
            if ((table[i].flags & LOBBS_V_SYSOP) && !lobbsCommandRequireSysop(ctx))
                return true;
            table[i].fn(ctx);
            return true;
        }
    }

    if (!sub)
        return false;

    for (size_t i = 0; i < count; i++) {
        if (!table[i].verb || !table[i].fn || strcasecmp(sub, table[i].verb) != 0)
            continue;
        if ((table[i].flags & LOBBS_V_LOGIN) && !lobbsCommandRequireLogin(ctx))
            return true;
        if ((table[i].flags & LOBBS_V_SYSOP) && !lobbsCommandRequireSysop(ctx))
            return true;
        table[i].fn(ctx);
        return true;
    }
    return false;
}

bool lobbsCtxLoggedIn(const LoBBSCommandCtx &ctx)
{
    return ctx.session.userUuid != 0;
}

bool lobbsCtxIsSysop(const LoBBSCommandCtx &ctx)
{
    return lobbsCtxLoggedIn(ctx) && ctx.session.isSysop;
}

uint64_t lobbsCtxUserUuid(const LoBBSCommandCtx &ctx)
{
    return ctx.session.userUuid;
}

bool lobbsCtxUsername(const LoBBSCommandCtx &ctx, char *buf, size_t bufCap)
{
    if (!buf || bufCap == 0)
        return false;
    buf[0] = '\0';
    if (!lobbsCtxLoggedIn(ctx))
        return false;
    strncpy(buf, ctx.session.username, bufCap - 1);
    buf[bufCap - 1] = '\0';
    return buf[0] != '\0';
}

bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx)
{
    if (lobbsCtxLoggedIn(ctx))
        return true;
    LoBBSResponse resp;
    lobbsResponseSetError(resp, "Login required.");
    lobbsCommandReplyResponse(ctx, resp);
    return false;
}

bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx)
{
    if (lobbsCtxIsSysop(ctx))
        return true;
    LoBBSResponse resp;
    lobbsResponseSetError(resp, "SysOp only.");
    lobbsCommandReplyResponse(ctx, resp);
    return false;
}

static bool lobbsPeelSlashLine(char *line, LoBBSCommandCtx &ctx, char **verbOut, char **restOut)
{
    ctx.reqId = 0;
    if (!line || line[0] != '/')
        return false;
    char *p = line + 1;
    if (isdigit((unsigned char)*p)) {
        char *start = p;
        while (isdigit((unsigned char)*p))
            p++;
        if (*p == ' ') {
            *p++ = '\0';
            ctx.reqId = (uint32_t)strtoul(start, nullptr, 10);
            lobbsSkipWs(p);
        } else {
            p = start;
        }
    }
    lobbsSkipWs(p);
    if (!*p)
        return false;
    *verbOut = p;
    while (*p && *p != ' ' && *p != '\t')
        p++;
    if (*p) {
        *p++ = '\0';
        lobbsSkipWs(p);
    }
    *restOut = p;
    return true;
}

void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, const LoBBSSession &session, char *line)
{
    LoBBSCommandCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.session = session;
    ctx.page = 1;

    lobbsReplyCacheGc(getTime());

    char *verb = nullptr;
    char *rest = nullptr;
    if (!lobbsPeelSlashLine(line, ctx, &verb, &rest))
        return;
    if (!verb[0]) {
        lobbsCommandReplyError(ctx, "Missing command.");
        return;
    }

    uint32_t pageOnly = 1;
    if (lobbsCommandIsPageOnly(verb, rest, pageOnly)) {
        ctx.page = pageOnly;
        lobbsReplySendCachedPage(ctx);
        return;
    }

    const LoBBSInstallState install = mod->installState();
    const bool isHelp = strcasecmp(verb, "help") == 0;
    const bool isInstall = strcasecmp(verb, "install") == 0;
    if (install != LoBBSInstallState::Ready && !isHelp && !isInstall) {
        if (install == LoBBSInstallState::Blank) {
            char mounts[48];
            lobbsInstallMountList(ctx, mounts, sizeof(mounts));
            char msg[160];
            snprintf(msg, sizeof(msg), "New LoBBS. Sysop: /install <%s> <user> <pass>", mounts[0] ? mounts : "flash");
            lobbsCommandReplyError(ctx, msg);
        } else {
            char msg[96];
            snprintf(msg, sizeof(msg), "LoBBS offline: cannot open %s.", lobbsInstallOfflineHome(*mod));
            lobbsCommandReplyError(ctx, msg);
        }
        return;
    }

    ctx.rest = rest;
    LoScalar args;
    args.setString(LOBBS_ARG_VERB, verb);
    args.setString(LOBBS_ARG_REST, rest);
    lobbsDoAction("slash_cmd", ctx, args);
}

#endif
