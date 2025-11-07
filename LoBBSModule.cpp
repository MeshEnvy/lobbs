#include "LoBBSModule.h"
#include "LoBBSDal.h"
#include "MeshService.h"
#include "airtime.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "lobbs.pb.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

// Static helper: case-insensitive substring search
static const char *stristr(const char *haystack, const char *needle)
{
    if (!*needle)
        return haystack;

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower(*h) == tolower(*n)) {
            h++;
            n++;
        }
        if (!*n)
            return haystack;
    }
    return nullptr;
}

// Static helper: comparator for case-insensitive alphabetical username sorting
static int compareUsernames(const void *a, const void *b)
{
    const meshtastic_LoBBSUser *u1 = (const meshtastic_LoBBSUser *)a;
    const meshtastic_LoBBSUser *u2 = (const meshtastic_LoBBSUser *)b;
    return strcasecmp(u1->username, u2->username);
}

// Static helper: load a user by UUID
static bool loadUserByUuid(LoBBSDal *dal, uint64_t uuid, meshtastic_LoBBSUser *outUser)
{
    auto users = dal->getDb()->select(
        "users",
        [uuid](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            return u->uuid == uuid;
        },
        nullptr);

    bool found = false;
    if (!users.empty()) {
        *outUser = *(const meshtastic_LoBBSUser *)users[0];
        found = true;
    }

    for (auto *userPtr : users) {
        delete[] (uint8_t *)userPtr;
    }

    return found;
}

// Static helper: format time ago (e.g., "2h ago", "5m ago", "1d ago")
static void formatTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize)
{
    uint32_t now = getTime();
    if (now < timestamp) {
        snprintf(buffer, bufferSize, "now");
        return;
    }

    uint32_t diff = now - timestamp;

    if (diff < 60) {
        snprintf(buffer, bufferSize, "%ds ago", diff);
    } else if (diff < 3600) {
        snprintf(buffer, bufferSize, "%dm ago", diff / 60);
    } else if (diff < 86400) {
        snprintf(buffer, bufferSize, "%dh ago", diff / 3600);
    } else {
        snprintf(buffer, bufferSize, "%dd ago", diff / 86400);
    }
}

