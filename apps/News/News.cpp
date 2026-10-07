#if !MESHTASTIC_EXCLUDE_LOBBS

#include "News.h"

#include "LoBBSStackGuard.h"

NewsApp::NewsApp(LoDb &lodb) : dal_(lodb) {}

#endif
