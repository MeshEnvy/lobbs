#pragma once

#include "../LoFile.h"
#include <stddef.h>
#include <stdint.h>

static constexpr const char *LOFS_MOUNT_ORDER[] = {"sd", "lofs", "extra", "internal"};
static constexpr int LOFS_MOUNT_ORDER_LEN = 4;
static constexpr int LOFS_MAX_MOUNTS = 6;

class LoFSVolume {
  public:
    virtual ~LoFSVolume() = default;

    /** Probe and mount if needed. False when this slot is absent on this hardware. */
    virtual bool begin() = 0;

    virtual LoFile open(const char *relPath, const char *mode) = 0;
    virtual LoFile open(const char *relPath, uint8_t mode) = 0;

    virtual bool exists(const char *relPath) = 0;
    virtual bool mkdir(const char *relPath) = 0;
    virtual bool remove(const char *relPath) = 0;
    virtual bool rename(const char *oldPath, const char *newPath) = 0;
    virtual bool rmdir(const char *relPath) = 0;

    virtual uint64_t totalBytes() = 0;
    virtual uint64_t usedBytes() = 0;
    virtual bool format() = 0;
    virtual void spaceHint(uint32_t *blockOut, uint32_t *slackOut) = 0;

    /** Background flush, merge, and cleaning (LoLog volumes). */
    virtual void maintain(uint32_t budgetMs) { (void)budgetMs; }

    static const char *normalizeRel(const char *rel);
};

struct LoFSMountSpec {
    const char *name;
    LoFSVolume *volume;
    bool shared;
    bool formattable;
};

struct LoFSMountTable {
    LoFSMountSpec specs[LOFS_MAX_MOUNTS];
    int count = 0;

    bool add(const LoFSMountSpec &spec);
};

void lofsPlatformMounts(LoFSMountTable &table);

uint32_t lofsPlatformSharedReserve(const char *mountName);
