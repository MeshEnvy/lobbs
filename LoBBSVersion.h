#pragma once

// LoBBS semver comes from version.properties [LOBBS], injected like APP_VERSION in bin/platformio-custom.py

#ifndef LOBBS_VERSION_SHORT
#error LOBBS_VERSION_SHORT must be set by the build environment
#endif

#ifndef LOBBS_VERSION
#error LOBBS_VERSION must be set by the build environment
#endif
