#if defined(LOBBS_PLATFORM_MESHTASTIC) || defined(LOBBS_PLATFORM_MESHCORE)
#include "platforms/LoPlatform.h"
#include "platforms/arduino/LoPlatformArduino.h"
#include <Arduino.h>
#include <cstdio>

void lobbsPlatformLogFormat(char *buf, size_t cap, const char *fmt, va_list args)
{
    vsnprintf(buf, cap, fmt, args);
}

uint32_t lobbsPlatformMillis()
{
    return millis();
}

uint32_t lobbsPlatformRandom(uint32_t bound)
{
    if (bound == 0)
        return 0;
    return (uint32_t)random(bound);
}

#endif
