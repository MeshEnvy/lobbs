#pragma once
#include "LoFileOps.h"
#include <stddef.h>
#include <stdint.h>

/** Platform-neutral open file handle. */
class LoFile {
  public:
    LoFile();
    ~LoFile();
    LoFile(LoFile &&other) noexcept;
    LoFile &operator=(LoFile &&other) noexcept;
    LoFile(const LoFile &) = delete;
    LoFile &operator=(const LoFile &) = delete;

    explicit operator bool() const;

    size_t read(uint8_t *buf, size_t len);
    size_t write(const uint8_t *buf, size_t len);
    bool seek(uint32_t pos);
    uint32_t size() const;
    bool isDirectory() const;
    const char *name() const;
    void close();
    void flush();
    LoFile openNextFile();

  private:
    friend LoFile LoFileAdopt(const LoFileOps *ops, void *handle);
    struct Storage {
        const LoFileOps *ops = nullptr;
        void *handle = nullptr;
    };
    Storage *storage_;
};

LoFile LoFileAdopt(const LoFileOps *ops, void *handle);
