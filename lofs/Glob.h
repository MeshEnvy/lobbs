#pragma once

#include <stddef.h>
#include <stdint.h>

/** Path-only glob helpers for absolute LoFS paths (no session cwd). */

bool lobfsGlobHasMeta(const char *s);
bool lobfsGlobMatch(const char *pat, const char *str);
bool lobfsGlobBeforeLastName(const char *path);
bool lobfsGlobLastComponentHasMeta(const char *path);

enum class LobfsGlobResolve : uint8_t {
    Ok,
    GlobBeforeLast,
    NoDir,
    NoMatch,
    ManyMatches,
};

/** Optional glob on last path component only; splits `path` in place while listing. */
LobfsGlobResolve lobfsGlobResolveOneFile(char *path, char *out, size_t outCap);
