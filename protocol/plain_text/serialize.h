#pragma once
#include "LoBBSCommandCtx.h"
#include "LoBBSResponse.h"
#include <string>

bool lobbsSerializePlainText(const LoBBSCommandCtx &ctx, const LoBBSResponse &resp, std::string &out);

