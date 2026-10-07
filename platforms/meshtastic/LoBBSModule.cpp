#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "platforms/meshtastic/LoBBSModule.h"
#include "platforms/meshtastic/LoBBSDispatchMeshtastic.h"
#include "platforms/LoPlatform.h"
#include <lofs/LoFS.h>

LoBBSModule::LoBBSModule() : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    kernel_.begin();
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    static uint32_t lastMaintainMs = 0;
    const uint32_t now = lobbsPlatformMillis();
    if (now - lastMaintainMs >= 5000) {
        lastMaintainMs = now;
        LoFS::maintain(15);
    }
    return lobbsDispatchReceived(&kernel_, mp);
}
#endif
