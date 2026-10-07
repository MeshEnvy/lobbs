#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSModule.h"
#include "apps/Auth/AuthDal.h"
#include "mesh/MeshTypes.h"
#include "mesh/NodeDB.h"
#include <cstring>

#include "LoBBSStackGuard.h"

static void lobbsTrimLine(char *line)
{
    if (!line)
        return;
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r' || line[len - 1] == '\n'))
        line[--len] = '\0';
    char *start = line;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != line)
        memmove(line, start, strlen(start) + 1);
}

static LoBBSSession lobbsResolveSession(LoBBSModule *mod, uint32_t wireNodeId)
{
    LoBBSSession session;
    session.nodeId = wireNodeId;
    AuthDal &auth = mod->auth().dal();
    LoScalar userRow;
    uint32_t sessionNodeId = wireNodeId;
    uint64_t authUserUuid = 0;
    std::string cwd;
    if (!auth.loadUserByNodeId(wireNodeId, &userRow, &sessionNodeId, &authUserUuid, &cwd))
        return session;
    session.nodeId = sessionNodeId;
    session.userUuid = authUserUuid;
    if (!cwd.empty() && cwd[0] == '/' && cwd.size() < sizeof(session.cwd))
        memcpy(session.cwd, cwd.c_str(), cwd.size() + 1);
    if (!AuthDal::userUsername(userRow, session.username, sizeof(session.username)))
        session.username[0] = '\0';
    session.isSysop = AuthDal::userIsSysop(userRow);
    return session;
}

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp)
{
    if (!isToUs(&mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    size_t copyLen = mp.decoded.payload.size;
    if (copyLen >= sizeof(mod->msgBuffer))
        copyLen = sizeof(mod->msgBuffer) - 1;
    memcpy(mod->msgBuffer, mp.decoded.payload.bytes, copyLen);
    mod->msgBuffer[copyLen] = '\0';

    lobbsTrimLine(mod->msgBuffer);
    if (mod->msgBuffer[0] == '\0' || mod->msgBuffer[0] != '/')
        return ProcessMessage::CONTINUE;

    const LoBBSSession session = lobbsResolveSession(mod, getFrom(&mp));
    lobbsCommandsHandle(mod, mp, session, mod->msgBuffer);
#ifdef ARCH_NRF52
    // The loop stack has no overflow trap; warn once while there is still room to see the log.
    static bool lowStackWarned = false;
    unsigned headroom = (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    if (!lowStackWarned && headroom < 1024) {
        lowStackWarned = true;
        LOG_WARN("LoBBS loop stack headroom low: %u B", headroom);
    }
#endif
    return ProcessMessage::CONTINUE;
}

#endif
