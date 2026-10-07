#include "../../core/LoBBSConfig.h"
#if LOBBS_SEED

#include "../../core/LoBBSKernel.h"
#include "../AppUtil.h"
#include "MailDal.h"
#include "MailSeed.h"
#include <cstdio>
#include <vector>

#include "core/LoBBSStackGuard.h"

void lobbsSeedMail(LoBBSKernel &kernel)
{
    MailDal &mail = kernel.mail().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&kernel, "sysop");
    if (!sysop)
        return;

    for (int i = 1; i <= 8; i++) {
        char fromName[16];
        snprintf(fromName, sizeof(fromName), "demo%02d", i);
        uint64_t fromUuid = lobbsAppUuidForUsername(&kernel, fromName);
        if (!fromUuid)
            continue;
        char body[64];
        snprintf(body, sizeof(body), "Demo mail #%d to sysop with extra text.", i);
        mail.sendMail(fromUuid, sysop, body);
    }

    std::vector<LoScalar> inbox = mail.getAllMailForUser(sysop);
    for (size_t j = 0; j < inbox.size() && j < 3; j++)
        mail.markMailAsRead(MailDal::mailUuid(inbox[j]));
}

#endif
