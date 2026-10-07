#pragma once
#include <lodb/LoDB.h>
#include <stdint.h>
#include <vector>

struct LoBBSNewsEntry {
    LoScalar news;
    bool isRead;
};

class NewsDal
{
  public:
    explicit NewsDal(LoDb &lodb);

    static uint64_t newsUuid(const LoScalar &n);
    static bool newsMessage(const LoScalar &n, char *buf, size_t bufCap);
    static uint64_t newsAuthorUuid(const LoScalar &n);
    static uint32_t newsTimestamp(const LoScalar &n);

    LoDbError postNews(uint64_t authorUserUuid, const char *message);
    std::vector<LoBBSNewsEntry> getAllNewsForUser(uint64_t userUuid);
    bool markNewsAsRead(uint64_t newsUuid, uint64_t userUuid);
    bool markNewsAsUnread(uint64_t newsUuid, uint64_t userUuid);
    uint16_t countUnreadNews(uint64_t userUuid);
    uint32_t countAllNews();
    bool deleteNewsUuid(uint64_t newsUuid);

  private:
    bool isNewsReadByUser(uint64_t newsUuid, uint64_t userUuid);
    LoDb &lodb_;
};

