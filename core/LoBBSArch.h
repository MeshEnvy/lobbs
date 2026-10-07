#pragma once

#if defined(ARCH_NRF52) || defined(NRF52_PLATFORM) || defined(ARDUINO_ARCH_NRF52)
#define LOBBS_ARCH_NRF52 1
#endif

#if defined(ARCH_ESP32) || defined(ESP32_PLATFORM) || defined(ESP32)
#define LOBBS_ARCH_ESP32 1
#endif

#if defined(ARCH_RP2040) || defined(RP2040_PLATFORM) || defined(RP2040)
#define LOBBS_ARCH_RP2040 1
#endif

#if defined(ARCH_PORTDUINO)
#define LOBBS_ARCH_PORTDUINO 1
#endif
