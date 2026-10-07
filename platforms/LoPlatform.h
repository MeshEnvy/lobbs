#pragma once
#include "core/LoWire.h"

#if !defined(LOBBS_PLATFORM_MESHTASTIC) && !defined(LOBBS_PLATFORM_MESHCORE) && !defined(LOBBS_PLATFORM_NATIVE)
#error Define LOBBS_PLATFORM_MESHTASTIC, LOBBS_PLATFORM_MESHCORE, or LOBBS_PLATFORM_NATIVE for this build
#endif

// Lifecycle (PAL contract):
// 1. Construct LoBBSKernel (no FS or hooks yet).
// 2. Host binds filesystem / mesh / RTC (MeshCore: before lobbsMeshCoreInit returns).
// 3. kernel.begin() mounts LoFS, runs wireup, and runs install init.
// 4. Only then may the host call lobbsCoreHandleInbound or platform DM/serial helpers.
// replyContext is valid only for the duration of a single inbound handler call.

#include <stddef.h>
#include <stdint.h>

enum class LoPlatformLogLevel : uint8_t { Debug, Info, Warn, Error };

enum class LoPlatformSetUnixResult : uint8_t { Ok, Invalid, Failed };

uint32_t lobbsPlatformMillis();
uint32_t lobbsPlatformUnixNow();
LoPlatformSetUnixResult lobbsPlatformSetUnix(uint32_t unixSec);
const char *lobbsPlatformClockName();

/** Returns a value in [0, bound). */
uint32_t lobbsPlatformRandom(uint32_t bound);

void lobbsPlatformLog(LoPlatformLogLevel level, const char *fmt, ...);

/** Host-specific work after a mount format (e.g. persist radio DB). */
void lobbsPlatformAfterFormat(const char *mount);

/** Optional stack headroom warning from the inbound handler path. */
void lobbsPlatformNoteHandlerStack();

uint32_t lobbsPlatformLocalNodeId();
/** Max bytes per outbound text frame (paginators target this). */
size_t lobbsPlatformMaxReplyBytes();
void lobbsPlatformSendReply(const LoInbound &inbound, const char *msg);
bool lobbsPlatformInstallAuthorized(const LoInbound &inbound);

#define LOBBS_LOG_DEBUG(fmt, ...) lobbsPlatformLog(LoPlatformLogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOBBS_LOG_INFO(fmt, ...) lobbsPlatformLog(LoPlatformLogLevel::Info, fmt, ##__VA_ARGS__)
#define LOBBS_LOG_WARN(fmt, ...) lobbsPlatformLog(LoPlatformLogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOBBS_LOG_ERROR(fmt, ...) lobbsPlatformLog(LoPlatformLogLevel::Error, fmt, ##__VA_ARGS__)
