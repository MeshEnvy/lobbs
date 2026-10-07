# LoBBS Plugin Author's Guide

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

How LoBBS plugins fit together, how to add hooks of your own, and checklists for a new app. Hook signatures, paging, storage, and stack limits are in [LoBBS internals](lobbs-internals.md).

## How plugins fit together

LoBBS core does not know what mail, news, or the wall are. It peels the request id and the verb, fires `slash_cmd`, and renders whatever result set comes back. Every command, including `/login` and `/help`, is a plugin that subscribed to hooks.

- Adding or removing an app is one line in `LoBBSWireup.cpp`. Nothing else changes, because nothing else calls it.
- Every handler runs and none can claim a hook. A new plugin can add to what LoBBS does, but it cannot quietly switch another one off. What the node does is the sum of what is installed, in priority order.
- Plugins never call each other. They meet at named hooks and pass LoScalar records. Remove a plugin and the ones listening to it keep working. Their handlers for its hooks are just never called.
- Data crosses the bus as LoScalar fields, not C++ types. A handler that only reads and writes numbered fields could later be a script instead of compiled code.

## Define your own hooks

The hooks in the internals table are the ones core and the bundled apps fire. They are not a closed set. A hook is only a name, so your app can fire its own and any other plugin can subscribe without including your headers.

Fire an action where something happened that others might react to:

```cpp
LoScalar args;
args.setUint64(LODB_F_ID, mailUuid);
args.setString(LODB_F_TITLE, toUsername);
lobbsDoAction("mail_sent", ctx, args);
```

Apply a filter where others might want to change a value before you use it. You seed the default, and the value you read back is what every handler agreed on:

```cpp
LoScalar post;
post.setString(LODB_F_DESCRIPTION, body);
lobbsApplyFilter("news_before_post", ctx, post, LoScalar());
post.getString(LODB_F_DESCRIPTION, body);
```

Another plugin subscribes by name:

```cpp
static void notifyOnMail(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    // args carries the mail id and recipient
}

lobbsAddAction("mail_sent", notifyOnMail, LOBBS_HOOK_PRIORITY_FEATURE);
```

These two hooks are examples. The bundled apps do not fire them.

Rules for a hook you own:

1. Prefix the name with your app (`mail_`, `wall_`). Unprefixed names are the shared hooks in the internals table.
2. Pass a string literal. The bus keeps the pointer.
3. One kind per name. A handler registered with a different kind is logged and skipped.
4. Use title (`98`) and description (`97`) for the obvious text. Give other args fields numbers in `0..93`.
5. Fire with the `ctx` of the command you are handling, so subscribers see the same sender and session.
6. Document it in the same change: a row in the hook table in `LoBBSHooks.h` and in [internals](lobbs-internals.md#hook-bus) with name, kind, initial value, and args.

## New command app

1. In `lobbsXRegisterCommands()`, register `slash_cmd`, `help_topics`, and `help_for_topic`. Add `status_lines`, `display_human`, or `seed` when needed.
2. One `LoBBSVerb` table drives `lobbsDispatchSub` and `lobbsHelpForTable`. Gate with `LOBBS_V_LOGIN` and `LOBBS_V_SYSOP`.
3. In `slash_cmd`, return quietly unless `lobbsSlashVerbIs` matches your verb.
4. Build a `LoBBSResponse` and call `lobbsCommandReplyResponse`. Do not page or parse `pN`.
5. Talk only through `ctx`, `args`, and the filter value. Do not call another plugin. Do not assume you run first or alone.
6. Include `LoBBSStackGuard.h` last in every `.cpp`. Keep functions under 512 bytes of stack on nRF52.
7. Add one `lobbsXRegisterCommands()` call in `LoBBSWireup.cpp`.
8. Do not `#include` `AuthDal.h` from feature code. Use the session on `ctx` and `AppUtil`.

## Runtime settings

1. Register a `config_keys` list filter and call `lobbsConfigPushKey` for each uint32 setting (default, min, max, help text shown by `/help config key`).
2. Read values with `lobbsConfigGet(ctx, "your.key")` in command handlers before calling your DAL.
3. Optional: register `config_validate` at a priority after `LOBBS_HOOK_PRIORITY_HELP` for cross-field checks.
4. Register `config_changed` to apply side effects (resize buffers, refresh caches).

Mail is the reference implementation for subcommands and help tables.

## New record type

1. Pick a table name. App fields use `0..93`. Body in field `97`, short name in `98`.
2. Do not set field `99`. Leave `95` and `96` unset on insert. `removeField` them before `update` when you want LoDB to stamp times.
3. `registerTable` at app construction. Prefer `LoDb::upsert` for keyed or singleton rows.
4. Build uuids with `lodb_new_uuid`. Do not print `uint64_t` with `%llu` on nRF52. Use `loU64ToDec` from `loutil/LoUtil.h`.
