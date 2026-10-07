#pragma once

#include "LoFSRawDevice.h"
#include "LoFSVolume.h"
#include "lofs/lolog/LoLog.h"

class LoFSLoLogVolume : public LoFSVolume {
  public:
    explicit LoFSLoLogVolume(LoFSRawDevice &device);

    bool begin() override;
    void maintain(uint32_t budgetMs) override;

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
    LoFSRawDevice &device_;
    LoLog log_;
    bool ready_ = false;

    static uint8_t modeFromString(const char *mode);
    LoFile wrapHandle(LoLog::OpenHandle *h, bool dirIter);
};

extern const LoFileOps kLoFSLoLogFileOps;
