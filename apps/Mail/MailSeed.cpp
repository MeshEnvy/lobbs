#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../AppUtil.h"
#include "MailDal.h"
#include "MailSeed.h"
#include <cstdio>
#include <vector>

#include "LoBBSStackGuard.h"

void lobbsSeedMail(LoBBSModule &mod)
{
    MailDal &mail = mod.mail().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&mod, "sysop");
    if (!sysop)
        return;

    for (int i = 1; i <= 8; i++) {
        char fromName[16];
        snprintf(fromName, sizeof(fromName), "demo%02d", i);
        uint64_t fromUuid = lobbsAppUuidForUsername(&mod, fromName);
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
