#pragma once

#include "lofs/lolog/LoLog.h"
#include "lofs/lolog/LoLogRamDevice.h"
#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

inline bool lologWriteFile(LoLog &log, const char *path, const uint8_t *data, size_t len)
{
    LoLog::OpenHandle *h = log.open(path, true, true);
    if (!h)
        return false;
    size_t off = 0;
    while (off < len) {
        size_t n = log.writeHandle(h, data + off, len - off);
        if (n == 0)
            break;
        off += n;
    }
    log.flushHandle(h);
    log.closeHandle(h);
    return off == len;
}

inline bool lologReadFile(LoLog &log, const char *path, std::vector<uint8_t> &out)
{
    LoLog::OpenHandle *h = log.open(path, false, false);
    if (!h)
        return false;
    out.clear();
    uint8_t buf[256];
    for (;;) {
        size_t n = log.readHandle(h, buf, sizeof(buf));
        if (n == 0)
            break;
        out.insert(out.end(), buf, buf + n);
    }
    log.closeHandle(h);
    return true;
}

inline int lologCountDir(LoLog &log, const char *dirPath)
{
    LoLog::OpenHandle *dir = log.openDir(dirPath);
    if (!dir)
        return -1;
    int n = 0;
    while (LoLog::OpenHandle *e = log.openNextInDir(dir)) {
        log.closeHandle(e);
        n++;
    }
    log.closeHandle(dir);
    return n;
}

/** Fresh chip: all 0xFF, mount without explicit format. */
inline bool lologMountErasedChip(LoLogRamDevice &dev, LoLog &log)
{
    dev.reset();
    return log.mount();
}
