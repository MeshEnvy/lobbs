#include "LoBBSDispatch.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSKernel.h"
#include "platforms/LoPlatform.h"
#include "apps/Auth/AuthDal.h"
#include <cstring>

#include "LoBBSStackGuard.h"
#include "apps/AppUtil.h"
#include "apps/Mail/MailDal.h"
#include "LoBBSConfig.h"

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

static LoBBSSession lobbsResolveSession(LoBBSKernel *kernel, uint32_t wireNodeId)
{
    LoBBSSession session;
    session.nodeId = wireNodeId;
    AuthDal &auth = kernel->auth().dal();
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

void lobbsCoreHandleInbound(LoBBSKernel *kernel, LoInbound &in)
{
    if (!kernel || !in.text)
        return;

    char *line = kernel->msgBuffer;
    size_t copyLen = strlen(in.text);
    if (copyLen >= sizeof(kernel->msgBuffer))
        copyLen = sizeof(kernel->msgBuffer) - 1;
    memcpy(line, in.text, copyLen);
    line[copyLen] = '\0';

    lobbsTrimLine(line);
    if (line[0] == '\0' || line[0] != '/')
        return;

    const LoBBSSession session = lobbsResolveSession(kernel, in.from.key);
    lobbsCommandsHandle(kernel, in, session, line);
    lobbsPlatformNoteHandlerStack();
}

void lobbsCoreDeliverUserMail(LoBBSKernel *kernel, LoInbound &in)
{
    if (!kernel || !in.text || !in.text[0] || in.text[0] == '/')
        return;

    uint64_t sysop = lobbsAppUuidForUsername(kernel, "sysop");
    if (!sysop)
        return;

    uint64_t fromUuid = 0;
    LoScalar user;
    (void)kernel->auth().dal().loadUserByNodeId(in.from.key, &user, nullptr, &fromUuid, nullptr);

    char body[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    strncpy(body, in.text, LOBBS_MESSAGE_BODY_MAX);
    body[LOBBS_MESSAGE_BODY_MAX] = '\0';

    if (kernel->mail().dal().sendMail(fromUuid, sysop, body) == LODB_OK)
        kernel->sendReply(in, "Message sent.");
    else
        kernel->sendReply(in, "Mailbox unavailable.");
}
