#include "LoFSNrfFlashBlockDevice.h"
#include <cstring>

#if defined(LOFS_NRF52)

#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "flash/flash_nrf5x.h"
#include <InternalFileSystem.h>
#elif defined(LOBBS_PLATFORM_MESHCORE)
extern "C" int flash_nrf5x_read(void *buffer, uint32_t address, uint32_t len);
extern "C" int flash_nrf5x_write(uint32_t address, const void *data, uint32_t len);
extern "C" void flash_nrf5x_flush(void);
extern "C" void flash_nrf5x_erase(uint32_t address);
#endif
#include <Adafruit_LittleFS.h>

LoFSNrfFlashBlockDevice::LoFSNrfFlashBlockDevice(void *startSym, void *endSym, uint32_t blockSize,
                                                 bool lockWithInternalFs, bool pageEraseOnFormat,
                                                 uint32_t flashPageSize)
    : startSym_(startSym), endSym_(endSym), blockSize_(blockSize), lockWithInternalFs_(lockWithInternalFs),
      pageEraseOnFormat_(pageEraseOnFormat), flashPageSize_(flashPageSize)
{
}

void LoFSNrfFlashBlockDevice::lockFs()
{
#if defined(LOBBS_PLATFORM_MESHTASTIC)
    if (lockWithInternalFs_)
        InternalFS._lockFS();
#else
    (void)lockWithInternalFs_;
#endif
}

void LoFSNrfFlashBlockDevice::unlockFs()
{
#if defined(LOBBS_PLATFORM_MESHTASTIC)
    if (lockWithInternalFs_)
        InternalFS._unlockFS();
#endif
}

static LoFSNrfFlashBlockDevice::BlockCtx *blockCtx(const lfs_config *c)
{
    return c ? static_cast<LoFSNrfFlashBlockDevice::BlockCtx *>(c->context) : nullptr;
}

static int readCb(const lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    auto *ctx = blockCtx(c);
    if (!ctx || !ctx->owner)
        return LFS_ERR_IO;
    const uint32_t blockSize = ctx->blockSize;
    ctx->owner->lockFs();
    int n = flash_nrf5x_read(buffer, ctx->base + block * blockSize + off, size);
    ctx->owner->unlockFs();
    return n > 0 ? 0 : LFS_ERR_IO;
}

static int progCb(const lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    auto *ctx = blockCtx(c);
    if (!ctx || !ctx->owner)
        return LFS_ERR_IO;
    const uint32_t blockSize = ctx->blockSize;
    ctx->owner->lockFs();
    int n = flash_nrf5x_write(ctx->base + block * blockSize + off, buffer, size);
    ctx->owner->unlockFs();
    return n > 0 ? 0 : LFS_ERR_IO;
}

static int eraseCb(const lfs_config *c, lfs_block_t block)
{
    auto *ctx = blockCtx(c);
    if (!ctx || !ctx->owner)
        return LFS_ERR_IO;
    const uint32_t blockSize = ctx->blockSize;
    uint8_t ff[128];
    if (blockSize > sizeof(ff))
        return LFS_ERR_IO;
    memset(ff, 0xFF, blockSize);
    ctx->owner->lockFs();
    int n = flash_nrf5x_write(ctx->base + block * blockSize, ff, blockSize);
    ctx->owner->unlockFs();
    return n > 0 ? 0 : LFS_ERR_IO;
}

static int syncCb(const lfs_config *c)
{
    auto *ctx = blockCtx(c);
    if (!ctx || !ctx->owner)
        return LFS_ERR_IO;
    ctx->owner->lockFs();
    flash_nrf5x_flush();
    ctx->owner->unlockFs();
    return 0;
}

bool LoFSNrfFlashBlockDevice::fill(lfs_config &cfg)
{
    const uint32_t start = (uint32_t)startSym_;
    const uint32_t end = (uint32_t)endSym_;
    if (!start || end <= start || blockSize_ == 0)
        return false;
    rangeStart_ = start;
    rangeEnd_ = end;
    blockCtx_.owner = this;
    blockCtx_.base = start;
    blockCtx_.blockSize = blockSize_;
    cfg.context = &blockCtx_;
    cfg.read = readCb;
    cfg.prog = progCb;
    cfg.erase = eraseCb;
    cfg.sync = syncCb;
    cfg.read_size = blockSize_;
    cfg.prog_size = blockSize_;
    cfg.block_size = blockSize_;
    cfg.block_count = (end - start) / blockSize_;
    cfg.lookahead = 128;
    return cfg.block_count > 0;
}

bool LoFSNrfFlashBlockDevice::prepareFormat()
{
    return eraseAll();
}

bool LoFSNrfFlashBlockDevice::eraseAll()
{
    if (!rangeStart_ || rangeEnd_ <= rangeStart_ || !pageEraseOnFormat_)
        return true;
    lockFs();
    flash_nrf5x_flush();
    for (uint32_t addr = rangeStart_; addr < rangeEnd_; addr += flashPageSize_)
        flash_nrf5x_erase(addr);
    unlockFs();
    return true;
}

#else

LoFSNrfFlashBlockDevice::LoFSNrfFlashBlockDevice(void *, void *, uint32_t, bool, bool, uint32_t) {}

bool LoFSNrfFlashBlockDevice::fill(lfs_config &cfg)
{
    (void)cfg;
    return false;
}

bool LoFSNrfFlashBlockDevice::prepareFormat()
{
    return false;
}

bool LoFSNrfFlashBlockDevice::eraseAll()
{
    return false;
}

#endif

#include "core/LoBBSStackGuard.h"
