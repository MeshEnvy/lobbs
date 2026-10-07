#if defined(LOBBS_PLATFORM_MESHCORE)
#include "core/LoBBSArch.h"
#include "LoFSMountsMeshcore.h"
#include "lofs/volumes/LoFSVolume.h"
#include "lofs/volumes/LoFSLittleFsVolume.h"
#include "lofs/volumes/LoFSNrfFlashBlockDevice.h"
#include "lofs/volumes/LoFSQspiNorBlockDevice.h"
#include "core/LoBBSConfig.h"
#if LOBBS_ARCH_ESP32
#include "lofs/volumes/LoFSArduinoFsVolume.h"
#include <FS.h>
#endif

#if LOBBS_ARCH_RP2040
#include "lofs/volumes/LoFSArduinoFsVolume.h"
#include <LittleFS.h>
#endif

#if defined(LOFS_NRF52)
#include <Adafruit_LittleFS.h>
extern "C" uint8_t __flash3_start[] __attribute__((weak));
extern "C" uint8_t __flash3_end[] __attribute__((weak));
#if defined(EXTRAFS) && !defined(QSPIFLASH)
#include <CustomLFS.h>
extern CustomLFS ExtraFS;
#endif
#if defined(QSPIFLASH)
#include <CustomLFS_QSPIFlash.h>
extern CustomLFS_QSPIFlash QSPIFlash;
#endif
#endif

#if LOBBS_ARCH_ESP32
#include <SPIFFS.h>
#endif

namespace {

#if defined(LOFS_NRF52)
// Reserve window: same tuning as Meshtastic (128 KiB cap, lock host FS during raw flash, page erase).
LoFSNrfFlashBlockDevice gReserveDevice(__flash3_start, __flash3_end, 128, true, true, LOBBS_NRF52_FLASH_PAGE_SIZE);
LoFSLittleFsVolume gReserve(gReserveDevice);
LoFSLittleFsVolume gInternal;
LoFSLittleFsVolume gSpare;
#if defined(QSPIFLASH)
LoFSLittleFsVolume gQspi;
#else
LoFSQspiNorBlockDevice gQspiDevice;
LoFSLittleFsVolume gQspi(gQspiDevice);
#endif
#endif

#if LOBBS_ARCH_ESP32
LoFSArduinoFsVolume<fs::SPIFFSFS> gInternal;
#endif

#if LOBBS_ARCH_RP2040
LoFSArduinoFsVolume<LittleFSFS> gInternal;
#endif

} // namespace

#if defined(LOFS_NRF52)
void lofsMeshcoreSetHostFilesystem(Adafruit_LittleFS &filesystem)
{
    gInternal.bind(filesystem);
}
#elif LOBBS_ARCH_ESP32
void lofsMeshcoreSetHostFilesystem(fs::SPIFFSFS &filesystem)
{
    gInternal.bind(filesystem);
}
#elif LOBBS_ARCH_RP2040
void lofsMeshcoreSetHostFilesystem(LittleFSFS &filesystem)
{
    gInternal.bind(filesystem);
}
#else
void lofsMeshcoreSetHostFilesystem(void *filesystem)
{
    (void)filesystem;
}
#endif

void lofsPlatformMounts(LoFSMountTable &table)
{
#if defined(LOFS_NRF52)
    if (__flash3_start && __flash3_end && (uintptr_t)__flash3_end > (uintptr_t)__flash3_start)
        table.add({"reserve", &gReserve, false, true});
#if defined(QSPIFLASH)
    gQspi.bind(QSPIFlash);
    table.add({"qspi", &gQspi, false, true});
#elif LOFS_BOARD_HAS_QSPI
    table.add({"qspi", &gQspi, false, true});
#endif
#if defined(EXTRAFS) && !defined(QSPIFLASH)
    gSpare.bind(ExtraFS);
    table.add({"spare", &gSpare, true, true});
#endif
    table.add({"internal", &gInternal, true, false});
#elif LOBBS_ARCH_ESP32
    table.add({"internal", &gInternal, true, false});
#elif LOBBS_ARCH_RP2040
    table.add({"internal", &gInternal, true, false});
#endif
}

#include "core/LoBBSStackGuard.h"
#endif
