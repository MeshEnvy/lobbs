# LoBBS - Meshtastic BBS on the Firmware

**A Meshtastic firmware plugin providing a full bulletin board system**

LoBBS is a Meshtastic plugin that runs a complete bulletin board system entirely inside the Meshtastic firmware. Once installed and built into your node, you can create user accounts, exchange private mail, broadcast news posts, and remotely administer the device without any sidecar services or host computer.

## Features

- **User directory** with username registration and secure password storage
- **Private mail inbox** with paging, read receipts, and inline `@mention` delivery
- **News feed** with threaded announcements and per-user read tracking
- Session-aware command parser with **contextual help**
- Backed by [LoDB](https://github.com/MeshEnvy/lodb) for on-device storage so the entire BBS persists across reboots

## Installation

LoBBS is a Meshtastic plugin that is automatically discovered and integrated by the Meshtastic Plugin Manager (MPM). To install LoBBS:

1. **Clone LoBBS into the plugins directory:**

```bash
cd /path/to/meshtastic/firmware
git clone https://github.com/MeshEnvy/lobbs.git src/plugins/lobbs
```

2. **Install LoDB dependency:**

LoBBS requires LoDB as a dependency. Install it as a plugin as well:

```bash
git clone https://github.com/MeshEnvy/lodb.git src/plugins/lodb
```

3. **Build and flash:**

The Meshtastic Plugin Manager automatically discovers both plugins, generates protobuf files, and integrates them into the build. Simply build and flash as usual:

```bash
pio run -e esp32 -t upload
```

After flashing, reboot the node. LoBBS registers automatically, so no additional firmware configuration is required.

**Note:** For detailed information about Meshtastic plugin development, see the [Plugin Development Guide](/path/to/meshtastic/src/plugins/README.md).

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

- Verify your node clock is roughly correct. Timestamps in mail and news rely on the RTC and GPS time sources provided by the firmware.
- Confirm that your node stays logged in (no `/bye` issued) if you expect to receive `@mentions`. Unauthenticated nodes receive only the login help banner.
- If protobuf generation fails, ensure both LoBBS and LoDB plugins are properly installed in `src/plugins/` and that the Meshtastic Plugin Manager can discover them.

## License

LoBBS is distributed under the MIT license. See the accompanying `LICENSE` file within this module for full text.
