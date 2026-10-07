#pragma once
#include "LoBBSInstall.h"
#include "LoBBSVersion.h"
#include "MeshModule.h"
#include "SinglePortModule.h"
#include "apps/Auth/Auth.h"
#include "apps/Config/Config.h"
#include "apps/Mail/Mail.h"
#include "apps/News/News.h"
#include "apps/Wall/Wall.h"
#include "apps/Yarn/Yarn.h"
#include <lodb/LoDB.h>
#include <string>

class LoBBSModule;

void lobbsBreadcrumb(const char *step);

class LoBBSModule : public SinglePortModule
{
    friend ProcessMessage lobbsDispatchReceived(class LoBBSModule *mod, const meshtastic_MeshPacket &mp);

  public:
    LoBBSModule();
    ~LoBBSModule();

    void sendReply(const meshtastic_MeshPacket &req, const char *msg);
    void sendReply(const meshtastic_MeshPacket &req, const std::string &msg) { sendReply(req, msg.c_str()); }

    ConfigApp &config() { return config_; }
    AuthApp &auth() { return auth_; }
    MailApp &mail() { return mail_; }
    NewsApp &news() { return news_; }
    WallApp &wall() { return wall_; }
    YarnApp &yarn() { return yarn_; }
    LoDb *lodb() { return lodb_; }
    LoBBSInstallState installState() const;

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];

  private:
    LoDb *lodb_;
    ConfigApp config_;
    AuthApp auth_;
    MailApp mail_;
    NewsApp news_;
    WallApp wall_;
    YarnApp yarn_;
};

#ifdef PIO_UNIT_TESTING
#include <string>
#include <vector>
extern std::vector<std::string> *lobbsTestReplySink;
#endif

