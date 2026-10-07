#include "FsCommands.h"
#include "../../core/LoBBSCommandRegistry.h"
#include "../../core/LoBBSHooks.h"
#include "../../core/LoBBSInstall.h"
#include "../../core/LoBBSKernel.h"
#include "../../core/LoBBSReply.h"
#include "../../core/LoBBSReplyCache.h"
#include "../../core/LoBBSResponse.h"
#include "../AppUtil.h"
#include "platforms/LoPlatform.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>
#include <lofs/Glob.h>
#include <lofs/LoFS.h>
#include <loutil/LoUtil.h>
#include <string>

#include "core/LoBBSStackGuard.h"

static constexpr int FS_LS_MAX_NAMES = 128;
static constexpr size_t FS_NAME_BYTES = 48;

struct FsLsCollect {
    const char *pattern;
    char names[FS_LS_MAX_NAMES][FS_NAME_BYTES];
    uint32_t sizes[FS_LS_MAX_NAMES];
    bool dirs[FS_LS_MAX_NAMES];
    int count;
    bool truncated;
};

/** One command at a time; ~7 KiB of entries must not live on the mesh handler stack. */
static FsLsCollect fsLsCollectScratch;

static bool fsLsCollectCb(void *ctx, const char *basename, bool isDirectory, uint32_t size)
{
    auto *c = (FsLsCollect *)ctx;
    if (!lobfsGlobMatch(c->pattern, basename))
        return true;
    if (c->count >= FS_LS_MAX_NAMES) {
        c->truncated = true;
        return false;
    }
    strncpy(c->names[c->count], basename, FS_NAME_BYTES - 1);
    c->names[c->count][FS_NAME_BYTES - 1] = '\0';
    c->sizes[c->count] = size;
    c->dirs[c->count] = isDirectory;
    c->count++;
    if (c->count >= FS_LS_MAX_NAMES) {
        c->truncated = true;
        return false;
    }
    return true;
}

/** Absolute `path` with an optional glob on the last name -> one file in `out`. Splits `path` in place while listing. */
static bool fsResolveOneFilePath(char *path, char *out, size_t outCap, LoBBSCommandCtx &ctx)
{
    switch (lobfsGlobResolveOneFile(path, out, outCap)) {
    case LobfsGlobResolve::Ok:
        return true;
    case LobfsGlobResolve::GlobBeforeLast:
        lobbsCommandReplyError(ctx, "glob only on the last name");
        return false;
    case LobfsGlobResolve::NoDir:
        lobbsCommandReplyError(ctx, "No such directory.");
        return false;
    case LobfsGlobResolve::NoMatch:
        lobbsCommandReplyError(ctx, "No match.");
        return false;
    case LobfsGlobResolve::ManyMatches:
        lobbsCommandReplyError(ctx, "Many matches.");
        return false;
    }
    return false;
}

/** Append `src`'s components to out[0..len), folding `.` and `..`. False if the result does not fit. */
static bool fsFoldPath(const char *src, char *out, size_t cap, size_t &len)
{
    const char *p = src;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t n = (size_t)(p - start);
        if (n == 1 && start[0] == '.')
            continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            while (len > 0 && out[len - 1] != '/')
                len--;
            if (len > 0)
                len--;
            continue;
        }
        if (len + 1 + n >= cap)
            return false;
        out[len++] = '/';
        memcpy(out + len, start, n);
        len += n;
    }
    return true;
}

/** Resolve a spec against the session cwd and fold `.` and `..`. False if the result does not fit. */
static bool fsResolvePath(const LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    size_t len = 0;
    if (spec[0] != '/' && !fsFoldPath(ctx.session.cwd, out, cap, len))
        return false;
    if (!fsFoldPath(spec, out, cap, len))
        return false;
    if (len == 0)
        out[len++] = '/';
    out[len] = '\0';
    return true;
}

static bool fsResolveOrReply(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    if (fsResolvePath(ctx, spec, out, cap))
        return true;
    lobbsCommandReplyError(ctx, "Path too long.");
    return false;
}

/** rm, rmdir and rmtree never resolve against cwd. */
static bool fsResolveAbsoluteOrReply(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    if (spec[0] != '/') {
        lobbsCommandReplyError(ctx, "Absolute path only.");
        return false;
    }
    return fsResolveOrReply(ctx, spec, out, cap);
}

