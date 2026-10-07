#if defined(LOBBS_PLATFORM_NATIVE)
#include "platforms/LoPlatform.h"
#include "core/LoBBSReply.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>

static std::chrono::steady_clock::time_point gStart = std::chrono::steady_clock::now();

uint32_t lobbsPlatformMillis()
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gStart);
    return (uint32_t)ms.count();
}

uint32_t lobbsPlatformUnixNow()
{
    return 1000;
}

LoPlatformSetUnixResult lobbsPlatformSetUnix(uint32_t unixSec)
{
    (void)unixSec;
    return LoPlatformSetUnixResult::Ok;
}

const char *lobbsPlatformClockName()
{
    return "native";
}

uint32_t lobbsPlatformRandom(uint32_t bound)
{
    if (bound == 0)
        return 0;
    static uint32_t state = 1;
    state = state * 1664525u + 1013904223u;
    return state % bound;
}

void lobbsPlatformLog(LoPlatformLogLevel level, const char *fmt, ...)
{
    (void)level;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

void lobbsPlatformAfterFormat(const char *mount)
{
    (void)mount;
}

void lobbsPlatformNoteHandlerStack()
{
}

uint32_t lobbsPlatformLocalNodeId()
{
    return 0xBB500001;
}

size_t lobbsPlatformMaxReplyBytes()
{
    return LOBBS_REPLY_BYTES;
}

void lobbsPlatformSendReply(const LoInbound &inbound, const char *msg)
{
    (void)inbound;
    (void)msg;
}

bool lobbsPlatformInstallAuthorized(const LoInbound &inbound)
{
    return inbound.channel == LoInboundChannel::Local;
}

#endif
