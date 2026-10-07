#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSModule.h"
#include "LoBBSConfig.h"
#include "LoBBSDispatch.h"
#include "LoBBSInstall.h"
#include "LoBBSWireup.h"
#include <lofs/LoFS.h>
#if LOBBS_SEED
#include "LoBBSSeed.h"
#endif
#include "LoBBSReply.h"
#include "MeshService.h"
#include <cstdio>
#include <cstring>

#include "LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

LoBBSModule::LoBBSModule()
    : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP), lodb_(new LoDb("db")), config_(*lodb_), auth_(*lodb_),
      mail_(*lodb_), news_(*lodb_), yarn_(*lodb_), wall_(*lodb_)
{
    LOBBS_BOOT_STEP("module ctor body");
    LOBBS_BOOT_STEP("LoFS::begin");
    LoFS::begin();
    LOBBS_BOOT_STEP("lobbsWireup");
    lobbsWireup();
#ifdef LOBBS_DEMO_MODE
    LOBBS_BOOT_STEP("lobbsInstallAutoSeed");
    lobbsInstallAutoSeed(*this);
    LOBBS_BOOT_STEP("lobbsSeedAll");
    lobbsSeedAll(*this);
#else
    LOBBS_BOOT_STEP("lobbsInstallInit");
    lobbsInstallInit(*this);
#endif
    LOBBS_BOOT_STEP("module ctor done");
}

LoBBSInstallState LoBBSModule::installState() const
{
    return lobbsInstallState(*this);
}

LoBBSModule::~LoBBSModule()
{
    delete lodb_;
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    return lobbsDispatchReceived(this, mp);
}

void lobbsBreadcrumb(const char *step)
{
    if (!step)
        return;
#if !defined(ARCH_PORTDUINO)
    Serial.print("INFO  | LoBBS ");
    Serial.println(step);
#else
    LOG_INFO("LoBBS %s", step);
#endif
}

#ifdef PIO_UNIT_TESTING
#include <vector>
std::vector<std::string> *lobbsTestReplySink = nullptr;
#endif

void LoBBSModule::sendReply(const meshtastic_MeshPacket &req, const char *msg)
{
    if (!msg)
        return;
#ifdef PIO_UNIT_TESTING
    if (lobbsTestReplySink) {
        lobbsTestReplySink->push_back(msg);
        return;
    }
#endif
    char mark[40];
    snprintf(mark, sizeof(mark), "reply alloc %uB", (unsigned)strlen(msg));
    lobbsBreadcrumb(mark);
    meshtastic_MeshPacket *reply = allocDataPacket();
    if (!reply) {
        lobbsBreadcrumb("reply alloc failed");
        LOG_WARN("LoBBS: no packet for reply (pool exhausted)");
        return;
    }
    static constexpr char truncMarker[] = "[...]";
    static constexpr size_t truncMarkerLen = sizeof(truncMarker) - 1;

    size_t msgLen = strlen(msg);
    bool isTruncated = msgLen > LOBBS_REPLY_BYTES;
    size_t payloadSize = isTruncated ? LOBBS_REPLY_BYTES : msgLen;
    reply->decoded.payload.size = payloadSize;

    if (isTruncated) {
        size_t copyLen = LOBBS_REPLY_BYTES > truncMarkerLen ? LOBBS_REPLY_BYTES - truncMarkerLen : 0;
        memcpy(reply->decoded.payload.bytes, msg, copyLen);
        memcpy(reply->decoded.payload.bytes + copyLen, truncMarker, truncMarkerLen);
    } else {
        memcpy(reply->decoded.payload.bytes, msg, payloadSize);
    }
    setReplyTo(reply, req);
    reply->decoded.want_response = false;
    snprintf(mark, sizeof(mark), "reply send %08x", reply->id);
    lobbsBreadcrumb(mark);
    // Phone ToRadio uses from=0; RoutingModule only forwards to the app when from != 0.
    const bool ccPhone = (req.from == 0);
    service->sendToMesh(reply, RX_SRC_LOCAL, ccPhone);
    lobbsBreadcrumb("reply out");
}

#endif
