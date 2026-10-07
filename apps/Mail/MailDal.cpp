#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailDal.h"
#include "../../LoBBSConfig.h"
#include "MailRecords.h"
#include "configuration.h"
#include "gps/RTC.h"
#include <cstring>

#include "LoBBSStackGuard.h"

MailDal::MailDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("mail");
}

uint64_t MailDal::mailUuid(const LoScalar &m)
{
    uint64_t v = 0;
    m.getUint64(LODB_F_ID, v);
    return v;
}

bool MailDal::mailMessage(const LoScalar &m, char *buf, size_t bufCap)
{
    std::string s;
    if (!m.getString(LODB_F_DESCRIPTION, s) || bufCap == 0)
        return false;
    strncpy(buf, s.c_str(), bufCap - 1);
    buf[bufCap - 1] = '\0';
    return true;
}

bool MailDal::mailRead(const LoScalar &m)
{
    bool v = false;
    m.getBool(MailField::FIELD_READ, v);
    return v;
}

uint32_t MailDal::mailTimestamp(const LoScalar &m)
{
    uint32_t v = 0;
    m.getUint32(LODB_F_CREATED, v);
    return v;
}

uint64_t MailDal::mailFromUuid(const LoScalar &m)
{
    uint64_t v = 0;
    m.getUint64(MailField::FIELD_FROM, v);
    return v;
}

uint64_t MailDal::mailToUuid(const LoScalar &m)
{
    uint64_t v = 0;
    m.getUint64(MailField::FIELD_TO, v);
    return v;
}

static int compareMailByTimestamp(const LoScalar &a, const LoScalar &b)
{
    uint32_t t1 = MailDal::mailTimestamp(a);
    uint32_t t2 = MailDal::mailTimestamp(b);
    if (t2 > t1)
        return 1;
    if (t2 < t1)
        return -1;
    return 0;
}

static constexpr uint32_t LOBBS_MAX_LIST_ROWS = 256;

LoDbError MailDal::sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message)
{
    lodb_uuid_t mailUuidVal = lodb_new_uuid(nullptr, toUserUuid ^ fromUserUuid);

    LoScalar mail;
    char msgBuf[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    strncpy(msgBuf, message, LOBBS_MESSAGE_BODY_MAX);
    msgBuf[LOBBS_MESSAGE_BODY_MAX] = '\0';
    mail.setString(LODB_F_DESCRIPTION, msgBuf);
    mail.setBool(MailField::FIELD_READ, false);
    mail.setUint64(MailField::FIELD_FROM, fromUserUuid);
    mail.setUint64(MailField::FIELD_TO, toUserUuid);

    LoDbError err = lodb_.insert("mail", mailUuidVal, mail);
    if (err != LODB_OK)
        LOG_ERROR("Failed to send mail");
    return err;
}

std::vector<LoScalar> MailDal::getAllMailForUser(uint64_t userUuid)
{
    auto mail_filter = [userUuid](const LoScalar &rec) -> bool { return MailDal::mailToUuid(rec) == userUuid; };
    return lodb_.select("mail", mail_filter, compareMailByTimestamp, LOBBS_MAX_LIST_ROWS);
}

bool MailDal::markMailAsRead(uint64_t mailUuidVal)
{
    LoScalar mail;
    LoDbError err = lodb_.get("mail", mailUuidVal, mail);
    if (err != LODB_OK)
        return false;

    mail.setBool(MailField::FIELD_READ, true);
    mail.removeField(LODB_F_CREATED);
    mail.removeField(LODB_F_UPDATED);
    return lodb_.update("mail", mailUuidVal, mail) == LODB_OK;
}

bool MailDal::markMailAsUnread(uint64_t mailUuidVal)
{
    LoScalar mail;
    LoDbError err = lodb_.get("mail", mailUuidVal, mail);
    if (err != LODB_OK)
        return false;

    mail.setBool(MailField::FIELD_READ, false);
    mail.removeField(LODB_F_CREATED);
    mail.removeField(LODB_F_UPDATED);
    return lodb_.update("mail", mailUuidVal, mail) == LODB_OK;
}

uint32_t MailDal::countAllMail()
{
    int n = lodb_.count("mail");
    return n < 0 ? 0 : (uint32_t)n;
}

uint16_t MailDal::countUnreadMail(uint64_t userUuid)
{
    auto mail_filter = [userUuid](const LoScalar &rec) -> bool {
        return MailDal::mailToUuid(rec) == userUuid && !MailDal::mailRead(rec);
    };
    auto rows = lodb_.select("mail", mail_filter, LoDbComparator());
    uint16_t count = (uint16_t)(rows.size() > 0xffff ? 0xffff : rows.size());
    return count;
}

bool MailDal::deleteMailUuid(uint64_t mailUuidVal)
{
    return lodb_.deleteRecord("mail", mailUuidVal) == LODB_OK;
}

bool MailDal::deleteMailInboxIndex(uint64_t inboxOwnerUuid, uint32_t oneBasedIndex)
{
    if (oneBasedIndex == 0)
        return false;
    auto mail = getAllMailForUser(inboxOwnerUuid);
    if (oneBasedIndex > mail.size())
        return false;
    uint64_t uuid = mailUuid(mail[oneBasedIndex - 1]);
    return deleteMailUuid(uuid);
}

#endif
