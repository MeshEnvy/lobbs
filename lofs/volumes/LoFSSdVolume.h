#pragma once

#include "LoFSVolume.h"

class LoFSSdVolume : public LoFSVolume {
  public:
    bool begin() override;

    LoFile open(const char *relPath, const char *mode) override;
    LoFile open(const char *relPath, uint8_t mode) override;

    bool exists(const char *relPath) override;
    bool mkdir(const char *relPath) override;
    bool remove(const char *relPath) override;
    bool rename(const char *oldPath, const char *newPath) override;
    bool rmdir(const char *relPath) override;

    uint64_t totalBytes() override;
    uint64_t usedBytes() override;
    bool format() override;
    void spaceHint(uint32_t *blockOut, uint32_t *slackOut) override;

  private:
    bool ready_ = false;
    static const char *sdPath(const char *rel);
};

extern const LoFileOps kLoFSSdFileOps;
