#if defined(LOBBS_PLATFORM_MESHTASTIC)
#include "lofs/volumes/LoFSVolume.h"
#include "core/LoBBSBootTrace.h"
#include "core/LoBBSConfig.h"
#include "configuration.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "lofs/volumes/LoFSLittleFsVolume.h"
#include "lofs/volumes/LoFSLoLogVolume.h"
#include "lofs/volumes/LoFSArduinoFsVolume.h"
#include "lofs/volumes/LoFSNrfFlashRawDevice.h"
#include "lofs/volumes/LoFSQspiNorBlockDevice.h"
#include "lofs/volumes/LoFSSdVolume.h"
#include <cstring>

#if defined(LOFS_NRF52)
extern "C" uint8_t __flash2_start[] __attribute__((weak));
extern "C" uint8_t __flash2_end[] __attribute__((weak));
#include "flash/flash_nrf5x.h"
#endif

namespace {

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
LoFSSdVolume gSd;
#endif

#if LOFS_BOARD_HAS_QSPI
LoFSQspiNorBlockDevice gLofsDevice;
LoFSLoLogVolume gLofs(gLofsDevice);
#elif defined(LOFS_NRF52)
LoFSNrfFlashRawDevice gLofsWindowDevice(__flash2_start, __flash2_end);
LoFSLoLogVolume gLofs(gLofsWindowDevice);
#endif

#if defined(LOFS_NRF52)
LoFSLittleFsVolume gInternal(FSCom);
#elif LOBBS_ARCH_ESP32 || LOBBS_ARCH_RP2040
LoFSArduinoFsVolume<decltype(FSCom)> gInternal(FSCom);
#elif LOBBS_ARCH_PORTDUINO
LoFSArduinoFsVolume<decltype(FSCom)> gInternal(FSCom);
#endif

void addSpec(LoFSMountTable &table, const char *name, LoFSVolume *vol, bool shared, bool formattable)
{
    table.add({name, vol, shared, formattable});
}

#if defined(LOFS_NRF52)
static bool flash2Present()
{
    return (uintptr_t)__flash2_end > (uintptr_t)__flash2_start;
}
#endif

} // namespace

void lofsPlatformMounts(LoFSMountTable &table)
{
    LOBBS_BOOT_STEP("LoFS: meshtastic mounts");

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    addSpec(table, "sd", &gSd, false, false);
#endif

#if LOFS_BOARD_HAS_QSPI
    addSpec(table, "lofs", &gLofs, false, true);
#elif defined(LOFS_NRF52)
    if (flash2Present())
        addSpec(table, "lofs", &gLofs, false, true);
#endif

    addSpec(table, "internal", &gInternal, true, true);
}

#include "core/LoBBSStackGuard.h"
#endif
