# LoBBS - Firmware-based BBS for Meshtastic

**A Meshtastic firmware plugin providing a full bulletin board system**

------

### Watch the Walkthrough

https://www.youtube.com/watch?v=FwtDY1QBXpQ

[![LoBBS Walkthrough](https://img.youtube.com/vi/FwtDY1QBXpQ/0.jpg)](https://www.youtube.com/watch?v=FwtDY1QBXpQ)

-------

LoBBS is a Meshtastic plugin that runs a complete bulletin board system entirely inside the Meshtastic firmware. Once installed and built into your node, you can create user accounts, exchange private mail, broadcast news posts, and remotely administer the device without any sidecar services or host computer.

## Features

- **User directory** with username registration and secure password storage
- **Private mail inbox** with paging, read receipts, and inline `@mention` delivery
- **News feed** with threaded announcements and per-user read tracking
- Session-aware command parser with **contextual help**
- Backed by [LoDB](https://github.com/MeshEnvy/lodb) for on-device storage so the entire BBS persists across reboots

## Installation

### Using Mesh Forge (easy)

Use our [Mesh Forge build profile](https://meshforge.org/builds/new/?plugin=lobbs) to flash a LoBBS-enabled version of Meshtastic to your device.

### Build it yourself (experimental)

LoBBS is a [Meshtastic plugin](https://meshforge.org/plugins) that is automatically discovered and integrated by the [Mesh Plugin Manager](https://pypi.org/project/mesh-plugin-manager/) (MPM). To install LoBBS:

1. **Install the Mesh Plugin Manager:**

```bash
pip install mesh-plugin-manager
```

2. **Install LoBBS and its dependencies:**

```bash
cd /path/to/meshtastic/firmware
mpm init
mpm install lobbs
```

> **Note:** `mpm` automatically installs dependencies such as [LoDB](https://github.com/MeshEnvy/lodb) which is required by LoBBS.

3. **Build and flash:**

The Mesh Plugin Manager automatically discovers both plugins, generates protobuf files, and integrates them into the build. Simply build and flash as usual:

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

## License

LoBBS is distributed under the MIT license. See the accompanying `LICENSE` file within this module for full text. While LoBBS is MIT, it must be combined with Meshtastic source code which is GPL. You must therefore follow all GPL guidelines regarding the combined source and binary distributions of Meshtastic. The LoBBS source code may be distributed independently under MIT.

## Disclaimer

LoBBS and MeshForge are independent projects not endorsed by or affiliated with the Meshtastic organization.
