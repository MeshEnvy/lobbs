#include "LoFSVolume.h"
#include "core/LoBBSArch.h"
#include <cstring>

const char *LoFSVolume::normalizeRel(const char *rel)
{
    if (!rel || rel[0] == '\0')
        return "/";
    return rel;
}

uint32_t lofsPlatformSharedReserve(const char *mountName)
{
    (void)mountName;
#if LOBBS_ARCH_NRF52
    return 16 * 1024;
#elif LOBBS_ARCH_PORTDUINO
    return 0;
#else
    return 128 * 1024;
#endif
}

bool LoFSMountTable::add(const LoFSMountSpec &spec)
{
    if (!spec.name || !spec.volume || count >= LOFS_MAX_MOUNTS)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(specs[i].name, spec.name) == 0) {
            specs[i] = spec;
            return true;
        }
    }
    specs[count++] = spec;
    return true;
}

#include "core/LoBBSStackGuard.h"
