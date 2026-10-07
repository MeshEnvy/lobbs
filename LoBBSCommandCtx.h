#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

/** Resolved once per incoming command in dispatch (Auth owns load rules). */
struct LoBBSSession {
    uint32_t nodeId = 0;
    uint64_t userUuid = 0;
    char username[LOBBS_USERNAME_BUFFER_SIZE] = {0};
    bool isSysop = false;
    char cwd[LOBBS_CWD_BUFFER_SIZE] = "/";
};

struct LoBBSCommandCtx {
    LoBBSModule *mod = nullptr;
    const meshtastic_MeshPacket *mp = nullptr;
    uint32_t reqId = 0;
    LoBBSSession session;
    /** Remainder after verb; mutable cursor for shift/takePage. */
    char *rest = nullptr;
    uint32_t page = 1;
};

#endif
