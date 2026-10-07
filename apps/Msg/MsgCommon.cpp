#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MsgCommon.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSResponse.h"
#include "../Mail/MailRecords.h"
#include "../News/NewsRecords.h"
#include <lodb/LoDB.h>
#include <string>

#include "LoBBSStackGuard.h"

bool lobbsMsgShiftIndex(LoBBSCommandCtx &ctx, size_t count, const char *usage, const char *badNumMsg, uint32_t &idxOut)
{
    for (size_t i = 0; i < count; i++)
        lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    if (!lobbsArgShiftUint(ctx, idxOut)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? badNumMsg : usage);
        lobbsCommandReplyResponse(ctx, resp);
        return false;
    }
    return true;
}

static bool lobbsMsgTryDisplayRecord(const LoScalar &record, uint32_t readField, std::string &lineOut)
{
    std::string title;
    if (!record.getString(LODB_F_TITLE, title))
        return false;
    if (!title.empty() && title[0] == '[' && record.has(readField)) {
        lineOut = title;
        return true;
    }
    if (title.find("From:") == 0 && record.has(LODB_F_DESCRIPTION)) {
        std::string body;
        if (record.getString(LODB_F_DESCRIPTION, body)) {
            lineOut = title + "\n" + body;
            return true;
        }
    }
    return false;
}

static void displayMsgHuman(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &record)
{
    (void)ctx;
    std::string line;
    if (lobbsMsgTryDisplayRecord(record, MailField::FIELD_READ, line) ||
        lobbsMsgTryDisplayRecord(record, NewsField::FIELD_LIST_READ, line))
        value.setString(LODB_F_TITLE, line);
}

void lobbsMsgRegisterDisplay()
{
    lobbsAddFilter("display_human", displayMsgHuman, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
