#include "NewsDal.h"
#include "../../core/LoBBSConfig.h"
#include "NewsRecords.h"
#include "platforms/LoPlatform.h"
#include <algorithm>
#include <cstring>

#include "core/LoBBSStackGuard.h"

NewsDal::NewsDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("news");
    lodb_.registerTable("news_reads");
}

uint64_t NewsDal::newsUuid(const LoScalar &n)
{
    uint64_t v = 0;
    n.getUint64(LODB_F_ID, v);
    return v;
}

bool NewsDal::newsMessage(const LoScalar &n, char *buf, size_t bufCap)
{
    std::string s;
    if (!n.getString(LODB_F_DESCRIPTION, s) || bufCap == 0)
        return false;
    strncpy(buf, s.c_str(), bufCap - 1);
    buf[bufCap - 1] = '\0';
    return true;
}

uint64_t NewsDal::newsAuthorUuid(const LoScalar &n)
{
    uint64_t v = 0;
    n.getUint64(NewsField::FIELD_AUTHOR, v);
    return v;
}

uint32_t NewsDal::newsTimestamp(const LoScalar &n)
{
    uint32_t v = 0;
    n.getUint32(LODB_F_CREATED, v);
    return v;
}

static void buildNewsReadKey(uint64_t newsUuid, uint64_t userUuid, char *out, size_t outSize)
{
    char newsHex[17];
    char userHex[17];
    lodb_uuid_to_hex(newsUuid, newsHex);
    lodb_uuid_to_hex(userUuid, userHex);
    snprintf(out, outSize, "%s:%s", newsHex, userHex);
}

static constexpr uint32_t LOBBS_MAX_LIST_ROWS = 256;

LoDbError NewsDal::postNews(uint64_t authorUserUuid, const char *message)
{
    lodb_uuid_t newsUuidVal = lodb_new_uuid(nullptr, authorUserUuid ^ (uint64_t)lobbsPlatformUnixNow());

    LoScalar news;
    char msgBuf[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    strncpy(msgBuf, message, LOBBS_MESSAGE_BODY_MAX);
    msgBuf[LOBBS_MESSAGE_BODY_MAX] = '\0';
    news.setString(LODB_F_DESCRIPTION, msgBuf);
    news.setUint64(NewsField::FIELD_AUTHOR, authorUserUuid);

    return lodb_.insert("news", newsUuidVal, news);
}

bool NewsDal::isNewsReadByUser(uint64_t newsUuidVal, uint64_t userUuid)
{
    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    LoScalar readRecord;
    LoDbError err = lodb_.get("news_reads", readUuid, readRecord);
    return err == LODB_OK;
}

bool NewsDal::markNewsAsRead(uint64_t newsUuidVal, uint64_t userUuid)
{
    if (isNewsReadByUser(newsUuidVal, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    LoScalar readRecord;
    readRecord.setUint64(NewsReadField::FIELD_NEWS_UUID, newsUuidVal);
    readRecord.setUint64(NewsReadField::FIELD_USER_UUID, userUuid);

    return lodb_.insert("news_reads", readUuid, readRecord) == LODB_OK;
}

bool NewsDal::markNewsAsUnread(uint64_t newsUuidVal, uint64_t userUuid)
{
    if (!isNewsReadByUser(newsUuidVal, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);
    return lodb_.deleteRecord("news_reads", readUuid) == LODB_OK;
}

std::vector<LoBBSNewsEntry> NewsDal::getAllNewsForUser(uint64_t userUuid)
{
    auto allNews = lodb_.select("news", LoDbFilter(), LoDbComparator());

    std::vector<LoBBSNewsEntry> newsWithStatus;
    newsWithStatus.reserve(allNews.size());
    for (const auto &newsRec : allNews) {
        uint64_t uuid = newsUuid(newsRec);
        newsWithStatus.push_back({newsRec, isNewsReadByUser(uuid, userUuid)});
    }

    std::sort(newsWithStatus.begin(), newsWithStatus.end(), [](const LoBBSNewsEntry &a, const LoBBSNewsEntry &b) {
        if (a.isRead != b.isRead)
            return !a.isRead;
        return newsTimestamp(a.news) > newsTimestamp(b.news);
    });

    if (newsWithStatus.size() > LOBBS_MAX_LIST_ROWS)
        newsWithStatus.resize(LOBBS_MAX_LIST_ROWS);
    return newsWithStatus;
}

uint32_t NewsDal::countAllNews()
{
    int n = lodb_.count("news");
    return n < 0 ? 0 : (uint32_t)n;
}

uint16_t NewsDal::countUnreadNews(uint64_t userUuid)
{
    auto allNews = lodb_.select("news", LoDbFilter(), LoDbComparator());
    uint16_t count = 0;
    for (const auto &newsRec : allNews) {
        if (!isNewsReadByUser(newsUuid(newsRec), userUuid)) {
            if (count < 0xffff)
                count++;
        }
    }
    return count;
}

bool NewsDal::deleteNewsUuid(uint64_t newsUuidVal)
{
    return lodb_.deleteRecord("news", newsUuidVal) == LODB_OK;
}

