#pragma once
#include <lodb/LoDB.h>
#include <loscalar/LoScalar.h>
#include <stddef.h>
#include <stdint.h>

class LoBBSKernel;
struct LoBBSCommandCtx;

/** Reply text for a failed write: "Disk full." for LODB_ERR_FULL, else `otherwise`. */
const char *lobbsDbErrorText(LoDbError err, const char *otherwise = "Failed.");

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap);
void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize);
void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen);

bool lobbsAppLoadUser(LoBBSKernel *kernel, uint64_t uuid, LoScalar *outUser);
/** Username for display; uses "unknown" when missing. */
void lobbsAppUsernameForUuid(LoBBSKernel *kernel, uint64_t uuid, char *buf, size_t bufCap);
uint64_t lobbsAppUuidForUsername(LoBBSKernel *kernel, const char *username);
/** Sets uuidOut and returns true, or replies and returns false. */
bool lobbsAppResolveUsername(LoBBSCommandCtx &ctx, const char *username, uint64_t &uuidOut);

/** Per-user quota record: user uuid, cycle start, then one counter per field from 2. */
namespace LoBBSQuotaField
{
inline constexpr uint32_t FIELD_USER_UUID = 0;
inline constexpr uint32_t FIELD_CYCLE_START = 1;
inline constexpr uint32_t FIELD_USED = 2;
} // namespace LoBBSQuotaField
static constexpr size_t LOBBS_QUOTA_MAX_COUNTERS = 4;

/** Fills `used` with this cycle's counters; zeros when missing or the cycle has expired. */
void lobbsQuotaUsed(LoDb &db, const char *table, uint64_t userUuid, uint32_t periodSec, uint32_t *used, size_t n);
/** Adds `add` to this cycle's counters, starting a new cycle when the last one expired. */
LoDbError lobbsQuotaAdd(LoDb &db, const char *table, uint64_t userUuid, uint32_t periodSec, const uint32_t *add, size_t n);

/** IEEE CRC32; pass 0xffffffff before the first byte, invert the return value for the digest. */
uint32_t lobbsCrc32Update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t lobbsCrc32(const uint8_t *data, size_t len);
