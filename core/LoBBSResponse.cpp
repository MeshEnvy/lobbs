#include "LoBBSResponse.h"
#include "LoBBSKernel.h"
#include "LoBBSReply.h"
#include "LoBBSReplyCache.h"
#include "protocol/machine/paginate.h"
#include "protocol/machine/serialize.h"
#include "protocol/plain_text/paginate.h"
#include "protocol/plain_text/serialize.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>

#include "LoBBSStackGuard.h"

void lobbsResponseSetError(LoBBSResponse &resp, const char *message)
{
    resp.ok = false;
    resp.error = message ? message : "";
    resp.records.clear();
}

void lobbsRecordPush(std::vector<LoScalar> &list, const char *title, const char *description)
{
    LoScalar rec;
    if (title && title[0])
        rec.setString(LODB_F_TITLE, title);
    if (description && description[0])
        rec.setString(LODB_F_DESCRIPTION, description);
    list.push_back(rec);
}

void lobbsResponseAppendRecord(LoBBSResponse &resp, const LoScalar &record)
{
    resp.records.push_back(record);
}
static void lobbsSendSentence(LoBBSCommandCtx &ctx, const char *msg)
{
    if (!ctx.kernel)
        return;
    if (!msg)
        msg = "";
    if (ctx.reqId == 0) {
        ctx.kernel->sendReply(ctx.inbound, msg);
        return;
    }
    char buf[LOBBS_REPLY_BYTES + 32];
    snprintf(buf, sizeof(buf), "<%u>%s", ctx.reqId, msg);
    ctx.kernel->sendReply(ctx.inbound, buf);
}

static bool lobbsRenderPage(LoBBSCommandCtx &ctx, const LoBBSResponse &resp)
{
    const char *errMsg = nullptr;
    uint32_t page = ctx.page ? ctx.page : 1;
    std::string pageText;
    if (ctx.reqId == 0) {
        std::string text;
        if (!lobbsSerializePlainText(ctx, resp, text)) {
            lobbsSendSentence(ctx, "Empty.");
            return false;
        }
        if (!lobbsPaginatePlainText(text, page, pageText, &errMsg)) {
            lobbsSendSentence(ctx, errMsg ? errMsg : "Empty.");
            return false;
        }
        ctx.kernel->sendReply(ctx.inbound, pageText.c_str());
        return true;
    }

    std::string doc;
    if (!lobbsSerializeMachine(resp, doc)) {
        lobbsSendSentence(ctx, "Empty.");
        return false;
    }
    if (!lobbsPaginateMachine(ctx.reqId, doc, page, pageText, &errMsg)) {
        lobbsSendSentence(ctx, errMsg ? errMsg : "Empty.");
        return false;
    }
    ctx.kernel->sendReply(ctx.inbound, pageText.c_str());
    return true;
}

void lobbsCommandReplyError(LoBBSCommandCtx &ctx, const char *message)
{
    lobbsReplyCacheErase(ctx.session.nodeId);
    lobbsSendSentence(ctx, message ? message : "");
}

void lobbsReplySendCachedPage(LoBBSCommandCtx &ctx)
{
    if (!ctx.kernel)
        return;
    LoBBSResponse resp;
    if (!lobbsReplyCacheLoad(ctx.session.nodeId, resp)) {
        lobbsSendSentence(ctx, "No cached reply.");
        return;
    }
    lobbsRenderPage(ctx, resp);
}

void lobbsCommandReplyResponse(LoBBSCommandCtx &ctx, const LoBBSResponse &resp)
{
    if (!ctx.kernel)
        return;

    if (!resp.ok) {
        lobbsSendSentence(ctx, resp.error.c_str());
        return;
    }

    lobbsReplyCacheStore(ctx.session.nodeId, resp);
    lobbsRenderPage(ctx, resp);
}

