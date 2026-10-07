#include "../../core/LoBBSConfig.h"
#if LOBBS_SEED

#include "../../core/LoBBSKernel.h"
#include "../AppUtil.h"
#include "YarnDal.h"
#include "YarnSeed.h"

#include "core/LoBBSStackGuard.h"

void lobbsSeedYarn(LoBBSKernel &kernel)
{
    YarnDal &yarn = kernel.yarn().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&kernel, "sysop");
    if (!sysop)
        return;
    const char *words1[] = {"Demo", "yarn", "seed", "line", "one."};
    const char *words2[] = {"Another", "short", "contribution."};
    yarn.appendWords(sysop, true, words1, 5, LOBBS_YARN_DEFAULT_PERIOD_SEC, LOBBS_YARN_DEFAULT_MAX_WORDS,
                     LOBBS_YARN_DEFAULT_MAX_CHARS);
    yarn.appendWords(sysop, true, words2, 3, LOBBS_YARN_DEFAULT_PERIOD_SEC, LOBBS_YARN_DEFAULT_MAX_WORDS,
                     LOBBS_YARN_DEFAULT_MAX_CHARS);
}

#endif
