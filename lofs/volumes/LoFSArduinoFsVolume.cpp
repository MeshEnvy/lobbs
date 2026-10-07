#include "LoFSArduinoFsVolume.h"

#if LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040 || LOBBS_ARCH_PORTDUINO
#include <FS.h>

namespace {

struct FsFileHandle {
    fs::File file;
};

void destroyFs(void *handle)
{
    delete static_cast<FsFileHandle *>(handle);
}

bool validFs(void *handle)
{
    auto *h = static_cast<FsFileHandle *>(handle);
    return h && h->file;
}

size_t readFs(void *handle, uint8_t *buf, size_t len)
{
    return validFs(handle) ? static_cast<FsFileHandle *>(handle)->file.read(buf, len) : 0;
}

size_t writeFs(void *handle, const uint8_t *buf, size_t len)
{
    return validFs(handle) ? static_cast<FsFileHandle *>(handle)->file.write(buf, len) : 0;
}

bool seekFs(void *handle, uint32_t pos)
{
    return validFs(handle) && static_cast<FsFileHandle *>(handle)->file.seek(pos);
}

uint32_t sizeFs(void *handle)
{
    return validFs(handle) ? (uint32_t)static_cast<FsFileHandle *>(handle)->file.size() : 0;
}

bool isDirFs(void *handle)
{
    return validFs(handle) && static_cast<FsFileHandle *>(handle)->file.isDirectory();
}

const char *nameFs(void *handle)
{
    return validFs(handle) ? static_cast<FsFileHandle *>(handle)->file.name() : "";
}

void closeFs(void *handle)
{
    if (auto *h = static_cast<FsFileHandle *>(handle)) {
        h->file.close();
        delete h;
    }
}

void flushFs(void *handle)
{
    if (validFs(handle))
        static_cast<FsFileHandle *>(handle)->file.flush();
}

void *openNextFs(void *dirHandle)
{
    auto *dir = static_cast<FsFileHandle *>(dirHandle);
    if (!dir || !dir->file)
        return nullptr;
    fs::File next = dir->file.openNextFile();
    if (!next)
        return nullptr;
    auto *out = new FsFileHandle();
    out->file = next;
    return out;
}

} // namespace

const LoFileOps kLoFSFsFileOps = {destroyFs, validFs,   readFs,  writeFs, seekFs,     sizeFs,
                                  isDirFs,   nameFs,    closeFs, flushFs, openNextFs};

#else

const LoFileOps kLoFSFsFileOps = {};

#endif

#include "core/LoBBSStackGuard.h"
