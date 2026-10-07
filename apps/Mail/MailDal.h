#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <lodb/LoDB.h>
#include <stdint.h>
#include <vector>

class MailDal
{
  public:
    explicit MailDal(LoDb &lodb);

    static uint64_t mailUuid(const LoScalar &m);
    static bool mailMessage(const LoScalar &m, char *buf, size_t bufCap);
    static bool mailRead(const LoScalar &m);
    static uint32_t mailTimestamp(const LoScalar &m);
    static uint64_t mailFromUuid(const LoScalar &m);
    static uint64_t mailToUuid(const LoScalar &m);

    LoDbError sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message);
    std::vector<LoScalar> getAllMailForUser(uint64_t userUuid);
    bool markMailAsRead(uint64_t mailUuid);
    bool markMailAsUnread(uint64_t mailUuid);
    uint16_t countUnreadMail(uint64_t userUuid);
    uint32_t countAllMail();
    bool deleteMailUuid(uint64_t mailUuid);
    bool deleteMailInboxIndex(uint64_t inboxOwnerUuid, uint32_t oneBasedIndex);

  private:
    LoDb &lodb_;
};

#endif
