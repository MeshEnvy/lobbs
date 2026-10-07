# LoBBS

On-firmware bulletin board for **Meshtastic** and **MeshCore**. This umbrella repo holds the firmware forks as submodules. The portable LoBBS plugin (core, apps, storage, and both platform providers) will live in `lobbs/` once it is extracted from the Meshtastic fork.

## Submodules

| Path | Repo | Branch |
| --- | --- | --- |
| [`meshtastic/`](meshtastic/) | [MeshEnvy/lobbs-meshtastic-firmware](https://github.com/MeshEnvy/lobbs-meshtastic-firmware) | `lobbs` |
| [`meshcore/`](meshcore/) | [MeshEnvy/lobbs-meshcore-firmware](https://github.com/MeshEnvy/lobbs-meshcore-firmware) | `lobbs` |

## Setup

```bash
git clone --recurse-submodules git@github.com:MeshEnvy/lobbs.git
# or, in an existing clone:
git submodule update --init --recursive
```

## Workflow

Work inside the submodules. Commit and push there, then commit the pin bump here.

```bash
cd meshtastic && git checkout lobbs && git pull --ff-only
# edit, build, commit, push in the submodule
cd .. && git add meshtastic && git commit -m "chore(meshtastic): bump pin"
```

## Fork diff policy

The forks stay upstream plus one new config file: `variants/lobbs/*.ini`, which defines the `*_lobbs` build envs and pulls in the plugin. No edits to upstream files. If an extension point is missing, the fix is the smallest possible change, listed here and paired with an upstream PR.

Until extraction finishes, LoBBS code still lives in-tree at `meshtastic/src/modules/LoBBS/`.

Architecture and greenfield rules: [`.cursor/rules/lobbs-project.mdc`](.cursor/rules/lobbs-project.mdc).
