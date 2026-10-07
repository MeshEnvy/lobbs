#include <lofs/Glob.h>
#include <lofs/LoFS.h>
#include <stdio.h>
#include <string.h>

bool lobfsGlobHasMeta(const char *s)
{
    for (; s && *s; s++) {
        if (*s == '*' || *s == '?')
            return true;
    }
    return false;
}

bool lobfsGlobMatch(const char *pat, const char *str)
{
    if (!*pat)
        return !*str;
    if (*pat == '*') {
        if (!pat[1])
            return true;
        for (; *str; str++) {
            if (lobfsGlobMatch(pat + 1, str))
                return true;
        }
        return lobfsGlobMatch(pat + 1, str);
    }
    if (*pat == '?') {
        if (!*str)
            return false;
        return lobfsGlobMatch(pat + 1, str + 1);
    }
    if (*pat != *str)
        return false;
    return lobfsGlobMatch(pat + 1, str + 1);
}

bool lobfsGlobBeforeLastName(const char *path)
{
    if (!path)
        return false;
    const char *lastSlash = strrchr(path, '/');
    const char *end = lastSlash ? lastSlash : path;
    for (const char *p = path; p < end; p++) {
        if (*p == '*' || *p == '?')
            return true;
    }
    return false;
}

bool lobfsGlobLastComponentHasMeta(const char *path)
{
    if (!path)
        return false;
    const char *lastSlash = strrchr(path, '/');
    const char *last = lastSlash ? lastSlash + 1 : path;
    return lobfsGlobHasMeta(last);
}

struct LobfsGlobOneFile {
    const char *pattern;
    const char *dirPath;
    char *out;
    size_t outCap;
    int count;
};

static bool lobfsGlobOneFileCb(void *ctx, const char *basename, bool isDirectory, uint32_t size)
{
    (void)size;
    if (isDirectory)
        return true;
    auto *mc = (LobfsGlobOneFile *)ctx;
    if (!lobfsGlobMatch(mc->pattern, basename))
        return true;
    if (mc->count == 0)
        snprintf(mc->out, mc->outCap, "%s/%s", mc->dirPath, basename);
    mc->count++;
    return true;
}

LobfsGlobResolve lobfsGlobResolveOneFile(char *path, char *out, size_t outCap)
{
    if (lobfsGlobBeforeLastName(path))
        return LobfsGlobResolve::GlobBeforeLast;

    if (!lobfsGlobLastComponentHasMeta(path)) {
        strncpy(out, path, outCap - 1);
        out[outCap - 1] = '\0';
        return LobfsGlobResolve::Ok;
    }

    char *lastSlash = strrchr(path, '/');
    LobfsGlobOneFile m{};
    m.pattern = lastSlash + 1;
    m.dirPath = lastSlash == path ? "/" : path;
    m.out = out;
    m.outCap = outCap;
    *lastSlash = '\0';
    bool listed = LoFS::list(m.dirPath, &m, lobfsGlobOneFileCb);
    *lastSlash = '/';

    if (!listed)
        return LobfsGlobResolve::NoDir;
    if (m.count == 0)
        return LobfsGlobResolve::NoMatch;
    if (m.count > 1)
        return LobfsGlobResolve::ManyMatches;
    return LobfsGlobResolve::Ok;
}
