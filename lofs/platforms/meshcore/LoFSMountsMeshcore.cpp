#if defined(LOBBS_PLATFORM_MESHCORE)
#include "core/LoBBSArch.h"
#include "LoFSMountsMeshcore.h"
#include "lofs/volumes/LoFSVolume.h"
#include "lofs/volumes/LoFSLittleFsVolume.h"
#include "lofs/volumes/LoFSLoLogVolume.h"
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
#if defined(EXTRAFS) && !defined(QSPIFLASH)
#include <CustomLFS.h>
extern CustomLFS ExtraFS;
#endif
#endif

#if LOBBS_ARCH_ESP32
#include <SPIFFS.h>
#endif

namespace {

#if defined(LOFS_NRF52)
LoFSLittleFsVolume gInternal;
#if defined(EXTRAFS) && !defined(QSPIFLASH)
LoFSLittleFsVolume gExtra;
#endif
#if LOFS_BOARD_HAS_QSPI
LoFSQspiNorBlockDevice gLofsDevice;
LoFSLoLogVolume gLofs(gLofsDevice);
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
#if LOFS_BOARD_HAS_QSPI
    table.add({"lofs", &gLofs, false, true});
#endif
#if defined(EXTRAFS) && !defined(QSPIFLASH)
    gExtra.bind(ExtraFS);
    table.add({"extra", &gExtra, true, true});
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
