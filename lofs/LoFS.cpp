#include "../LoBBSBootTrace.h"
#include "SPILock.h"
#include "configuration.h"
#include <lofs/LoFS.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#if LOBBS_EXTRA_QSPI
#if !defined(NRF52840_XXAA) || !defined(PIN_QSPI_SCK)
#error LOBBS_EXTRA_QSPI requires NRF52840_XXAA and PIN_QSPI_SCK
#endif
#include "LoFSQspi.h"
#endif

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
#endif

#if defined(ARCH_NRF52)
#include "flash/flash_nrf5x.h"

/** Second internal-flash partition bounds from the nRF52840 linker scripts. Weak: other nRF linker scripts get no `/flash2`. */
extern "C" uint8_t __flash2_start[] __attribute__((weak));
extern "C" uint8_t __flash2_end[] __attribute__((weak));

static constexpr uint32_t LOFS_FLASH2_BLOCK = 128;

// The flash_nrf5x page cache is shared with InternalFS (which the BLE task also writes), so /flash2 block IO holds its lock.
static int lofsFlash2Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    InternalFS._lockFS();
    int n = flash_nrf5x_read(buffer, (uint32_t)c->context + block * LOFS_FLASH2_BLOCK + off, size);
    InternalFS._unlockFS();
    return n > 0 ? 0 : -1;
}

static int lofsFlash2Prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    InternalFS._lockFS();
    int n = flash_nrf5x_write((uint32_t)c->context + block * LOFS_FLASH2_BLOCK + off, buffer, size);
    InternalFS._unlockFS();
    return n > 0 ? 0 : -1;
}

static int lofsFlash2Erase(const struct lfs_config *c, lfs_block_t block)
{
    uint8_t ff[LOFS_FLASH2_BLOCK];
    memset(ff, 0xFF, sizeof(ff));
    InternalFS._lockFS();
    int n = flash_nrf5x_write((uint32_t)c->context + block * LOFS_FLASH2_BLOCK, ff, sizeof(ff));
    InternalFS._unlockFS();
    return n > 0 ? 0 : -1;
}

static int lofsFlash2Sync(const struct lfs_config *c)
{
    (void)c;
    InternalFS._lockFS();
    flash_nrf5x_flush();
    InternalFS._unlockFS();
    return 0;
}

static struct lfs_config lofsFlash2Cfg;
static Adafruit_LittleFS lofsFlash2(&lofsFlash2Cfg);

static bool lofsFlash2Format()
{
    const uint32_t start = (uint32_t)lofsFlash2Cfg.context;
    const uint32_t end = start + lofsFlash2Cfg.block_count * LOFS_FLASH2_BLOCK;
    LOG_WARN("LoFS: formatting /flash2 (0x%x-0x%x)", (unsigned)start, (unsigned)end);
    LOBBS_BOOT_STEP("flash2: erase partition pages");
    InternalFS._lockFS();
    flash_nrf5x_flush();
    for (uint32_t addr = start; addr < end; addr += FLASH_NRF52_PAGE_SIZE)
        flash_nrf5x_erase(addr);
    InternalFS._unlockFS();
    LOBBS_BOOT_STEP("flash2: lfs format");
    return lofsFlash2.format() && lofsFlash2.begin();
}

static bool lofsFlash2Begin()
{
    LOBBS_BOOT_STEP("flash2: begin enter");
    const uint32_t start = (uint32_t)__flash2_start;
    const uint32_t end = (uint32_t)__flash2_end;
    if (!start || end <= start)
        return false;
    lofsFlash2Cfg.context = (void *)start;
    lofsFlash2Cfg.read = lofsFlash2Read;
    lofsFlash2Cfg.prog = lofsFlash2Prog;
    lofsFlash2Cfg.erase = lofsFlash2Erase;
    lofsFlash2Cfg.sync = lofsFlash2Sync;
    lofsFlash2Cfg.read_size = LOFS_FLASH2_BLOCK;
    lofsFlash2Cfg.prog_size = LOFS_FLASH2_BLOCK;
    lofsFlash2Cfg.block_size = LOFS_FLASH2_BLOCK;
    lofsFlash2Cfg.block_count = (end - start) / LOFS_FLASH2_BLOCK;
    lofsFlash2Cfg.lookahead = 128;
    LOBBS_BOOT_STEP("flash2: lfs mount");
    if (lofsFlash2.begin())
        return true;
    return lofsFlash2Format();
}

