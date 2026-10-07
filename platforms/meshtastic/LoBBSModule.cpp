#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "platforms/meshtastic/LoBBSModule.h"
#include "platforms/meshtastic/LoBBSDispatchMeshtastic.h"

LoBBSModule::LoBBSModule() : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    kernel_.begin();
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    return lobbsDispatchReceived(&kernel_, mp);
}
#endif
