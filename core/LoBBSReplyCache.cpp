#include "LoBBSReplyCache.h"
#include "apps/Config/ConfigCommands.h"
#include "platforms/LoPlatform.h"
#include <cstddef>
#include <map>
#include <string>

#include "LoBBSStackGuard.h"

struct LobbsReplyCacheEntry {
    LoBBSResponse resp;
    uint32_t expiresSec = 0;
};

static std::map<uint32_t, LobbsReplyCacheEntry> lobbsReplyCacheBySession;

static size_t lobbsResponsePayloadBytes(const LoBBSResponse &resp)
{
    size_t n = resp.error.size();
    for (const LoScalar &rec : resp.records) {
        std::string line;
        if (rec.encode(line, SIZE_MAX))
            n += line.size() + 1;
    }
    return n;
}

void lobbsReplyCacheGc(uint32_t nowSec)
{
    for (auto it = lobbsReplyCacheBySession.begin(); it != lobbsReplyCacheBySession.end();) {
        if (nowSec > it->second.expiresSec)
            it = lobbsReplyCacheBySession.erase(it);
        else
            ++it;
    }
    while (lobbsReplyCacheBySession.size() > LOBBS_REPLY_CACHE_MAX_ENTRIES) {
        lobbsReplyCacheBySession.erase(lobbsReplyCacheBySession.begin());
    }
}

void lobbsReplyCacheErase(uint32_t sessionNodeId)
{
    lobbsReplyCacheBySession.erase(sessionNodeId);
}

bool lobbsReplyCacheStore(uint32_t sessionNodeId, const LoBBSResponse &resp)
{
    if (!resp.ok)
        return false;

    if (lobbsResponsePayloadBytes(resp) > LOBBS_REPLY_CACHE_MAX_BYTES) {
        lobbsReplyCacheErase(sessionNodeId);
        return false;
    }

    LobbsReplyCacheEntry entry;
    entry.resp = resp;
    entry.expiresSec = lobbsPlatformUnixNow() + lobbsReplyCacheTtlSec();
    lobbsReplyCacheBySession[sessionNodeId] = std::move(entry);
    while (lobbsReplyCacheBySession.size() > LOBBS_REPLY_CACHE_MAX_ENTRIES) {
        lobbsReplyCacheBySession.erase(lobbsReplyCacheBySession.begin());
    }
    return true;
}

bool lobbsReplyCacheLoad(uint32_t sessionNodeId, LoBBSResponse &out)
{
    auto it = lobbsReplyCacheBySession.find(sessionNodeId);
    if (it == lobbsReplyCacheBySession.end())
        return false;
    uint32_t now = lobbsPlatformUnixNow();
    if (now > it->second.expiresSec) {
        lobbsReplyCacheBySession.erase(it);
        return false;
    }
    it->second.expiresSec = now + lobbsReplyCacheTtlSec();
    out = it->second.resp;
    return true;
}

