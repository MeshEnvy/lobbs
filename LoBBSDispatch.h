#pragma once
#include "MeshModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

class LoBBSModule;

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp);

