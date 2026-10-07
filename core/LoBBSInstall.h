#pragma once
#include "LoBBSConfig.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSKernel;
struct LoBBSCommandCtx;

enum class LoBBSInstallState : uint8_t { Blank, Ready, Offline };

/** Every LoBBS file lives under `<mount root>/lobbs`: `install.ls`, `db/` (LoDB), `apps/<app>/`, `home/<user>/`. */
static constexpr const char *LOBBS_HOME_DIR = "lobbs";
static constexpr const char *LOBBS_INSTALL_MARKER_NAME = "install.ls";
static constexpr uint32_t LOBBS_INSTALL_FIELD_VERSION = 0;

LoBBSInstallState lobbsInstallState(const LoBBSKernel &kernel);
/** Mount root of the install (e.g. `/extra`), empty when not Ready. */
const char *lobbsInstallRoot(const LoBBSKernel &kernel);
/** LoBBS home that failed to open while Offline (e.g. `/extra/lobbs`). */
const char *lobbsInstallOfflineHome(const LoBBSKernel &kernel);

void lobbsInstallInit(LoBBSKernel &kernel);
void lobbsInstallDatabaseOpened(LoBBSKernel &kernel);
/** `install_mounts` names joined with `|`, e.g. `sd|qspi|reserve|spare|internal`. */
void lobbsInstallMountList(LoBBSCommandCtx &ctx, char *out, size_t cap);

/** `<root>/lobbs`, e.g. `/extra/lobbs`. */
bool lobbsInstallHome(const char *root, char *out, size_t cap);
/** Writes `<root>/lobbs/install.ls`; its presence marks the install. */
bool lobbsInstallWriteMarker(const char *root);

#if LOBBS_SEED
void lobbsInstallAutoSeed(LoBBSKernel &kernel);
#endif

void lobbsInstallRegisterCommands();

