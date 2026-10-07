# LoBBS

On-firmware bulletin board for **Meshtastic** and **MeshCore**. This repo is the portable LoBBS plugin, a PlatformIO library: core, apps, storage, and both platform providers. It is being extracted from the Meshtastic fork.

## Firmware forks

| Repo | Branch |
| --- | --- |
| [MeshEnvy/lobbs-meshtastic-firmware](https://github.com/MeshEnvy/lobbs-meshtastic-firmware) | `lobbs` |
| [MeshEnvy/lobbs-meshcore-firmware](https://github.com/MeshEnvy/lobbs-meshcore-firmware) | `lobbs` |

Each fork builds standalone. Meshtastic pulls the plugin via `lobbs-overrides.ini` on every env; MeshCore adds `LOBBS_PLATFORM_MESHCORE` and optional example wiring. Committed pins use `lib_deps` by commit hash.

## Local development

Clone the plugin and the forks side by side:

```bash
git clone git@github.com:MeshEnvy/lobbs.git
git clone -b lobbs git@github.com:MeshEnvy/lobbs-meshtastic-firmware.git
git clone -b lobbs git@github.com:MeshEnvy/lobbs-meshcore-firmware.git
```

Point a fork at your working copy with a gitignored override that sets `lib_deps` to `symlink://../lobbs`. Committed config always carries the real pin. Details: [`.cursor/rules/release.mdc`](.cursor/rules/release.mdc).

## Releases

The plugin is tagged `vX.Y.Z`. Each fork release is tagged `lobbs-vX.Y.Z.<lobbs sha7>-<platform>-v<upstream version>.<upstream sha7>`, for example `lobbs-v2.1.0.abc1234-meshtastic-v2.7.15.567b8ea`. Full procedure: [`.cursor/rules/release.mdc`](.cursor/rules/release.mdc).

## Fork diff policy

Meshtastic: `lobbs-overrides.ini` plus minimal upstream hooks (`Modules.cpp`, linker scripts for extra flash). MeshCore: `lobbs-overrides.ini`, `-Isrc`, and example chat integration. No broad upstream edits. If an extension point is missing, the fix is the smallest possible change, listed in the fork README and paired with an upstream PR when feasible.

This repo is the plugin source; forks pin it by commit.

## Platforms

LoBBS ships for **Meshtastic** and **MeshCore** only (`LOBBS_PLATFORM_MESHTASTIC`, `LOBBS_PLATFORM_MESHCORE`). Host-side checks use `LOBBS_PLATFORM_NATIVE` in `ci/host/` (protocol paging tests). Command integration tests run from the Meshtastic fork `native-macos` env, which upstream builds with Portduino. That is Meshtastic’s desktop harness, not a third LoBBS product platform.

## Layout

| Path | Role |
| --- | --- |
| `core/` | Portable BBS kernel (dispatch, hooks, install, reply cache) |
| `platforms/` | Meshtastic and MeshCore PAL, plus native host PAL for CI |
| `lofs/`, `lodb/`, `loscalar/`, `loutil/` | Storage and codecs |
| `apps/` | Bundled BBS features |
| `protocol/` | Human and machine paging |
| `ci/host/` | Plugin-only protocol tests (`pio test -e host`) |
| `tests/` | Headers consumed by the Meshtastic fork native tests |

Architecture and greenfield rules: [`.cursor/rules/lobbs-project.mdc`](.cursor/rules/lobbs-project.mdc).
