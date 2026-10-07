#if !MESHTASTIC_EXCLUDE_LOBBS

#include "serialize.h"
#include "LoBBSHooks.h"
#include <lodb/LoDB.h>

#include "LoBBSStackGuard.h"

static void lobbsDisplayGeneric(const LoScalar &record, std::string &lineOut)
{
    std::string title;
    std::string body;
    record.getString(LODB_F_TITLE, title);
    record.getString(LODB_F_DESCRIPTION, body);
    if (title.empty() && body.empty()) {
        lineOut.clear();
        return;
    }
    if (title.empty()) {
        lineOut = body;
        return;
    }
    if (body.empty()) {
        lineOut = title;
        return;
    }
    lineOut = title + " (" + body + ")";
}

bool lobbsSerializePlainText(const LoBBSCommandCtx &ctx, const LoBBSResponse &resp, std::string &out)
{
    out.clear();
    if (!resp.ok) {
        out = resp.error;
        return !out.empty();
    }
    for (const LoScalar &rec : resp.records) {
        std::string line;
        lobbsDisplayGeneric(rec, line);
        LoScalar shown;
        shown.setString(LODB_F_TITLE, line);
        lobbsApplyFilter("display_human", const_cast<LoBBSCommandCtx &>(ctx), shown, rec);
        line.clear();
        shown.getString(LODB_F_TITLE, line);
        if (line.empty())
            continue;
        if (!out.empty())
            out.push_back('\n');
        out += line;
    }
    return !out.empty();
}

#endif
