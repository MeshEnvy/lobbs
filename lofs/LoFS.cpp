#include "LoFS.h"
#include "volumes/LoFSVolume.h"
#include <cstring>
#include <stdio.h>
#include <string>

#include "core/LoBBSStackGuard.h"

LoFS::Mount LoFS::mounts[6];
int LoFS::mountCount = 0;
bool LoFS::begun = false;

static uint32_t lofsCrc32Update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; data && i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xedb88320 & (~((crc & 1) - 1)));
    }
    return crc;
}

static int lofsMountPreferenceRank(const char *name)
{
    if (!name)
        return 999;
    for (int i = 0; i < LOFS_MOUNT_ORDER_LEN; i++) {
        if (strcmp(name, LOFS_MOUNT_ORDER[i]) == 0)
            return i;
    }
    return 999;
}

int LoFS::mountPreferenceRank(const char *name)
{
    return lofsMountPreferenceRank(name);
}

void LoFS::begin()
{
    if (begun)
        return;
    mountCount = 0;

    LoFSMountTable table;
    lofsPlatformMounts(table);

    for (int o = 0; o < LOFS_MOUNT_ORDER_LEN && mountCount < LOFS_MAX_MOUNTS; o++) {
        const char *want = LOFS_MOUNT_ORDER[o];
        for (int i = 0; i < table.count; i++) {
            if (strcmp(table.specs[i].name, want) != 0)
                continue;
            LoFSMountSpec &spec = table.specs[i];
            if (!spec.volume || !spec.volume->begin())
                break;
            mounts[mountCount++] = {spec.name, spec.volume, true, spec.shared, false, spec.formattable};
            break;
        }
    }

    for (int i = 0; i < mountCount; i++) {
        if (!mounts[i].shared) {
            mounts[i].dbSafe = true;
            continue;
        }
        mounts[i].dbSafe = true;
        const int selfRank = lofsMountPreferenceRank(mounts[i].name);
        for (int j = 0; j < mountCount; j++) {
            if (j == i || !mounts[j].present)
                continue;
            if (lofsMountPreferenceRank(mounts[j].name) < selfRank) {
                mounts[i].dbSafe = false;
                break;
            }
        }
    }

    begun = true;
}

#ifdef PIO_UNIT_TESTING
void LoFS::resetForTests()
{
    begun = false;
    mountCount = 0;
}
#endif

LoFS::Mount *LoFS::findByName(const char *name, size_t len)
{
    for (int i = 0; i < mountCount; i++) {
        if (strncmp(mounts[i].name, name, len) == 0 && mounts[i].name[len] == '\0' && mounts[i].present)
            return &mounts[i];
    }
    return nullptr;
}

bool LoFS::mountPresent(const char *name)
{
    return findByName(name, strlen(name)) != nullptr;
}

bool LoFS::mountDbSafe(const char *name)
{
    Mount *m = name ? findByName(name, strlen(name)) : nullptr;
    return m && m->dbSafe;
}

bool LoFS::format(const char *name)
{
    Mount *m = name ? findByName(name, strlen(name)) : nullptr;
    if (!m || !m->formattable || !m->volume)
        return false;
    return m->volume->format();
}

void LoFS::eachPresentMount(void (*fn)(void *ctx, const char *name), void *ctx)
{
    if (!fn)
        return;
    for (int i = 0; i < mountCount; i++) {
        if (mounts[i].present)
            fn(ctx, mounts[i].name);
    }
}

bool LoFS::resolve(const char *filepath, Resolved &out)
{
    out = {};
    if (!filepath || filepath[0] != '/')
        return false;

    if (filepath[1] == '\0') {
        out.kind = PathKind::VirtualRoot;
        return true;
    }

    const char *p = filepath + 1;
    const char *slash = strchr(p, '/');
    size_t nameLen = slash ? (size_t)(slash - p) : strlen(p);
    if (nameLen == 0 || nameLen >= 16)
        return false;

    Mount *m = findByName(p, nameLen);
    if (!m)
        return false;

    out.volume = m->volume;
    if (!slash || slash[1] == '\0') {
        out.kind = PathKind::MountRoot;
        out.rel = "/";
        return true;
    }

    out.rel = slash;
    out.kind = PathKind::Normal;
    return true;
}

