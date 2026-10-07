#include "AuthCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSResponse.h"
#include "../AppUtil.h"
#include "../Config/ConfigCommon.h"
#include "AuthDal.h"
#include "AuthRecords.h"
#include "mesh/NodeDB.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <string>

#include "LoBBSStackGuard.h"

static bool passwordLongEnough(LoBBSCommandCtx &ctx, const char *password)
{
    const uint32_t minLen = lobbsConfigGet(ctx, "password.min");
    return password && strlen(password) >= minLen;
}

static void authAppendUserRecord(LoBBSResponse &resp, const LoScalar &user)
{
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    AuthDal::userUsername(user, name, sizeof(name));
    std::string line = name;
    if (AuthDal::userIsSysop(user))
        line += "*";
    LoScalar rec;
    rec.setString(LODB_F_TITLE, line.c_str());
    rec.setUint64(LODB_F_ID, AuthDal::userUuid(user));
    rec.setBool(AuthUser::FIELD_SYSOP, AuthDal::userIsSysop(user));
    lobbsResponseAppendRecord(resp, rec);
}

static void handleWhoami(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    char uname[LOBBS_USERNAME_BUFFER_SIZE];
    lobbsCtxUsername(ctx, uname, sizeof(uname));
    char title[96];
    snprintf(title, sizeof(title), "Logged in as %s.", uname);
    LoScalar rec;
    rec.setString(LODB_F_TITLE, title);
    if (ctx.session.isSysop)
        rec.setString(LODB_F_DESCRIPTION, "You are the SysOp.");
    rec.setUint64(LODB_F_ID, lobbsCtxUserUuid(ctx));
    rec.setBool(AuthUser::FIELD_SYSOP, ctx.session.isSysop);
    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static void handleLogout(LoBBSCommandCtx &ctx)
{
    ctx.mod->auth().dal().logoutUser(ctx.session.nodeId);
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, "Goodbye!");
    lobbsCommandReplyResponse(ctx, resp);
}

