#pragma once

#include "LoBBSDal.h"
#include "SinglePortModule.h"
#include "lobbs.pb.h"

#define LOBBS_VERSION "1.0.0"
#define LOBBS_HEADER "LoBBS v" LOBBS_VERSION "\nCommands:\n"

/**
 * LoBBS (Lo-Fi Bulletin Board System) Module
 *
 * A simple BBS-style messaging system for Meshtastic.
 * Handles text messages on TEXT_MESSAGE_APP port.
 *
 * Uses LoDB for storage:
 * - Users table: /lodb/lobbs/users/<username>.pr
 * - Sessions table: /lodb/lobbs/sessions/<nodeid_hex>.pr
 */
class LoBBSModule : public SinglePortModule
{
  public:
    LoBBSModule();
    static constexpr size_t MAX_REPLY_BYTES = 200;

  protected:
    /**
     * Handle an incoming message
     */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];
    char replyBuffer[256];

  private:
    // Data access layer for database operations
    LoBBSDal *dal;

    void sendReply(NodeNum to, const std::string &msg);
};
