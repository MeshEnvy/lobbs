#include "LoFile.h"

LoFile LoFileAdopt(const LoFileOps *ops, void *handle)
{
    LoFile out;
    if (!ops || !handle)
        return out;
    out.storage_ = new LoFile::Storage();
    out.storage_->ops = ops;
    out.storage_->handle = handle;
    return out;
}

LoFile::LoFile() : storage_(nullptr) {}

LoFile::~LoFile()
{
    close();
}

LoFile::LoFile(LoFile &&other) noexcept : storage_(other.storage_)
{
    other.storage_ = nullptr;
}

LoFile &LoFile::operator=(LoFile &&other) noexcept
{
    if (this != &other) {
        close();
        storage_ = other.storage_;
        other.storage_ = nullptr;
    }
    return *this;
}

LoFile::operator bool() const
{
    return storage_ && storage_->ops && storage_->handle && storage_->ops->valid(storage_->handle);
}

void LoFile::close()
{
    if (storage_) {
        if (storage_->ops && storage_->handle)
            storage_->ops->close(storage_->handle);
        delete storage_;
        storage_ = nullptr;
    }
}

size_t LoFile::read(uint8_t *buf, size_t len)
{
    return (storage_ && storage_->ops && storage_->handle) ? storage_->ops->read(storage_->handle, buf, len) : 0;
}

size_t LoFile::write(const uint8_t *buf, size_t len)
{
    return (storage_ && storage_->ops && storage_->handle) ? storage_->ops->write(storage_->handle, buf, len) : 0;
}

bool LoFile::seek(uint32_t pos)
{
    return storage_ && storage_->ops && storage_->handle && storage_->ops->seek(storage_->handle, pos);
}

uint32_t LoFile::size() const
{
    return (storage_ && storage_->ops && storage_->handle) ? storage_->ops->size(storage_->handle) : 0;
}

bool LoFile::isDirectory() const
{
    return storage_ && storage_->ops && storage_->handle && storage_->ops->isDirectory(storage_->handle);
}

const char *LoFile::name() const
{
    return (storage_ && storage_->ops && storage_->handle) ? storage_->ops->name(storage_->handle) : "";
}

void LoFile::flush()
{
    if (storage_ && storage_->ops && storage_->handle)
        storage_->ops->flush(storage_->handle);
}

LoFile LoFile::openNextFile()
{
    LoFile out;
    if (!storage_ || !storage_->ops || !storage_->handle)
        return out;
    void *next = storage_->ops->openNextFile(storage_->handle);
    if (!next)
        return out;
    return LoFileAdopt(storage_->ops, next);
}

#include "core/LoBBSStackGuard.h"