const char *LoFS::backendPath(const Resolved &r)
{
    return r.rel;
}

bool LoFS::isMountPoint(const char *path)
{
    if (!path)
        return false;
    if (strcmp(path, "/") == 0)
        return true;
    Resolved r;
    if (!resolve(path, r))
        return false;
    return r.kind == PathKind::MountRoot;
}

const char *LoFS::mountNameForPath(const char *path)
{
    if (!path || path[0] != '/')
        return nullptr;
    if (path[1] == '\0')
        return nullptr;
    const char *p = path + 1;
    const char *slash = strchr(p, '/');
    size_t nameLen = slash ? (size_t)(slash - p) : strlen(p);
    Mount *m = findByName(p, nameLen);
    return m ? m->name : nullptr;
}

bool LoFS::isDirectory(const char *path)
{
    Resolved r;
    if (!resolve(path, r))
        return false;
    if (r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return true;
    LoFile f = open(path, "r");
    if (!f)
        return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

bool LoFS::refuseMountPointMutation(const char *filepath)
{
    return isMountPoint(filepath);
}

LoFile LoFS::open(const char *filepath, uint8_t mode)
{
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || !r.volume)
        return LoFile();
    return r.volume->open(backendPath(r), mode);
}

LoFile LoFS::open(const char *filepath, const char *mode)
{
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || !r.volume)
        return LoFile();
    return r.volume->open(backendPath(r), mode);
}

bool LoFS::exists(const char *filepath)
{
    Resolved r;
    if (!resolve(filepath, r))
        return false;
    if (r.kind == PathKind::VirtualRoot)
        return true;
    if (r.kind == PathKind::MountRoot)
        return true;
    return r.volume && r.volume->exists(backendPath(r));
}

bool LoFS::mkdir(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || !r.volume)
        return false;
    return r.volume->mkdir(backendPath(r));
}

bool LoFS::remove(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot || !r.volume)
        return false;
    return r.volume->remove(backendPath(r));
}

bool LoFS::rename(const char *oldfilepath, const char *newfilepath)
{
    if (refuseMountPointMutation(oldfilepath) || refuseMountPointMutation(newfilepath))
        return false;
    Resolved oldR;
    Resolved newR;
    if (!resolve(oldfilepath, oldR) || !resolve(newfilepath, newR) || !oldR.volume || !newR.volume)
        return false;
    if (oldR.kind == PathKind::VirtualRoot || newR.kind == PathKind::VirtualRoot)
        return false;
    if (oldR.volume != newR.volume)
        return false;
    return oldR.volume->rename(backendPath(oldR), backendPath(newR));
}

static bool lobfsEachDirEntry(LoFile &dir, void *ctx, LoFS::ListCallback fn)
{
    while (true) {
        LoFile file = dir.openNextFile();
        if (!file)
            break;

        std::string pathFromFile = file.name();
        bool isDir = file.isDirectory();
        uint32_t size = isDir ? 0 : (uint32_t)file.size();
        file.close();

        size_t lastSlash = pathFromFile.rfind('/');
        std::string entryName = (lastSlash != std::string::npos) ? pathFromFile.substr(lastSlash + 1) : pathFromFile;

        if (entryName == "." || entryName == "..")
            continue;

        if (!fn(ctx, entryName.c_str(), isDir, size))
            return false;
    }
    return true;
}

bool LoFS::list(const char *dirpath, void *ctx, ListCallback fn)
{
    if (!dirpath || !fn)
        return false;

    Resolved r;
    if (!resolve(dirpath, r))
        return false;

    if (r.kind == PathKind::VirtualRoot) {
        for (int i = 0; i < mountCount; i++) {
            if (!mounts[i].present)
                continue;
            if (!fn(ctx, mounts[i].name, true, 0))
                return false;
        }
        return true;
    }

    LoFile dir = open(dirpath, "r");
    if (!dir)
        return false;
    if (!dir.isDirectory()) {
        dir.close();
        return false;
    }

    bool ok = lobfsEachDirEntry(dir, ctx, fn);
    dir.close();
    return ok;
}

