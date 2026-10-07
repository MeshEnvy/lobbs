#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Yarn.h"

#include "../../LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

YarnApp::YarnApp(LoDb &lodb) : dal_(lodb)
{
    LOBBS_BOOT_STEP("apps init done (yarn last)");
}

#endif
