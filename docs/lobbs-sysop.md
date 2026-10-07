# LoBBS SysOp Guide

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

If you run a LoBBS node, you are the System Operator (SysOp) and this guide is for you.

Normal player commands are in the [daily user guide](lobbs-daily.md).

## Install

[Install on MeshForge](https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware) or compile and flash from source.

A new LoBBS node has no database until you install it. Only `/help` and `/install` work until then. The install hint lists the mounts this hardware can hold a database on:

| Hardware                 | Install mounts                                       |
| ------------------------ | ---------------------------------------------------- |
| nRF52840                 | `sd`, `extra`, `flash2` (100 KB set aside for LoBBS) |
| ESP32 and other hardware | `sd`, `extra`, `flash`                               |

Mounts are listed in preference order. `sd` appears when a card is present. `extra` (about 2 MB) appears on Seeed XIAO nRF52840 kit and SenseCAP Solar Node builds (`LOBBS_EXTRA_QSPI=1`). On nRF52840 `/flash` is Meshtastic's own 28 KB and is not offered.

`/install <mount> <user> <pass>` creates the database on that mount and makes `<user>` the SysOp. Use it from the local client (phone app connected over serial/BLE) or from a remote DM encrypted with a configured admin key. If the target already holds a LoBBS database, the same command adopts it and you must supply an existing SysOp username and password. Once installed, `/install` replies `Already installed at /<mount>.`

Everything LoBBS stores lives under `/<mount>/lobbs`:

| Path                          | Contents                                             |
| ----------------------------- | ---------------------------------------------------- |
| `/<mount>/lobbs/install.ls`   | Install record. Its presence marks the install       |
| `/<mount>/lobbs/db/`          | The database (users, mail, news, wall, yarn, config) |
| `/<mount>/lobbs/apps/<app>/`  | Reserved for per-app files                           |
| `/<mount>/lobbs/home/<user>/` | Reserved for user files                              |

On boot LoBBS checks the install mounts in preference order and uses the first one holding `lobbs/install.ls`. Nothing is stored on `/flash`, so a Meshtastic wipe of `/flash` does not lose the install. Pulling the SD card that holds it makes the node look new until the card is back.

## Demo builds

Add `-D LOBBS_DEMO_MODE` to `build_flags` (or `PLATFORMIO_BUILD_FLAGS="-D LOBBS_DEMO_MODE" pio run -e <env>`) to wipe `/<mount>/lobbs` on the first install mount every boot and seed demo data. Accounts: `sysop` and `demo01` through `demo12`, password `demo1`. Sample mail, news, wall paint, and yarn text are included for paging tests.

## SysOp commands

SysOp-only lines reply `SysOp only.` to everyone else.

Config keys (uint32, defaults in parentheses): `session.max` (16), `session.idle` (86400 s), `wall.period` (3600), `wall.cells` (1), `yarn.period` (3600), `yarn.words` (1), `yarn.chars` (32), `pager.ttl` (300), `password.min` (5). Run `/config` with no args to list current values and ranges. `/help config key` explains one setting.