bool LoFS::stat(const char *filepath, uint32_t *sizeOut, bool *isDirOut)
{
    if (sizeOut)
        *sizeOut = 0;
    if (isDirOut)
        *isDirOut = false;

    Resolved r;
    if (!resolve(filepath, r))
        return false;
    if (r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot) {
        if (isDirOut)
            *isDirOut = true;
        return true;
    }

    LoFile f = open(filepath, "r");
    if (!f)
        return false;
    if (isDirOut)
        *isDirOut = f.isDirectory();
    if (sizeOut && !f.isDirectory())
        *sizeOut = (uint32_t)f.size();
    f.close();
    return true;
}

bool LoFS::writeAt(const char *filepath, uint32_t offset, const uint8_t *data, size_t len)
{
    if (!filepath || !data || len == 0)
        return false;
    if (refuseMountPointMutation(filepath))
        return false;

    Resolved r;
    if (!resolve(filepath, r) || r.kind != PathKind::Normal)
        return false;

    const bool creating = !exists(filepath);
    bool appendAtEnd = false;
    LoFile f = open(filepath, creating ? "w" : "r+");
    if (!f && !creating && offset > 0) {
        f = open(filepath, "a");
        appendAtEnd = true;
    }
    if (!f)
        return false;
    if (f.isDirectory()) {
        f.close();
        return false;
    }

    bool ok = false;
    if (!appendAtEnd && offset > 0 && !f.seek(offset)) {
        f.close();
        return false;
    }
    size_t w = f.write(data, len);
    ok = (w == len);
    f.flush();
    f.close();
    return ok;
}

bool LoFS::copy(const char *src, const char *dst)
{
    if (refuseMountPointMutation(src) || refuseMountPointMutation(dst))
        return false;
    if (exists(dst))
        return false;

    Resolved srcR;
    Resolved dstR;
    if (!resolve(src, srcR) || !resolve(dst, dstR))
        return false;
    if (srcR.kind == PathKind::VirtualRoot || srcR.kind == PathKind::MountRoot)
        return false;
    if (dstR.kind == PathKind::VirtualRoot || dstR.kind == PathKind::MountRoot)
        return false;

    LoFile srcFile = open(src, "r");
    if (!srcFile)
        return false;
    if (srcFile.isDirectory()) {
        srcFile.close();
        return false;
    }

    LoFile dstFile = open(dst, "w");
    if (!dstFile) {
        srcFile.close();
        return false;
    }

    unsigned char buffer[128];
    bool ok = true;
    while (true) {
        size_t n = srcFile.read(buffer, sizeof(buffer));
        if (n == 0)
            break;
        size_t w = dstFile.write(buffer, n);
        if (w != n) {
            ok = false;
            break;
        }
    }

    dstFile.flush();
    dstFile.close();
    srcFile.close();

    if (!ok)
        remove(dst);
    return ok;
}

LoFSMoveResult LoFS::move(const char *src, const char *dst)
{
    if (!src || !dst)
        return LoFSMoveResult::Failed;
    if (isMountPoint(src) || isMountPoint(dst))
        return LoFSMoveResult::RefusedMount;
    if (exists(dst))
        return LoFSMoveResult::DstExists;

    const char *srcMount = mountNameForPath(src);
    const char *dstMount = mountNameForPath(dst);
    if (srcMount && dstMount && strcmp(srcMount, dstMount) == 0) {
        return rename(src, dst) ? LoFSMoveResult::Ok : LoFSMoveResult::Failed;
    }

    bool isDir = false;
    uint32_t sz = 0;
    if (!stat(src, &sz, &isDir))
        return LoFSMoveResult::SrcMissing;
    if (isDir)
        return LoFSMoveResult::CrossMountDir;

    if (!hasRoom(dst, sz))
        return LoFSMoveResult::NoSpace;

    if (!copy(src, dst))
        return LoFSMoveResult::CopyFailed;
    if (remove(src))
        return LoFSMoveResult::Ok;
    return LoFSMoveResult::SrcNotRemoved;
}