static void handleLogin(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    const char *username = lobbsArgShift(ctx);
    const char *password = lobbsArgShift(ctx);
    LoBBSResponse resp;
    if (!username || !password) {
        lobbsResponseSetError(resp, "Usage: /login user password");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (!auth.isValidUsername(username)) {
        lobbsResponseSetError(resp, "Invalid username.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (!passwordLongEnough(ctx, password) || !auth.isValidPassword(password)) {
        lobbsResponseSetError(resp, "Password too short.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoScalar dbUser;
    if (auth.loadUserByUsername(username, &dbUser)) {
        if (!auth.verifyPassword(&dbUser, password)) {
            lobbsResponseSetError(resp, "Invalid password");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
        const uint32_t sessionKey = getFrom(ctx.mp);
        if (!auth.loginUser(username, sessionKey)) {
            lobbsResponseSetError(resp, "Error creating session");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
    } else if (LoDbError err = auth.createUser(username, password, getFrom(ctx.mp))) {
        lobbsResponseSetError(resp, lobbsDbErrorText(err, "Error creating account"));
        lobbsCommandReplyResponse(ctx, resp);
        return;
    } else {
        auth.loadUserByUsername(username, &dbUser);
    }
    char title[96];
    snprintf(title, sizeof(title), "Welcome %s!", username);
    LoScalar rec;
    rec.setString(LODB_F_TITLE, title);
    if (AuthDal::userIsSysop(dbUser))
        rec.setString(LODB_F_DESCRIPTION, "You are the SysOp.");
    rec.setBool(AuthUser::FIELD_SYSOP, AuthDal::userIsSysop(dbUser));
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static bool passwdApply(AuthDal &auth, const char *username, const char *newPass, const char *confirm, LoBBSCommandCtx &ctx)
{
    LoBBSResponse resp;
    if (strcmp(newPass, confirm) != 0) {
        lobbsResponseSetError(resp, "Passwords do not match.");
        lobbsCommandReplyResponse(ctx, resp);
        return false;
    }
    if (!passwordLongEnough(ctx, newPass) || !auth.isValidPassword(newPass)) {
        lobbsResponseSetError(resp, "Password too short.");
        lobbsCommandReplyResponse(ctx, resp);
        return false;
    }
    if (LoDbError err = auth.setPasswordByUsername(username, newPass)) {
        lobbsResponseSetError(resp, lobbsDbErrorText(err, "Error updating password."));
        lobbsCommandReplyResponse(ctx, resp);
        return false;
    }
    lobbsRecordPush(resp.records, "Password updated.");
    lobbsCommandReplyResponse(ctx, resp);
    return true;
}

static void handlePasswd(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    const char *a = lobbsArgShift(ctx);
    const char *b = lobbsArgShift(ctx);
    const char *c = lobbsArgShift(ctx);
    if (a && b && !c) {
        if (!lobbsCommandRequireLogin(ctx))
            return;
        char uname[LOBBS_USERNAME_BUFFER_SIZE];
        lobbsCtxUsername(ctx, uname, sizeof(uname));
        passwdApply(auth, uname, a, b, ctx);
        return;
    }
    if (a && b && c) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        if (!auth.isValidUsername(a)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, "Invalid username.");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
        LoScalar dbUser;
        if (!auth.loadUserByUsername(a, &dbUser)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, "User not found.");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
        passwdApply(auth, a, b, c, ctx);
        return;
    }
    LoBBSResponse resp;
    lobbsResponseSetError(resp, "Usage: /passwd new confirm\nUsage: /passwd user new confirm (sysop)");
    lobbsCommandReplyResponse(ctx, resp);
}

static void usersSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);
    AuthDal &auth = ctx.mod->auth().dal();
    auto users = auth.listUsers(nullptr);
    LoBBSResponse resp;
    if (users.empty()) {
        lobbsResponseSetError(resp, "No users found");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (const LoScalar &user : users)
        authAppendUserRecord(resp, user);
    lobbsCommandReplyResponse(ctx, resp);
}

static void usersSubFind(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "find") != 0) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /users find text");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    char filterBuf[128];
    strncpy(filterBuf, lobbsArgRest(ctx), sizeof(filterBuf) - 1);
    filterBuf[sizeof(filterBuf) - 1] = '\0';

    AuthDal &auth = ctx.mod->auth().dal();
    auto users = auth.listUsers(filterBuf[0] ? filterBuf : nullptr);
    LoBBSResponse resp;
    if (users.empty()) {
        lobbsResponseSetError(resp, "No users match filter.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (const LoScalar &user : users)
        authAppendUserRecord(resp, user);
    lobbsCommandReplyResponse(ctx, resp);
}

static void usersSubKick(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "kick") != 0 || !user) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /users kick user");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, ctx.mod->auth().dal().kickUserByUsername(user) ? "Sessions cleared." : "User not found.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void usersSubPromote(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "promote") != 0 || !user) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /users promote user");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoBBSResponse resp;
    LoDbError err = ctx.mod->auth().dal().setUserSysopByUsername(user, true);
    lobbsRecordPush(resp.records, err == LODB_OK ? "Promoted." : lobbsDbErrorText(err, "User not found."));
    lobbsCommandReplyResponse(ctx, resp);
}

static void usersSubDemote(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "demote") != 0 || !user) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /users demote user");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    AuthDal &auth = ctx.mod->auth().dal();
    LoScalar target;
    LoBBSResponse resp;
    if (!auth.loadUserByUsername(user, &target)) {
        lobbsResponseSetError(resp, "User not found.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (AuthDal::userIsSysop(target) && auth.countSysopUsers() <= 1) {
        lobbsResponseSetError(resp, "Cannot demote last SysOp.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoDbError err = auth.setUserSysopByUsername(user, false);
    lobbsRecordPush(resp.records, err == LODB_OK ? "Demoted." : lobbsDbErrorText(err));
    lobbsCommandReplyResponse(ctx, resp);
}

static const LoBBSVerb usersVerbs[] = {
    {"list", usersSubList, LOBBS_V_LOGIN, "list — all users"},
    {"find", usersSubFind, LOBBS_V_LOGIN, "find text — filter by username"},
    {"kick", usersSubKick, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "kick user — sysop: clear sessions"},
    {"promote", usersSubPromote, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "promote user — sysop: grant sysop"},
    {"demote", usersSubDemote, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "demote user — sysop: revoke sysop"},
};

static const LoBBSVerb whoamiHelpVerbs[] = {
    {"whoami", nullptr, LOBBS_V_LOGIN, "whoami — show logged-in user"},
};

static const LoBBSVerb loginHelpVerbs[] = {
    {"login", nullptr, 0, "login user pass — sign in or create account (pass min 5 chars)"},
};

static const LoBBSVerb logoutHelpVerbs[] = {
    {"logout", nullptr, LOBBS_V_LOGIN, "logout — end session"},
};

static const LoBBSVerb passwdHelpVerbs[] = {
    {"passwd", nullptr, LOBBS_V_LOGIN, "passwd new confirm — change your password"},
    {"passwd", nullptr, LOBBS_V_LOGIN | LOBBS_V_SYSOP, "passwd user new confirm — sysop: reset user password"},
};

static void handleUsers(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    if (!lobbsDispatchSub(ctx, usersVerbs, sizeof(usersVerbs) / sizeof(usersVerbs[0]))) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Unknown command. Try /help users");
        lobbsCommandReplyResponse(ctx, resp);
    }
}

static void slashAuth(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    std::string v;
    if (!ctx || !args.getString(LOBBS_ARG_VERB, v))
        return;
    LoBBSCommandCtx &c = *ctx;
    const char *verb = v.c_str();
    if (strcasecmp(verb, "whoami") == 0) {
        handleWhoami(c);
        return;
    }
    if (strcasecmp(verb, "login") == 0) {
        handleLogin(c);
        return;
    }
    if (strcasecmp(verb, "logout") == 0) {
        handleLogout(c);
        return;
    }
    if (strcasecmp(verb, "passwd") == 0) {
        handlePasswd(c);
        return;
    }
    if (strcasecmp(verb, "users") == 0) {
        handleUsers(c);
        return;
    }
}

static void filterAuthConfigKeys(LoBBSCommandCtx *ctx, std::vector<LoScalar> &keys, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsConfigPushKey(keys, "session.max", LOBBS_SESSION_DEFAULT_MAX, 1, 64, "Max concurrent login sessions");
    lobbsConfigPushKey(keys, "session.idle", LOBBS_SESSION_DEFAULT_IDLE_SEC, 60, 30 * 86400,
                       "Idle seconds before session expires");
    lobbsConfigPushKey(keys, "password.min", 5, 1, 32, "Minimum password length");
}

static void actionAuthConfigChanged(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !ctx->mod)
        return;
    std::string key;
    if (!args.getString(LODB_F_TITLE, key))
        return;
    if (key == "session.max" || key == "session.idle") {
        AuthDal::SessionConfig cfg = {lobbsConfigGet(*ctx, "session.max"), lobbsConfigGet(*ctx, "session.idle")};
        ctx->mod->auth().dal().applySessionConfig(cfg);
    }
}

static void filterAuthHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx)
        return;
    if (lobbsCtxLoggedIn(*ctx)) {
        lobbsRecordPush(topics, "whoami", "show your account");
        lobbsRecordPush(topics, "logout", "sign out");
        lobbsRecordPush(topics, "passwd", "change your password");
        lobbsRecordPush(topics, "users", "list users");
    } else {
        lobbsRecordPush(topics, "login", "sign in or create an account");
    }
}