#if LOBBS_EXTRA_QSPI
static struct lfs_config lofsQspiCfg;
static Adafruit_LittleFS lofsQspi(&lofsQspiCfg);

static bool lofsQspiFormat()
{
    LOG_WARN("LoFS: formatting /extra (QSPI)");
    LOBBS_BOOT_STEP("extra: lfs format");
    return lofsQspi.format() && lofsQspi.begin();
}

static bool lofsQspiBegin()
{
    LOBBS_BOOT_STEP("extra: begin enter");
    LOBBS_BOOT_STEP("extra: qspi config");
    if (!lofsQspiConfig(lofsQspiCfg))
        return false;
    LOBBS_BOOT_STEP("extra: lfs mount");
    if (lofsQspi.begin())
        return true;
    return lofsQspiFormat();
}
#endif

static Adafruit_LittleFS &lofsFs(LoFS::Backend b)
{
    if (b == LoFS::Backend::Flash2)
        return lofsFlash2;
#if LOBBS_EXTRA_QSPI
    if (b == LoFS::Backend::Extra)
        return lofsQspi;
#endif
    return FSCom;
}
#else
static auto &lofsFs(bool flash2)
{
    (void)flash2;
    return FSCom;
}
#endif

#include "../apps/AppUtil.h"
#include "LoBBSStackGuard.h"
#include <stdio.h>

LoFS::Mount LoFS::mounts[4];
int LoFS::mountCount = 0;
bool LoFS::begun = false;

static bool lobfsSdPresent()
{
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        concurrency::LockGuard g(spiLock);
        SDHandler.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
        if (SD.begin(SDCARD_CS, SDHandler, SD_SPI_FREQUENCY))
            cardType = SD.cardType();
    }
    return cardType != CARD_NONE;
#else
    return false;
#endif
}

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
static const char *convertToSDMode(const char *modeStr)
{
    return modeStr;
}
static const char *convertToSDMode(uint8_t mode)
{
    return (mode == 0) ? "r" : "w";
}
#else
static uint8_t convertToSDMode(const char *modeStr)
{
    return (strcmp(modeStr, "r") == 0) ? FILE_READ : FILE_WRITE;
}
static uint8_t convertToSDMode(uint8_t mode)
{
    return (mode == 0) ? FILE_READ : FILE_WRITE;
}
#endif
#endif

void LoFS::begin()
{
    if (begun)
        return;
    mountCount = 0;
    bool hasFlash2 = false;

    // Registration order is install preference order.
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    LOBBS_BOOT_STEP("LoFS: sd present check");
    if (lobfsSdPresent())
        mounts[mountCount++] = Mount{"sd", Backend::Sd, true, false, true};
#endif

#if LOBBS_EXTRA_QSPI
    LOBBS_BOOT_STEP("LoFS: lofsQspiBegin");
    if (lofsQspiBegin())
        mounts[mountCount++] = Mount{"extra", Backend::Extra, true, false, true};
#endif

#if defined(ARCH_NRF52)
    LOBBS_BOOT_STEP("LoFS: lofsFlash2Begin");
    if (lofsFlash2Begin()) {
        mounts[mountCount++] = Mount{"flash2", Backend::Flash2, true, false, true};
        hasFlash2 = true;
    }
#endif

    LOBBS_BOOT_STEP("LoFS: register flash mount");
    mounts[mountCount++] = Mount{"flash", Backend::Flash, true, true, !hasFlash2};
    begun = true;
    LOBBS_BOOT_STEP("LoFS: begin done");
}

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
    if (!m)
        return false;
#if defined(ARCH_NRF52)
    if (m->backend == Backend::Flash2)
        return lofsFlash2Format();
