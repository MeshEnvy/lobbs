#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnDal.h"
#include "../AppUtil.h"
#include "YarnRecords.h"
#include <cstdio>
#include <cstring>
#include <string>

#include "LoBBSStackGuard.h"

YarnDal::YarnDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("yarn_current");
    lodb_.registerTable("yarn_seen");
    lodb_.registerTable("yarn_quota");
}

static lodb_uuid_t yarnCurrentRecordUuid()
{
    return lodb_new_uuid("lobbs_yarn_current_v1", 0);
}

static lodb_uuid_t yarnSeenRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    return lodb_new_uuid(hex, 0);
}

void YarnDal::loadCurrent(CurrentState &out)
{
    out.text[0] = '\0';
    out.total_words_appended = 0;
    LoScalar rec;
    if (lodb_.get("yarn_current", yarnCurrentRecordUuid(), rec) != LODB_OK)
        return;
    std::string text;
    if (rec.getString(LODB_F_DESCRIPTION, text))
        strncpy(out.text, text.c_str(), LOBBS_YARN_BODY_MAX);
    out.text[LOBBS_YARN_BODY_MAX] = '\0';
    rec.getUint32(YarnCurrentField::FIELD_TOTAL_WORDS, out.total_words_appended);
}

LoDbError YarnDal::saveCurrent(CurrentState &cur)
{
    lodb_uuid_t id = yarnCurrentRecordUuid();
    LoScalar rec;
    rec.setString(LODB_F_DESCRIPTION, cur.text);
    rec.setUint32(YarnCurrentField::FIELD_TOTAL_WORDS, cur.total_words_appended);
    return lodb_.upsert("yarn_current", id, rec);
}

void YarnDal::trimTailToMax(char *text)
{
    if (!text)
        return;
    while (strlen(text) > LOBBS_YARN_BODY_MAX) {
        char *sp = strchr(text, ' ');
        if (!sp) {
            text[LOBBS_YARN_BODY_MAX] = '\0';
            return;
        }
        memmove(text, sp + 1, strlen(sp + 1) + 1);
    }
}

bool YarnDal::isValidWordToken(const char *word)
{
    if (!word || !word[0])
        return false;
    for (const char *p = word; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x21 || c > 0x7e)
            return false;
    }
    return true;
}

bool YarnDal::formatYarnView(char *out, size_t outCap)
{
    if (!out || outCap == 0)
        return false;
    CurrentState cur = {};
    loadCurrent(cur);
    if (!cur.text[0]) {
        snprintf(out, outCap, "Yarn empty.");
        return true;
    }
    snprintf(out, outCap, "%s", cur.text);
    return true;
}

uint32_t YarnDal::totalWordsAppended()
{
    CurrentState cur = {};
    loadCurrent(cur);
    return cur.total_words_appended;
}

uint32_t YarnDal::newWordsForUser(uint64_t userUuid)
{
    CurrentState cur = {};
    loadCurrent(cur);
    LoScalar seen;
    if (lodb_.get("yarn_seen", yarnSeenRecordUuid(userUuid), seen) != LODB_OK)
        return cur.total_words_appended;
    uint32_t lastSeen = 0;
    seen.getUint32(YarnSeenField::FIELD_LAST_TOTAL_WORDS, lastSeen);
    if (cur.total_words_appended <= lastSeen)
        return 0;
    return cur.total_words_appended - lastSeen;
}

bool YarnDal::markYarnSeen(uint64_t userUuid)
{
    CurrentState cur = {};
    loadCurrent(cur);
    LoScalar seen;
    seen.setUint64(YarnSeenField::FIELD_USER_UUID, userUuid);
    seen.setUint32(YarnSeenField::FIELD_LAST_TOTAL_WORDS, cur.total_words_appended);
    lodb_uuid_t id = yarnSeenRecordUuid(userUuid);
    return lodb_.upsert("yarn_seen", id, seen) == LODB_OK;
}

const char *YarnDal::appendWords(uint64_t userUuid, bool isSysop, const char *const *words, int wordCount, uint32_t periodSeconds,
                                 uint32_t maxWords, uint32_t maxChars)
{
    if (!words || wordCount <= 0)
        return "No words.";
    for (int i = 0; i < wordCount; i++) {
        if (!isValidWordToken(words[i]))
            return "Bad word.";
        if (strlen(words[i]) > LOBBS_YARN_BODY_MAX)
            return "Word too long.";
    }
    CurrentState cur = {};
    loadCurrent(cur);

    uint32_t charCost = 0;
    bool needSpace = cur.text[0] != '\0';
    for (int i = 0; i < wordCount; i++) {
        if (needSpace)
            charCost += 1;
        charCost += (uint32_t)strlen(words[i]);
        needSpace = true;
    }

    uint32_t add[2] = {(uint32_t)wordCount, charCost};
    if (!isSysop) {
        if (add[0] > maxWords || add[1] > maxChars)
            return "Too many words.";
        uint32_t used[2];
        lobbsQuotaUsed(lodb_, "yarn_quota", userUuid, periodSeconds, used, 2);
        if (used[0] + add[0] > maxWords)
            return "Quota full.";
        if (used[1] + add[1] > maxChars)
            return "Too many chars.";
    }

    static constexpr size_t kCandidateCap = LOBBS_YARN_BODY_MAX + 64;
    std::string candidate(cur.text);
    for (int i = 0; i < wordCount; i++) {
        const char *w = words[i];
        size_t wlen = strlen(w);
        if (!candidate.empty()) {
            if (candidate.size() + 1 >= kCandidateCap)
                break;
            candidate += ' ';
        }
        if (candidate.size() + wlen >= kCandidateCap)
            break;
        candidate += w;
    }
    char *text = &candidate[0];
    trimTailToMax(text);
    if (strlen(text) > LOBBS_YARN_BODY_MAX)
        return "Too many chars.";
    strncpy(cur.text, text, LOBBS_YARN_BODY_MAX);
    cur.text[LOBBS_YARN_BODY_MAX] = '\0';
    cur.total_words_appended += (uint32_t)wordCount;
    LoDbError err = saveCurrent(cur);
    if (err != LODB_OK)
        return lobbsDbErrorText(err, "Save failed.");
    if (!isSysop && (err = lobbsQuotaAdd(lodb_, "yarn_quota", userUuid, periodSeconds, add, 2)) != LODB_OK)
        return lobbsDbErrorText(err, "Quota save.");
    return nullptr;
}

#endif
