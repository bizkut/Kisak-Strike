#ifndef KISAK_PS4_SCALEFORM_MENU_ACTIONS_H
#define KISAK_PS4_SCALEFORM_MENU_ACTIONS_H

#include <string.h>

enum KisakPs4ScaleformMenuAction
{
    kKisakPs4ScaleformMenuActionNone = 0,
    kKisakPs4ScaleformMenuActionStartSinglePlayer
};

inline KisakPs4ScaleformMenuAction KisakPs4ScaleformMenuActionForCommand(
    const char *command )
{
    if ( command &&
         ( strcmp( command, "OpenCreateSinglePlayerGameDialog" ) == 0 ||
           strcmp( command,
               "OpenCreateSinglePlayerGameDialog_AcceptNotConnectedToLive" ) == 0 ) )
    {
        return kKisakPs4ScaleformMenuActionStartSinglePlayer;
    }
    return kKisakPs4ScaleformMenuActionNone;
}

inline const char *KisakPs4ScaleformElementForMenuAction(
    KisakPs4ScaleformMenuAction action )
{
    return action == kKisakPs4ScaleformMenuActionStartSinglePlayer
        ? "StartSinglePlayer" : NULL;
}

#endif
