#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "platforms/meshtastic/LoBBSDispatchMeshtastic.h"
#include "core/LoBBSDispatch.h"
#include "core/LoBBSKernel.h"
#include "core/LoWire.h"
#include "mesh/MeshTypes.h"
#include <cstring>

LoInbound lobbsInboundFromPacket(const meshtastic_MeshPacket &mp, char *payloadBuf, size_t payloadBufLen)
{
    LoInbound in;
    in.replyContext = &mp;
    in.from.key = getFrom(&mp);
    in.channel = (mp.from == 0) ? LoInboundChannel::Local : LoInboundChannel::Radio;
    in.text = payloadBuf;
    if (payloadBuf && payloadBufLen > 0 && mp.decoded.payload.size > 0) {
        size_t copyLen = mp.decoded.payload.size;
        if (copyLen >= payloadBufLen)
            copyLen = payloadBufLen - 1;
        memcpy(payloadBuf, mp.decoded.payload.bytes, copyLen);
        payloadBuf[copyLen] = '\0';
    } else if (payloadBuf && payloadBufLen > 0) {
        payloadBuf[0] = '\0';
    }
    return in;
}

ProcessMessage lobbsDispatchReceived(LoBBSKernel *kernel, const meshtastic_MeshPacket &mp)
{
    if (!isToUs(&mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    static char payloadBuf[256];
    LoInbound in = lobbsInboundFromPacket(mp, payloadBuf, sizeof(payloadBuf));

    lobbsCoreHandleInbound(kernel, in);
    return ProcessMessage::CONTINUE;
}
#endif