bool LoFS::crc32File(const char *filepath, uint32_t *crcOut)
{
    if (!filepath || !crcOut)
        return false;
    LoFile f = open(filepath, "r");
    if (!f)
        return false;
    if (f.isDirectory()) {
        f.close();
        return false;
    }
    uint32_t crc = 0xffffffff;
    uint8_t chunk[64];
    while (true) {
        size_t n = f.read(chunk, sizeof(chunk));
        if (n == 0)
            break;
        crc = lofsCrc32Update(crc, chunk, n);
    }
    f.close();
    *crcOut = ~crc;
    return true;
}

LoFSMoveResult LoFS::moveIfCrc32Matches(const char *src, const char *dst, uint32_t expectedCrc)
{
    uint32_t crc = 0;
    if (!crc32File(src, &crc))
        return LoFSMoveResult::SrcMissing;
    if (crc != expectedCrc)
        return LoFSMoveResult::CrcMismatch;
    return move(src, dst);
}

uint64_t LoFS::totalBytes(const char *mountRoot)
{
    Resolved r;
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot || !r.volume)
        return 0;
    return r.volume->totalBytes();
}

uint64_t LoFS::usedBytes(const char *mountRoot)
{
    Resolved r;
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot || !r.volume)
        return 0;
    return r.volume->usedBytes();
}

uint64_t LoFS::freeBytes(const char *mountRoot)
{
    uint64_t total = totalBytes(mountRoot);
    uint64_t used = usedBytes(mountRoot);
    if (total == 0 || used > total)
        return 0;
    return total - used;
}

uint32_t LoFS::mountReserve(const char *name)
{
    Mount *m = name ? findByName(name, strlen(name)) : nullptr;
    return (m && m->shared) ? lofsPlatformSharedReserve(name) : 0;
}

bool LoFS::hasRoom(const char *path, uint32_t bytes)
{
    Resolved r;
    const char *name = mountNameForPath(path);
    if (!name || !resolve(path, r) || !r.volume)
        return true;
    char root[20];
    snprintf(root, sizeof(root), "/%s", name);
    const uint64_t total = totalBytes(root);
    if (total == 0)
        return true;
    const uint64_t used = usedBytes(root);
    const uint64_t freeB = used < total ? total - used : 0;

    uint32_t block = 4096;
    uint32_t slack = 8192;
    r.volume->spaceHint(&block, &slack);
    const uint64_t need = (((uint64_t)bytes + block - 1) / block) * block + slack + mountReserve(name);
    return freeB >= need;
}

bool LoFS::rmdir(const char *filepath, bool recursive)
{
    if (refuseMountPointMutation(filepath))
        return false;

    if (!exists(filepath))
        return true;

    if (recursive) {
        LoFile dir = open(filepath, "r");
        if (!dir)
            return false;

        if (!dir.isDirectory()) {
            dir.close();
            return remove(filepath);
        }

        bool result = true;
        while (true) {
            LoFile file = dir.openNextFile();
            if (!file)
                break;

            std::string pathFromFile = file.name();
            bool isDir = file.isDirectory();
            file.close();

            size_t lastSlash = pathFromFile.rfind('/');
            std::string entryName = (lastSlash != std::string::npos) ? pathFromFile.substr(lastSlash + 1) : pathFromFile;

            if (entryName == "." || entryName == "..")
                continue;

            std::string fullPath = std::string(filepath) + "/" + entryName;

            if (isDir) {
                if (!rmdir(fullPath.c_str(), true))
                    result = false;
            } else {
                if (!remove(fullPath.c_str()))
                    result = false;
            }
        }
        dir.close();

        if (!result)
            return false;
    }

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot || !r.volume)
        return false;

    return r.volume->rmdir(backendPath(r));
}
