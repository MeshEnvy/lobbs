#pragma once
#include "core/LoWire.h"
#include "MeshModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

class LoBBSKernel;

LoInbound lobbsInboundFromPacket(const meshtastic_MeshPacket &mp, char *payloadBuf, size_t payloadBufLen);
ProcessMessage lobbsDispatchReceived(LoBBSKernel *kernel, const meshtastic_MeshPacket &mp);
