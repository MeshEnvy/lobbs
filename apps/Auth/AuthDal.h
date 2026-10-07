#pragma once
#include "../../LoBBSConfig.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <string>
#include <vector>

static constexpr uint32_t LOBBS_SESSION_DEFAULT_MAX = 16;
static constexpr uint32_t LOBBS_SESSION_DEFAULT_IDLE_SEC = 86400;

class AuthDal
{
  public:
    struct SessionConfig {
        uint32_t maxSessions;
        uint32_t idleSeconds;
    };

    AuthDal(LoDb &lodb);

    static uint64_t userUuid(const LoScalar &user);
    static bool userUsername(const LoScalar &user, char *buf, size_t bufCap);
    static bool userIsSysop(const LoScalar &user);

    bool isValidUsername(const char *username);
    bool isValidPassword(const char *password);
    bool loadUserByUsername(const char *username, LoScalar *user);
    bool loadUserByUuid(uint64_t uuid, LoScalar *user);
    bool loadUserByNodeId(uint32_t nodeId, LoScalar *user, uint32_t *sessionNodeIdOut = nullptr,
                          uint64_t *authUserUuidOut = nullptr, std::string *cwdOut = nullptr);
    bool setSessionCwd(uint32_t nodeId, const char *cwd);
    std::vector<LoScalar> listUsers(const char *filterSubstr);
    LoDbError createUser(const char *username, const char *password, uint32_t nodeId, bool asSysop = false);
    bool verifyPassword(const LoScalar *user, const char *password);
    bool loginUser(const char *username, uint32_t nodeId);
    bool logoutUser(uint32_t nodeId);
    uint64_t getUserUuidByUsername(const char *username);

    LoDbError setUserSysopByUsername(const char *username, bool isSysop);
    LoDbError setPasswordByUsername(const char *username, const char *password);
    uint32_t countSysopUsers();
    uint32_t countAllUsers();
    bool kickUserByUsername(const char *username);
    void clearSessions();
    void applySessionConfig(const SessionConfig &cfg);

  private:
    struct Session {
        bool used;
        uint32_t nodeId;
        uint32_t lastActiveMs;
        uint64_t userUuid;
        char cwd[LOBBS_CWD_BUFFER_SIZE];
    };

    static void hashPassword(const char *password, uint8_t *hash);
    bool sessionExpired(const Session &s) const;
    Session *findSession(uint32_t nodeId);
    LoDb &lodb_;
    std::vector<Session> sessions_;
    SessionConfig sessionCfg_ = {LOBBS_SESSION_DEFAULT_MAX, LOBBS_SESSION_DEFAULT_IDLE_SEC};
};

