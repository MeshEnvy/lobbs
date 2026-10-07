#pragma once
#include "core/LoBBSArch.h"

#if defined(LOFS_NRF52)
class Adafruit_LittleFS;
void lofsMeshcoreSetHostFilesystem(Adafruit_LittleFS &filesystem);
#elif LOBBS_ARCH_ESP32
namespace fs {
class SPIFFSFS;
}
void lofsMeshcoreSetHostFilesystem(fs::SPIFFSFS &filesystem);
#elif LOBBS_ARCH_RP2040
class LittleFSFS;
void lofsMeshcoreSetHostFilesystem(LittleFSFS &filesystem);
#else
void lofsMeshcoreSetHostFilesystem(void *filesystem);
#endif
