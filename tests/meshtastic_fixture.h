#pragma once
#if LOBBS_SEED

#include "core/LoBBSDispatch.h"
#include "core/LoWire.h"
#include "gps/RTC.h"
#include "mesh/NodeDB.h"
#include <cstring>
#include <memory>

static constexpr uint32_t kLobbsTestBbsNode = 0xBB500001;
static constexpr uint32_t kLobbsTestClientNode = 0xABCD0001;

class LobbsTestNodeDB : public NodeDB
{
  public:
    LobbsTestNodeDB() { myNodeInfo.my_node_num = kLobbsTestBbsNode; }
};

inline void lobbsTestResetRtc()
{
    resetRTCStateForTests();
    struct timeval tv = {1000, 0};
    setRTCSystemTimeForTests(&tv);
}

inline void lobbsTestBindNodeDb(std::unique_ptr<LobbsTestNodeDB> &nodeDb)
{
    nodeDb = std::make_unique<LobbsTestNodeDB>();
    nodeDB = nodeDb.get();
}

inline void lobbsTestClearNodeDb(std::unique_ptr<LobbsTestNodeDB> &nodeDb)
{
    nodeDb.reset();
    nodeDB = nullptr;
    resetRTCStateForTests();
}

inline void lobbsTestSendRadioLine(LoBBSKernel *kernel, const char *line, uint32_t fromNode = kLobbsTestClientNode)
{
    static char payloadBuf[256];
    LoInbound in;
    in.from.key = fromNode;
    in.channel = LoInboundChannel::Radio;
    in.replyContext = nullptr;
    size_t len = strlen(line);
    if (len >= sizeof(payloadBuf))
        len = sizeof(payloadBuf) - 1;
    memcpy(payloadBuf, line, len);
    payloadBuf[len] = '\0';
    in.text = payloadBuf;
    lobbsCoreHandleInbound(kernel, in);
}

inline void lobbsTestSendLocalLine(LoBBSKernel *kernel, const char *line)
{
    static char payloadBuf[256];
    LoInbound in;
    in.from.key = 0;
    in.channel = LoInboundChannel::Local;
    in.replyContext = nullptr;
    size_t len = strlen(line);
    if (len >= sizeof(payloadBuf))
        len = sizeof(payloadBuf) - 1;
    memcpy(payloadBuf, line, len);
    payloadBuf[len] = '\0';
    in.text = payloadBuf;
    lobbsCoreHandleInbound(kernel, in);
}

#endif
