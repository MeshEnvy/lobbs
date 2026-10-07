#if !MESHTASTIC_EXCLUDE_LOBBS

#include "paginate.h"
#include "LoBBSReply.h"
#include <cstdio>

#include "LoBBSStackGuard.h"

bool lobbsPaginateMachine(uint32_t reqId, const std::string &document, uint32_t page1, std::string &pageOut, const char **errMsg)
{
    pageOut.clear();
    if (page1 == 0)
        page1 = 1;

    char prefix[40];
    size_t prefixLen = (size_t)snprintf(prefix, sizeof(prefix), "<%u>ok\n", reqId);
    if (prefixLen + document.size() <= LOBBS_REPLY_BYTES) {
        if (page1 > 1) {
            if (errMsg)
                *errMsg = "No such page.";
            return false;
        }
        pageOut = prefix;
        pageOut += document;
        return true;
    }

    // Size every page for the widest header at this digit count; widen until the page count fits.
    uint32_t widest = 9;
    size_t budget = 0;
    uint32_t pages = 0;
    while (true) {
        prefixLen = (size_t)snprintf(prefix, sizeof(prefix), "<%u>ok [%u:%u]\n", reqId, widest, widest);
        budget = LOBBS_REPLY_BYTES - prefixLen;
        pages = (uint32_t)((document.size() + budget - 1) / budget);
        if (pages <= widest)
            break;
        widest = widest * 10 + 9;
    }

    if (page1 > pages) {
        if (errMsg)
            *errMsg = "No such page.";
        return false;
    }
    snprintf(prefix, sizeof(prefix), "<%u>ok [%u:%u]\n", reqId, page1, pages);
    pageOut = prefix;
    pageOut.append(document, (size_t)(page1 - 1) * budget, budget);
    return true;
}

#endif
