#include "ConfigCommands.h"
#include "../../core/LoBBSCommandRegistry.h"
#include "../../core/LoBBSHooks.h"
#include "../../core/LoBBSKernel.h"
#include "../../core/LoBBSReplyCache.h"
#include "../../core/LoBBSResponse.h"
#include "../AppUtil.h"
#include "ConfigCommon.h"
#include "ConfigDal.h"
#include "ConfigRecords.h"
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "core/LoBBSStackGuard.h"

static uint32_t gLobbsPagerTtlSec = LOBBS_REPLY_CACHE_TTL_SEC;

uint32_t lobbsReplyCacheTtlSec()
{
    return gLobbsPagerTtlSec;
}

static void filterConfigKeysPager(LoBBSCommandCtx *ctx, std::vector<LoScalar> &keys, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsConfigPushKey(keys, "pager.ttl", LOBBS_REPLY_CACHE_TTL_SEC, 60, 3600, "Seconds /pN cache lasts");
}

static void filterConfigValidateRange(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    (void)ctx;
    std::string existing;
    if (value.getString(LODB_F_ERROR, existing) && !existing.empty())
        return;
    uint32_t proposed = 0;
    uint32_t minV = 0;
    uint32_t maxV = 0;
    if (!value.getUint32(ConfigValidateField::FIELD_VALUE, proposed))
        return;
    if (!args.getUint32(ConfigKeyField::FIELD_MIN, minV) || !args.getUint32(ConfigKeyField::FIELD_MAX, maxV))
        return;
    if (proposed < minV || proposed > maxV) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Must be %u-%u.", (unsigned)minV, (unsigned)maxV);
        value.setString(LODB_F_ERROR, buf);
    }
}

static void actionConfigChangedPager(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)ctx;
    std::string key;
    if (!args.getString(LODB_F_TITLE, key) || key != "pager.ttl")
        return;
    uint32_t ttl = LOBBS_REPLY_CACHE_TTL_SEC;
    args.getUint32(ConfigValidateField::FIELD_VALUE, ttl);
    gLobbsPagerTtlSec = ttl;
}

static void handleConfig(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    ConfigDal &cfg = ctx.kernel->config().dal();
    const char *key = lobbsArgShift(ctx);
    if (!key) {
        LoBBSResponse resp;
        lobbsRecordPush(resp.records, "Use /help config key for details");
        for (const LoScalar &def : cfg.keyDefs(ctx)) {
            std::string name;
            if (!def.getString(LODB_F_TITLE, name))
                continue;
            uint32_t minV = 0;
            uint32_t maxV = 0;
            def.getUint32(ConfigKeyField::FIELD_MIN, minV);
            def.getUint32(ConfigKeyField::FIELD_MAX, maxV);
            uint32_t val = cfg.effectiveValue(ctx, name.c_str());
            char line[96];
            snprintf(line, sizeof(line), "%s %u (%u-%u)", name.c_str(), (unsigned)val, (unsigned)minV, (unsigned)maxV);
            lobbsRecordPush(resp.records, line);
        }
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }

    const char *next = lobbsArgPeek(ctx);
    if (!next) {
        cfg.ensureRegistry(ctx);
        const LoScalar *def = cfg.findKeyDef(key);
        if (!def) {
            lobbsCommandReplyError(ctx, "Unknown setting.");
            return;
        }
        uint32_t val = cfg.effectiveValue(ctx, key);
        uint32_t defV = 0;
        uint32_t minV = 0;
        uint32_t maxV = 0;
        std::string help;
        def->getUint32(ConfigKeyField::FIELD_DEFAULT, defV);
        def->getUint32(ConfigKeyField::FIELD_MIN, minV);
        def->getUint32(ConfigKeyField::FIELD_MAX, maxV);
        def->getString(LODB_F_DESCRIPTION, help);
        char line[128];
        snprintf(line, sizeof(line), "%s = %u (default %u, %u-%u). %s", key, (unsigned)val, (unsigned)defV, (unsigned)minV,
                 (unsigned)maxV, help.c_str());
        lobbsCommandReply(ctx, line);
        return;
    }

    if (strcasecmp(next, "reset") == 0) {
        lobbsArgShift(ctx);
        if (lobbsArgHasMore(ctx)) {
            lobbsCommandReplyError(ctx, "Usage: /config key reset");
            return;
        }
        if (const char *err = cfg.resetKey(ctx, key)) {
            lobbsCommandReplyError(ctx, err);
            return;
        }
        char reply[64];
        snprintf(reply, sizeof(reply), "%s reset.", key);
        lobbsCommandReply(ctx, reply);
        return;
    }

    uint32_t value = 0;
    if (!lobbsArgShiftUint(ctx, value) || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /config key value");
        return;
    }
    if (const char *err = cfg.validateAndSet(ctx, key, value)) {
        lobbsCommandReplyError(ctx, err);
        return;
    }
    char reply[64];
    snprintf(reply, sizeof(reply), "%s = %u.", key, (unsigned)value);
    lobbsCommandReply(ctx, reply);
}

static void slashConfig(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "config"))
        return;
    handleConfig(*ctx);
}

static const LoBBSVerb configHelpVerbs[] = {
    {"config", nullptr, LOBBS_V_SYSOP, "config — sysop: list/get/set/reset settings (/help config key explains one)"},
};

static void filterConfigHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "config", "sysop settings");
}

static void filterConfigHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    lobbsHelpForTable(ctx, value, args, "config", configHelpVerbs, sizeof(configHelpVerbs) / sizeof(configHelpVerbs[0]));
    if (!ctx || !ctx->kernel || value.has(LODB_F_DESCRIPTION) || !lobbsCtxIsSysop(*ctx))
        return;
    std::string query;
    if (!args.getString(LODB_F_TITLE, query) || strncasecmp(query.c_str(), "config ", 7) != 0)
        return;
    const char *key = query.c_str() + 7;
    ConfigDal &cfg = ctx->kernel->config().dal();
    cfg.ensureRegistry(*ctx);
    const LoScalar *def = cfg.findKeyDef(key);
    if (!def)
        return;
    std::string help;
    uint32_t defV = 0;
    uint32_t minV = 0;
    uint32_t maxV = 0;
    def->getString(LODB_F_DESCRIPTION, help);
    def->getUint32(ConfigKeyField::FIELD_DEFAULT, defV);
    def->getUint32(ConfigKeyField::FIELD_MIN, minV);
    def->getUint32(ConfigKeyField::FIELD_MAX, maxV);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: %s. Default %u, range %u-%u.", key, help.c_str(), (unsigned)defV, (unsigned)minV,
             (unsigned)maxV);
    value.setString(LODB_F_DESCRIPTION, buf);
}

void lobbsConfigRegisterCommands()
{
    lobbsAddFilter("config_keys", filterConfigKeysPager, LOBBS_HOOK_PRIORITY_HELP);
    lobbsAddFilter("config_validate", filterConfigValidateRange, LOBBS_HOOK_PRIORITY_HELP);
    lobbsAddAction("config_changed", actionConfigChangedPager, LOBBS_HOOK_PRIORITY_HELP);
    lobbsAddAction("slash_cmd", slashConfig, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterConfigHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterConfigHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
}

