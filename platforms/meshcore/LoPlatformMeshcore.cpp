#if defined(LOBBS_PLATFORM_MESHCORE)
#include "core/LoBBSArch.h"
#include <Arduino.h>
#if LOBBS_ARCH_ESP32
#include <FS.h>
#endif
#if LOBBS_ARCH_RP2040
#include <LittleFS.h>
#endif
#include <Mesh.h>
#include <MeshCore.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/ContactInfo.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "platforms/LoPlatform.h"
#include "platforms/arduino/LoPlatformArduino.h"
#include "core/LoBBSDispatch.h"
#include "core/LoBBSKernel.h"
#include <lofs/LoFS.h>
#include "platforms/meshcore/LoPlatformMeshcore.h"
#include "lofs/platforms/meshcore/LoFSMountsMeshcore.h"

static LoBBSKernel *gCore = nullptr;
static BaseChatMesh *gMesh = nullptr;
static uint32_t gLocalNodeId = 0;
static mesh::RTCClock *gRtc = nullptr;

static uint32_t lobbsHashPubKey(const uint8_t *pub, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= pub[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t lobbsMeshUnixNow()
{
    return gRtc ? gRtc->getCurrentTime() : 0;
}

uint32_t lobbsPlatformUnixNow()
{
    return lobbsMeshUnixNow();
}

LoPlatformSetUnixResult lobbsPlatformSetUnix(uint32_t unixSec)
{
    if (!gRtc)
        return LoPlatformSetUnixResult::Failed;
    if (unixSec < 946684800u)
        return LoPlatformSetUnixResult::Invalid;
    gRtc->setCurrentTime(unixSec);
    return LoPlatformSetUnixResult::Ok;
}

const char *lobbsPlatformClockName()
{
    return gRtc ? "device" : "none";
}

void lobbsPlatformLog(LoPlatformLogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const char *tag = "DEBUG";
    switch (level) {
    case LoPlatformLogLevel::Debug:
        tag = "DEBUG";
        break;
    case LoPlatformLogLevel::Info:
        tag = "INFO ";
        break;
    case LoPlatformLogLevel::Warn:
        tag = "WARN ";
        break;
    case LoPlatformLogLevel::Error:
        tag = "ERROR";
        break;
    }
    Serial.printf("%s | LoBBS ", tag);
    char buf[256];
    lobbsPlatformLogFormat(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
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
    return gLocalNodeId;
}

size_t lobbsPlatformMaxReplyBytes()
{
    return MAX_TEXT_LEN;
}

#if defined(LOFS_NRF52)
#include <Adafruit_LittleFS.h>
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, Adafruit_LittleFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock)
{
    gCore = core;
    gMesh = &mesh;
    lofsMeshcoreSetHostFilesystem(filesystem);
    if (selfPubKey32)
        gLocalNodeId = lobbsHashPubKey(selfPubKey32, PUB_KEY_SIZE);
    gRtc = &rtcClock;
    if (core)
        core->begin();
}
#elif LOBBS_ARCH_ESP32
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, fs::SPIFFSFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock)
{
    gCore = core;
    gMesh = &mesh;
    lofsMeshcoreSetHostFilesystem(filesystem);
    if (selfPubKey32)
        gLocalNodeId = lobbsHashPubKey(selfPubKey32, PUB_KEY_SIZE);
    gRtc = &rtcClock;
    if (core)
        core->begin();
}
#elif LOBBS_ARCH_RP2040
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, LittleFSFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock)
{
    gCore = core;
    gMesh = &mesh;
    lofsMeshcoreSetHostFilesystem(filesystem);
    if (selfPubKey32)
        gLocalNodeId = lobbsHashPubKey(selfPubKey32, PUB_KEY_SIZE);
    gRtc = &rtcClock;
    if (core)
        core->begin();
}
#else
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, void *filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock)
{
    gCore = core;
    gMesh = &mesh;
    lofsMeshcoreSetHostFilesystem(filesystem);
    if (selfPubKey32)
        gLocalNodeId = lobbsHashPubKey(selfPubKey32, PUB_KEY_SIZE);
    gRtc = &rtcClock;
    if (core)
        core->begin();
}
#endif

void lobbsMeshCoreLoop(LoBBSKernel *core)
{
    (void)core;
    LoFS::maintain(15);
}

static void lobbsFillInbound(LoInbound &in, const ContactInfo &from, const char *text)
{
    in.from.key = lobbsHashPubKey(from.id.pub_key, PUB_KEY_SIZE);
    in.channel = LoInboundChannel::Radio;
    in.text = text;
    in.replyContext = &from;
}

bool lobbsMeshCoreHandleDm(LoBBSKernel *core, const void *contact, const char *text)
{
    if (!core || !contact || !text || !text[0])
        return false;

    const ContactInfo &from = *static_cast<const ContactInfo *>(contact);
    LoInbound in;
    lobbsFillInbound(in, from, text);

    if (text[0] != '/')
        return false;
    lobbsCoreHandleInbound(core, in);
    return true;
}

bool lobbsMeshCoreDeliverMail(LoBBSKernel *core, const void *contact, const char *text)
{
    if (!core || !contact || !text || !text[0] || text[0] == '/')
        return false;
    const ContactInfo &from = *static_cast<const ContactInfo *>(contact);
    LoInbound in;
    lobbsFillInbound(in, from, text);
    lobbsCoreDeliverUserMail(core, in);
    return true;
}

bool lobbsMeshCoreHandleSerialLine(LoBBSKernel *core, const char *line)
{
    if (!core || !line || !line[0])
        return false;
    if (line[0] != '/')
        return false;

    LoInbound in;
    in.from.key = lobbsPlatformLocalNodeId();
    in.channel = LoInboundChannel::Local;
    in.text = line;
    in.replyContext = nullptr;
    lobbsCoreHandleInbound(core, in);
    return true;
}

static void lobbsSendChunk(const ContactInfo &to, const char *chunk, size_t len)
{
    if (!gMesh || !chunk || len == 0)
        return;
    char buf[MAX_TEXT_LEN + 1];
    if (len > MAX_TEXT_LEN)
        len = MAX_TEXT_LEN;
    memcpy(buf, chunk, len);
    buf[len] = '\0';

    uint32_t expected_ack = 0;
    uint32_t est_timeout = 0;
    (void)gMesh->sendMessage(to, lobbsMeshUnixNow(), 0, buf, expected_ack, est_timeout);
}

void lobbsPlatformSendReply(const LoInbound &inbound, const char *msg)
{
    if (!msg)
        return;
    if (!inbound.replyContext) {
        if (inbound.channel == LoInboundChannel::Local)
            Serial.println(msg);
        return;
    }
    const ContactInfo &to = *static_cast<const ContactInfo *>(inbound.replyContext);
    size_t len = strlen(msg);
    size_t maxBytes = lobbsPlatformMaxReplyBytes();
    if (len > maxBytes)
        len = maxBytes;
    lobbsSendChunk(to, msg, len);
}

bool lobbsPlatformInstallAuthorized(const LoInbound &inbound)
{
    return inbound.channel == LoInboundChannel::Local;
}
#endif
