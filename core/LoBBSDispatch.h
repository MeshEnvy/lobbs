#pragma once
#include "LoWire.h"

class LoBBSKernel;

void lobbsCoreHandleInbound(LoBBSKernel *kernel, LoInbound &in);
/** Non-command user text (e.g. DM body) to sysop mail. */
void lobbsCoreDeliverUserMail(LoBBSKernel *kernel, LoInbound &in);
