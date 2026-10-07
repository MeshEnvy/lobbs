#include "Yarn.h"

#include "../../core/LoBBSBootTrace.h"
#include "core/LoBBSStackGuard.h"

YarnApp::YarnApp(LoDb &lodb) : dal_(lodb)
{
    LOBBS_BOOT_STEP("apps init done (yarn last)");
}

