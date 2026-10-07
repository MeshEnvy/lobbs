#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSWireup.h"
#include "LoBBSHooks.h"
#include "LoBBSInstall.h"
#include "apps/Auth/AuthCommands.h"
#include "apps/Config/ConfigCommands.h"
#include "apps/Fs/FsCommands.h"
#include "apps/Help/HelpCommands.h"
#include "apps/Mail/MailCommands.h"
#include "apps/Msg/MsgCommon.h"
#include "apps/News/NewsCommands.h"
#include "apps/Status/StatusCommands.h"
#include "apps/Time/TimeCommands.h"
#include "apps/Wall/WallCommands.h"
#include "apps/Yarn/YarnCommands.h"

#include "LoBBSStackGuard.h"

void lobbsWireup()
{
    lobbsHooksReset();
    lobbsMsgRegisterDisplay();
    lobbsInstallRegisterCommands();
    lobbsConfigRegisterCommands();
    lobbsHelpRegisterCommands();
    lobbsAuthRegisterCommands();
    lobbsMailRegisterCommands();
    lobbsNewsRegisterCommands();
    lobbsYarnRegisterCommands();
    lobbsWallRegisterCommands();
    lobbsStatusRegisterCommands();
    lobbsTimeRegisterCommands();
    lobbsFsRegisterCommands();
}

#endif
