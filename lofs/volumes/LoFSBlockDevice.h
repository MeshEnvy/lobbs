#pragma once

struct lfs_config;

/** Fills LittleFS block callbacks for a raw partition or external NOR. */
class LoFSBlockDevice {
  public:
    virtual ~LoFSBlockDevice() = default;
    virtual bool fill(lfs_config &cfg) = 0;
    virtual bool prepareFormat() { return true; }
};
