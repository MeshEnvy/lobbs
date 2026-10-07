#include "LoFSLoLogVolume.h"
#include <cstring>
#include <utility>

namespace {

struct LoLogFileHandle {
    LoLog *log;
    LoLog::OpenHandle *handle;
    bool ownsHandle;
    explicit LoLogFileHandle(LoLog *l, LoLog::OpenHandle *h, bool owns) : log(l), handle(h), ownsHandle(owns) {}
    ~LoLogFileHandle()
    {
        if (ownsHandle && log && handle)
            log->closeHandle(handle);
    }
};

void destroyLoLog(void *handle)
{
    delete static_cast<LoLogFileHandle *>(handle);
}

bool validLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->handle;
}

size_t readLoLog(void *handle, uint8_t *buf, size_t len)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle ? h->log->readHandle(h->handle, buf, len) : 0;
}

size_t writeLoLog(void *handle, const uint8_t *buf, size_t len)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle ? h->log->writeHandle(h->handle, buf, len) : 0;
}

bool seekLoLog(void *handle, uint32_t pos)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle && h->log->seekHandle(h->handle, pos);
}

uint32_t sizeLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle ? h->log->sizeHandle(h->handle) : 0;
}

bool isDirLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle && h->log->isDirHandle(h->handle);
}

const char *nameLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    return h && h->log && h->handle ? h->log->nameHandle(h->handle) : "";
}

void closeLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    if (h && h->ownsHandle && h->log && h->handle) {
        h->log->closeHandle(h->handle);
        h->handle = nullptr;
    }
}

void flushLoLog(void *handle)
{
    auto *h = static_cast<LoLogFileHandle *>(handle);
    if (h && h->log && h->handle)
        h->log->flushHandle(h->handle);
}

void *openNextLoLog(void *dirHandle)
{
    auto *dir = static_cast<LoLogFileHandle *>(dirHandle);
    if (!dir || !dir->log || !dir->handle || !dir->log->isDirHandle(dir->handle))
        return nullptr;
    LoLog::OpenHandle *next = dir->log->openNextInDir(dir->handle);
    if (!next)
        return nullptr;
    return new LoLogFileHandle(dir->log, next, true);
}

} // namespace

const LoFileOps kLoFSLoLogFileOps = {
    destroyLoLog, validLoLog,   readLoLog,  writeLoLog, seekLoLog, sizeLoLog,
    isDirLoLog,   nameLoLog,     closeLoLog, flushLoLog, openNextLoLog,
};

LoFSLoLogVolume::LoFSLoLogVolume(LoFSRawDevice &device) : device_(device), log_(device) {}

uint8_t LoFSLoLogVolume::modeFromString(const char *mode)
{
    if (!mode || strcmp(mode, "r") == 0)
        return 0;
    return 1;
}

LoFile LoFSLoLogVolume::wrapHandle(LoLog::OpenHandle *h, bool dirIter)
{
    (void)dirIter;
    if (!h)
        return LoFile();
    return LoFileAdopt(&kLoFSLoLogFileOps, new LoLogFileHandle(&log_, h, true));
}

bool LoFSLoLogVolume::begin()
{
    ready_ = log_.mount();
    return ready_;
}

void LoFSLoLogVolume::maintain(uint32_t budgetMs)
{
    if (ready_)
        log_.maintain(budgetMs);
}

LoFile LoFSLoLogVolume::open(const char *relPath, const char *mode)
{
    if (!ready_)
        return LoFile();
    const char *rel = normalizeRel(relPath);
    const bool write = modeFromString(mode) != 0;
    if (strcmp(mode, "r") == 0 && log_.isDirectory(rel)) {
        LoLog::OpenHandle *d = log_.openDir(rel);
        return wrapHandle(d, true);
    }
    LoLog::OpenHandle *h = log_.open(rel, write, write);
    return wrapHandle(h, false);
}

LoFile LoFSLoLogVolume::open(const char *relPath, uint8_t mode)
{
    return open(relPath, mode ? "w" : "r");
}

bool LoFSLoLogVolume::exists(const char *relPath)
{
    return ready_ && log_.exists(normalizeRel(relPath));
}

bool LoFSLoLogVolume::mkdir(const char *relPath)
{
    return ready_ && log_.mkdir(normalizeRel(relPath));
}

bool LoFSLoLogVolume::remove(const char *relPath)
{
    return ready_ && log_.remove(normalizeRel(relPath));
}

bool LoFSLoLogVolume::rename(const char *oldPath, const char *newPath)
{
    return ready_ && log_.rename(normalizeRel(oldPath), normalizeRel(newPath));
}

bool LoFSLoLogVolume::rmdir(const char *relPath)
{
    return ready_ && log_.rmdir(normalizeRel(relPath), true);
}

uint64_t LoFSLoLogVolume::totalBytes()
{
    return ready_ ? log_.totalBytes() : 0;
}

uint64_t LoFSLoLogVolume::usedBytes()
{
    return ready_ ? log_.usedBytes() : 0;
}

bool LoFSLoLogVolume::format()
{
    if (!device_.begin())
        return false;
    ready_ = log_.format();
    return ready_;
}

void LoFSLoLogVolume::spaceHint(uint32_t *blockOut, uint32_t *slackOut)
{
    log_.spaceHint(blockOut, slackOut);
}

#include "core/LoBBSStackGuard.h"
