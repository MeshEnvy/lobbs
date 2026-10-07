#pragma once
#include "LoBBSInstall.h"
#include "LoBBSVersion.h"
#include "LoWire.h"
#include "apps/Auth/Auth.h"
#include "apps/Config/Config.h"
#include "apps/Mail/Mail.h"
#include "apps/News/News.h"
#include "apps/Wall/Wall.h"
#include "apps/Yarn/Yarn.h"
#include <lodb/LoDB.h>
#include <string>

class LoBBSKernel;

void lobbsBreadcrumb(const char *step);

class LoBBSKernel
{
  public:
    LoBBSKernel();
    ~LoBBSKernel();

    /** Host binds FS (MeshCore) then calls once before inbound traffic. */
    void begin();

    void sendReply(const LoInbound &req, const char *msg);
    void sendReply(const LoInbound &req, const std::string &msg) { sendReply(req, msg.c_str()); }

    ConfigApp &config() { return config_; }
    AuthApp &auth() { return auth_; }
    MailApp &mail() { return mail_; }
    NewsApp &news() { return news_; }
    WallApp &wall() { return wall_; }
    YarnApp &yarn() { return yarn_; }
    LoDb *lodb() { return lodb_; }
    LoBBSInstallState installState() const;

    char msgBuffer[256];

  private:
    bool begun_ = false;
    LoDb *lodb_;
    ConfigApp config_;
    AuthApp auth_;
    MailApp mail_;
    NewsApp news_;
    YarnApp yarn_;
    WallApp wall_;
};
