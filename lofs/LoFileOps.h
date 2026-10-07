#pragma once

#include <stddef.h>
#include <stdint.h>

/** Type-erased Arduino-style file handle for LoFile. */
struct LoFileOps {
    void (*destroy)(void *handle);
    bool (*valid)(void *handle);
    size_t (*read)(void *handle, uint8_t *buf, size_t len);
    size_t (*write)(void *handle, const uint8_t *buf, size_t len);
    bool (*seek)(void *handle, uint32_t pos);
    uint32_t (*size)(void *handle);
    bool (*isDirectory)(void *handle);
    const char *(*name)(void *handle);
    void (*close)(void *handle);
    void (*flush)(void *handle);
    /** Returns a new heap handle for the next directory entry, or nullptr. */
    void *(*openNextFile)(void *dirHandle);
};