#endif
#if LOBBS_EXTRA_QSPI
    if (m->backend == Backend::Extra)
        return lofsQspiFormat();
#endif
#if defined(ARCH_NRF52) || defined(ARCH_ESP32) || defined(ARCH_RP2040)
    if (m->backend == Backend::Flash) {
        concurrency::LockGuard g(spiLock);
        return FSCom.format();
    }
#endif
    return false;
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

    out.backend = m->backend;
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
    return r.backend == Backend::Sd ? r.rel + 1 : r.rel;
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

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    File f;
#else
    File f(FSCom);
#endif
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        f = SD.open(bp, FILE_O_READ);
#endif
    } else {
        f = lofsFs(r.backend == Backend::Flash2).open(bp, FILE_O_READ);
    }
#else
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        f = SD.open(bp, FILE_O_READ);
#endif
    } else {
        f = lofsFs(r.backend).open(bp, FILE_O_READ);
    }
#endif
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

File LoFS::open(const char *filepath, uint8_t mode)
{
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    const char *modeStr = (mode == 0) ? FILE_O_READ : FILE_O_WRITE;
    return open(filepath, modeStr);
#else
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot) {
        return File(FSCom);
    }

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.open(bp, convertToSDMode(mode));
#endif
    }
    return lofsFs(r.backend).open(bp, mode);
#endif
}

File LoFS::open(const char *filepath, const char *mode)
{
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot) {
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return File();
#else
        return File(FSCom);
#endif
    }

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return SD.open(bp, mode);
#else
        return SD.open(bp, convertToSDMode(mode));
#endif
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    return lofsFs(r.backend == Backend::Flash2).open(bp, mode);
#else
    uint8_t flashMode = (mode && strcmp(mode, "r") == 0) ? 0 : 1;
    return lofsFs(r.backend).open(bp, flashMode);
#endif
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

    const char *bp = backendPath(r);
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.exists(bp);
#endif
    }
#if defined(ARCH_NRF52)
    return lofsFs(r.backend).exists(bp);
#else
    return lofsFs(r.backend == Backend::Flash2).exists(bp);
#endif
}

bool LoFS::mkdir(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.mkdir(bp);
#endif
    }
#if defined(ARCH_NRF52)
    return lofsFs(r.backend).mkdir(bp);
#else
    return lofsFs(r.backend == Backend::Flash2).mkdir(bp);
#endif
}

bool LoFS::remove(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.remove(bp);
#endif
    }
#if defined(ARCH_NRF52)
    return lofsFs(r.backend).remove(bp);
#else
    return lofsFs(r.backend == Backend::Flash2).remove(bp);
#endif
}

bool LoFS::rename(const char *oldfilepath, const char *newfilepath)
{
    if (refuseMountPointMutation(oldfilepath) || refuseMountPointMutation(newfilepath))
        return false;

    Resolved oldR;
    Resolved newR;
    if (!resolve(oldfilepath, oldR) || !resolve(newfilepath, newR))
        return false;
    if (oldR.kind == PathKind::VirtualRoot || newR.kind == PathKind::VirtualRoot)
        return false;
    if (oldR.backend != newR.backend)
        return false;

    const char *oldBp = backendPath(oldR);
    const char *newBp = backendPath(newR);

    concurrency::LockGuard g(spiLock);
    if (oldR.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.rename(oldBp, newBp);
#endif
    }
#if defined(ARCH_NRF52)
    return lofsFs(oldR.backend).rename(oldBp, newBp);
#else
    return lofsFs(oldR.backend == Backend::Flash2).rename(oldBp, newBp);
#endif
}

