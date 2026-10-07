#pragma once
#include "LoBBSConfig.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
struct LoBBSCommandCtx;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

enum class LoBBSInstallState : uint8_t { Blank, Ready, Offline };

/** Every LoBBS file lives under `<mount root>/lobbs`: `install.ls`, `db/` (LoDB), `apps/<app>/`, `home/<user>/`. */
static constexpr const char *LOBBS_HOME_DIR = "lobbs";
static constexpr const char *LOBBS_INSTALL_MARKER_NAME = "install.ls";
static constexpr uint32_t LOBBS_INSTALL_FIELD_VERSION = 0;

LoBBSInstallState lobbsInstallState(const LoBBSModule &mod);
/** Mount root of the install (e.g. `/extra`), empty when not Ready. */
const char *lobbsInstallRoot(const LoBBSModule &mod);
/** LoBBS home that failed to open while Offline (e.g. `/extra/lobbs`). */
const char *lobbsInstallOfflineHome(const LoBBSModule &mod);

void lobbsInstallInit(LoBBSModule &mod);
void lobbsInstallDatabaseOpened(LoBBSModule &mod);
/** `install_mounts` names joined with `|`, e.g. `sd|flash2`. */
void lobbsInstallMountList(LoBBSCommandCtx &ctx, char *out, size_t cap);

bool lobbsInstallAuthorized(const meshtastic_MeshPacket &mp);
/** `<root>/lobbs`, e.g. `/extra/lobbs`. */
bool lobbsInstallHome(const char *root, char *out, size_t cap);
/** Writes `<root>/lobbs/install.ls`; its presence marks the install. */
bool lobbsInstallWriteMarker(const char *root);

#if LOBBS_SEED
void lobbsInstallAutoSeed(LoBBSModule &mod);
#endif

void lobbsInstallRegisterCommands();

