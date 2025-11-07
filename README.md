# LoBBS - Meshtastic BBS on the Firmware

LoBBS is a full bulletin board system that runs entirely inside the Meshtastic firmware. Once the module is built into your node you can create user accounts, exchange private mail, broadcast news posts, and remotely administer the device without any sidecar services or host computer.

## Features

- **User directory** with username registration and secure password storage
- **Private mail inbox** with paging, read receipts, and inline `@mention` delivery
- **News feed** with threaded announcements and per-user read tracking
- Session-aware command parser with **contextual help**
- Backed by [LoDB](https://github.com/MeshEnvy/lodb) for on-device storage so the entire BBS persists across reboots

## Prerequisites

- A working Meshtastic firmware checkout with a build environment configured for your target board
- Python environment compatible with PlatformIO (used for the existing Meshtastic build pipeline)
- Git access to clone the LoBBS and LoDB submodules

## Installation

**Step 1: Vendor LoBBS and LoDB**

LoBBS ships as a firmware module and depends on LoDB for its embedded database layer. Add both submodules to your firmware tree and pull their contents:

```bash
git submodule add git@github.com/MeshEnvy/lobbs.git src/modules/LoBBSModule
git submodule add git@github.com/MeshEnvy/lodb.git src/lodb
git submodule update --init --recursive
```

**Step 2: Wire LoDB code generation into PlatformIO**

The protobuf definitions in `lobbs.proto` must be regenerated whenever they change. Append the following helper to `bin/platformio-custom.py` so PlatformIO can discover the generator provided by LoDB:

```python
# Ensure LoDB helper utilities are available when LoDB is vendored as a submodule
lodb_helper_path = join(env["PROJECT_DIR"], "src", "lodb") if env else None
if lodb_helper_path and lodb_helper_path not in sys.path:
    sys.path.append(lodb_helper_path)

try:
    from gen_proto import register_protobufs  # type: ignore
    register_protobufs(
        env,
        "$BUILD_DIR/src/modules/LoBBSModule/LoBBSModule.cpp.o",
        "$PROJECT_DIR/src/modules/LoBBSModule/lobbs.proto",
    )
except Exception:
    print("Warning: gen_proto utilities not found; protobufs will not auto-regenerate")
```

**Step 3: Build and flash**

Use the standard Meshtastic PlatformIO targets to compile and upload. The example below targets an `esp32` environment; adjust the environment name to match your board:

```bash
pio run -e esp32 -t upload
```

After flashing, reboot the node. LoBBS registers automatically through `Modules.cpp`, so no additional firmware configuration is required.

## Using LoBBS

- **Joining the BBS** — Send a direct message to your node containing `/hi <username> <password>`. The command logs you in if the account exists or creates a new account if it does not.
- **Logging out** — Use `/bye` to terminate the current session and clear the binding between your node ID and account.
- **Mail** — `/mail` lists the 10 most recent messages, `/mail 3` reads message 3, and `/mail 5-` starts the listing at item 5. Mention another user in any authenticated message using `@username` to deliver instant mail.
- **News** — `/news` mirrors the mail workflow for public announcements. Append a message after the command (for example `/news Hello mesh!`) to post a new item.
- **User discovery** — `/users` returns the directory. Supply an optional filter string (e.g. `/users mesh`) to narrow the results.

LoBBS replies inline with human-readable summaries. Unread content is flagged with an asterisk in list views, and relative timestamps (for example, `2h ago`) provide context for each entry.

## Storage Layout

All user, mail, and news data is persisted via LoDB in the device filesystem. Clearing the filesystem, reflashing without preserving SPIFFS/LittleFS, or performing a full factory reset will delete the BBS contents. Regular backups of the filesystem are recommended for production deployments.

## Troubleshooting

- Ensure the LoDB generator is reachable; if protobufs fail to regenerate you will see the warning printed during the PlatformIO build.
- Verify your node clock is roughly correct. Timestamps in mail and news rely on the RTC and GPS time sources provided by the firmware.
- Confirm that your node stays logged in (no `/bye` issued) if you expect to receive `@mentions`. Unauthenticated nodes receive only the login help banner.

## License

LoBBS is distributed under the MIT license. See the accompanying `LICENSE` file within this module for full text.
