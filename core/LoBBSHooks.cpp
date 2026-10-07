#include "LoBBSHooks.h"
#include "platforms/LoPlatform.h"
#include <cstring>

#include "LoBBSStackGuard.h"

enum class LobbsHookKind : uint8_t { Action, Record, List };

struct LobbsHookEntry {
    const char *name = nullptr;
    LobbsHookKind kind = LobbsHookKind::Action;
    void (*fn)() = nullptr;
    int priority = LOBBS_HOOK_PRIORITY_DEFAULT;
};

static std::vector<LobbsHookEntry> lobbsHooks;

void lobbsHooksReset()
{
    lobbsHooks.clear();
}

static void lobbsAddHook(const char *name, LobbsHookKind kind, void (*fn)(), int priority)
{
    if (!name || !fn)
        return;
    LobbsHookEntry entry;
    entry.name = name;
    entry.kind = kind;
    entry.fn = fn;
    entry.priority = priority;
    size_t insertAt = lobbsHooks.size();
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (priority < lobbsHooks[i].priority) {
            insertAt = i;
            break;
        }
    }
    lobbsHooks.insert(lobbsHooks.begin() + insertAt, entry);
}

void lobbsAddAction(const char *name, LoBBSActionFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::Action, reinterpret_cast<void (*)()>(fn), priority);
}

void lobbsAddFilter(const char *name, LoBBSRecordFilterFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::Record, reinterpret_cast<void (*)()>(fn), priority);
}

void lobbsAddFilter(const char *name, LoBBSListFilterFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::List, reinterpret_cast<void (*)()>(fn), priority);
}

void lobbsDoAction(const char *name, LoBBSCommandCtx &ctx, const LoScalar &args)
{
    if (!name)
        return;
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (!lobbsHooks[i].name || strcasecmp(lobbsHooks[i].name, name) != 0)
            continue;
        if (lobbsHooks[i].kind != LobbsHookKind::Action) {
            LOBBS_LOG_ERROR("LoBBS hook %s: handler registered with the wrong kind", name);
            continue;
        }
        reinterpret_cast<LoBBSActionFn>(lobbsHooks[i].fn)(&ctx, args);
    }
}

void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, LoScalar &value, const LoScalar &args)
{
    if (!name)
        return;
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (!lobbsHooks[i].name || strcasecmp(lobbsHooks[i].name, name) != 0)
            continue;
        if (lobbsHooks[i].kind != LobbsHookKind::Record) {
            LOBBS_LOG_ERROR("LoBBS hook %s: handler registered with the wrong kind", name);
            continue;
        }
        reinterpret_cast<LoBBSRecordFilterFn>(lobbsHooks[i].fn)(&ctx, value, args);
    }
}

void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, std::vector<LoScalar> &value, const LoScalar &args)
{
    if (!name)
        return;
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (!lobbsHooks[i].name || strcasecmp(lobbsHooks[i].name, name) != 0)
            continue;
        if (lobbsHooks[i].kind != LobbsHookKind::List) {
            LOBBS_LOG_ERROR("LoBBS hook %s: handler registered with the wrong kind", name);
            continue;
        }
        reinterpret_cast<LoBBSListFilterFn>(lobbsHooks[i].fn)(&ctx, value, args);
    }
}

