#pragma once
#include "core/LoBBSCommandCtx.h"
#include "core/LoBBSResponse.h"
#include <string>

bool lobbsSerializePlainText(const LoBBSCommandCtx &ctx, const LoBBSResponse &resp, std::string &out);

