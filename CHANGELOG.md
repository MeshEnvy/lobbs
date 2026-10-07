# Changelog

LoBBS product history for [lobbs-meshtastic-firmware](https://github.com/MeshEnvy/lobbs-meshtastic-firmware). Meshtastic upstream release notes live in [meshtastic/firmware](https://github.com/meshtastic/firmware); the `[VERSION]` section in `version.properties` tracks that base.

LoBBS semver is the `[LOBBS]` section in `version.properties` when present. Builds inject `LOBBS_VERSION` / `LOBBS_VERSION_SHORT` (see repo root `bin/readprops.py`, `bin/platformio-custom.py`). Until LoBBS 2.0.0 ships, dev builds may show a pre-release `[LOBBS]` value on branch `lobbs`.

Release tags use:

`lobbs-{lobbsSemVer}.{lobbsSha7}-meshtastic-{mtSemVer}.{mtSha7}`

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Planned major release **2.0.0** (not tagged yet).

### Changed

- LoFS: mount names are `sd`, `lofs`, `extra`, `internal`. **`/lofs`** is the exclusive LoLog slot (owned QSPI NOR or nRF52840 flash2 window, never both in the mount table). QSPI Meshtastic envs use `nrf52840_s140_v7_qspi.ld` (no flash2 gap; InternalFS still at `0xED000`). MeshCore `/extra` adopts host ExtraFS; QSPI is not adopted as LittleFS. Dropped `qspi`, `spare`, `reserve`, and flash3 mounts. Every LittleFS mount is formattable via `/format`; SD stays refused. Host tests: `ci/host` `test_lolog_host`.

### Added

- Stateless slash CLI: `/[id] command args…`, replies optionally prefixed `<id>`. No menu stack. The last successful reply is cached for `/pN` paging.
- Plugin command IoC: Auth, Mail, News, Help, Status, Wall, Yarn apps register via filters.
- Long replies page with `/p2`, `/p3`, … against the cached last reply, with or without a request id.
- `/wall`: shared 12×12 ASCII canvas, batch paint, erase tokens, paint quota, new/seen on `/status` when logged in.
- `/news`: announcement feed (list, read, post, unread, sysop delete).
- `/yarn`: collaborative word tail and sysop quotas.
- Public `/status` and `/hi`; all-time totals when logged out; unread mail/news when logged in.
- Machine replies echo the request id and mark pages separately: `<id>ok [n:max]` (single-page replies are `<id>ok`).
- SysOp `/cd` and `/pwd`: per-session working directory for `/ls`, `/cat` and `/hex`. `/rm`, `/rmdir` and `/rmtree` take absolute paths only.
- LoFS mount table (`/flash`, optional `/sd` and `/extra`), virtual `/`, `/install` blank-node setup, and SysOp `/cp`, `/mv`, `/mkdir`, `/stat`, `/df`.
- SysOp chunked upload: `/upload` (offset base62 chunks) and CRC-checked `/commit`; host script [bin/lobbs_chunkify.py](bin/lobbs_chunkify.py).
- `/passwd` (self and sysop reset) and `/time` (show Unix time; sysop set clock).
- Logins are held in RAM and end on reboot or after an idle timeout. SysOp `/config` sets `session.max`, `session.idle`, wall/yarn quotas, `pager.ttl`, and `password.min`. `/help config key` explains a setting. Overrides live in one LoDB `config` table. Logging in never writes flash.
- Ignore non-slash DMs and self-originated loopback (`from` = local node).
- nRF52840: LoBBS gets its own 100 KB `/flash2` partition between the app and Meshtastic's filesystem. `/install` offers `flash2` there instead of `/flash`. Install points come from the `install_mounts` filter, in preference order `sd`, `extra`, `flash2`, `flash`.
- Seeed XIAO nRF52840 kit and SenseCAP Solar: `/extra` on 2 MB onboard QSPI (`LoFSQspi`, JEDEC-gated mount).
- All LoBBS files live under `/<mount>/lobbs` (`install.ls`, `db/`, reserved `apps/` and `home/`). Boot finds the install by scanning install mounts for `lobbs/install.ls`; nothing is kept on `/flash`. `/install` on an installed node replies `Already installed at /<mount>.`
- SysOp `/format <mount> [code]` erases any mount but `sd` after a 2-minute confirmation code. `/flash` re-saves radio settings; the install mount leaves LoBBS blank. `/ls` shows `dir/` and file sizes. `/df` reads `extra: 1.8M free of 2.0M (12% used) [lobbs]`.
- LoBBS writes keep a free-space reserve on `/flash` (16 KB on nRF52, 128 KB elsewhere) so Meshtastic can still save its own files. Writes that would cut into it reply `Disk full.`. `/df` shows the reserve.

### Changed

- LoFS: cross-mount `move`, streaming `crc32File`, and `moveIfCrc32Matches`; glob helpers in `lofs/Glob`. FsCommands maps results to CLI replies; upload gap check unchanged in `/upload`.
- Operator role renamed from admin to SysOp in CLI text, help, and user records (`is_sysop` in auth proto, field 4 unchanged).
- LoBBS protos and DALs split into per-app modules; LoDB hardening for nRF52.
- `/login` for sign-in; build-time LoBBS versioning from `version.properties`.
- Topic commands for mail, news, and users (development on branch; superseded by stateless CLI for 2.0.0).

### Removed

- Numbered menu stack and session page buffer from the shipping 2.0.0 line (experimental on branch only).

## [1.3.0] - 2026-10-01

### Added

- LoBBS integrated in the Meshtastic firmware tree (fork of meshtastic/firmware on branch `lobbs`).
- LoBBS enabled on ESP32 builds; nRF52 compatibility and LoFS path fixes.

### Changed

- Canonical product home moved from archived [MeshEnvy/lobbs](https://github.com/MeshEnvy/lobbs) plugin monorepo to this repository.

## [1.2.1] - 2025-12-09

Released from the archived MPM plugin monorepo ([MeshEnvy/lobbs](https://github.com/MeshEnvy/lobbs)).

### Changed

- Updated module registration from `#pragma MPM_MODULE` to `MPM_REGISTER_MESHTASTIC_MODULE` comment directive.
- Renamed module variable from `lobbsPlugin` to `lobbsModule` for consistency.

## [1.2.0] - 2025-12-09

### Added

- Logo assets with logo.pxd and logo.webp files.
- Admin user functionality: first registered user automatically becomes administrator.

### Changed

- Walkthrough video link in README.
- Updated README with improved documentation, license information, and links.
- Replaced manual memory management with LoDb::freeRecords() in LoBBSModule.
- Added extern declaration for lobbsPlugin and updated MPM module pragma.

## [1.1.1] - 2025-12-05

### Changed

- Refactored header structure: removed LoBBS and meta headers, updated include paths, introduced new plugin header.

## [1.1.0] - 2025-12-05

### Added

- MPM plugin compatibility.

### Changed

- Direct messages now properly filter out broadcasts.

## [1.0.1] - 2025-12-05

### Changed

- Enhanced installation documentation with MeshForge method.
- Updated installation instructions for MPM integration.

## [1.0.0] - 2025-11-28

### Added

- Initial release of LoBBS (LoDB Bulletin Board System).
- User registration and authentication.
- Bulletin board messaging system.
- Direct messaging between users.
- Session management.
- User directory and search functionality.

[Unreleased]: https://github.com/MeshEnvy/lobbs-meshtastic-firmware/compare/lobbs-1.3.0.f18d6d6-meshtastic-2.7.26.54e0d8d...lobbs
[1.3.0]: https://github.com/MeshEnvy/lobbs-meshtastic-firmware/releases/tag/lobbs-1.3.0.f18d6d6-meshtastic-2.7.26.54e0d8d
[1.2.1]: https://github.com/MeshEnvy/lobbs/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/MeshEnvy/lobbs/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/MeshEnvy/lobbs/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/MeshEnvy/lobbs/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/MeshEnvy/lobbs/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/MeshEnvy/lobbs/compare/c911244...v1.0.0
