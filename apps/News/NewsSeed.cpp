#include "../../core/LoBBSConfig.h"
#if LOBBS_SEED

#include "../../core/LoBBSKernel.h"
#include "../AppUtil.h"
#include "NewsDal.h"
#include "NewsSeed.h"
#include <cstdio>

#include "core/LoBBSStackGuard.h"

void lobbsSeedNews(LoBBSKernel &kernel)
{
    NewsDal &news = kernel.news().dal();
    for (int i = 1; i <= 10; i++) {
        char authorName[16];
        snprintf(authorName, sizeof(authorName), "demo%02d", ((i - 1) % 12) + 1);
        uint64_t author = lobbsAppUuidForUsername(&kernel, authorName);
        if (!author)
            continue;
        char body[80];
        snprintf(body, sizeof(body), "Demo news item %d for paging tests on the board.", i);
        news.postNews(author, body);
    }
}

#endif
