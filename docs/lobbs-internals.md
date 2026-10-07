# LoBBS Internals

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

Player commands are in the [daily user guide](lobbs-daily.md). The machine wire format is in the [Machine API](lobbs-machine.md). Why plugins work the way they do, how to define your own hooks, and the new-app checklist are in the [plugin author's guide](lobbs-plugins.md).

## Request flow

1. A DM line starting with `/` may include an optional request id (`/42 mail list`).
2. Page-only verbs (`/p2`, `/p 2`, `/43 p2`) load the cached `LoBBSResponse` for that sender and render a page.
3. Otherwise `slash_cmd` runs. Each plugin compares the verb and returns quietly when it is not the owner.
4. Handlers build a `LoBBSResponse` and call `lobbsCommandReplyResponse`. Successful replies are cached, then page 1 is rendered.
5. Without a request id, plain text uses `{p n/t}` footers. With an id, machine replies use `<id>ok [n:max]`.

There is no central unknown-command reply. If no module owns the verb, the node stays silent.

## One verb, rest untouched

After peeling `/[id]`, the registry takes one verb token and leaves the rest on `ctx.rest`.

Example: `/mail send ben this is my long message` keeps `this is my long message` intact. Mail peels `send`, then `ben`, then treats the rest as the body. Nothing re-tokenizes the whole line up front.

Handlers use `lobbsArgShift`, `lobbsArgPeek`, `lobbsArgRest`, `lobbsArgShiftUint`, `lobbsArgPeekIsUint`, and `lobbsArgShiftMany` on `ctx.rest`. Do not use partial `atoi` on numeric tokens.

Subcommands use one `LoBBSVerb` table for dispatch (`lobbsDispatchSub`) and help (`lobbsHelpForTable`). Rows with `fn == nullptr` are help-only. `LOBBS_V_LOGIN` and `LOBBS_V_SYSOP` gate dispatch and hide sysop rows from help when the caller is not a sysop.

## Paging and reply cache

Every successful command answers with page 1 and stores the full result set in RAM for that sender's node id.

| Constant                        | Value | Role                                        |
| ------------------------------- | ----- | ------------------------------------------- |
| `LOBBS_REPLY_CACHE_TTL_SEC`     | 300   | About five minutes, reset on each page read |
| `LOBBS_REPLY_CACHE_MAX_BYTES`   | 8192  | Larger payloads are not cached              |
| `LOBBS_REPLY_CACHE_MAX_ENTRIES` | 32    | Oldest entry dropped when full              |

A new successful command replaces the cache. Errors do not change it. Over-limit payloads send page 1 and clear the cache, so `/p2` replies `No cached reply.`. Past the end: `No such page.`

Follow with `/p2` or `/43 p2` (request id optional). Plain-text serialization runs `display_human` on each record before paging.

## Help and `/hi`

`/help` with no remainder builds a topic list: built-in rows (`help`, `hi`, `pN`) plus rows from `help_topics`. Logged-in users see feature topics. Sysop-only topics follow each plugin's rules.

`/help mail` or `/help mail send` runs `help_for_topic`. The query is in `args` as `LODB_F_TITLE`. Plugins set `LODB_F_DESCRIPTION` on the value record when they recognize the query. If nothing matches, the reply is `No help found for …`.

`/hi` is a welcome screen, not the command catalog. `/status` uses `status_lines`.

## Hook bus

Source: `LoBBSHooks.h`. Modeled on WordPress actions and filters. The bus is shaped so a handler could later come from a script instead of compiled C++. Why it works this way, and how to define hooks of your own, is in the [plugin author's guide](lobbs-plugins.md).

Rules:

- Hooks are named by string, matched case-insensitively. The bus stores the name pointer, so pass a string literal. Any number of handlers, added at runtime, no cap. `lobbsHooksReset()` clears all.
- There is no list of allowed names. Firing a name nobody registered does nothing. Registering a name nobody fires is never called.
- Every hook receives `args` as a `LoScalar`. Data crosses the bus as LoScalar fields, not C++ types.
- No hook claims or short-circuits. Every handler runs.
- Priority is fixed at registration (lower first, ties keep registration order). Bands: `HELP` 0, `AUTH` 10, `FEATURE` 20, `STATUS` 30, `TIME` 31. Install registers at `AUTH - 1`.
- A handler registered with the wrong kind for a name is logged and skipped.
- Handlers talk only through `ctx`, `args`, and the value. No handler calls another plugin.

Three kinds:

- Action: `(ctx, args)`, fire and forget.
- Record filter: `(ctx, LoScalar &value, args)`. Caller seeds `value`, each handler edits it or leaves it.
- List filter: `(ctx, std::vector<LoScalar> &value, args)`. Same, over a list.

| Hook              | Kind   | Initial value          | args                                             | Role                                                                               |
| ----------------- | ------ | ---------------------- | ------------------------------------------------ | ---------------------------------------------------------------------------------- |
| `slash_cmd`       | action | n/a                    | verb (`LOBBS_ARG_VERB`), rest (`LOBBS_ARG_REST`) | Compare with `lobbsSlashVerbIs`, parse `ctx->rest`                                 |
| `seed`            | action | n/a                    | empty                                            | Demo and test builds (`LOBBS_SEED`). Each app seeds its tables                     |
| `help_topics`     | list   | built-in topics        | empty                                            | Append `{title, description}` rows                                                 |
| `help_for_topic`  | record | title = topic          | title = query                                    | Set description when the topic matches                                             |
| `status_lines`    | list   | empty                  | empty                                            | Append `{title, description}` (`Users` / `12 total` renders as `Users (12 total)`) |
| `display_human`   | record | generic title line     | source record                                    | Rewrite title for records you recognize                                            |
| `config_keys`     | list   | empty                  | empty                                            | Append key definition rows (`default`, `min`, `max` in fields 0–2)                 |
| `config_validate` | record | title = key, 0 = value | key definition record                            | Set `LODB_F_ERROR` to reject; empty means ok                                       |
| `config_changed`  | action | n/a                    | title = key, field 0 = effective value           | Apply new settings after set, reset, or database open                              |
| `install_mounts`  | list   | empty                  | empty                                            | Append `{title = mount name}` rows a database may be installed on                  |

These are the hooks core and the bundled apps fire. Apps add their own names the same way.

Something happened: action. Contribute to one shared value: record filter. Contribute to a shared list: list filter. A new need gets a new hook name, not a new kind.

## Result sets and rendering

Handlers build `LoBBSResponse` (`ok`, `error`, `records`) with `lobbsRecordPush`, `lobbsResponseAppendRecord`, or `lobbsResponseSetError`, then call `lobbsCommandReplyResponse`. `lobbsCommandReply` and `lobbsCommandReplyError` wrap it. Handlers never page.

Human: generic `title (description)` per record, then `display_human`, packed into 200 bytes with `{p n/t}`.

Machine: records encoded as LoScalar lines, sliced into pages with `<id>ok [n:max]`. See the Machine API guide.

`lobbsMsgRegisterDisplay()` (wireup) registers one `display_human` handler for mail and news list and read rows. Mail and news command modules do not register their own `display_human` filters.

## LoFS and install

LoFS exposes `/` as a virtual root listing mounts. Writable paths start with `/<mount>/…`. Slot names in install preference order: `sd`, `lofs`, `extra`, `internal`. Each slot maps to a `LoFSVolume` provider (adopted host LittleFS, LoLog on `/lofs`, or SD). Host composition lives in `lofs/platforms/meshtastic/LoFSMountsMeshtastic.cpp` and `lofs/platforms/meshcore/LoFSMountsMeshcore.cpp`. Mount roots cannot be removed, renamed, or used as copy sources. Cross-mount `mv` copies files then deletes the source. Directories require same-mount rename.

`/lofs` is the exclusive LoLog slot. On boards with owned QSPI NOR (`LOFS_BOARD_HAS_QSPI`), LoLog uses `LoFSQspiNorBlockDevice` (about 2 MiB). Meshtastic QSPI envs link `nrf52840_s140_v7_qspi.ld`: application flash runs to `0xED000` with no `__flash2` gap. Other nRF52840 Meshtastic builds use `nrf52840_s140_v7.ld`, which stops the app at `0xD4000` and provides `__flash2_start` / `__flash2_end` (100 KiB) for `/lofs` via `LoFSNrfFlashRawDevice`. Meshtastic `InternalFS` stays at the Adafruit default (`0xED000` on nRF52840). ESP32 and boards with neither QSPI nor flash2 have no `/lofs`.

`/lofs` uses **LoLog** (`lofs/lolog/`, `LoFSLoLogVolume`): append-only data segments, on-flash index runs, background clean/flush via `LoFS::maintain()` from the Meshtastic module `runOnce` and `lobbsMeshCoreLoop`. `/format lofs` erases the volume and starts a new epoch. There is no LittleFS on `/lofs`. MeshCore does not adopt host QSPI LittleFS: when `QSPIFLASH` is set, the chip is `/lofs` LoLog like Meshtastic.

`/extra` is MeshCore only: host `ExtraFS` LittleFS at `0xD4000` when `EXTRAFS` is enabled and the board has no QSPI (`!QSPIFLASH`). It is shared with the radio. Do not mount `/lofs` on the same flash range as `/extra`.

`/internal` is the host primary filesystem (`InternalFS` / `FSCom` on Meshtastic, the filesystem pointer passed to `lobbsMeshCoreInit` on MeshCore). It is shared: LoBBS keeps a reserve on writes. `/format internal` on Meshtastic runs `FSCom.format()` then `nodeDB->saveToDisk()`. Formatting the install mount reruns `lobbsInstallInit`.

`LoFS::list` callbacks get `(ctx, basename, isDirectory, size)`. `size` is the file length, 0 for directories and mounts.

Each mount has `dbSafe` (computed at boot: non-shared volumes are always db-safe; shared volumes are db-safe only when no higher-preference mount is present) and `formattable`. LittleFS mounts are formattable; `sd` is not. Install points come from the `install_mounts` list filter in preference order above.

`lobbsMeshCoreInit` calls `lofsMeshcoreSetHostFilesystem` before `LoFS::begin`.

Shared mounts keep reserve bytes free (16 KiB nRF52, 128 KiB other hardware, 0 Portduino). `LoFS::hasRoom(path, bytes)` rounds `bytes` up to the block size, adds slack for metadata, adds the mount reserve, and compares with free space. It returns true when the mount reports no size. LoDB checks it before every record write and returns `LODB_ERR_FULL`. FsCommands checks it for `/mkdir`, `/cp`, `/upload`, and cross-mount `/mv`. Apps map `LODB_ERR_FULL` to `Disk full.` via `lobbsDbErrorText`. Deletes are never blocked.

LoBBS home: everything lives under `/<mount>/lobbs` (`LOBBS_HOME_DIR`). `install.ls` marks the install and is written last. At boot `lobbsInstallInit` walks `install_mounts` in order and takes the first mount holding `lobbs/install.ls`. No marker lives on `/internal`.

| State   | Behavior                                                                |
| ------- | ----------------------------------------------------------------------- |
| Blank   | No `lobbs/install.ls` on any install mount. Only `/help` and `/install` |
| Ready   | Marker found and `LoDb::open(home)` succeeded                           |
| Offline | Marker found but the database failed to open. Non-help commands error   |

`/install` authorization matches admin PKI in `AdminModule` (local client or encrypted DM with a configured admin key). Only `/install` creates SysOp accounts.

## Sessions

Logins live in RAM, not LoDB: `AuthDal` holds a vector of `{nodeId, userUuid, lastActiveMs, cwd}` slots, keyed by sender node id. Slot count and idle expiry come from `session.max` and `session.idle` in the config registry (defaults 16 and 86400 s). Each lookup refreshes `lastActiveMs`. A slot idle past the expiry, or whose user row is gone, is freed on lookup. Logging in reuses the node's slot, takes a free or expired one, or evicts the longest idle. Shrinking the limit keeps the most recently active. Sessions end on reboot, and `clearSessions()` runs whenever the database opens. Logins never write flash; overrides live in the `config` table.

## Config registry

SysOp `/config` lists, reads, sets, and resets uint32 settings. Plugins declare keys on `config_keys` via `lobbsConfigPushKey`. `/help config key` replies with the key's help text, default, and range. Validation runs on `config_validate` (core registers a min/max range check at help priority). Successful sets upsert one row in table `config` (title = key); reset deletes the row so defaults cost no flash. `config_changed` fires after each set or reset and for every key when the database opens (`lobbsInstallDatabaseOpened`).

SysOp file commands, chunked upload, and path rules are documented in the [SysOp guide](lobbs-sysop.md).

## LoScalar

One record is one line of numbered fields, `number:value`, joined by `|` in ascending field order, numbers `0..99`. Values escape `\`, `|`, and newline with a backslash. Typed accessors cover strings, `uint32`, `uint64`, bools, and 32-byte keys as hex. `encode` takes a byte limit and fails rather than write past it.

```
95:1790000000|96:1790000000|97:see you at the trailhead|99:9168245037165234184
```

Why this shape:

- One format everywhere. The same line is a row on disk, a record in a `LoBBSResponse`, a hook argument, and a line in a machine reply. Nothing translates between a storage format and a wire format, and a client decodes exactly what the node stored.
- You can read it. `/cat` on a row file, or the SD card in a laptop, shows the fields as text. Debugging a node in the field needs no decoder.
- No schema compiler. A record type is a `*Records.h` header of field numbers. A new field is a new constant, with no generated code and no build step.
- Numbers instead of names. A field costs two digits and a colon, not a key name. That matters when the whole reply is 200 bytes.
- Decoding keeps every field, including numbers the reader does not know. Two apps, or a firmware and a client a release apart, can share a record without agreeing on all of it.
- Flat on purpose. There is no nesting and no quoting. A news body full of quotes needs no JSON parser. A structured value, like the wall grid, is one string field the owning app parses.
- Small. The codec is one class with no dependencies, which fits nRF52 flash and the 512-byte stack cap.

On nRF52, do not format `uint64_t` with `%llu`. Use `loU64ToDec` from `loutil/LoUtil.h`. `loHumanBytes` prints short sizes (`9B`, `1.2K`, `1.8M`).

## LoDB

`LoDb::open(root)` stores tables at `<root>/<db>/<table>/` (LoBBS: `/<mount>/lobbs/db/<table>/`), one `<16 hex uuid>.ls` file per row. Writes go to `<path>.w` then rename. API: `registerTable`, `insert`, `get`, `update`, `upsert`, `deleteRecord`, `select`, `count`. Row cap `LODB_MAX_RECORD_BYTES` (1024). Uuid from `lodb_new_uuid`. Prefer `upsert` for keyed or singleton rows. Runtime quotas and session limits use the config registry, not per-app singleton tables.

### Field numbers

App fields use `0..93` (`LODB_F_USER_LIMIT` is 94). Each app's `*Records.h` is the schema. System fields count down from 99.

| Number | Name                 | Who writes it                    |
| ------ | -------------------- | -------------------------------- |
| 99     | `LODB_F_ID`          | LoDB, the row uuid               |
| 98     | `LODB_F_TITLE`       | The app. Human renderer line     |
| 97     | `LODB_F_DESCRIPTION` | The app. Mail, news, yarn bodies |
| 96     | `LODB_F_CREATED`     | LoDB unless set. Unix seconds    |
| 94     | `LODB_F_ERROR`       | Any filter. User-facing error    |
| 95     | `LODB_F_UPDATED`     | LoDB unless set. Unix seconds    |

Strip 95 and 96 before `update` to get default stamps. Times are Unix seconds, not milliseconds.

## Stack budget

`LoBBSStackGuard.h`: on nRF52 every LoBBS function is capped at 512 bytes of stack (`-Wstack-usage=512` as an error). Commands run on the 4 KB loop task shared with the router. Include the guard last in every LoBBS `.cpp`. Keep large buffers off the stack.

## Verifying

Product platforms are Meshtastic and MeshCore only. Build `pio run -e rak4631` (Meshtastic fork) or a MeshCore terminal-chat env, and run `trunk fmt` before commit.

Protocol paging tests (no fork): `cd ci/host && pio test -e host -f test_protocol_host` (`LOBBS_PLATFORM_NATIVE`).

Command integration tests use the Meshtastic fork’s `native-macos` env (`pio test -e native-macos -f test_lobbs_commands`). That runs on upstream’s Portduino desktop target. It is not a separate LoBBS platform.

## Plugin checklist

See [lobbs-plugins.md](lobbs-plugins.md).
