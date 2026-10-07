#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Auth.h"

#include "LoBBSStackGuard.h"

AuthApp::AuthApp(LoDb &lodb) : dal_(lodb) {}

#endif
