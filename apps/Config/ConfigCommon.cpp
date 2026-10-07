#if !MESHTASTIC_EXCLUDE_LOBBS

#include "ConfigCommon.h"
#include "../../LoBBSModule.h"
#include "ConfigDal.h"
#include <cstring>

#include "LoBBSStackGuard.h"

void lobbsConfigPushKey(std::vector<LoScalar> &keys, const char *key, uint32_t def, uint32_t min, uint32_t max, const char *help)
{
    if (!key)
        return;
    LoScalar rec;
    rec.setString(LODB_F_TITLE, key);
    if (help)
        rec.setString(LODB_F_DESCRIPTION, help);
    rec.setUint32(ConfigKeyField::FIELD_DEFAULT, def);
    rec.setUint32(ConfigKeyField::FIELD_MIN, min);
    rec.setUint32(ConfigKeyField::FIELD_MAX, max);
    keys.push_back(rec);
}

uint32_t lobbsConfigGet(LoBBSCommandCtx &ctx, const char *key)
{
    if (!ctx.mod || !key)
        return 0;
    return ctx.mod->config().dal().effectiveValue(ctx, key);
}

#endif