static bool fsPathWithin(const char *path, const char *ancestor)
{
    if (!path || !ancestor)
        return false;
    size_t alen = strlen(ancestor);
    if (strcmp(path, ancestor) == 0)
        return true;
    if (alen == 0 || ancestor[0] != '/')
        return false;
    if (strncmp(path, ancestor, alen) != 0)
        return false;
    return path[alen] == '/' || path[alen] == '\0';
}

static void fsSessionPrep(LoBBSCommandCtx &ctx)
{
    if (LoFS::isDirectory(ctx.session.cwd))
        return;
    if (ctx.kernel->auth().dal().setSessionCwd(ctx.session.nodeId, "/"))
        strncpy(ctx.session.cwd, "/", sizeof(ctx.session.cwd));
}

static bool fsRefuseCwdTarget(LoBBSCommandCtx &ctx, const char *path, const char *verbLabel)
{
    if (!fsPathWithin(path, ctx.session.cwd))
        return false;
    (void)verbLabel;
    lobbsCommandReplyError(ctx, "Cannot remove the current directory.");
    return true;
}

static void handleLs(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);

    char spec[256];
    const char *pathTok = lobbsArgShift(ctx);
    if (!fsResolveOrReply(ctx, pathTok ? pathTok : ".", spec, sizeof(spec)))
        return;
    if (lobfsGlobBeforeLastName(spec)) {
        lobbsCommandReplyError(ctx, "glob only on the last name");
        return;
    }

    const char *dir = spec;
    const char *pat = "*";
    if (lobfsGlobLastComponentHasMeta(spec)) {
        char *lastSlash = strrchr(spec, '/');
        pat = lastSlash + 1;
        *lastSlash = '\0';
        if (lastSlash == spec)
            dir = "/";
    }

    fsLsCollectScratch = {};
    fsLsCollectScratch.pattern = pat;
    if (!LoFS::list(dir, &fsLsCollectScratch, fsLsCollectCb)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    LoBBSResponse resp;
    for (int i = 0; i < fsLsCollectScratch.count; i++) {
        char line[FS_NAME_BYTES + LO_HUMAN_BYTES_LEN + 2];
        char size[LO_HUMAN_BYTES_LEN];
        if (fsLsCollectScratch.dirs[i])
            snprintf(line, sizeof(line), "%s/", fsLsCollectScratch.names[i]);
        else
            snprintf(line, sizeof(line), "%s %s", fsLsCollectScratch.names[i], loHumanBytes(fsLsCollectScratch.sizes[i], size));
        lobbsRecordPush(resp.records, line);
    }
    if (fsLsCollectScratch.truncated)
        lobbsRecordPush(resp.records, "(list cut off)");
    if (resp.records.empty()) {
        lobbsResponseSetError(resp, "Empty.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    lobbsCommandReplyResponse(ctx, resp);
}

static void fsReplyFileText(LoBBSCommandCtx &ctx, const char *path, bool hex)
{
    LoFile f = LoFS::open(path, "r");
    if (!f) {
        lobbsCommandReplyError(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }

    std::string body;
    body.reserve(4096);
    static constexpr size_t kMaxBody = LOBBS_REPLY_CACHE_MAX_BYTES;
    uint8_t chunk[64];
    bool truncated = false;
    while (true) {
        size_t n = f.read(chunk, sizeof(chunk));
        if (n == 0)
            break;
        if (hex) {
            for (size_t i = 0; i < n; i++) {
                if (body.size() + 3 > kMaxBody) {
                    truncated = true;
                    break;
                }
                char pair[4];
                snprintf(pair, sizeof(pair), "%02x ", chunk[i]);
                body += pair;
            }
        } else {
            size_t room = kMaxBody > body.size() ? kMaxBody - body.size() : 0;
            if (n > room) {
                body.append((const char *)chunk, room);
                truncated = true;
                break;
            }
            body.append((const char *)chunk, n);
        }
        if (truncated)
            break;
    }
    if (truncated)
        body += "[...]";
    f.close();

    LoScalar rec;
    rec.setString(LODB_F_DESCRIPTION, body);
    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static bool fsResolveFileArg(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    char full[256];
    if (!fsResolveOrReply(ctx, spec, full, sizeof(full)))
        return false;
    return fsResolveOneFilePath(full, out, cap, ctx);
}

static void handleCat(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReplyError(ctx, "Usage: /cat path");
        return;
    }

    char path[256];
    if (!fsResolveFileArg(ctx, spec, path, sizeof(path)))
        return;
    fsReplyFileText(ctx, path, false);
}

static void handleHex(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReplyError(ctx, "Usage: /hex path");
        return;
    }

    char path[256];
    if (!fsResolveFileArg(ctx, spec, path, sizeof(path)))
        return;
    fsReplyFileText(ctx, path, true);
}

static void handleRm(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /rm /path");
        return;
    }
    char spec[256];
    if (!fsResolveAbsoluteOrReply(ctx, arg, spec, sizeof(spec)))
        return;
    if (lobfsGlobHasMeta(spec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }

    LoFile f = LoFS::open(spec, "r");
    if (!f) {
        lobbsCommandReplyError(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }
    f.close();

    if (LoFS::remove(spec))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleRmdir(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /rmdir /path");
        return;
    }
    char spec[256];
    if (!fsResolveAbsoluteOrReply(ctx, arg, spec, sizeof(spec)))
        return;
    if (lobfsGlobHasMeta(spec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (LoFS::isMountPoint(spec)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, spec, "rmdir"))
        return;

    if (!LoFS::isDirectory(spec)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    struct DirEmptyCtx {
        bool empty;
    } dec{true};
    auto dirHasEntry = [](void *v, const char *basename, bool isDirectory, uint32_t size) -> bool {
        (void)isDirectory;
        (void)basename;
        (void)size;
        ((DirEmptyCtx *)v)->empty = false;
        return false;
    };
    if (!LoFS::list(spec, &dec, dirHasEntry)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    if (!dec.empty) {
        lobbsCommandReplyError(ctx, "Not empty.");
        return;
    }

    if (LoFS::rmdir(spec, false))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleRmtree(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *a = lobbsArgShift(ctx);
    const char *b = lobbsArgShift(ctx);
    if (!a || !b || strcmp(a, b) != 0 || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /rmtree /path /path");
        return;
    }

    char path[256];
    if (!fsResolveAbsoluteOrReply(ctx, a, path, sizeof(path)))
        return;
    if (LoFS::isMountPoint(path)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, path, "rmtree"))
        return;
    if (lobfsGlobHasMeta(path)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (!LoFS::exists(path)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    if (LoFS::rmdir(path, true))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleMkdir(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /mkdir path");
        return;
    }
    char path[256];
    if (!fsResolveOrReply(ctx, arg, path, sizeof(path)))
        return;
    if (LoFS::isMountPoint(path) || strcmp(path, "/") == 0) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (LoFS::exists(path)) {
        lobbsCommandReplyError(ctx, "Already exists.");
        return;
    }
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        lobbsCommandReplyError(ctx, "No parent.");
        return;
    }
    *slash = '\0';
    bool parentIsDir = LoFS::isDirectory(path);
    *slash = '/';
    if (!parentIsDir) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }
    if (!LoFS::hasRoom(path, 1)) {
        lobbsCommandReplyError(ctx, "Disk full.");
        return;
    }
    if (LoFS::mkdir(path))
        lobbsCommandReply(ctx, "Created.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleStat(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /stat path");
        return;
    }
    char path[256];
    if (!fsResolveOrReply(ctx, arg, path, sizeof(path)))
        return;
    uint32_t size = 0;
    bool isDir = false;
    if (!LoFS::stat(path, &size, &isDir)) {
        lobbsCommandReplyError(ctx, "Not found.");
        return;
    }
    char line[64];
    if (isDir)
        snprintf(line, sizeof(line), "dir");
    else
        snprintf(line, sizeof(line), "file %u", (unsigned)size);
    lobbsCommandReply(ctx, line);
}

static void handleDf(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    LoBBSResponse resp;
    struct Ctx {
        LoBBSResponse *resp;
        const char *installRoot;
    } dc{&resp, lobbsInstallRoot(*ctx.kernel)};
    LoFS::eachPresentMount(
        [](void *v, const char *name) {
            auto *dc = (Ctx *)v;
            char root[16];
            snprintf(root, sizeof(root), "/%s", name);
            const uint64_t total = LoFS::totalBytes(root);
            const uint64_t used = LoFS::usedBytes(root);
            const uint32_t reserve = LoFS::mountReserve(name);
            const char *tag = strcmp(dc->installRoot, root) == 0 ? " [lobbs]" : "";
            char line[80];
            if (total == 0) {
                snprintf(line, sizeof(line), "%s: size unknown%s", name, tag);
            } else {
                const uint64_t taken = used + reserve;
                const uint64_t freeB = taken < total ? total - taken : 0;
                const unsigned pct = (unsigned)(used * 100 / total);
                char freeS[LO_HUMAN_BYTES_LEN], totalS[LO_HUMAN_BYTES_LEN], resS[LO_HUMAN_BYTES_LEN];
                if (reserve)
                    snprintf(line, sizeof(line), "%s: %s free of %s (%u%% used, %s reserved)%s", name, loHumanBytes(freeB, freeS),
                             loHumanBytes(total, totalS), pct, loHumanBytes(reserve, resS), tag);
                else
                    snprintf(line, sizeof(line), "%s: %s free of %s (%u%% used)%s", name, loHumanBytes(freeB, freeS),
                             loHumanBytes(total, totalS), pct, tag);
            }
            lobbsRecordPush(dc->resp->records, line);
        },
        &dc);
    if (resp.records.empty())
        lobbsResponseSetError(resp, "No mounts.");
    lobbsCommandReplyResponse(ctx, resp);
}

static constexpr uint32_t FS_FORMAT_CONFIRM_MS = 120000;

/** One pending /format confirmation at a time, bound to the node that asked. */
static struct {
    uint32_t nodeId;
    uint32_t armedMs;
    char mount[16];
    char code[7];
} fsFormatPending;

static void handleFormat(LoBBSCommandCtx &ctx)
{
    const char *name = lobbsArgShift(ctx);
    const char *code = lobbsArgShift(ctx);
    if (!name || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /format <mount> [code]");
        return;
    }
    if (!LoFS::mountPresent(name)) {
        lobbsCommandReplyError(ctx, "No such mount.");
        return;
    }
    if (strcmp(name, "sd") == 0) {
        lobbsCommandReplyError(ctx, "Refused: format SD cards on a PC.");
        return;
    }
    const char *installRoot = lobbsInstallRoot(*ctx.kernel);
    const bool isFlash = strcmp(name, "internal") == 0;
    const bool isInstall = installRoot[0] && strcmp(installRoot + 1, name) == 0;

    char msg[160];
    if (!code) {
        static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
        for (size_t i = 0; i < sizeof(fsFormatPending.code) - 1; i++)
            fsFormatPending.code[i] = alphabet[lobbsPlatformRandom(sizeof(alphabet) - 1)];
        fsFormatPending.code[sizeof(fsFormatPending.code) - 1] = '\0';
        fsFormatPending.nodeId = ctx.session.nodeId;
        fsFormatPending.armedMs = lobbsPlatformMillis();
        strncpy(fsFormatPending.mount, name, sizeof(fsFormatPending.mount) - 1);
        fsFormatPending.mount[sizeof(fsFormatPending.mount) - 1] = '\0';
        char root[16], size[LO_HUMAN_BYTES_LEN];
        snprintf(root, sizeof(root), "/%s", name);
        const char *note = isInstall ? " This uninstalls LoBBS." : isFlash ? " Radio settings are saved again." : "";
        snprintf(msg, sizeof(msg), "Formats /%s (%s): everything on it is erased.%s Run /format %s %s within 2 min.", name,
                 loHumanBytes(LoFS::totalBytes(root), size), note, name, fsFormatPending.code);
        lobbsCommandReply(ctx, msg);
        return;
    }

    const bool ok = fsFormatPending.code[0] && fsFormatPending.nodeId == ctx.session.nodeId &&
                    strcmp(fsFormatPending.mount, name) == 0 && strcmp(fsFormatPending.code, code) == 0 &&
                    (lobbsPlatformMillis() - fsFormatPending.armedMs) < FS_FORMAT_CONFIRM_MS;
    fsFormatPending.code[0] = '\0';
    if (!ok) {
        snprintf(msg, sizeof(msg), "Wrong or expired code. Run /format %s for a new one.", name);
        lobbsCommandReplyError(ctx, msg);
        return;
    }
    if (!LoFS::format(name)) {
        lobbsCommandReplyError(ctx, "Format failed.");
        return;
    }
    lobbsPlatformAfterFormat(name);
    if (isInstall)
        lobbsInstallInit(*ctx.kernel);
    snprintf(msg, sizeof(msg), "Formatted /%s.%s", name,
             lobbsInstallState(*ctx.kernel) == LoBBSInstallState::Blank ? " LoBBS is blank. Run /install." : "");
    lobbsCommandReply(ctx, msg);
}

/** One command at a time; keeps two path buffers off the mesh handler stack for /cp and /mv. */
static struct {
    char src[256];
    char dst[256];
} fsPairScratch;

static constexpr size_t FS_UPLOAD_DECODE_MAX = 160;
static uint8_t fsUploadDecodeScratch[FS_UPLOAD_DECODE_MAX];

static int fsB62CharValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 36;
    return -1;
}

static int fsB62DecodedByteCount(size_t charLen)
{
    switch (charLen) {
    case 2:
        return 1;
    case 3:
        return 2;
    case 5:
        return 3;
    case 6:
        return 4;
    case 7:
        return 5;
    case 9:
        return 6;
    case 10:
        return 7;
    case 11:
        return 8;
    default:
        return -1;
    }
}

static int fsB62WidthForBytes(int byteCount)
{
    switch (byteCount) {
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 5;
    case 4:
        return 6;
    case 5:
        return 7;
    case 6:
        return 9;
    case 7:
        return 10;
    case 8:
        return 11;
    default:
        return -1;
    }
}

/** Decode one fixed-width base62 chunk into out. Returns byte count or -1. */
static int fsB62DecodeChunkSpan(const char *b62, size_t charLen, uint8_t *out, size_t outCap)
{
    if (!b62 || !out)
        return -1;
    int byteCount = fsB62DecodedByteCount(charLen);
    if (byteCount < 0 || (size_t)byteCount > outCap)
        return -1;

    uint64_t val = 0;
    for (size_t i = 0; i < charLen; i++) {
        int d = fsB62CharValue(b62[i]);
        if (d < 0)
            return -1;
        val = val * 62 + (uint64_t)d;
    }

    uint64_t maxVal = byteCount >= 8 ? UINT64_MAX : ((uint64_t)1 << (8 * (unsigned)byteCount)) - 1;
    if (val > maxVal)
        return -1;

    for (int i = 0; i < byteCount; i++)
        out[i] = (uint8_t)((val >> (8 * (byteCount - 1 - i))) & 0xff);
    return byteCount;
}

/** Decode concatenated base62 segments (lobbs_chunkify.py format). */
static int fsB62DecodePayload(const char *b62, uint8_t *out, size_t outCap)
{
    if (!b62 || !out)
        return -1;
    size_t p = 0;
    size_t outPos = 0;
    const size_t slen = strlen(b62);
    while (p < slen) {
        bool matched = false;
        for (int k = 8; k >= 1; k--) {
            const int w = fsB62WidthForBytes(k);
            if (w < 0 || p + (size_t)w > slen)
                continue;
            const int n = fsB62DecodeChunkSpan(b62 + p, (size_t)w, out + outPos, outCap - outPos);
            if (n == k) {
                p += (size_t)w;
                outPos += (size_t)n;
                matched = true;
                break;
            }
        }
        if (!matched)
            return -1;
    }
    return (int)outPos;
}

static bool fsParentIsDirectory(const char *path)
{
    if (!path || path[0] != '/')
        return false;
    char parent[256];
    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *slash = strrchr(parent, '/');
    if (!slash)
        return false;
    if (slash == parent) {
        parent[1] = '\0';
        return LoFS::isDirectory(parent);
    }
    *slash = '\0';
    return LoFS::isDirectory(parent);
}

static bool fsParseCrcFromBasename(const char *path, uint32_t &crcOut)
{
    crcOut = 0;
    if (!path)
        return false;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *found = nullptr;
    for (const char *p = base; *p;) {
        const char *dot = strchr(p, '.');
        size_t segLen = dot ? (size_t)(dot - p) : strlen(p);
        if (segLen == 8) {
            bool hex = true;
            for (size_t i = 0; i < 8; i++) {
                if (!isxdigit((unsigned char)p[i])) {
                    hex = false;
                    break;
                }
            }
            if (hex)
                found = p;
        }
        if (!dot)
            break;
        p = dot + 1;
    }
    if (!found)
        return false;
    char hex[9];
    memcpy(hex, found, 8);
    hex[8] = '\0';
    char *end = nullptr;
    unsigned long v = strtoul(hex, &end, 16);
    if (end != hex + 8)
        return false;
    crcOut = (uint32_t)v;
    return true;
}

static void fsReplyMove(LoBBSCommandCtx &ctx, LoFSMoveResult r)
{
    switch (r) {
    case LoFSMoveResult::Ok:
        lobbsCommandReply(ctx, "Moved.");
        break;
    case LoFSMoveResult::RefusedMount:
        lobbsCommandReplyError(ctx, "Refused.");
        break;
    case LoFSMoveResult::DstExists:
        lobbsCommandReplyError(ctx, "Destination exists.");
        break;
    case LoFSMoveResult::SrcMissing:
        lobbsCommandReplyError(ctx, "No such file.");
        break;
    case LoFSMoveResult::SrcIsDir:
    case LoFSMoveResult::CrossMountDir:
        lobbsCommandReplyError(ctx, "Cross-mount directory move not supported.");
        break;
    case LoFSMoveResult::NoSpace:
        lobbsCommandReplyError(ctx, "Disk full.");
        break;
    case LoFSMoveResult::CopyFailed:
    case LoFSMoveResult::Failed:
        lobbsCommandReplyError(ctx, "Failed.");
        break;
    case LoFSMoveResult::SrcNotRemoved:
        lobbsCommandReplyError(ctx, "Copied, but source not removed.");
        break;
    case LoFSMoveResult::CrcMismatch:
        lobbsCommandReplyError(ctx, "CRC mismatch.");
        break;
    }
}

static void handleCp(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *srcSpec = lobbsArgShift(ctx);
    const char *dstSpec = lobbsArgShift(ctx);
    if (!srcSpec || !dstSpec || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /cp src dst");
        return;
    }
    if (lobfsGlobHasMeta(dstSpec)) {
        lobbsCommandReplyError(ctx, "No glob on destination.");
        return;
    }
    auto &src = fsPairScratch.src;
    auto &dst = fsPairScratch.dst;
    if (!fsResolveFileArg(ctx, srcSpec, src, sizeof(src)))
        return;
    if (!fsResolveOrReply(ctx, dstSpec, dst, sizeof(dst)))
        return;
    if (LoFS::isMountPoint(src)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    bool srcIsDir = false;
    uint32_t srcSz = 0;
    if (LoFS::stat(src, &srcSz, &srcIsDir) && srcIsDir) {
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }
    if (LoFS::exists(dst)) {
        lobbsCommandReplyError(ctx, "Destination exists.");
        return;
    }
    if (!LoFS::hasRoom(dst, srcSz)) {
        lobbsCommandReplyError(ctx, "Disk full.");
        return;
    }
    if (!LoFS::copy(src, dst)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    lobbsCommandReply(ctx, "Copied.");
}

static void handleMv(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *srcSpec = lobbsArgShift(ctx);
    const char *dstSpec = lobbsArgShift(ctx);
    if (!srcSpec || !dstSpec || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /mv src dst");
        return;
    }
    if (lobfsGlobHasMeta(dstSpec)) {
        lobbsCommandReplyError(ctx, "No glob on destination.");
        return;
    }
    auto &src = fsPairScratch.src;
    auto &dst = fsPairScratch.dst;
    if (!fsResolveOrReply(ctx, srcSpec, src, sizeof(src)))
        return;
    if (!fsResolveOrReply(ctx, dstSpec, dst, sizeof(dst)))
        return;
    if (fsRefuseCwdTarget(ctx, src, "mv"))
        return;
    fsReplyMove(ctx, LoFS::move(src, dst));
}

static void handleUpload(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *pathSpec = lobbsArgShift(ctx);
    if (!pathSpec) {
        lobbsCommandReplyError(ctx, "Usage: /upload path [offset:b62]");
        return;
    }
    if (lobfsGlobHasMeta(pathSpec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }

    char path[256];
    if (!fsResolveOrReply(ctx, pathSpec, path, sizeof(path)))
        return;
    if (LoFS::isMountPoint(path)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }

    const char *chunkTok = lobbsArgShift(ctx);
    if (!chunkTok) {
        uint32_t size = 0;
        bool isDir = false;
        if (LoFS::stat(path, &size, &isDir)) {
            if (isDir) {
                lobbsCommandReplyError(ctx, "Is a directory.");
                return;
            }
        } else if (!fsParentIsDirectory(path)) {
            lobbsCommandReplyError(ctx, "No such directory.");
            return;
        }
        char line[32];
        snprintf(line, sizeof(line), "Size %u.", (unsigned)size);
        lobbsCommandReply(ctx, line);
        return;
    }
    if (lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /upload path offset:b62");
        return;
    }

    const char *colon = strchr(chunkTok, ':');
    if (!colon || colon == chunkTok) {
        lobbsCommandReplyError(ctx, "Bad offset.");
        return;
    }

    char offBuf[16];
    size_t offLen = (size_t)(colon - chunkTok);
    if (offLen >= sizeof(offBuf)) {
        lobbsCommandReplyError(ctx, "Bad offset.");
        return;
    }
    memcpy(offBuf, chunkTok, offLen);
    offBuf[offLen] = '\0';
    char *end = nullptr;
    unsigned long offVal = strtoul(offBuf, &end, 10);
    if (end != offBuf + offLen || offVal > UINT32_MAX) {
        lobbsCommandReplyError(ctx, "Bad offset.");
        return;
    }
    const uint32_t offset = (uint32_t)offVal;
    const char *b62 = colon + 1;
    if (!b62[0]) {
        lobbsCommandReplyError(ctx, "Bad data.");
        return;
    }

    uint32_t curSize = 0;
    bool isDir = false;
    if (LoFS::stat(path, &curSize, &isDir)) {
        if (isDir) {
            lobbsCommandReplyError(ctx, "Is a directory.");
            return;
        }
    } else {
        curSize = 0;
        if (!fsParentIsDirectory(path)) {
            lobbsCommandReplyError(ctx, "No such directory.");
            return;
        }
    }

    if (offset > curSize) {
        char line[40];
        snprintf(line, sizeof(line), "Gap: size %u.", (unsigned)curSize);
        lobbsCommandReplyError(ctx, line);
        return;
    }

    int decodedLen = fsB62DecodePayload(b62, fsUploadDecodeScratch, FS_UPLOAD_DECODE_MAX);
    if (decodedLen < 0) {
        lobbsCommandReplyError(ctx, "Bad data.");
        return;
    }

    if (!LoFS::hasRoom(path, (uint32_t)decodedLen)) {
        lobbsCommandReplyError(ctx, "Disk full.");
        return;
    }
    if (!LoFS::writeAt(path, offset, fsUploadDecodeScratch, (size_t)decodedLen)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }

    uint32_t newSize = 0;
    if (!LoFS::stat(path, &newSize, &isDir) || isDir) {
        newSize = offset + (uint32_t)decodedLen;
        if (newSize < offset) {
            lobbsCommandReplyError(ctx, "Failed.");
            return;
        }
    }
    char line[32];
    snprintf(line, sizeof(line), "Size %u.", (unsigned)newSize);
    lobbsCommandReply(ctx, line);
}

static void handleCommit(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *srcSpec = lobbsArgShift(ctx);
    const char *dstSpec = lobbsArgShift(ctx);
    if (!srcSpec || !dstSpec || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /commit src dst");
        return;
    }
    if (lobfsGlobHasMeta(srcSpec) || lobfsGlobHasMeta(dstSpec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    auto &src = fsPairScratch.src;
    auto &dst = fsPairScratch.dst;
    if (!fsResolveOrReply(ctx, srcSpec, src, sizeof(src)))
        return;
    if (!fsResolveOrReply(ctx, dstSpec, dst, sizeof(dst)))
        return;

    uint32_t nameCrc = 0;
    if (!fsParseCrcFromBasename(src, nameCrc)) {
        lobbsCommandReplyError(ctx, "No CRC in name.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, src, "commit"))
        return;
    fsReplyMove(ctx, LoFS::moveIfCrc32Matches(src, dst, nameCrc));
}

static void handleCd(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    char path[LOBBS_CWD_BUFFER_SIZE];
    if (!fsResolveOrReply(ctx, arg ? arg : "/", path, sizeof(path)))
        return;
    if (lobfsGlobHasMeta(path)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (!LoFS::isDirectory(path)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }
    if (!ctx.kernel->auth().dal().setSessionCwd(ctx.session.nodeId, path)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    memcpy(ctx.session.cwd, path, strlen(path) + 1);
    lobbsCommandReply(ctx, ctx.session.cwd);
}

static void handlePwd(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    lobbsCommandReply(ctx, ctx.session.cwd);
}

static const LoBBSVerb fsVerbs[] = {
    {"cd", handleCd, LOBBS_V_SYSOP, "cd [path] — set working directory (default /)"},
    {"pwd", handlePwd, LOBBS_V_SYSOP, "pwd — show working directory"},
    {"ls", handleLs, LOBBS_V_SYSOP, "ls [path] — list directory (default cwd; /p2 …; * on last name only)"},
    {"cat", handleCat, LOBBS_V_SYSOP, "cat path — read a file (one match if glob)"},
    {"hex", handleHex, LOBBS_V_SYSOP, "hex path — hex dump (one match if glob)"},
    {"rm", handleRm, LOBBS_V_SYSOP, "rm /path — delete a file (absolute path)"},
    {"rmdir", handleRmdir, LOBBS_V_SYSOP, "rmdir /path — remove empty directory (absolute path)"},
    {"rmtree", handleRmtree, LOBBS_V_SYSOP, "rmtree /path /path — recursive delete (absolute path, typed twice)"},
    {"mkdir", handleMkdir, LOBBS_V_SYSOP, "mkdir path — create directory"},
    {"cp", handleCp, LOBBS_V_SYSOP, "cp src dst — copy file (no overwrite)"},
    {"mv", handleMv, LOBBS_V_SYSOP, "mv src dst — move/rename (files; dirs same mount only)"},
    {"upload", handleUpload, LOBBS_V_SYSOP, "upload path [offset:b62] — chunked write or show size"},
    {"commit", handleCommit, LOBBS_V_SYSOP, "commit src dst — move when file CRC matches name"},
    {"stat", handleStat, LOBBS_V_SYSOP, "stat path — file size or dir"},
    {"df", handleDf, LOBBS_V_SYSOP, "df — free space per mount"},
    {"format", handleFormat, LOBBS_V_SYSOP, "format <mount> [code] — erase a mount (asks for a code first)"},
};

static void slashFs(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    std::string v;
    if (!ctx || !args.getString(LOBBS_ARG_VERB, v))
        return;
    LoBBSCommandCtx &c = *ctx;
    const char *verb = v.c_str();
    const size_t n = sizeof(fsVerbs) / sizeof(fsVerbs[0]);
    for (size_t i = 0; i < n; i++) {
        if (!fsVerbs[i].verb || strcasecmp(verb, fsVerbs[i].verb) != 0)
            continue;
        if ((fsVerbs[i].flags & LOBBS_V_SYSOP) && !lobbsCommandRequireSysop(c))
            return;
        fsVerbs[i].fn(c);
        return;
    }
}

static void filterFsHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxIsSysop(*ctx))
        return;
    lobbsRecordPush(topics, "cd", "set working directory");
    lobbsRecordPush(topics, "pwd", "show working directory");
    lobbsRecordPush(topics, "ls", "list files");
    lobbsRecordPush(topics, "cat", "print a file");
    lobbsRecordPush(topics, "hex", "hex dump a file");
    lobbsRecordPush(topics, "rm", "delete a file");
    lobbsRecordPush(topics, "rmdir", "remove an empty directory");
    lobbsRecordPush(topics, "rmtree", "remove a directory tree");
    lobbsRecordPush(topics, "mkdir", "create a directory");
    lobbsRecordPush(topics, "cp", "copy a file");
    lobbsRecordPush(topics, "mv", "move or rename");
    lobbsRecordPush(topics, "upload", "chunked file upload");
    lobbsRecordPush(topics, "commit", "CRC-checked move");
    lobbsRecordPush(topics, "stat", "file or directory info");
    lobbsRecordPush(topics, "df", "filesystem space");
    lobbsRecordPush(topics, "format", "erase a mount");
}

static void filterFsHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    const size_t n = sizeof(fsVerbs) / sizeof(fsVerbs[0]);
    for (size_t i = 0; i < n; i++) {
        if (!fsVerbs[i].verb || !fsVerbs[i].help)
            continue;
        lobbsHelpForTable(ctx, value, args, fsVerbs[i].verb, &fsVerbs[i], 1);
    }
}

void lobbsFsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashFs, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterFsHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterFsHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
}

