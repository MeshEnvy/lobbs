#pragma once

#include "plugin.h"
#include "LoBBSDal.h"
#include "SinglePortModule.h"
#include "lobbs.pb.h"
#include <string>
#include <vector>


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

    // Helper: split message into chunks for multi-part sending
    std::vector<std::string> splitMessage(const std::string &msg, size_t maxChunkSize);
};

extern LoBBSModule *lobbsModule;
