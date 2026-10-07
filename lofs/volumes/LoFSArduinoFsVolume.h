#pragma once

#include "core/LoBBSArch.h"
#include "LoFSVolume.h"
#include <cstring>

#if LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040 || LOBBS_ARCH_PORTDUINO
#include <FS.h>

extern const LoFileOps kLoFSFsFileOps;

template <typename FsType> class LoFSArduinoFsVolume : public LoFSVolume {
  public:
    LoFSArduinoFsVolume() = default;
    explicit LoFSArduinoFsVolume(FsType &fs) : fs_(&fs) {}

    void bind(FsType &fs)
    {
        fs_ = &fs;
        ready_ = false;
    }

    bool begin() override
    {
        ready_ = fs_ != nullptr;
        return ready_;
    }

    LoFile open(const char *relPath, const char *mode) override
    {
        if (!ready_ || !fs_)
            return LoFile();
        return LoFileAdopt(&kLoFSFsFileOps, new fs::File(fs_->open(normalizeRel(relPath), mode)));
    }

    LoFile open(const char *relPath, uint8_t mode) override
    {
        return open(relPath, mode ? "w" : "r");
    }

    bool exists(const char *relPath) override
    {
        return ready_ && fs_ && fs_->exists(normalizeRel(relPath));
    }

    bool mkdir(const char *relPath) override
    {
        return ready_ && fs_ && fs_->mkdir(normalizeRel(relPath));
    }

    bool remove(const char *relPath) override
    {
        return ready_ && fs_ && fs_->remove(normalizeRel(relPath));
    }

    bool rename(const char *oldPath, const char *newPath) override
    {
        return ready_ && fs_ && fs_->rename(normalizeRel(oldPath), normalizeRel(newPath));
    }

    bool rmdir(const char *relPath) override
    {
        return ready_ && fs_ && fs_->rmdir(normalizeRel(relPath));
    }

    uint64_t totalBytes() override
    {
#if LOBBS_ARCH_PORTDUINO
        return 0;
#else
        return ready_ && fs_ ? fs_->totalBytes() : 0;
#endif
    }

    uint64_t usedBytes() override
    {
#if LOBBS_ARCH_PORTDUINO
        return 0;
#else
        return ready_ && fs_ ? fs_->usedBytes() : 0;
#endif
    }

    bool format() override
    {
#if LOBBS_ARCH_PORTDUINO
        return false;
#else
        return ready_ && fs_ && fs_->format();
#endif
    }

    void spaceHint(uint32_t *blockOut, uint32_t *slackOut) override
    {
        if (!blockOut || !slackOut)
            return;
        *blockOut = 4096;
        *slackOut = 8192;
    }

  private:
    FsType *fs_ = nullptr;
    bool ready_ = false;
};

#endif
