#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../AppUtil.h"
#include "YarnDal.h"
#include "YarnSeed.h"

#include "LoBBSStackGuard.h"

void lobbsSeedYarn(LoBBSModule &mod)
{
    YarnDal &yarn = mod.yarn().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&mod, "sysop");
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
