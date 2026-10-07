#pragma once

#include <stddef.h>
#include <stdarg.h>

#if defined(LOBBS_PLATFORM_MESHTASTIC) || defined(LOBBS_PLATFORM_MESHCORE)

void lobbsPlatformLogFormat(char *buf, size_t cap, const char *fmt, va_list args);

#endif
