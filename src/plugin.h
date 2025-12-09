#pragma once

#define LOBBS_VERSION "1.2.1"
#define LOBBS_HEADER "LoBBS v" LOBBS_VERSION "\nCommands:\n"

#include "LoBBSModule.h"

/**
 * This plugin registers a Module with Meshtastic. Modules are the primary way
 * to extend Meshtastic with new functionality. Modules are registered with the
 * MPM_REGISTER_MESHTASTIC_MODULE comment directive below.
 */
// MPM_REGISTER_MESHTASTIC_MODULE: LoBBSModule, lobbsModule, []

