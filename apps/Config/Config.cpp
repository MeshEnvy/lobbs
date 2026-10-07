#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Config.h"

#include "LoBBSStackGuard.h"

ConfigApp::ConfigApp(LoDb &lodb) : dal_(lodb) {}

#endif