// Static helper: truncate message for list view
static void truncateMessage(const char *message, char *buffer, size_t bufferSize, size_t maxLen)
{
    size_t msgLen = strlen(message);
    if (msgLen <= maxLen) {
        strncpy(buffer, message, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        size_t copyLen = maxLen < bufferSize - 4 ? maxLen : bufferSize - 4;
        strncpy(buffer, message, copyLen);
        buffer[copyLen] = '\0';
        strncat(buffer, "...", bufferSize - strlen(buffer) - 1);
    }
}

static void freeMailMessages(std::vector<void *> &mailMessages)
{
    for (auto *mailPtr : mailMessages) {
        delete[] (uint8_t *)mailPtr;
    }
    mailMessages.clear();
}

static void freeNewsEntries(std::vector<LoBBSNewsEntry> &newsItems)
{
    for (auto &entry : newsItems) {
        delete[] (uint8_t *)entry.news;
    }
    newsItems.clear();
}

LoBBSModule::LoBBSModule() : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    // Create data access layer with host node ID as salt
    dal = new LoBBSDal(nodeDB->getNodeNum());
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    LOG_DEBUG("LoBBS received DM from=0x%0x, id=%d, msg=%.*s", mp.from, mp.id, mp.decoded.payload.size, mp.decoded.payload.bytes);

    meshtastic_LoBBSUser existingUser = meshtastic_LoBBSUser_init_zero;
    bool isAuthenticated = dal->loadUserByNodeId(mp.from, &existingUser);
    if (isAuthenticated) {
        LOG_DEBUG("User node ID: %08x", mp.from);
        LOG_DEBUG("User UUID: " LODB_UUID_FMT, LODB_UUID_ARGS(existingUser.uuid));
        LOG_DEBUG("User: %s", existingUser.username);
        LOG_DEBUG("User is authenticated: %d", isAuthenticated);
    } else {
        LOG_DEBUG("User node ID: %08x", mp.from);
        LOG_DEBUG("User is not authenticated");
    }

    // Copy payload to mutable buffer and null terminate (there is no null terminator in the payload)
    memcpy(msgBuffer, mp.decoded.payload.bytes, mp.decoded.payload.size);
    msgBuffer[mp.decoded.payload.size] = '\0';

    // Check for @mention mail sending BEFORE tokenizing (authenticated users only)
    // This must happen before strtok destroys the buffer
    if (isAuthenticated && strchr(msgBuffer, '@')) {
        LOG_DEBUG("Processing @mention mail from node=0x%0x", mp.from);

        // Extract message content and find all @mentions
        std::vector<std::string> recipients;
        std::string messageContent = std::string(msgBuffer);

        // Parse the message for @mentions
        char *token = msgBuffer;
        bool foundAny = false;

        while (*token) {
            if (*token == '@') {
                foundAny = true;
                // Found a mention, extract username
                token++; // Skip @
                char *usernameStart = token;
                while (*token && (isalnum(*token) || *token == '_')) {
                    token++;
                }

                // Save username (only if not already in list)
                char username[LOBBS_USERNAME_BUFFER_SIZE];
                size_t usernameLen = token - usernameStart;
                if (usernameLen > 0 && usernameLen < LOBBS_USERNAME_BUFFER_SIZE) {
                    strncpy(username, usernameStart, usernameLen);
                    username[usernameLen] = '\0';
                    std::string usernameStr(username);
                    // Only add if not already in recipients list
                    if (std::find(recipients.begin(), recipients.end(), usernameStr) == recipients.end()) {
                        recipients.push_back(usernameStr);
                    }
                }
            } else {
                token++;
            }
        }

        if (foundAny && !recipients.empty()) {
            // Validate and send to each recipient
            int successCount = 0;
            int failCount = 0;
            std::string failedUsers;

            for (const auto &recipient : recipients) {
                // Get recipient UUID
                uint64_t recipientUuid = dal->getUserUuidByUsername(recipient.c_str());
                if (recipientUuid == 0) {
                    LOG_WARN("User not found: %s", recipient.c_str());
                    if (failCount > 0)
                        failedUsers += ", ";
                    failedUsers += "@" + recipient;
                    failCount++;
                    continue;
                }

                // Send mail
                if (dal->sendMail(existingUser.uuid, recipientUuid, messageContent.c_str())) {
                    LOG_INFO("Sent mail from %s to %s", existingUser.username, recipient.c_str());
                    successCount++;
                } else {
                    LOG_ERROR("Failed to send mail to %s", recipient.c_str());
                    if (failCount > 0)
                        failedUsers += ", ";
                    failedUsers += "@" + recipient;
                    failCount++;
                }
            }

            // Send confirmation
            if (successCount > 0 && failCount == 0) {
                if (successCount == 1) {
                    snprintf(replyBuffer, sizeof(replyBuffer), "Mail sent to @%s", recipients[0].c_str());
                } else {
                    snprintf(replyBuffer, sizeof(replyBuffer), "Mail sent to %d users", successCount);
                }
                sendReply(mp.from, replyBuffer);
            } else if (successCount > 0 && failCount > 0) {
                snprintf(replyBuffer, sizeof(replyBuffer), "Mail sent to %d users. Failed: %s", successCount,
                         failedUsers.c_str());
                sendReply(mp.from, replyBuffer);
            } else {
                snprintf(replyBuffer, sizeof(replyBuffer), "Failed to send mail. Users not found: %s", failedUsers.c_str());
                sendReply(mp.from, replyBuffer);
            }

            return ProcessMessage::CONTINUE;
        }
    }

    // Tokenize for command processing (this modifies msgBuffer)
    char *cmdName = strtok(msgBuffer, " ");
    LOG_DEBUG("Token: %s", cmdName);

    if (strcasecmp(cmdName, "/hi") == 0) {
        LOG_DEBUG("Processing /hi command from node=0x%0x", mp.from);

        // Get username
        char *username = strtok(NULL, " ");
        if (!username) {
            sendReply(mp.from, "Usage: /hi <username> <password>");
            return ProcessMessage::CONTINUE;
        }

        // Validate username length
        size_t usernameLen = strlen(username);
        if (usernameLen == 0 || usernameLen > LOBBS_MAX_USERNAME_LEN || !dal->isValidUsername(username)) {
            sendReply(
                mp.from,
                "Username not found or is invalid. Username must be 1-" LOBBS_XSTR(
                    LOBBS_MAX_USERNAME_LEN) " characters and contain only letters, numbers, and common special characters.");
            return ProcessMessage::CONTINUE;
        }

        // Get password (rest of the string)
        char *password = strtok(NULL, "");
        if (!password) {
            sendReply(mp.from, "Usage: /hi <username> <password>");
            return ProcessMessage::CONTINUE;
        }

        // Validate password is at least 5 characters long
        if (strlen(password) < 5 || strlen(password) > 50 || !dal->isValidPassword(password)) {
            sendReply(mp.from, "Password incorrect or invalid. Password must be between 5 and 50 characters long and contain "
                               "only letters, numbers, and common "
                               "special characters.");
            return ProcessMessage::CONTINUE;
        }

        LOG_DEBUG("Processing /hi command: username=%s from node=0x%0x", username, mp.from);

        // Try to load existing user
        meshtastic_LoBBSUser existingUser = meshtastic_LoBBSUser_init_zero;
        bool userExists = dal->loadUserByUsername(username, &existingUser);

        if (userExists) {
            // User exists - verify password
            if (dal->verifyPassword(&existingUser, password)) {
                LOG_DEBUG("User %s authenticated successfully from node 0x%0x", username, mp.from);

                // Log in user (create/update session)
                if (dal->loginUser(username, mp.from)) {
                    snprintf(replyBuffer, sizeof(replyBuffer), "Welcome back %s!", username);
                    sendReply(mp.from, replyBuffer);
                } else {
                    sendReply(mp.from, "Error creating session");
                }
            } else {
                LOG_WARN("Invalid password for user %s from node 0x%0x", username, mp.from);
                sendReply(mp.from, "Invalid password");
            }
        } else {
            // New user - create account
            LOG_DEBUG("Creating new user: %s for node 0x%0x", username, mp.from);

            if (dal->createUser(username, password, mp.from)) {
                snprintf(replyBuffer, sizeof(replyBuffer), "Welcome %s!", username);
                sendReply(mp.from, replyBuffer);
            } else {
                sendReply(mp.from, "Error creating account");
            }
        }

        return ProcessMessage::CONTINUE;
    }

    if (!isAuthenticated) {
        const char *helpMsg = LOBBS_HEADER "/hi <user> <pass> - Login or create account\n";
        LOG_DEBUG("Help message: %s", helpMsg);
        sendReply(mp.from, helpMsg);
        return ProcessMessage::CONTINUE;
    }

    /**
     * ================================
     * Authenticated commands
     * ================================
     */

    if (strcasecmp(cmdName, "/bye") == 0) {
        LOG_INFO("Processing /bye command from node=0x%0x", mp.from);

        dal->logoutUser(mp.from);
        sendReply(mp.from, "Goodbye!");

        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/users") == 0) {
        LOG_INFO("Processing /users command from node=0x%0x", mp.from);

        char *filterStr = strtok(NULL, " ");

        // Parse optional filter argument
        if (filterStr && !dal->isValidUsername(filterStr)) {
            sendReply(mp.from, "Filter must contain only letters, numbers, and common special characters.");
            return ProcessMessage::CONTINUE;
        }

        // Build filter lambda for username matching (captures filterStr)
        auto username_filter = [filterStr](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            return !filterStr || !filterStr[0] || stristr(u->username, filterStr) != nullptr;
        };

        // Execute synchronous select with filter and sort
        auto users = dal->getDb()->select("users", username_filter, compareUsernames);

        // Build and send response
        if (users.empty()) {
            std::string reply;
            LOG_DEBUG("No users match '%s'", filterStr);
            if (filterStr && filterStr[0]) {
                reply += "No users match '";
                reply += filterStr;
                reply += "'";
            } else {
                reply += "No users found";
            }
        } else {
            LOG_DEBUG("# users: %d", users.size());
            // Build user directory message
            std::string userListMsg = "User directory:\n";
            for (size_t i = 0; i < users.size(); i++) {
                const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)users[i];
                if (i > 0) {
                    userListMsg += ", ";
                }
                userListMsg += u->username;
            }
            LOG_DEBUG("User list message: %s", userListMsg.c_str());

            sendReply(mp.from, userListMsg.c_str());

            // Free allocated records
            for (auto *userPtr : users) {
                delete[] (uint8_t *)userPtr;
            }
        }

        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/mail") == 0) {
        LOG_INFO("Processing /mail command from node=0x%0x", mp.from);

        // Parse optional arguments
        char *arg1 = strtok(NULL, " ");

        // Determine action: list (default), list with offset, or read specific message
        uint32_t offset = 0;
        int readMessageId = -1;
        bool isReadCommand = false;

        if (arg1) {
            if (!isdigit(static_cast<unsigned char>(*arg1))) {
                sendReply(mp.from, "Invalid mail command");
                return ProcessMessage::CONTINUE;
            }

            char *argCopy = arg1;
            size_t len = strlen(argCopy);
            if (len > 0 && argCopy[len - 1] == '-') {
                // "/mail <n>-" format for listing from offset
                argCopy[len - 1] = '\0';
                offset = atoi(argCopy);
                if (offset > 0)
                    offset--; // Convert to 0-based
            } else {
                // "/mail <n>" format for reading message
                readMessageId = atoi(argCopy);
                isReadCommand = true;
            }
        }

        // Get mail for current user
        const uint32_t MAIL_PAGE_SIZE = 10;
        auto mailMessages = dal->getMailForUser(existingUser.uuid, offset, MAIL_PAGE_SIZE);

        if (isReadCommand && readMessageId > 0) {
            // Read specific message
            if (readMessageId > (int)mailMessages.size()) {
                sendReply(mp.from, "Invalid message number");
                freeMailMessages(mailMessages);
                return ProcessMessage::CONTINUE;
            }

            const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[readMessageId - 1];

            // Get sender username
            meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
            bool foundSender = loadUserByUuid(dal, mail->from_user_uuid, &sender);

            // Format time
            char timeStr[32];
            formatTimeAgo(mail->timestamp, timeStr, sizeof(timeStr));

            // Build message
            std::string reply;
            reply += "From: @";
            reply += foundSender ? sender.username : "unknown";
            reply += " (";
            reply += timeStr;
            reply += ")\n";
            reply += mail->message;

            sendReply(mp.from, reply.c_str());

            // Mark as read
            dal->markMailAsRead(mail->uuid);

            freeMailMessages(mailMessages);
            return ProcessMessage::CONTINUE;
        }

        // List mail (default action)
        if (mailMessages.empty()) {
            sendReply(mp.from, "No mail");
            return ProcessMessage::CONTINUE;
        }

        // Count unread messages
        int unreadCount = 0;
        for (auto *mailPtr : mailMessages) {
            const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailPtr;
            if (!mail->read) {
                unreadCount++;
            }
        }

        // Build mail list
        std::string mailList;
        if (unreadCount > 0) {
            char unreadStr[32];
            snprintf(unreadStr, sizeof(unreadStr), "(%d unread)\n", unreadCount);
            mailList += unreadStr;
        }

        for (size_t i = 0; i < mailMessages.size(); i++) {
            const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[i];

            // Get sender username
            meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
            bool foundSender = loadUserByUuid(dal, mail->from_user_uuid, &sender);

            // Format message entry
            char entryBuffer[256];
            char timeStr[32];
            char truncMsg[50];
            formatTimeAgo(mail->timestamp, timeStr, sizeof(timeStr));
            truncateMessage(mail->message, truncMsg, sizeof(truncMsg), 25);

            snprintf(entryBuffer, sizeof(entryBuffer), "[%d]%s @%s: %s (%s)\n", (int)(offset + i + 1), mail->read ? "" : "*",
                     foundSender ? sender.username : "unknown", truncMsg, timeStr);

            mailList += entryBuffer;
        }

        sendReply(mp.from, mailList.c_str());

        freeMailMessages(mailMessages);

        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/news") == 0) {
        LOG_INFO("Processing /news command from node=0x%0x", mp.from);

        // Parse optional arguments
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");

        uint32_t offset = 0;
        int readNewsId = -1;
        bool isReadCommand = false;
        bool isPostCommand = false;

        if (arg1) {
            if (strcasecmp(arg1, "r") == 0 && arg2) {
                readNewsId = atoi(arg2);
                isReadCommand = true;
            } else if (strcasecmp(arg1, "l") == 0 && arg2) {
                offset = atoi(arg2);
                if (offset > 0)
                    offset--; // Convert to 0-based
            } else if (isdigit((unsigned char)arg1[0])) {
                // "/news <n>" or "/news <n>-"
                char *argCopy = arg1;
                size_t len = strlen(argCopy);
                if (len > 0 && argCopy[len - 1] == '-') {
                    argCopy[len - 1] = '\0';
                    offset = atoi(argCopy);
                    if (offset > 0)
                        offset--; // Convert to 0-based
                } else {
                    readNewsId = atoi(argCopy);
                    isReadCommand = true;
                }
            } else {
                isPostCommand = true;
            }
        }

        if (isPostCommand) {
            // This is a news post - get the full message from original payload
            const char *msgStart = (const char *)mp.decoded.payload.bytes;
            size_t payloadSize = mp.decoded.payload.size;

            // Skip command prefix
            const char *payloadEnd = msgStart + payloadSize;
            while (msgStart < payloadEnd && *msgStart != ' ')
                msgStart++;
            while (msgStart < payloadEnd && *msgStart == ' ')
                msgStart++;

            if (msgStart >= payloadEnd || *msgStart == '\0') {
                sendReply(mp.from, "News message cannot be empty");
                return ProcessMessage::CONTINUE;
            }

            if (!isalpha((unsigned char)*msgStart)) {
                sendReply(mp.from, "News must start with a letter");
                return ProcessMessage::CONTINUE;
            }

            std::string newsMessage(msgStart, payloadEnd - msgStart);
            if (dal->postNews(existingUser.uuid, newsMessage.c_str())) {
                sendReply(mp.from, "News posted");
            } else {
                sendReply(mp.from, "Failed to post news");
            }
            return ProcessMessage::CONTINUE;
        }

        // Get news for current user
        const uint32_t NEWS_PAGE_SIZE = 10;
        auto newsItems = dal->getNewsForUser(existingUser.uuid, offset, NEWS_PAGE_SIZE);

        if (isReadCommand && readNewsId > 0) {
            if (readNewsId > (int)newsItems.size()) {
                sendReply(mp.from, "Invalid news number");
                freeNewsEntries(newsItems);
                return ProcessMessage::CONTINUE;
            }

            auto &entry = newsItems[readNewsId - 1];
            const meshtastic_LoBBSNews *news = entry.news;

            // Get author username
            meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
            bool foundAuthor = loadUserByUuid(dal, news->author_user_uuid, &author);

            char timeStr[32];
            formatTimeAgo(news->timestamp, timeStr, sizeof(timeStr));

            std::string reply;
            reply += "From: @";
            reply += foundAuthor ? author.username : "unknown";
            reply += " (";
            reply += timeStr;
            reply += ")\n";
            reply += news->message;

            sendReply(mp.from, reply.c_str());

            dal->markNewsAsRead(news->uuid, existingUser.uuid);

            freeNewsEntries(newsItems);
            return ProcessMessage::CONTINUE;
        }

        if (newsItems.empty()) {
            sendReply(mp.from, "No news");
            return ProcessMessage::CONTINUE;
        }

        int unreadCount = 0;
        for (const auto &entry : newsItems) {
            if (!entry.isRead)
                unreadCount++;
        }

        std::string newsList;
        if (unreadCount > 0) {
            char unreadStr[32];
            snprintf(unreadStr, sizeof(unreadStr), "(%d unread)\n", unreadCount);
            newsList += unreadStr;
        }

        for (size_t i = 0; i < newsItems.size(); i++) {
            const auto &entry = newsItems[i];
            const meshtastic_LoBBSNews *news = entry.news;

            meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
            bool foundAuthor = loadUserByUuid(dal, news->author_user_uuid, &author);

            char entryBuffer[256];
            char timeStr[32];
            char truncMsg[50];
            formatTimeAgo(news->timestamp, timeStr, sizeof(timeStr));
            truncateMessage(news->message, truncMsg, sizeof(truncMsg), 25);

            snprintf(entryBuffer, sizeof(entryBuffer), "[%d]%s @%s: %s (%s)\n", (int)(offset + i + 1), entry.isRead ? "" : "*",
                     foundAuthor ? author.username : "unknown", truncMsg, timeStr);

            newsList += entryBuffer;
        }

        sendReply(mp.from, newsList.c_str());

        freeNewsEntries(newsItems);

        return ProcessMessage::CONTINUE;
    }

    std::string helpMsg = LOBBS_HEADER "/bye - Logout\n"
                                       "/users [filter] - List users (optional filter)\n"
                                       "/mail [<n>|<n>-] - List/read mail\n"
                                       "@user <msg> - Send mail\n"
                                       "/news [<n>|<n>-] - List/read news\n"
                                       "/news <msg> - Post news";
    LOG_DEBUG("Help message: %s", helpMsg.c_str());
    sendReply(mp.from, helpMsg);
    return ProcessMessage::CONTINUE;
}

void LoBBSModule::sendReply(NodeNum to, const std::string &msg)
{
    meshtastic_MeshPacket *reply = allocDataPacket();
    static constexpr char truncMarker[] = "[...]";
    static constexpr size_t truncMarkerLen = sizeof(truncMarker) - 1;

    bool isTruncated = msg.size() > MAX_REPLY_BYTES;
    size_t payloadSize = isTruncated ? MAX_REPLY_BYTES : msg.size();
    reply->decoded.payload.size = payloadSize;

    if (isTruncated) {
        size_t copyLen = MAX_REPLY_BYTES > truncMarkerLen ? MAX_REPLY_BYTES - truncMarkerLen : 0;
        memcpy(reply->decoded.payload.bytes, msg.c_str(), copyLen);
        memcpy(reply->decoded.payload.bytes + copyLen, truncMarker, truncMarkerLen);
    } else {
        memcpy(reply->decoded.payload.bytes, msg.c_str(), payloadSize);
    }
    reply->to = to;
    reply->decoded.want_response = false;
    service->sendToMesh(reply);
}
