#include "LoBBSConfig.h"
#if LOBBS_SEED

#include "LoBBSCommandCtx.h"
#include "LoBBSHooks.h"
#include "LoBBSKernel.h"
#include "LoBBSSeed.h"

#include "LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

void lobbsSeedAll(LoBBSKernel &kernel)
{
    LOBBS_BOOT_STEP("seed all: lobbsDoAction seed");
    LoBBSCommandCtx ctx;
    ctx.kernel = &kernel;
    lobbsDoAction("seed", ctx, LoScalar());
    LOBBS_BOOT_STEP("seed all: done");
}

#endif
