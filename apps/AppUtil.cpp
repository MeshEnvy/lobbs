#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AppUtil.h"
#include "../LoBBSCommandCtx.h"
#include "../LoBBSCommandRegistry.h"
#include "../LoBBSConfig.h"
#include "../LoBBSModule.h"
#include "../LoBBSResponse.h"
#include "Auth/AuthDal.h"
#include "gps/RTC.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>

#include "LoBBSStackGuard.h"

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap)
{
    size_t n = 0;
    if (dst && dstCap > 0) {
        if (src) {
            while (n + 1 < dstCap && n < srcCap && src[n] != '\0')
                n++;
            memcpy(dst, src, n);
        }
        dst[n] = '\0';
    }
}

void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize)
{
    uint32_t now = getTime();
    if (now < timestamp) {
        snprintf(buffer, bufferSize, "now");
        return;
    }
    uint32_t diff = now - timestamp;
    if (diff < 60)
        snprintf(buffer, bufferSize, "%ds ago", diff);
    else if (diff < 3600)
        snprintf(buffer, bufferSize, "%dm ago", diff / 60);
    else if (diff < 86400)
        snprintf(buffer, bufferSize, "%dh ago", diff / 3600);
    else
        snprintf(buffer, bufferSize, "%dd ago", diff / 86400);
}

void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen)
{
    if (!message)
        message = "";
    size_t msgLen = strnlen(message, LOBBS_MESSAGE_BODY_MAX);
    if (msgLen <= maxLen) {
        snprintf(buffer, bufferSize, "%.*s", (int)msgLen, message);
    } else {
        size_t copyLen = maxLen < bufferSize - 4 ? maxLen : bufferSize - 4;
        snprintf(buffer, bufferSize, "%.*s...", (int)copyLen, message);
    }
}

bool lobbsAppLoadUser(LoBBSModule *mod, uint64_t uuid, LoScalar *outUser)
{
    if (!mod || !outUser || uuid == 0)
        return false;
    return mod->auth().dal().loadUserByUuid(uuid, outUser);
}

void lobbsAppUsernameForUuid(LoBBSModule *mod, uint64_t uuid, char *buf, size_t bufCap)
{
    if (!buf || bufCap == 0)
        return;
    buf[0] = '\0';
    LoScalar user;
    if (!lobbsAppLoadUser(mod, uuid, &user) || !AuthDal::userUsername(user, buf, bufCap) || !buf[0])
        lobbsAppCopyCapped(buf, bufCap, "unknown", 7);
}

uint64_t lobbsAppUuidForUsername(LoBBSModule *mod, const char *username)
{
    if (!mod || !username || !username[0])
        return 0;
    return mod->auth().dal().getUserUuidByUsername(username);
}

bool lobbsAppResolveUsername(LoBBSCommandCtx &ctx, const char *username, uint64_t &uuidOut)
{
    uuidOut = lobbsAppUuidForUsername(ctx.mod, username);
    if (uuidOut != 0)
        return true;
    char msg[64];
    snprintf(msg, sizeof(msg), "User '%s' not found.", username ? username : "");
    LoBBSResponse resp;
    lobbsResponseSetError(resp, msg);
    lobbsCommandReplyResponse(ctx, resp);
    return false;
}

static uint32_t quotaLoad(LoDb &db, const char *table, uint64_t userUuid, uint32_t periodSec, uint32_t *used, size_t n)
{
    memset(used, 0, n * sizeof(uint32_t));
    LoScalar rec;
    uint32_t start = 0;
    if (db.get(table, userUuid, rec) != LODB_OK || !rec.getUint32(LoBBSQuotaField::FIELD_CYCLE_START, start) || start == 0)
        return 0;
    if (getTime() >= start + periodSec)
        return 0;
    for (size_t i = 0; i < n; i++)
        rec.getUint32(LoBBSQuotaField::FIELD_USED + (uint32_t)i, used[i]);
    return start;
}

void lobbsQuotaUsed(LoDb &db, const char *table, uint64_t userUuid, uint32_t periodSec, uint32_t *used, size_t n)
{
    quotaLoad(db, table, userUuid, periodSec, used, n);
}

uint32_t lobbsCrc32Update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; data && i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xedb88320 & (~((crc & 1) - 1)));
    }
    return crc;
}

uint32_t lobbsCrc32(const uint8_t *data, size_t len)
{
    return ~lobbsCrc32Update(0xffffffff, data, len);
}

const char *lobbsDbErrorText(LoDbError err, const char *otherwise)
{
    return err == LODB_ERR_FULL ? "Disk full." : otherwise;
}

LoDbError lobbsQuotaAdd(LoDb &db, const char *table, uint64_t userUuid, uint32_t periodSec, const uint32_t *add, size_t n)
{
    if (n > LOBBS_QUOTA_MAX_COUNTERS)
        return LODB_ERR_INVALID;
    uint32_t used[LOBBS_QUOTA_MAX_COUNTERS];
    uint32_t start = quotaLoad(db, table, userUuid, periodSec, used, n);
    if (start == 0)
        start = getTime();
    LoScalar rec;
    rec.setUint64(LoBBSQuotaField::FIELD_USER_UUID, userUuid);
    rec.setUint32(LoBBSQuotaField::FIELD_CYCLE_START, start);
    for (size_t i = 0; i < n; i++)
        rec.setUint32(LoBBSQuotaField::FIELD_USED + (uint32_t)i, used[i] + add[i]);
    return db.upsert(table, userUuid, rec);
}

#endif
