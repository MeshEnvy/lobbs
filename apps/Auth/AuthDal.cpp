#include "AuthDal.h"
#include "AuthRecords.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "mesh/NodeDB.h"
#include "mesh/Throttle.h"
#include <SHA256.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include "LoBBSStackGuard.h"

static void normalizeUsername(const char *username, char *normalized)
{
    size_t len = strlen(username);
    if (len > LOBBS_MAX_USERNAME_LEN)
        len = LOBBS_MAX_USERNAME_LEN;
    for (size_t i = 0; i < len; i++) {
        normalized[i] = tolower(username[i]);
    }
    normalized[len] = '\0';
}

AuthDal::AuthDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("users");
}

uint64_t AuthDal::userUuid(const LoScalar &user)
{
    uint64_t v = 0;
    user.getUint64(LODB_F_ID, v);
    return v;
}

bool AuthDal::userUsername(const LoScalar &user, char *buf, size_t bufCap)
{
    std::string name;
    if (!user.getString(AuthUser::FIELD_USERNAME, name) || bufCap == 0)
        return false;
    strncpy(buf, name.c_str(), bufCap - 1);
    buf[bufCap - 1] = '\0';
    return true;
}

bool AuthDal::userIsSysop(const LoScalar &user)
{
    bool v = false;
    return user.getBool(AuthUser::FIELD_SYSOP, v) && v;
}

static lodb_uuid_t usernameToUuid(const char *username)
{
    char normalized[LOBBS_USERNAME_BUFFER_SIZE];
    normalizeUsername(username, normalized);
    return lodb_new_uuid(normalized, nodeDB->getNodeNum());
}

static const char *stristrLocal(const char *haystack, const char *needle)
{
    if (!needle || !*needle)
        return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n)
            return haystack;
    }
    return nullptr;
}

static int compareUsersByName(const LoScalar &a, const LoScalar &b)
{
    char na[LOBBS_USERNAME_BUFFER_SIZE];
    char nb[LOBBS_USERNAME_BUFFER_SIZE];
    AuthDal::userUsername(a, na, sizeof(na));
    AuthDal::userUsername(b, nb, sizeof(nb));
    return strcasecmp(na, nb);
}

bool AuthDal::loadUserByUuid(uint64_t uuid, LoScalar *user)
{
    return lodb_.get("users", uuid, *user) == LODB_OK;
}

std::vector<LoScalar> AuthDal::listUsers(const char *filterSubstr)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    return lodb_.select(
        "users",
        [filterSubstr, hasFilter](const LoScalar &rec) -> bool {
            if (!hasFilter)
                return true;
            char name[LOBBS_USERNAME_BUFFER_SIZE];
            AuthDal::userUsername(rec, name, sizeof(name));
            return stristrLocal(name, filterSubstr) != nullptr;
        },
        compareUsersByName);
}

bool AuthDal::isValidUsername(const char *username)
{
    if (!isalpha(username[0])) {
        return false;
    }
    for (size_t i = 1; username[i] != '\0'; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            return false;
        }
    }
    return true;
}

bool AuthDal::isValidPassword(const char *password)
{
    for (size_t i = 0; password[i] != '\0'; i++) {
        char c = password[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '!' && c != '@' && c != '#' && c != '$' && c != '%') {
            return false;
        }
    }
    return true;
}

void AuthDal::hashPassword(const char *password, uint8_t *hash)
{
    SHA256 sha256;
    sha256.reset();
    sha256.update(password, strlen(password));
    sha256.finalize(hash, 32);
}

bool AuthDal::loadUserByUsername(const char *username, LoScalar *user)
{
    lodb_uuid_t userUuid = usernameToUuid(username);
    LoDbError err = lodb_.get("users", userUuid, *user);
    if (err == LODB_OK) {
        LOG_DEBUG("Loaded user by username: %s", username);
        return true;
    }
    LOG_DEBUG("User not found: %s", username);
    return false;
}

void AuthDal::applySessionConfig(const SessionConfig &cfg)
{
    if (cfg.maxSessions < sessions_.size()) {
        const uint32_t now = millis();
        std::sort(sessions_.begin(), sessions_.end(), [now](const Session &a, const Session &b) {
            if (a.used != b.used)
                return a.used;
            return (now - a.lastActiveMs) < (now - b.lastActiveMs);
        });
    }
    sessions_.resize(cfg.maxSessions, Session{});
    sessionCfg_ = cfg;
}

bool AuthDal::sessionExpired(const Session &s) const
{
    return !Throttle::isWithinTimespanMs(s.lastActiveMs, sessionCfg_.idleSeconds * 1000u);
}

AuthDal::Session *AuthDal::findSession(uint32_t nodeId)
{
    for (auto &s : sessions_) {
        if (!s.used || s.nodeId != nodeId)
            continue;
        if (sessionExpired(s)) {
            LOG_INFO("Session for node 0x%08x expired", nodeId);
            s = Session{};
            return nullptr;
        }
        return &s;
    }
    return nullptr;
}

void AuthDal::clearSessions()
{
    sessions_.clear();
}

