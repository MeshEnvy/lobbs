#pragma once

#define LOFS_VERSION "0.3.0"

#include "FSCommon.h"
#include "configuration.h"
#include <Stream.h>

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#include <FS.h>
#include <SD.h>
#ifndef FILE_READ
#define FILE_READ O_READ
#endif
#ifndef FILE_WRITE
#define FILE_WRITE O_WRITE
#endif
#endif

/** Free space LoFS keeps on shared mounts (`/flash`) so Meshtastic can still rewrite its own files. */
#ifndef LOFS_SHARED_RESERVE_BYTES
#if defined(ARCH_NRF52)
#define LOFS_SHARED_RESERVE_BYTES (16 * 1024)
#elif defined(ARCH_PORTDUINO)
#define LOFS_SHARED_RESERVE_BYTES 0
#else
#define LOFS_SHARED_RESERVE_BYTES (128 * 1024)
#endif
#endif

/**
 * Unified filesystem with a mount table in install preference order: /sd (when present), /extra (when
 * LOBBS_EXTRA_QSPI), /flash2 (nRF52840 second internal-flash partition), /flash.
 * "/" is a virtual root that lists mounts. Every other path must start with "/<mount>/..." or "/<mount>".
 * Absolute paths only; no glob metacharacters. Upload gap policy lives in FsCommands, not writeAt.
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

class LoFS
{
  public:
    enum class Backend : uint8_t { Flash = 0, Sd = 1, Extra = 2, Flash2 = 3 };

    static void begin();

    static File open(const char *filepath, uint8_t mode);
    static File open(const char *filepath, const char *mode);

    static bool exists(const char *filepath);
    static bool mkdir(const char *filepath);
    static bool remove(const char *filepath);
    static bool rename(const char *oldfilepath, const char *newfilepath);
    static bool rmdir(const char *filepath, bool recursive = false);

    static bool copy(const char *src, const char *dst);
    /** Same-mount rename or cross-mount file copy+delete. */
    static LoFSMoveResult move(const char *src, const char *dst);
    static bool crc32File(const char *filepath, uint32_t *crcOut);
    static LoFSMoveResult moveIfCrc32Matches(const char *src, const char *dst, uint32_t expectedCrc);
    /** Write len bytes at offset; extends file. Does not truncate an existing file. */
    static bool writeAt(const char *filepath, uint32_t offset, const uint8_t *data, size_t len);
    static bool stat(const char *filepath, uint32_t *sizeOut, bool *isDirOut);

    static uint64_t totalBytes(const char *mountRoot);
    static uint64_t usedBytes(const char *mountRoot);
    static uint64_t freeBytes(const char *mountRoot);
    /** Bytes LoFS writes leave free on this mount (`LOFS_SHARED_RESERVE_BYTES` on shared mounts, else 0). */
    static uint32_t mountReserve(const char *name);
    /** True when writing `bytes` under `path` leaves the mount's reserve free. True when the mount reports no size or
     * the path does not resolve (the write itself then fails). */
    static bool hasRoom(const char *path, uint32_t bytes);
    /** True when a database may live on this mount. `flash` only qualifies when there is no `flash2` mount. */
    static bool mountDbSafe(const char *name);
    /** Erase and reformat a dedicated mount (`flash2`, `extra`). False for shared or SD mounts. */
    static bool format(const char *name);

    static bool isMountPoint(const char *path);
    static bool isDirectory(const char *path);
    /** Returns mount name ("sd", "extra", "flash2", "flash") or nullptr. */
    static const char *mountNameForPath(const char *path);

    static bool mountPresent(const char *name);
    static void eachPresentMount(void (*fn)(void *ctx, const char *name), void *ctx);

    /** `size` is the file length in bytes; 0 for directories and mounts. */
    typedef bool (*ListCallback)(void *ctx, const char *basename, bool isDirectory, uint32_t size);
    static bool list(const char *dirpath, void *ctx, ListCallback fn);

  private:
    struct Mount {
        const char *name;
        Backend backend;
        bool present;
        bool shared;
        bool dbSafe;
    };

    static Mount mounts[];
    static int mountCount;
    static bool begun;

    enum class PathKind { Invalid, VirtualRoot, MountRoot, Normal };

    struct Resolved {
        PathKind kind = PathKind::Invalid;
        Backend backend = Backend::Flash;
        /** Points into the caller's path (or a static "/"); valid while that path is. */
        const char *rel = nullptr;
    };

    static bool resolve(const char *filepath, Resolved &out);
    static Mount *findByName(const char *name, size_t len);
    static bool refuseMountPointMutation(const char *filepath);
    static const char *backendPath(const Resolved &r);
};