| Command                     | Notes                                                                                                                                                                                                                                                                                           |
| --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/install mount user pass`  | First-time setup. Blank node only; adopts an existing database with SysOp credentials                                                                                                                                                                                                           |
| `/passwd user new confirm`  | Reset a user's password                                                                                                                                                                                                                                                                         |
| `/users kick user`          | Log that user out                                                                                                                                                                                                                                                                               |
| `/config`                   | List settings. `/config key`, `/config key value`, `/config key reset` (SysOp only)                                                                                                                                                                                                             |
| `/users promote user`       | Make them a SysOp                                                                                                                                                                                                                                                                               |
| `/users demote user`        | Remove SysOp (not the last SysOp)                                                                                                                                                                                                                                                               |
| `/mail list user`           | Their inbox                                                                                                                                                                                                                                                                                     |
| `/mail read user N`         | Read their message (does not mark it read)                                                                                                                                                                                                                                                      |
| `/news delete N`            | Delete a news item                                                                                                                                                                                                                                                                              |
| `/time unix`                | Set the clock. The time must be after this firmware was built, or the reply is `Invalid time.`                                                                                                                                                                                                  |
| `/cd [path]`                | Set your working directory (default `/`). Each login keeps its own, reset on re-login/reboot                                                                                                                                                                                                    |
| `/pwd`                      | Show your working directory                                                                                                                                                                                                                                                                     |
| `/ls [path]`                | List a directory, default the working directory (`*` glob on the last name only). Directories end in `/`, files show a size like `9B` or `1.2K`                                                                                                                                                 |
| `/cat path`                 | Read a file as text                                                                                                                                                                                                                                                                             |
| `/hex path`                 | Hex dump a file                                                                                                                                                                                                                                                                                 |
| `/rm /path`                 | Delete a file (absolute path only)                                                                                                                                                                                                                                                              |
| `/rmdir /path`              | Remove an empty directory (absolute path only)                                                                                                                                                                                                                                                  |
| `/rmtree /path /path`       | Recursive delete (absolute path, typed twice)                                                                                                                                                                                                                                                   |
| `/mkdir path`               | Create a directory under a mount (`/flash/...`, etc.)                                                                                                                                                                                                                                           |
| `/cp src dst`               | Copy a file (no overwrite; directories refused)                                                                                                                                                                                                                                                 |
| `/mv src dst`               | Rename on one mount, or copy then delete across mounts (files only)                                                                                                                                                                                                                             |
| `/upload path [offset:b62]` | Chunked file write at byte offset, or show current size                                                                                                                                                                                                                                         |
| `/commit src dst`           | Move when file CRC32 matches the 8-hex segment in the source name                                                                                                                                                                                                                               |
| `/stat path`                | `file N` or `dir`                                                                                                                                                                                                                                                                               |
| `/df`                       | One line per mount, e.g. `extra: 1.8M free of 2.0M (12% used) [lobbs]`. Free space excludes the `/flash` reserve. `[lobbs]` marks the install mount                                                                                                                                             |
| `/format mount [code]`      | Erase a mount. The first call replies with a code; repeat with the code within 2 minutes from the same node. On `/flash` the radio settings are saved again from memory (BLE pairings are lost). On the install mount LoBBS goes blank until `/install`. `sd` is refused (format cards on a PC) |

SysOps painting the wall or adding yarn do not use quota.

## Files

Paths use mount prefixes: `/flash/...`, `/flash2/...` on nRF52840, `/sd/...`, `/extra/...` on builds with `LOBBS_EXTRA_QSPI=1` and a QSPI flash chip. `/` lists mounts. Do not `/rmtree` `<mount>/lobbs` on your install mount unless you intend to wipe the BBS.

`/flash` is shared with Meshtastic's own settings and node database. LoBBS leaves a reserve free there (16 KB on nRF52, 128 KB elsewhere). Posts, mail, uploads, and copies that would cut into it reply `Disk full.`. Deleting files always works. `/flash2`, `/sd` and `/extra` have no reserve.

Relative paths work under your session working directory for `ls`, `cat`, `hex`, `stat`, `mkdir`, `cp`, `mv`, `upload`, and `commit`. `rm`, `rmdir`, and `rmtree` require absolute paths.

## Chunked upload

`/upload path offset:b62` writes base62-decoded bytes at `offset`. `/upload path` with no offset replies with the current size. `/commit src dst` verifies CRC32 from the source filename before replacing the destination.

From the firmware repo root:

```bash
python src/modules/LoBBS/bin/lobbs_chunkify.py local.txt /flash/path/dest.txt
```

The script prints `/mkdir`, `/upload`, and `/commit` lines. Temp files use `<name>.<crc8>.tmp`.
