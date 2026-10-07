#pragma once
#include "LoBBSConfig.h"
#include "LoWire.h"
#include <stdint.h>

class LoBBSKernel;

/** Resolved once per incoming command in dispatch (Auth owns load rules). */
struct LoBBSSession {
    uint32_t nodeId = 0;
    uint64_t userUuid = 0;
    char username[LOBBS_USERNAME_BUFFER_SIZE] = {0};
    bool isSysop = false;
    char cwd[LOBBS_CWD_BUFFER_SIZE] = "/";
};

struct LoBBSCommandCtx {
    LoBBSKernel *kernel = nullptr;
    LoInbound inbound;
    uint32_t reqId = 0;
    LoBBSSession session;
    /** Remainder after verb; mutable cursor for shift/takePage. */
    char *rest = nullptr;
    uint32_t page = 1;
};
