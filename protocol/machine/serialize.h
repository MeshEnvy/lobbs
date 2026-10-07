#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSResponse.h"
#include <string>

bool lobbsSerializeMachine(const LoBBSResponse &resp, std::string &out);

#endif
