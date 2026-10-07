#include "LoFSLittleFsVolume.h"
#include <cstring>
#include <utility>

#if defined(LOFS_NRF52)

using namespace Adafruit_LittleFS_Namespace;

namespace {

struct AdafruitFileHandle {
    File file;
    explicit AdafruitFileHandle(File f) : file(std::move(f)) {}
};

void destroyAdafruit(void *handle)
{
    delete static_cast<AdafruitFileHandle *>(handle);
}

bool validAdafruit(void *handle)
{
    auto *h = static_cast<AdafruitFileHandle *>(handle);
    return h && h->file;
}

size_t readAdafruit(void *handle, uint8_t *buf, size_t len)
{
    return validAdafruit(handle) ? static_cast<AdafruitFileHandle *>(handle)->file.read(buf, len) : 0;
}

size_t writeAdafruit(void *handle, const uint8_t *buf, size_t len)
{
    return validAdafruit(handle) ? static_cast<AdafruitFileHandle *>(handle)->file.write(buf, len) : 0;
}

bool seekAdafruit(void *handle, uint32_t pos)
{
    return validAdafruit(handle) && static_cast<AdafruitFileHandle *>(handle)->file.seek(pos);
}

uint32_t sizeAdafruit(void *handle)
{
    return validAdafruit(handle) ? (uint32_t)static_cast<AdafruitFileHandle *>(handle)->file.size() : 0;
}

bool isDirAdafruit(void *handle)
{
    return validAdafruit(handle) && static_cast<AdafruitFileHandle *>(handle)->file.isDirectory();
}

const char *nameAdafruit(void *handle)
{
    return validAdafruit(handle) ? static_cast<AdafruitFileHandle *>(handle)->file.name() : "";
}

void closeAdafruit(void *handle)
{
    if (auto *h = static_cast<AdafruitFileHandle *>(handle)) {
        h->file.close();
        delete h;
    }
}

void flushAdafruit(void *handle)
{
    if (validAdafruit(handle))
        static_cast<AdafruitFileHandle *>(handle)->file.flush();
}

void *openNextAdafruit(void *dirHandle)
{
    auto *dir = static_cast<AdafruitFileHandle *>(dirHandle);
    if (!dir || !dir->file)
        return nullptr;
    File next = dir->file.openNextFile();
    if (!next)
        return nullptr;
    return new AdafruitFileHandle(std::move(next));
}

} // namespace

const LoFileOps kLoFSAdafruitLittleFsFileOps = {
    destroyAdafruit, validAdafruit,   readAdafruit,  writeAdafruit, seekAdafruit, sizeAdafruit,
    isDirAdafruit,   nameAdafruit,     closeAdafruit, flushAdafruit, openNextAdafruit,
};

static LoFile wrapAdafruitFile(File &&f)
{
    if (!f)
        return LoFile();
    return LoFileAdopt(&kLoFSAdafruitLittleFsFileOps, new AdafruitFileHandle(std::move(f)));
}

LoFSLittleFsVolume::LoFSLittleFsVolume(Adafruit_LittleFS &adopted) : kind_(Kind::Adopted), adopted_(&adopted) {}

LoFSLittleFsVolume::LoFSLittleFsVolume(LoFSBlockDevice &device) : kind_(Kind::Owned), device_(&device) {}

void LoFSLittleFsVolume::bind(Adafruit_LittleFS &fs)
{
    kind_ = Kind::Adopted;
    adopted_ = &fs;
    ready_ = false;
}

Adafruit_LittleFS &LoFSLittleFsVolume::activeFs()
{
    return kind_ == Kind::Adopted ? *adopted_ : ownedVol_;
}

Adafruit_LittleFS &LoFSLittleFsVolume::fs()
{
    return activeFs();
}

bool LoFSLittleFsVolume::begin()
{
    if (kind_ == Kind::Adopted) {
        ready_ = adopted_ != nullptr;
        return ready_;
    }
    ready_ = false;
    if (!device_ || !device_->fill(cfg_))
        return false;
    if (ownedVol_.begin())
        return ready_ = true;
    if (!device_->prepareFormat())
        return false;
    return ready_ = (ownedVol_.format() && ownedVol_.begin());
}

uint8_t LoFSLittleFsVolume::modeFromString(const char *mode)
{
    if (!mode || strcmp(mode, "r") == 0)
        return 0;
    return 1;
}

LoFile LoFSLittleFsVolume::open(const char *relPath, const char *mode)
{
    if (!ready_)
        return LoFile();
    return wrapAdafruitFile(activeFs().open(normalizeRel(relPath), modeFromString(mode)));
}

LoFile LoFSLittleFsVolume::open(const char *relPath, uint8_t mode)
{
    if (!ready_)
        return LoFile();
    return wrapAdafruitFile(activeFs().open(normalizeRel(relPath), mode));
}

bool LoFSLittleFsVolume::exists(const char *relPath)
{
    return ready_ && activeFs().exists(normalizeRel(relPath));
}

bool LoFSLittleFsVolume::mkdir(const char *relPath)
{
    return ready_ && activeFs().mkdir(normalizeRel(relPath));
}

bool LoFSLittleFsVolume::remove(const char *relPath)
{
    return ready_ && activeFs().remove(normalizeRel(relPath));
}

bool LoFSLittleFsVolume::rename(const char *oldPath, const char *newPath)
{
    return ready_ && activeFs().rename(normalizeRel(oldPath), normalizeRel(newPath));
}

bool LoFSLittleFsVolume::rmdir(const char *relPath)
{
    return ready_ && activeFs().rmdir(normalizeRel(relPath));
}

bool LoFSLittleFsVolume::littleFsBytes(Adafruit_LittleFS &fs, uint64_t &totalOut, uint64_t &usedOut)
{
    lfs_t *lfs = fs._getFS();
    const lfs_config *cfg = lfs ? lfs->cfg : nullptr;
    if (!cfg)
        return false;
    totalOut = (uint64_t)cfg->block_size * cfg->block_count;
    uint32_t blocks = 0;
    fs._lockFS();
    int err = lfs_traverse(
        lfs,
        [](void *ctx, lfs_block_t block) {
            (void)block;
            (*(uint32_t *)ctx)++;
            return 0;
        },
        &blocks);
    fs._unlockFS();
    if (err)
        return false;
    usedOut = (uint64_t)blocks * cfg->block_size;
    return true;
}

uint64_t LoFSLittleFsVolume::totalBytes()
{
    uint64_t total = 0;
    uint64_t used = 0;
    if (!ready_ || !littleFsBytes(activeFs(), total, used))
        return 0;
    return total;
}

uint64_t LoFSLittleFsVolume::usedBytes()
{
    uint64_t total = 0;
    uint64_t used = 0;
    if (!ready_ || !littleFsBytes(activeFs(), total, used))
        return 0;
    return used;
}

bool LoFSLittleFsVolume::format()
{
    if (!ready_)
        return false;
    if (device_ && !device_->prepareFormat())
        return false;
    return activeFs().format() && activeFs().begin();
}

void LoFSLittleFsVolume::spaceHint(uint32_t *blockOut, uint32_t *slackOut)
{
    if (!blockOut || !slackOut)
        return;
    *blockOut = 128;
    *slackOut = 512;
}

#endif

#include "core/LoBBSStackGuard.h"
