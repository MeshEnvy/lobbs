#include "../../core/LoBBSConfig.h"
#if LOBBS_SEED

#include "../../core/LoBBSKernel.h"
#include "../AppUtil.h"
#include "WallDal.h"
#include "WallSeed.h"

#include "core/LoBBSStackGuard.h"

void lobbsSeedWall(LoBBSKernel &kernel)
{
    WallDal &wall = kernel.wall().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&kernel, "sysop");
    if (!sysop)
        return;
    const char *tokens[] = {"a1#", "a2#", "b2#", "c3x", "d4y"};
    wall.applyPaintTokens(sysop, true, tokens, 5, LOBBS_WALL_DEFAULT_PERIOD_SEC, LOBBS_WALL_DEFAULT_MAX_CELLS);
}

#endif
