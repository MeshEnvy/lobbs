#pragma once
#include "core/LoBBSArch.h"
#include <stdint.h>

struct LoBBSKernel;

class BaseChatMesh;

namespace mesh {
class RTCClock;
}

#if defined(LOFS_NRF52)
class Adafruit_LittleFS;
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, Adafruit_LittleFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock);
#elif LOBBS_ARCH_ESP32
namespace fs {
class SPIFFSFS;
}
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, fs::SPIFFSFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock);
#elif LOBBS_ARCH_RP2040
class LittleFSFS;
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, LittleFSFS &filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock);
#else
void lobbsMeshCoreInit(LoBBSKernel *core, BaseChatMesh &mesh, void *filesystem, const uint8_t *selfPubKey32,
                       mesh::RTCClock &rtcClock);
#endif

void lobbsMeshCoreLoop(LoBBSKernel *core);
bool lobbsMeshCoreHandleDm(LoBBSKernel *core, const void *contact, const char *text);
bool lobbsMeshCoreDeliverMail(LoBBSKernel *core, const void *contact, const char *text);
bool lobbsMeshCoreHandleSerialLine(LoBBSKernel *core, const char *line);
