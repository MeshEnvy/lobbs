#include "LoFSSdVolume.h"
#include "core/LoBBSArch.h"
#include <cstring>

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#include <SD.h>
#include <SPI.h>
#ifdef SDCARD_USE_SPI1
extern SPIClass SPI_HSPI;
#define SDHandler SPI_HSPI
#else
#define SDHandler SPI
#endif
#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY 4000000U
#endif

#if !defined(FILE_O_READ) && !LOBBS_ARCH_PORTDUINO
#define FILE_O_READ 0
#define FILE_O_WRITE 1
#endif

namespace {

struct SdFileHandle {
    File file;
};

void destroySd(void *handle)
{
    delete static_cast<SdFileHandle *>(handle);
}

bool validSd(void *handle)
{
    auto *h = static_cast<SdFileHandle *>(handle);
    return h && h->file;
}

size_t readSd(void *handle, uint8_t *buf, size_t len)
{
    return validSd(handle) ? static_cast<SdFileHandle *>(handle)->file.read(buf, len) : 0;
}

size_t writeSd(void *handle, const uint8_t *buf, size_t len)
{
    return validSd(handle) ? static_cast<SdFileHandle *>(handle)->file.write(buf, len) : 0;
}

bool seekSd(void *handle, uint32_t pos)
{
    return validSd(handle) && static_cast<SdFileHandle *>(handle)->file.seek(pos);
}

uint32_t sizeSd(void *handle)
{
    return validSd(handle) ? (uint32_t)static_cast<SdFileHandle *>(handle)->file.size() : 0;
}

bool isDirSd(void *handle)
{
    return validSd(handle) && static_cast<SdFileHandle *>(handle)->file.isDirectory();
}

const char *nameSd(void *handle)
{
    return validSd(handle) ? static_cast<SdFileHandle *>(handle)->file.name() : "";
}

void closeSd(void *handle)
{
    if (auto *h = static_cast<SdFileHandle *>(handle)) {
        h->file.close();
        delete h;
    }
}

void flushSd(void *handle)
{
    if (validSd(handle))
        static_cast<SdFileHandle *>(handle)->file.flush();
}

void *openNextSd(void *dirHandle)
{
    auto *dir = static_cast<SdFileHandle *>(dirHandle);
    if (!dir || !dir->file)
        return nullptr;
    File next = dir->file.openNextFile();
    if (!next)
        return nullptr;
    auto *out = new SdFileHandle();
    out->file = next;
    return out;
}

#if LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040 || LOBBS_ARCH_PORTDUINO
static const char *sdModeStr(const char *mode)
{
    return mode;
}
#else
static uint8_t sdModeFromStr(const char *mode)
{
    return (mode && strcmp(mode, "r") == 0) ? FILE_READ : FILE_WRITE;
}
#endif

} // namespace

const LoFileOps kLoFSSdFileOps = {destroySd,     validSd,   readSd,     writeSd, seekSd,     sizeSd,
                                  isDirSd,       nameSd,    closeSd,    flushSd, openNextSd};

static bool lobfsSdPresent()
{
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        SDHandler.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
        if (SD.begin(SDCARD_CS, SDHandler, SD_SPI_FREQUENCY))
            cardType = SD.cardType();
    }
    return cardType != CARD_NONE;
}

const char *LoFSSdVolume::sdPath(const char *rel)
{
    if (!rel || rel[0] == '\0')
        return "/";
    if (rel[0] == '/')
        return rel + 1;
    return rel;
}

bool LoFSSdVolume::begin()
{
    ready_ = lobfsSdPresent();
    return ready_;
}

LoFile LoFSSdVolume::open(const char *relPath, const char *mode)
{
    if (!ready_)
        return LoFile();
#if LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040 || LOBBS_ARCH_PORTDUINO
    File f = SD.open(sdPath(relPath), sdModeStr(mode));
#else
    File f = SD.open(sdPath(relPath), sdModeFromStr(mode));
#endif
    if (!f)
        return LoFile();
    auto *h = new SdFileHandle();
    h->file = std::move(f);
    return LoFileAdopt(&kLoFSSdFileOps, h);
}

LoFile LoFSSdVolume::open(const char *relPath, uint8_t mode)
{
#if LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040 || LOBBS_ARCH_PORTDUINO
    return open(relPath, mode ? "w" : "r");
#else
    if (!ready_)
        return LoFile();
    File f = SD.open(sdPath(relPath), mode ? FILE_WRITE : FILE_READ);
    if (!f)
        return LoFile();
    auto *h = new SdFileHandle();
    h->file = std::move(f);
    return LoFileAdopt(&kLoFSSdFileOps, h);
#endif
}

bool LoFSSdVolume::exists(const char *relPath)
{
    return ready_ && SD.exists(sdPath(relPath));
}

bool LoFSSdVolume::mkdir(const char *relPath)
{
    return ready_ && SD.mkdir(sdPath(relPath));
}

bool LoFSSdVolume::remove(const char *relPath)
{
    return ready_ && SD.remove(sdPath(relPath));
}

bool LoFSSdVolume::rename(const char *oldPath, const char *newPath)
{
    return ready_ && SD.rename(sdPath(oldPath), sdPath(newPath));
}

bool LoFSSdVolume::rmdir(const char *relPath)
{
    return ready_ && SD.rmdir(sdPath(relPath));
}

uint64_t LoFSSdVolume::totalBytes()
{
    return ready_ ? SD.totalBytes() : 0;
}

uint64_t LoFSSdVolume::usedBytes()
{
    return ready_ ? SD.usedBytes() : 0;
}

bool LoFSSdVolume::format()
{
    return false;
}

void LoFSSdVolume::spaceHint(uint32_t *blockOut, uint32_t *slackOut)
{
    if (!blockOut || !slackOut)
        return;
    *blockOut = 512;
    *slackOut = 64 * 1024;
}

#else

const LoFileOps kLoFSSdFileOps = {};

const char *LoFSSdVolume::sdPath(const char *rel)
{
    return rel;
}

bool LoFSSdVolume::begin()
{
    return false;
}

LoFile LoFSSdVolume::open(const char *, const char *)
{
    return LoFile();
}

LoFile LoFSSdVolume::open(const char *, uint8_t)
{
    return LoFile();
}

bool LoFSSdVolume::exists(const char *)
{
    return false;
}

bool LoFSSdVolume::mkdir(const char *)
{
    return false;
}

bool LoFSSdVolume::remove(const char *)
{
    return false;
}

bool LoFSSdVolume::rename(const char *, const char *)
{
    return false;
}

bool LoFSSdVolume::rmdir(const char *)
{
    return false;
}

uint64_t LoFSSdVolume::totalBytes()
{
    return 0;
}

uint64_t LoFSSdVolume::usedBytes()
{
    return 0;
}

bool LoFSSdVolume::format()
{
    return false;
}

void LoFSSdVolume::spaceHint(uint32_t *blockOut, uint32_t *slackOut)
{
    if (blockOut)
        *blockOut = 512;
    if (slackOut)
        *slackOut = 0;
}

#endif

#include "core/LoBBSStackGuard.h"
