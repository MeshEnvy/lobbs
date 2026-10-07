#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandCtx.h"
#include "LoBBSResponse.h"
#include <loscalar/LoScalar.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

/**
 * Hook bus modeled on WordPress actions and filters.
 *
 * A filter threads one value through its handlers in priority order. The caller supplies the
 * initial value (defaults go there), each handler modifies it in place or leaves it alone, and the
 * caller gets the result. Actions are fire-and-forget. Every hook receives `args` as a LoScalar.
 *
 * The hook name fixes the value kind and the field conventions:
 *
 * | Hook             | Kind   | Initial value (caller)          | args                  |
 * |------------------|--------|---------------------------------|-----------------------|
 * | `slash_cmd`      | action | n/a                             | verb, rest            |
 * | `seed`           | action | n/a                             | none                  |
 * | `help_topics`    | list   | builtin topics                  | none                  |
 * | `help_for_topic` | record | title = topic, no description   | title = query         |
 * | `status_lines`   | list   | empty                           | none                  |
 * | `display_human`  | record | title = generic rendering       | the record to render  |
 * | `config_keys`    | list   | empty                           | none                  |
 * | `config_validate`| record | title = key, field 0 = value    | key definition record |
 * | `config_changed` | action | n/a                             | title = key, field 0 = value |
 * | `install_mounts` | list   | empty (title = mount name)      | none                  |
 *
 * Records use `title` and `description` (LODB_F_TITLE / LODB_F_DESCRIPTION) unless noted.
 * Validation filters may set LODB_F_ERROR on the value record; set succeeds only if it stays empty.
 * `slash_cmd` args use LOBBS_ARG_VERB / LOBBS_ARG_REST; C++ handlers parse ctx->rest.
 */

static constexpr int LOBBS_HOOK_PRIORITY_HELP = 0;
static constexpr int LOBBS_HOOK_PRIORITY_AUTH = 10;
static constexpr int LOBBS_HOOK_PRIORITY_FEATURE = 20;
static constexpr int LOBBS_HOOK_PRIORITY_STATUS = 30;
static constexpr int LOBBS_HOOK_PRIORITY_TIME = 31;
static constexpr int LOBBS_HOOK_PRIORITY_DEFAULT = 10;

static constexpr uint32_t LOBBS_ARG_VERB = 0;
static constexpr uint32_t LOBBS_ARG_REST = 1;

using LoBBSActionFn = void (*)(LoBBSCommandCtx *ctx, const LoScalar &args);
using LoBBSRecordFilterFn = void (*)(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args);
using LoBBSListFilterFn = void (*)(LoBBSCommandCtx *ctx, std::vector<LoScalar> &value, const LoScalar &args);

void lobbsHooksReset();
void lobbsAddAction(const char *name, LoBBSActionFn fn, int priority = LOBBS_HOOK_PRIORITY_DEFAULT);
void lobbsAddFilter(const char *name, LoBBSRecordFilterFn fn, int priority = LOBBS_HOOK_PRIORITY_DEFAULT);
void lobbsAddFilter(const char *name, LoBBSListFilterFn fn, int priority = LOBBS_HOOK_PRIORITY_DEFAULT);
void lobbsDoAction(const char *name, LoBBSCommandCtx &ctx, const LoScalar &args);
void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, LoScalar &value, const LoScalar &args);
void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, std::vector<LoScalar> &value, const LoScalar &args);

#endif
