#pragma once

#include "core/LoBBSConfig.h"
#include "LoFSBlockDevice.h"
#include "LoFSVolume.h"

#if defined(LOFS_NRF52)
#include <Adafruit_LittleFS.h>

class LoFSLittleFsVolume : public LoFSVolume {
  public:
    LoFSLittleFsVolume() = default;
    explicit LoFSLittleFsVolume(Adafruit_LittleFS &adopted);
    explicit LoFSLittleFsVolume(LoFSBlockDevice &device);
    void bind(Adafruit_LittleFS &fs);

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

    Adafruit_LittleFS &fs();

  private:
    enum class Kind { Adopted, Owned };

    Kind kind_;
    Adafruit_LittleFS *adopted_ = nullptr;
    LoFSBlockDevice *device_ = nullptr;
    lfs_config cfg_{};
    Adafruit_LittleFS ownedVol_{&cfg_};
    bool ready_ = false;
    Adafruit_LittleFS &activeFs();
    static uint8_t modeFromString(const char *mode);
    static bool littleFsBytes(Adafruit_LittleFS &fs, uint64_t &totalOut, uint64_t &usedOut);
};

extern const LoFileOps kLoFSAdafruitLittleFsFileOps;

#endif
