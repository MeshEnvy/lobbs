#include "../../core/LoBBSConfig.h"
#if LOBBS_SEED

#include "../../core/LoBBSKernel.h"
#include "AuthDal.h"
#include "AuthSeed.h"
#include <cstdio>

#include "core/LoBBSStackGuard.h"

void lobbsSeedAuth(LoBBSKernel &kernel)
{
    AuthDal &auth = kernel.auth().dal();
    static constexpr const char *kPass = "demo1";
    auth.createUser("sysop", kPass, 0xDE000001, true);
    auth.logoutUser(0xDE000001);
    for (int i = 1; i <= 12; i++) {
        char name[16];
        snprintf(name, sizeof(name), "demo%02d", i);
        uint32_t nodeId = 0xDE000010u + (uint32_t)i;
        auth.createUser(name, kPass, nodeId);
        auth.logoutUser(nodeId);
    }
}

#endif
