# LoBBS

On-firmware bulletin board for **Meshtastic** and **MeshCore**: thin firmware forks, portable logic in [`lobbs/`](lobbs/) (when present) and [`lo-star`](lo-star/), with Meshtastic glue in [`src/`](src/) until the fork delegates fully into the library.

## Submodules

| Path | Role |
|------|------|
| [`meshtastic/`](meshtastic/) | Meshtastic fork; track **`develop`** for upstream sync, **`lobbs`** for integrated in-tree BBS (legacy line) |
| [`lo-star/`](lo-star/) | Shared portable helpers |
| [`lotato/`](lotato/) | Reference product layout (Lotato) |

After clone:

```bash
git submodule update --init --recursive
```

Submodule setup, upstream tagging, and release naming: [`lo-star/README.md`](lo-star/README.md).

Architecture and greenfield rules: [`.cursor/rules/lobbs-project.mdc`](.cursor/rules/lobbs-project.mdc).
