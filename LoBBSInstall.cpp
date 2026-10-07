#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSInstall.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSConfig.h"
#include "LoBBSHooks.h"
#include "LoBBSModule.h"
#include "apps/AppUtil.h"
#include "apps/Auth/AuthDal.h"
#include "configuration.h"
#include "mesh/NodeDB.h"
#include "mesh/Router.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <lofs/LoFS.h>
#include <loscalar/LoScalar.h>

#include "LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

static LoBBSInstallState gInstallState = LoBBSInstallState::Blank;
static char gInstallRoot[32] = {0};
static char gOfflineHome[32] = {0};

LoBBSInstallState lobbsInstallState(const LoBBSModule &mod)
{
    (void)mod;
    return gInstallState;
}

const char *lobbsInstallRoot(const LoBBSModule &mod)
{
    (void)mod;
    return gInstallRoot;
}

const char *lobbsInstallOfflineHome(const LoBBSModule &mod)
{
    (void)mod;
    return gOfflineHome;
}

static void filterInstallMountsDefault(LoBBSCommandCtx *ctx, std::vector<LoScalar> &value, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    LoFS::eachPresentMount(
        [](void *v, const char *name) {
            if (!LoFS::mountDbSafe(name))
                return;
            LoScalar rec;
            rec.setString(LODB_F_TITLE, name);
            ((std::vector<LoScalar> *)v)->push_back(rec);
        },
        &value);
}

static std::vector<LoScalar> lobbsInstallMounts(LoBBSCommandCtx &ctx)
{
    std::vector<LoScalar> mounts;
    lobbsApplyFilter("install_mounts", ctx, mounts, LoScalar());
    return mounts;
}

void lobbsInstallMountList(LoBBSCommandCtx &ctx, char *out, size_t cap)
{
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    std::string name;
    for (const LoScalar &rec : lobbsInstallMounts(ctx)) {
        if (!rec.getString(LODB_F_TITLE, name))
            continue;
        size_t len = strlen(out);
        if (len && len + 1 < cap)
            strncat(out, "|", cap - len - 1);
        len = strlen(out);
        strncat(out, name.c_str(), cap - len - 1);
    }
}

bool lobbsInstallHome(const char *root, char *out, size_t cap)
{
    if (!root || root[0] != '/' || !out)
        return false;
    int n = snprintf(out, cap, "%s/%s", root, LOBBS_HOME_DIR);
    return n > 0 && (size_t)n < cap;
}

static bool lobbsInstallMarkerPath(const char *root, char *out, size_t cap)
{
    int n = snprintf(out, cap, "%s/%s/%s", root, LOBBS_HOME_DIR, LOBBS_INSTALL_MARKER_NAME);
    return n > 0 && (size_t)n < cap;
}

bool lobbsInstallWriteMarker(const char *root)
{
    char path[48];
    if (!root || root[0] != '/' || !lobbsInstallMarkerPath(root, path, sizeof(path)))
        return false;
    LoScalar rec;
    rec.setString(LOBBS_INSTALL_FIELD_VERSION, LOBBS_VERSION);
    std::string line;
    if (!rec.encode(line, 256))
        return false;

    File f = LoFS::open(path, FILE_O_WRITE);
    if (!f)
        return false;
    size_t w = f.write((const uint8_t *)line.data(), line.size());
    f.flush();
    f.close();
    return w == line.size();
}

static bool lobbsOpenHome(LoBBSModule &mod, const char *root)
{
    char home[32];
    LoDb *db = mod.lodb();
    return db && lobbsInstallHome(root, home, sizeof(home)) && db->open(home) == LODB_OK;
}

void lobbsInstallInit(LoBBSModule &mod)
{
    gInstallState = LoBBSInstallState::Blank;
    gInstallRoot[0] = '\0';
    gOfflineHome[0] = '\0';

    LoBBSCommandCtx ctx;
    ctx.mod = &mod;
    char root[16] = {0};
    std::string name;
    for (const LoScalar &rec : lobbsInstallMounts(ctx)) {
        char candidate[16], marker[48];
        if (!rec.getString(LODB_F_TITLE, name))
            continue;
        snprintf(candidate, sizeof(candidate), "/%s", name.c_str());
        if (!lobbsInstallMarkerPath(candidate, marker, sizeof(marker)) || !LoFS::exists(marker))
            continue;
        if (root[0])
            LOG_WARN("LoBBS: ignoring second install at %s (using %s)", candidate, root);
        else
            strncpy(root, candidate, sizeof(root) - 1);
    }
    if (!root[0]) {
        LOG_INFO("LoBBS: blank (no install found)");
        return;
    }

    if (!lobbsOpenHome(mod, root)) {
        gInstallState = LoBBSInstallState::Offline;
        lobbsInstallHome(root, gOfflineHome, sizeof(gOfflineHome));
        LOG_ERROR("LoBBS offline: failed to open database at %s", gOfflineHome);
        return;
    }

    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    lobbsInstallDatabaseOpened(mod);
    LOG_INFO("LoBBS ready at %s", root);
}

void lobbsInstallDatabaseOpened(LoBBSModule &mod)
{
    mod.auth().dal().clearSessions();
    mod.config().dal().notifyDatabaseOpened(mod);
}

