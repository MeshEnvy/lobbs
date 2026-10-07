#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Mail.h"

#include "LoBBSStackGuard.h"

MailApp::MailApp(LoDb &lodb) : dal_(lodb) {}

#endif
