#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "core/LoBBSArch.h"
#include "platforms/LoPlatform.h"
#include "platforms/arduino/LoPlatformArduino.h"
#include "core/LoBBSReply.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "MeshService.h"
#include "mesh/MeshModule.h"
#include "mesh/NodeDB.h"
#include "mesh/Router.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "mesh/generated/meshtastic/portnums.pb.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#if LOBBS_ARCH_NRF52 && __has_include(<task.h>)
#include <task.h>
#endif

uint32_t lobbsPlatformUnixNow()
{
    return getTime();
}

LoPlatformSetUnixResult lobbsPlatformSetUnix(uint32_t unixSec)
{
    struct timeval tv;
    tv.tv_sec = (time_t)unixSec;
    tv.tv_usec = 0;
    RTCSetResult r = perhapsSetRTC(RTCQualityNTP, &tv, true);
    if (r == RTCSetResultSuccess)
        return LoPlatformSetUnixResult::Ok;
    if (r == RTCSetResultInvalidTime)
        return LoPlatformSetUnixResult::Invalid;
    return LoPlatformSetUnixResult::Failed;
}

const char *lobbsPlatformClockName()
{
    return RtcName(getRTCQuality());
}

void lobbsPlatformLog(LoPlatformLogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[256];
    lobbsPlatformLogFormat(buf, sizeof(buf), fmt, args);
    va_end(args);
    switch (level) {
    case LoPlatformLogLevel::Debug:
        LOG_DEBUG("%s", buf);
        break;
    case LoPlatformLogLevel::Info:
        LOG_INFO("%s", buf);
        break;
    case LoPlatformLogLevel::Warn:
        LOG_WARN("%s", buf);
        break;
    case LoPlatformLogLevel::Error:
        LOG_ERROR("%s", buf);
        break;
    }
}

void lobbsPlatformAfterFormat(const char *mount)
{
    if (!mount || strcmp(mount, "internal") != 0)
        return;
    if (nodeDB)
        nodeDB->saveToDisk();
}

void lobbsPlatformNoteHandlerStack()
{
#if LOBBS_ARCH_NRF52 && __has_include(<task.h>)
    static bool lowStackWarned = false;
    unsigned headroom = (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    if (!lowStackWarned && headroom < 1024) {
        lowStackWarned = true;
        lobbsPlatformLog(LoPlatformLogLevel::Warn, "LoBBS loop stack headroom low: %u B", headroom);
    }
#endif
}

uint32_t lobbsPlatformLocalNodeId()
{
    return nodeDB ? nodeDB->getNodeNum() : 0;
}

size_t lobbsPlatformMaxReplyBytes()
{
    return LOBBS_REPLY_BYTES;
}

static void lobbsSendMeshReply(const meshtastic_MeshPacket &req, const char *msg, size_t maxBytes)
{
    if (!msg || !service)
        return;
    if (!router)
        return;
    meshtastic_MeshPacket *reply = router->allocForSending();
    if (!reply)
        return;
    reply->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;

    static constexpr char truncMarker[] = "[...]";
    static constexpr size_t truncMarkerLen = sizeof(truncMarker) - 1;

    size_t msgLen = strlen(msg);
    bool isTruncated = msgLen > maxBytes;
    size_t payloadSize = isTruncated ? maxBytes : msgLen;
    reply->decoded.payload.size = payloadSize;

    if (isTruncated) {
        size_t copyLen = maxBytes > truncMarkerLen ? maxBytes - truncMarkerLen : 0;
        memcpy(reply->decoded.payload.bytes, msg, copyLen);
        memcpy(reply->decoded.payload.bytes + copyLen, truncMarker, truncMarkerLen);
    } else {
        memcpy(reply->decoded.payload.bytes, msg, payloadSize);
    }
    setReplyTo(reply, req);
    reply->decoded.want_response = false;
    const bool ccPhone = (req.from == 0);
    service->sendToMesh(reply, RX_SRC_LOCAL, ccPhone);
}

void lobbsPlatformSendReply(const LoInbound &inbound, const char *msg)
{
    if (!inbound.replyContext)
        return;
    lobbsSendMeshReply(*static_cast<const meshtastic_MeshPacket *>(inbound.replyContext), msg, lobbsPlatformMaxReplyBytes());
}

bool lobbsPlatformInstallAuthorized(const LoInbound &inbound)
{
    if (inbound.channel == LoInboundChannel::Local)
        return true;
    if (!inbound.replyContext)
        return false;
    const meshtastic_MeshPacket &mp = *static_cast<const meshtastic_MeshPacket *>(inbound.replyContext);
    if (mp.from == 0)
        return true;
#if !MESHTASTIC_EXCLUDE_PKI
    if (mp.pki_encrypted) {
        if ((config.security.admin_key[0].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[0].bytes, 32) == 0) ||
            (config.security.admin_key[1].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[1].bytes, 32) == 0) ||
            (config.security.admin_key[2].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[2].bytes, 32) == 0))
            return true;
    }
#endif
    return false;
}
#endif