bool AuthDal::loadUserByNodeId(uint32_t nodeId, LoScalar *user, uint32_t *sessionNodeIdOut, uint64_t *authUserUuidOut,
                               std::string *cwdOut)
{
    Session *s = findSession(nodeId);
    if (!s) {
        LOG_DEBUG("No session found for node 0x%08x", nodeId);
        return false;
    }

    if (lodb_.get("users", s->userUuid, *user) != LODB_OK) {
        LOG_WARN("Session 0x%08x references missing user, removing session", nodeId);
        *s = Session{};
        return false;
    }

    s->lastActiveMs = millis();
    LOG_DEBUG("Loaded user by node ID: 0x%08x -> UUID: " LODB_UUID_FMT, nodeId, LODB_UUID_ARGS(s->userUuid));
    if (sessionNodeIdOut)
        *sessionNodeIdOut = nodeId;
    if (authUserUuidOut)
        *authUserUuidOut = s->userUuid;
    if (cwdOut)
        cwdOut->assign(s->cwd);
    return true;
}

bool AuthDal::setSessionCwd(uint32_t nodeId, const char *cwd)
{
    Session *s = findSession(nodeId);
    if (!s)
        return false;
    strncpy(s->cwd, cwd ? cwd : "", sizeof(s->cwd) - 1);
    s->cwd[sizeof(s->cwd) - 1] = '\0';
    return true;
}

LoDbError AuthDal::createUser(const char *username, const char *password, uint32_t nodeId, bool asSysop)
{
    lodb_uuid_t userUuid = usernameToUuid(username);

    LoScalar user;
    user.setString(AuthUser::FIELD_USERNAME, username);
    uint8_t hash[32];
    hashPassword(password, hash);
    user.setBytesHex(AuthUser::FIELD_PASSWORD, hash, 32);
    user.setBool(AuthUser::FIELD_SYSOP, asSysop);
    LoDbError err = lodb_.insert("users", userUuid, user);
    if (err != LODB_OK) {
        LOG_ERROR("Failed to create user: %s", username);
        return err;
    }

    LOG_INFO("Created user: %s (sysop: %s)", username, asSysop ? "yes" : "no");
    return loginUser(username, nodeId) ? LODB_OK : LODB_ERR_INVALID;
}

bool AuthDal::verifyPassword(const LoScalar *user, const char *password)
{
    uint8_t stored[32];
    size_t n = 0;
    if (!user->getBytesHex(AuthUser::FIELD_PASSWORD, stored, sizeof(stored), n) || n != 32)
        return false;
    uint8_t providedHash[32];
    hashPassword(password, providedHash);
    return memcmp(stored, providedHash, 32) == 0;
}

bool AuthDal::loginUser(const char *username, uint32_t nodeId)
{
    Session *s = findSession(nodeId);
    if (!s) {
        const uint32_t now = millis();
        uint32_t oldestAge = 0;
        for (auto &c : sessions_) {
            if (!c.used || sessionExpired(c)) {
                s = &c;
                break;
            }
            if (!s || now - c.lastActiveMs > oldestAge) {
                s = &c;
                oldestAge = now - c.lastActiveMs;
            }
        }
        if (!s)
            return false;
        if (s->used && !sessionExpired(*s))
            LOG_INFO("Session table full, evicting node 0x%08x", s->nodeId);
    }

    *s = Session{};
    s->used = true;
    s->nodeId = nodeId;
    s->userUuid = usernameToUuid(username);
    s->lastActiveMs = millis();
    LOG_INFO("Created session for user %s on node 0x%08x", username, nodeId);
    return true;
}

bool AuthDal::logoutUser(uint32_t nodeId)
{
    Session *s = findSession(nodeId);
    if (s) {
        *s = Session{};
        LOG_INFO("Logged out node 0x%08x", nodeId);
        return true;
    }
    LOG_WARN("No session found to log out for node 0x%08x", nodeId);
    return false;
}

uint64_t AuthDal::getUserUuidByUsername(const char *username)
{
    uint64_t userUuid = usernameToUuid(username);
    LoScalar user;
    LoDbError err = lodb_.get("users", userUuid, user);
    if (err == LODB_OK)
        return userUuid;
    return 0;
}

LoDbError AuthDal::setUserSysopByUsername(const char *username, bool isSysop)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return LODB_ERR_NOT_FOUND;
    uint64_t uuid = userUuid(user);
    user.setBool(AuthUser::FIELD_SYSOP, isSysop);
    user.removeField(LODB_F_CREATED);
    user.removeField(LODB_F_UPDATED);
    return lodb_.update("users", uuid, user);
}

LoDbError AuthDal::setPasswordByUsername(const char *username, const char *password)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return LODB_ERR_NOT_FOUND;
    uint64_t uuid = userUuid(user);
    uint8_t hash[32];
    hashPassword(password, hash);
    user.setBytesHex(AuthUser::FIELD_PASSWORD, hash, 32);
    user.removeField(LODB_F_CREATED);
    user.removeField(LODB_F_UPDATED);
    return lodb_.update("users", uuid, user);
}

uint32_t AuthDal::countSysopUsers()
{
    return (uint32_t)lodb_.count("users", [](const LoScalar &rec) -> bool { return AuthDal::userIsSysop(rec); });
}

uint32_t AuthDal::countAllUsers()
{
    int n = lodb_.count("users");
    return n < 0 ? 0 : (uint32_t)n;
}

bool AuthDal::kickUserByUsername(const char *username)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return false;
    uint64_t userUuidVal = userUuid(user);
    for (auto &s : sessions_) {
        if (s.used && s.userUuid == userUuidVal)
            s = Session{};
    }
    return true;
}

