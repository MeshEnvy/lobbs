#include "LoBBSConfig.h"
#if LOBBS_SEED

#include "LoBBSCommandCtx.h"
#include "LoBBSHooks.h"
#include "LoBBSModule.h"
#include "LoBBSSeed.h"

#include "LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

void lobbsSeedAll(LoBBSModule &mod)
{
    LOBBS_BOOT_STEP("seed all: lobbsDoAction seed");
    LoBBSCommandCtx ctx;
    ctx.mod = &mod;
    lobbsDoAction("seed", ctx, LoScalar());
    LOBBS_BOOT_STEP("seed all: done");
}

#endif