static bool lobfsEachDirEntry(File &dir, void *ctx, LoFS::ListCallback fn)
{
    while (true) {
        File file = dir.openNextFile();
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

    File dir = open(dirpath, FILE_O_READ);
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

    File f = open(filepath, FILE_O_READ);
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
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    File f = open(filepath, creating ? "w" : "r+");
#else
    (void)creating;
    File f = open(filepath, (uint8_t)1);
#endif
    if (!f)
        return false;
    if (f.isDirectory()) {
        f.close();
        return false;
    }

    bool ok = false;
    {
        concurrency::LockGuard g(spiLock);
        if (f.seek(offset)) {
            size_t w = f.write(data, len);
            ok = (w == len);
        }
        f.flush();
        f.close();
    }
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

    File srcFile = open(src, FILE_O_READ);
    if (!srcFile)
        return false;
    if (srcFile.isDirectory()) {
        srcFile.close();
        return false;
    }

    File dstFile = open(dst, FILE_O_WRITE);
    if (!dstFile) {
        srcFile.close();
        return false;
    }

    unsigned char buffer[128];
    bool ok = true;
    while (true) {
        size_t n = 0;
        {
            concurrency::LockGuard g(spiLock);
            n = srcFile.read(buffer, sizeof(buffer));
        }
        if (n == 0)
            break;
        size_t w = 0;
        {
            concurrency::LockGuard g(spiLock);
            w = dstFile.write(buffer, n);
        }
        if (w != n) {
            ok = false;
            break;
        }
    }

    {
        concurrency::LockGuard g(spiLock);
        dstFile.flush();
        dstFile.close();
        srcFile.close();
    }

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
    File f = open(filepath, FILE_O_READ);
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
        crc = lobbsCrc32Update(crc, chunk, n);
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
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot)
        return 0;
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.totalBytes();
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040)
    return FSCom.totalBytes();
#elif defined(ARCH_NRF52)
    const lfs_config *cfg = lofsFs(r.backend)._getFS()->cfg;
    return cfg ? (uint64_t)cfg->block_size * cfg->block_count : 0;
#else
    return 0;
#endif
}

#if defined(ARCH_NRF52)
static int lofsCountBlock(void *ctx, lfs_block_t block)
{
    (void)block;
    (*(uint32_t *)ctx)++;
    return 0;
}
#endif

uint64_t LoFS::usedBytes(const char *mountRoot)
{
    Resolved r;
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot)
        return 0;
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.usedBytes();
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040)
    return FSCom.usedBytes();
#elif defined(ARCH_NRF52)
    Adafruit_LittleFS &fs = lofsFs(r.backend);
    lfs_t *lfs = fs._getFS();
    uint32_t blocks = 0;
    fs._lockFS();
    int err = lfs_traverse(lfs, lofsCountBlock, &blocks);
    fs._unlockFS();
    if (err || !lfs->cfg)
        return 0;
    return (uint64_t)blocks * lfs->cfg->block_size;
#else
    return 0;
#endif
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
    return (m && m->shared) ? LOFS_SHARED_RESERVE_BYTES : 0;
}

bool LoFS::hasRoom(const char *path, uint32_t bytes)
{
    Resolved r;
    const char *name = mountNameForPath(path);
    if (!name || !resolve(path, r))
        return true;
    char root[20];
    snprintf(root, sizeof(root), "/%s", name);
    const uint64_t total = totalBytes(root);
    if (total == 0)
        return true;
    const uint64_t used = usedBytes(root);
    const uint64_t freeB = used < total ? total - used : 0;

    uint32_t block = 4096;
    uint32_t slack = 2 * 4096;
    if (r.backend == Backend::Sd) {
        block = 512;
        slack = 64 * 1024;
    }
#if defined(ARCH_NRF52)
    if (r.backend == Backend::Flash || r.backend == Backend::Flash2) {
        block = 128;
        slack = 4 * 128;
    }
#endif
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
        File dir = open(filepath, FILE_O_READ);
        if (!dir)
            return false;

        if (!dir.isDirectory()) {
            dir.close();
            return remove(filepath);
        }

        bool result = true;
        while (true) {
            File file = dir.openNextFile();
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
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.rmdir(bp);
#endif
    }
#if defined(ARCH_NRF52)
    return lofsFs(r.backend).rmdir(bp);
#else
    return lofsFs(r.backend == Backend::Flash2).rmdir(bp);
#endif
}