static void filterAuthHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "whoami", whoamiHelpVerbs, sizeof(whoamiHelpVerbs) / sizeof(whoamiHelpVerbs[0]));
    lobbsHelpForTable(ctx, value, args, "login", loginHelpVerbs, sizeof(loginHelpVerbs) / sizeof(loginHelpVerbs[0]));
    lobbsHelpForTable(ctx, value, args, "logout", logoutHelpVerbs, sizeof(logoutHelpVerbs) / sizeof(logoutHelpVerbs[0]));
    lobbsHelpForTable(ctx, value, args, "passwd", passwdHelpVerbs, sizeof(passwdHelpVerbs) / sizeof(passwdHelpVerbs[0]));
    lobbsHelpForTable(ctx, value, args, "users", usersVerbs, sizeof(usersVerbs) / sizeof(usersVerbs[0]));
}

static void filterAuthStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char title[32];
    char value[32];
    uint32_t n = ctx->mod->auth().dal().countAllUsers();
    snprintf(title, sizeof(title), "Users");
    snprintf(value, sizeof(value), "%u total", (unsigned)n);
    lobbsRecordPush(lines, title, value);
}

static bool displayAuthRecord(const LoScalar &record, std::string &lineOut)
{
    if (record.has(LODB_F_ID) && record.has(AuthUser::FIELD_SYSOP) && record.has(LODB_F_TITLE)) {
        std::string title;
        if (record.getString(LODB_F_TITLE, title)) {
            lineOut = title;
            return true;
        }
    }
    std::string title;
    std::string body;
    if (record.getString(LODB_F_TITLE, title) && record.getString(LODB_F_DESCRIPTION, body) &&
        (title.rfind("Welcome ", 0) == 0 || title.rfind("Logged in as ", 0) == 0)) {
        lineOut = title + "\n" + body;
        return true;
    }
    return false;
}

static void displayAuthHuman(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &record)
{
    (void)ctx;
    std::string line;
    if (displayAuthRecord(record, line))
        value.setString(LODB_F_TITLE, line);
}

#if LOBBS_SEED
#include "AuthSeed.h"
static void actionAuthSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->mod)
        lobbsSeedAuth(*ctx->mod);
}
#endif

void lobbsAuthRegisterCommands()
{
    lobbsAddFilter("config_keys", filterAuthConfigKeys, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddAction("config_changed", actionAuthConfigChanged, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddAction("slash_cmd", slashAuth, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("help_topics", filterAuthHelpTopics, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("help_for_topic", filterAuthHelpForTopic, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("status_lines", filterAuthStatusLines, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("display_human", displayAuthHuman, LOBBS_HOOK_PRIORITY_AUTH);
#if LOBBS_SEED
    lobbsAddAction("seed", actionAuthSeed, LOBBS_HOOK_PRIORITY_AUTH);
#endif
}