bool lobbsInstallAuthorized(const meshtastic_MeshPacket &mp)
{
    if (mp.from == 0)
        return true;
#if !MESHTASTIC_EXCLUDE_PKI
    if (mp.pki_encrypted) {
        if ((config.security.admin_key[0].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[0].bytes, 32) == 0) ||
            (config.security.admin_key[1].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[1].bytes, 32) == 0) ||
            (config.security.admin_key[2].size == 32 && memcmp(mp.public_key.bytes, config.security.admin_key[2].bytes, 32) == 0))
            return true;
    }
#endif
    return false;
}

static bool lobbsInstallLocToRoot(LoBBSCommandCtx &ctx, const char *loc, char *rootOut, size_t cap)
{
    if (!loc || !rootOut)
        return false;
    std::string name;
    for (const LoScalar &rec : lobbsInstallMounts(ctx)) {
        if (rec.getString(LODB_F_TITLE, name) && name == loc && LoFS::mountPresent(loc)) {
            snprintf(rootOut, cap, "/%s", loc);
            return true;
        }
    }
    return false;
}

static void handleInstall(LoBBSCommandCtx &ctx)
{
    if (gInstallState != LoBBSInstallState::Blank) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Already installed at %s.", gInstallRoot[0] ? gInstallRoot : gOfflineHome);
        lobbsCommandReplyError(ctx, msg);
        return;
    }
    if (!lobbsInstallAuthorized(*ctx.mp)) {
        lobbsCommandReplyError(ctx, "Not authorized.");
        return;
    }

    const char *loc = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    const char *pass = lobbsArgShift(ctx);
    if (!loc || !user || !pass || lobbsArgHasMore(ctx)) {
        char mounts[48];
        lobbsInstallMountList(ctx, mounts, sizeof(mounts));
        char msg[96];
        snprintf(msg, sizeof(msg), "Usage: /install <%s> <user> <pass>", mounts);
        lobbsCommandReplyError(ctx, msg);
        return;
    }

    char root[16];
    if (!lobbsInstallLocToRoot(ctx, loc, root, sizeof(root))) {
        lobbsCommandReplyError(ctx, "Mount not available.");
        return;
    }

    if (!lobbsOpenHome(*ctx.mod, root)) {
        lobbsCommandReplyError(ctx, "Failed to open database.");
        return;
    }

    AuthDal &auth = ctx.mod->auth().dal();
    lobbsInstallDatabaseOpened(*ctx.mod);
    const bool hasUsers = auth.countAllUsers() > 0;
    if (hasUsers) {
        LoScalar row;
        if (!auth.loadUserByUsername(user, &row) || !AuthDal::userIsSysop(row) || !auth.verifyPassword(&row, pass)) {
            lobbsCommandReplyError(ctx, "Existing database: sysop credentials required.");
            return;
        }
        auth.loginUser(user, getFrom(ctx.mp));
    } else if (LoDbError err = auth.createUser(user, pass, getFrom(ctx.mp), true)) {
        lobbsCommandReplyError(ctx, lobbsDbErrorText(err, "Failed to create sysop."));
        return;
    }

    if (!lobbsInstallWriteMarker(root)) {
        lobbsCommandReplyError(ctx, "Failed to write install marker.");
        return;
    }

    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    lobbsCommandReply(ctx, "Installed.");
}

static void slashInstall(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    std::string v;
    if (!ctx || !args.getString(LOBBS_ARG_VERB, v))
        return;
    if (strcasecmp(v.c_str(), "install") != 0)
        return;
    handleInstall(*ctx);
}

void lobbsInstallRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashInstall, LOBBS_HOOK_PRIORITY_AUTH - 1);
    lobbsAddFilter("install_mounts", filterInstallMountsDefault);
}

#if LOBBS_SEED
void lobbsInstallAutoSeed(LoBBSModule &mod)
{
    LOBBS_BOOT_STEP("autoseed: enter");
    gInstallState = LoBBSInstallState::Blank;
    gInstallRoot[0] = '\0';
    gOfflineHome[0] = '\0';

    LoBBSCommandCtx ctx;
    ctx.mod = &mod;
    LOBBS_BOOT_STEP("autoseed: install_mounts");
    std::vector<LoScalar> mounts = lobbsInstallMounts(ctx);
    std::string name;
    if (mounts.empty() || !mounts[0].getString(LODB_F_TITLE, name)) {
        LOG_ERROR("LoBBS seed: no install mount");
        return;
    }
    char root[16];
    snprintf(root, sizeof(root), "/%s", name.c_str());
    char home[32];
    lobbsInstallHome(root, home, sizeof(home));
    LOG_INFO("LoBBS> autoseed: rmdir %s", home);
    LoFS::rmdir(home, true);
    LOG_INFO("LoBBS> autoseed: lodb open %s", home);
    if (!lobbsOpenHome(mod, root) || !lobbsInstallWriteMarker(root)) {
        LOG_ERROR("LoBBS seed install failed at %s", root);
        return;
    }
    LOBBS_BOOT_STEP("autoseed: lobbsInstallDatabaseOpened");
    lobbsInstallDatabaseOpened(mod);
    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    LOG_INFO("LoBBS seed install at %s", root);
}
#endif

#endif
