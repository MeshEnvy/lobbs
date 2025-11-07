#pragma once

#include "lobbs.pb.h"
#include "lodb/LoDB.h"
#include <stdint.h>
#include <vector>

struct LoBBSNewsEntry {
    meshtastic_LoBBSNews *news;
    bool isRead;
};

// Maximum username length (from lobbs.options: meshtastic.LoBBSUser.username max_size:32)
#define LOBBS_MAX_USERNAME_LEN 32
#define LOBBS_USERNAME_BUFFER_SIZE (LOBBS_MAX_USERNAME_LEN + 1) // +1 for null terminator

// Stringify macro for converting numeric defines to string literals
#define LOBBS_XSTR(x) LOBBS_STR(x)
#define LOBBS_STR(x) #x

/**
 * LoBBS Data Access Layer
 *
 * Handles all database operations for LoBBS:
 * - User management (creation, lookup, authentication)
 * - Session management (login, logout)
 * - Password hashing and verification
 */
class LoBBSDal
{
  public:
    /**
     * Constructor
     * @param hostNodeId Node ID used as salt for UUID generation
     */
    LoBBSDal(uint32_t hostNodeId);

    /**
     * Destructor
     */
    ~LoBBSDal();

    /**
     * Validate username format
     */
    bool isValidUsername(const char *username);

    /**
     * Validate password format
     */
    bool isValidPassword(const char *password);

    /**
     * Load user by username from LoDB
     */
    bool loadUserByUsername(const char *username, meshtastic_LoBBSUser *user);

    /**
     * Load user by node ID (via session lookup)
     */
    bool loadUserByNodeId(uint32_t nodeId, meshtastic_LoBBSUser *user);

    /**
     * Create a new user account
     */
    bool createUser(const char *username, const char *password, uint32_t nodeId);

    /**
     * Verify password for a user
     */
    bool verifyPassword(const meshtastic_LoBBSUser *user, const char *password);

    /**
     * Log in a user (create session)
     */
    bool loginUser(const char *username, uint32_t nodeId);

    /**
     * Log out a user (delete session)
     */
    bool logoutUser(uint32_t nodeId);

    /**
     * Get user UUID by username lookup
     * @return User UUID, or 0 if user not found
     */
    uint64_t getUserUuidByUsername(const char *username);

    /**
     * Send a mail message from one user to another
     */
    bool sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message);

    /**
     * Get mail for a user (sorted by timestamp descending)
     * Returns vector of mail records - caller must free
     */
    std::vector<void *> getMailForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);

    /**
     * Mark a mail message as read
     */
    bool markMailAsRead(uint64_t mailUuid);

    /**
     * Post a news item
     */
    bool postNews(uint64_t authorUserUuid, const char *message);

    /**
     * Get news for a user (with read status, sorted unread first then by timestamp desc)
     * Returns vector of news records - caller must free
     */
    std::vector<LoBBSNewsEntry> getNewsForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);

    /**
     * Check if a news item has been read by a user
     */
    bool isNewsReadByUser(uint64_t newsUuid, uint64_t userUuid);

    /**
     * Mark a news item as read by a user
     */
    bool markNewsAsRead(uint64_t newsUuid, uint64_t userUuid);

    /**
     * Get the underlying database instance
     */
    LoDb *getDb() { return db; }

  private:
    /**
     * Hash a password using SHA256
     */
    static void hashPassword(const char *password, uint8_t *hash);

    // LoDB database instance
    LoDb *db;

    // Host node ID used as salt for user UUID generation
    uint32_t hostNodeId;
};
