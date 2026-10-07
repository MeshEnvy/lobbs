# LoBBS

On-firmware bulletin board for **Meshtastic** and **MeshCore**. This repo is the portable LoBBS plugin, a PlatformIO library: core, apps, storage, and both platform providers. It is being extracted from the Meshtastic fork.

## Firmware forks

| Repo | Branch |
| --- | --- |
| [MeshEnvy/lobbs-meshtastic-firmware](https://github.com/MeshEnvy/lobbs-meshtastic-firmware) | `lobbs` |
| [MeshEnvy/lobbs-meshcore-firmware](https://github.com/MeshEnvy/lobbs-meshcore-firmware) | `lobbs` |

Each fork builds standalone. Its `variants/lobbs/` config pins this plugin by commit in `lib_deps`, so cloning one fork is enough to build it.

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

The forks stay upstream plus one new folder: `variants/lobbs/`, which defines the `*_lobbs` build envs and pulls in the plugin. No edits to upstream files. If an extension point is missing, the fix is the smallest possible change, listed here and paired with an upstream PR.

Meshtastic integration lives in `lobbs-meshtastic-firmware` (`lobbs-overrides.ini`, `variants/lobbs/`). This repo is the plugin source.

Architecture and greenfield rules: [`.cursor/rules/lobbs-project.mdc`](.cursor/rules/lobbs-project.mdc).
