#include "serialize.h"
#include <cstddef>

#include "core/LoBBSStackGuard.h"

bool lobbsSerializeMachine(const LoBBSResponse &resp, std::string &out)
{
    out.clear();
    if (!resp.ok) {
        out = resp.error;
        return true;
    }
    for (const LoScalar &rec : resp.records) {
        if (!out.empty())
            out.push_back('\n');
        std::string line;
        if (!rec.encode(line, SIZE_MAX))
            return false;
        out += line;
    }
    return true;
}

