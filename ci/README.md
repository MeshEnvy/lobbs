# LoBBS CI helpers

Host-side checks in the plugin repo without flashing firmware.

## Host protocol tests

```bash
cd ci/host
pio test -e host -f test_protocol_host
```

Uses `LOBBS_PLATFORM_NATIVE` (not Meshtastic or MeshCore). Build output lives under `ci/host/.pio/` (gitignored).

## Command integration tests

Run from the **Meshtastic fork** (full kernel + LoFS + apps against upstream’s Portduino `native-macos` target):

```bash
cd lobbs-meshtastic-firmware
pio test -e native-macos -f test_lobbs_commands
```

Portduino is part of Meshtastic’s native build. LoBBS does not support it as a standalone platform.

MeshCore firmware builds are verified with `pio run` on a representative env (for example `RAK_4631_terminal_chat`). Bench `/install` and long-reply checks on hardware are manual.
