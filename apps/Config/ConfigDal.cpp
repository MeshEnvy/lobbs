#if !MESHTASTIC_EXCLUDE_LOBBS

#include "ConfigDal.h"
#include "../../LoBBSCommandCtx.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../AppUtil.h"
#include "ConfigRecords.h"
#include <cstdio>
#include <cstring>

#include "LoBBSStackGuard.h"

ConfigDal::ConfigDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("config");
}

lodb_uuid_t ConfigDal::rowUuid(const char *key) const
{
    return lodb_new_uuid(key, 0);
}

void ConfigDal::resetCache()
{
    overrides_.clear();
    registry_.clear();
    registryBuilt_ = false;
    overridesLoaded_ = false;
}

void ConfigDal::reloadOverrides()
{
    overrides_.clear();
    overridesLoaded_ = true;
    auto rows = lodb_.select(
        "config", [](const LoScalar &) { return true; }, LoDbComparator());
    for (const LoScalar &rec : rows) {
        std::string key;
        if (!rec.getString(LODB_F_TITLE, key) || key.empty())
            continue;
        uint32_t val = 0;
        if (rec.getUint32(ConfigStoreField::FIELD_VALUE, val))
            overrides_[key] = val;
    }
}

void ConfigDal::ensureRegistry(LoBBSCommandCtx &ctx)
{
    if (registryBuilt_)
        return;
    registryBuilt_ = true;
    registry_.clear();
    lobbsApplyFilter("config_keys", ctx, registry_, LoScalar());
}

const std::vector<LoScalar> &ConfigDal::keyDefs(LoBBSCommandCtx &ctx)
{
    ensureRegistry(ctx);
    return registry_;
}

const LoScalar *ConfigDal::findKeyDef(const char *key)
{
    if (!key)
        return nullptr;
    for (const LoScalar &rec : registry_) {
        std::string title;
        if (rec.getString(LODB_F_TITLE, title) && strcasecmp(title.c_str(), key) == 0)
            return &rec;
    }
    return nullptr;
}

uint32_t ConfigDal::effectiveValue(LoBBSCommandCtx &ctx, const char *key)
{
    ensureRegistry(ctx);
    const LoScalar *def = findKeyDef(key);
    if (!def)
        return 0;
    uint32_t fallback = 0;
    def->getUint32(ConfigKeyField::FIELD_DEFAULT, fallback);
    if (!overridesLoaded_)
        return fallback;
    auto it = overrides_.find(key);
    if (it != overrides_.end())
        return it->second;
    return fallback;
}

void ConfigDal::fireConfigChanged(LoBBSCommandCtx &ctx, const char *key, uint32_t value)
{
    LoScalar args;
    args.setString(LODB_F_TITLE, key);
    args.setUint32(ConfigValidateField::FIELD_VALUE, value);
    lobbsDoAction("config_changed", ctx, args);
}

const char *ConfigDal::validateAndSet(LoBBSCommandCtx &ctx, const char *key, uint32_t value)
{
    ensureRegistry(ctx);
    const LoScalar *def = findKeyDef(key);
    if (!def)
        return "Unknown setting.";

    LoScalar probe;
    probe.setString(LODB_F_TITLE, key);
    probe.setUint32(ConfigValidateField::FIELD_VALUE, value);
    lobbsApplyFilter("config_validate", ctx, probe, *def);

    std::string err;
    if (probe.getString(LODB_F_ERROR, err) && !err.empty()) {
        strncpy(lastValidateError_, err.c_str(), sizeof(lastValidateError_) - 1);
        lastValidateError_[sizeof(lastValidateError_) - 1] = '\0';
        return lastValidateError_;
    }

    LoScalar row;
    row.setString(LODB_F_TITLE, key);
    row.setUint32(ConfigStoreField::FIELD_VALUE, value);
    LoDbError dbErr = lodb_.upsert("config", rowUuid(key), row);
    if (dbErr != LODB_OK)
        return lobbsDbErrorText(dbErr);
    overrides_[key] = value;
    fireConfigChanged(ctx, key, value);
    return nullptr;
}

const char *ConfigDal::resetKey(LoBBSCommandCtx &ctx, const char *key)
{
    ensureRegistry(ctx);
    const LoScalar *def = findKeyDef(key);
    if (!def)
        return "Unknown setting.";
    uint32_t fallback = 0;
    def->getUint32(ConfigKeyField::FIELD_DEFAULT, fallback);
    lodb_.deleteRecord("config", rowUuid(key));
    overrides_.erase(key);
    fireConfigChanged(ctx, key, fallback);
    return nullptr;
}

void ConfigDal::notifyDatabaseOpened(LoBBSModule &mod)
{
    resetCache();
    reloadOverrides();
    LoBBSCommandCtx ctx;
    ctx.mod = &mod;
    ensureRegistry(ctx);
    for (const LoScalar &rec : registry_) {
        std::string key;
        if (!rec.getString(LODB_F_TITLE, key) || key.empty())
            continue;
        fireConfigChanged(ctx, key.c_str(), effectiveValue(ctx, key.c_str()));
    }
}

#endif
