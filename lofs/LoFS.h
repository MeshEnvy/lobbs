#pragma once

#define LOFS_VERSION "0.5.0"

#include "LoFile.h"

/**
 * Unified filesystem with a mount table in install preference order: /sd, /qspi, /reserve, /spare, /internal.
 * "/" is a virtual root that lists mounts. Every other path must start with "/<mount>/..." or "/<mount>".
 */
enum class LoFSMoveResult : uint8_t {
    Ok,
    RefusedMount,
    DstExists,
    SrcMissing,
    SrcIsDir,
    CrossMountDir,
    NoSpace,
    CopyFailed,
    SrcNotRemoved,
    CrcMismatch,
    Failed,
};

class LoFSVolume;

class LoFS
{
  public:
    static void begin();
#ifdef PIO_UNIT_TESTING
    /** Clears mount state so begin() can run again (host / Unity harness). */
    static void resetForTests();
#endif

    static LoFile open(const char *filepath, uint8_t mode);
    static LoFile open(const char *filepath, const char *mode);

    static bool exists(const char *filepath);
    static bool mkdir(const char *filepath);
    static bool remove(const char *filepath);
    static bool rename(const char *oldfilepath, const char *newfilepath);
    static bool rmdir(const char *filepath, bool recursive = false);

    static bool copy(const char *src, const char *dst);
    static LoFSMoveResult move(const char *src, const char *dst);
    static bool crc32File(const char *filepath, uint32_t *crcOut);
    static LoFSMoveResult moveIfCrc32Matches(const char *src, const char *dst, uint32_t expectedCrc);
    static bool writeAt(const char *filepath, uint32_t offset, const uint8_t *data, size_t len);
    static bool stat(const char *filepath, uint32_t *sizeOut, bool *isDirOut);

    static uint64_t totalBytes(const char *mountRoot);
    static uint64_t usedBytes(const char *mountRoot);
    static uint64_t freeBytes(const char *mountRoot);
    static uint32_t mountReserve(const char *name);
    static bool hasRoom(const char *path, uint32_t bytes);
    static bool mountDbSafe(const char *name);
    static bool format(const char *name);

    static bool isMountPoint(const char *path);
    static bool isDirectory(const char *path);
    static const char *mountNameForPath(const char *path);

    static bool mountPresent(const char *name);
    static void eachPresentMount(void (*fn)(void *ctx, const char *name), void *ctx);

    typedef bool (*ListCallback)(void *ctx, const char *basename, bool isDirectory, uint32_t size);
    static bool list(const char *dirpath, void *ctx, ListCallback fn);

  private:
    struct Mount {
        const char *name;
        LoFSVolume *volume;
        bool present;
        bool shared;
        bool dbSafe;
        bool formattable;
    };

    static Mount mounts[];
    static int mountCount;
    static bool begun;

    enum class PathKind { Invalid, VirtualRoot, MountRoot, Normal };

    struct Resolved {
        PathKind kind = PathKind::Invalid;
        LoFSVolume *volume = nullptr;
        const char *rel = nullptr;
    };

    static bool resolve(const char *filepath, Resolved &out);
    static Mount *findByName(const char *name, size_t len);
    static bool refuseMountPointMutation(const char *filepath);
    static const char *backendPath(const Resolved &r);
    static int mountPreferenceRank(const char *name);
};
