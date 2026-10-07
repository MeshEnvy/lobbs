#include "LoBBSKernel.h"
#include "platforms/LoPlatform.h"
#include "LoBBSConfig.h"
#include "LoBBSWireup.h"
#include "LoBBSInstall.h"
#include <lofs/LoFS.h>
#if LOBBS_SEED
#include "LoBBSSeed.h"
#endif
#include "LoBBSReply.h"
#include <cstdio>
#include <cstring>

#include "LoBBSBootTrace.h"
#include "LoBBSStackGuard.h"

LoBBSKernel::LoBBSKernel()
    : lodb_(new LoDb("db")), config_(*lodb_), auth_(*lodb_), mail_(*lodb_), news_(*lodb_), yarn_(*lodb_), wall_(*lodb_)
{
}

void LoBBSKernel::begin()
{
    if (begun_)
        return;
    begun_ = true;
    LOBBS_BOOT_STEP("core begin");
    LOBBS_BOOT_STEP("LoFS::begin");
    LoFS::begin();
    LOBBS_BOOT_STEP("lobbsWireup");
    lobbsWireup();
#ifdef LOBBS_DEMO_MODE
    LOBBS_BOOT_STEP("lobbsInstallAutoSeed");
    lobbsInstallAutoSeed(*this);
    LOBBS_BOOT_STEP("lobbsSeedAll");
    lobbsSeedAll(*this);
#else
    LOBBS_BOOT_STEP("lobbsInstallInit");
    lobbsInstallInit(*this);
#endif
    LOBBS_BOOT_STEP("core begin done");
}

LoBBSInstallState LoBBSKernel::installState() const
{
    return lobbsInstallState(*this);
}

LoBBSKernel::~LoBBSKernel()
{
    delete lodb_;
}

#ifdef PIO_UNIT_TESTING
#include <vector>
std::vector<std::string> *lobbsTestReplySink = nullptr;
#endif

void LoBBSKernel::sendReply(const LoInbound &req, const char *msg)
{
    if (!msg)
        return;
#ifdef PIO_UNIT_TESTING
    if (lobbsTestReplySink) {
        lobbsTestReplySink->push_back(msg);
        return;
    }
#endif
    char mark[40];
    snprintf(mark, sizeof(mark), "reply alloc %uB", (unsigned)strlen(msg));
    lobbsBreadcrumb(mark);
    lobbsPlatformSendReply(req, msg);
    lobbsBreadcrumb("reply out");
}

void lobbsBreadcrumb(const char *step)
{
    if (!step)
        return;
    LOBBS_LOG_INFO("LoBBS %s", step);
}
