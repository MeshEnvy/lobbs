#if defined(LOBBS_PLATFORM_NATIVE)
#include "lofs/volumes/LoFSVolume.h"

void lofsPlatformMounts(LoFSMountTable &table)
{
    (void)table;
}

#include "core/LoBBSStackGuard.h"
#endif
