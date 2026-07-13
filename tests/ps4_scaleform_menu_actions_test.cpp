#include "ps4/scaleform_menu_actions.h"

#include <cassert>
#include <cstring>

int main()
{
    const KisakPs4ScaleformMenuAction direct =
        KisakPs4ScaleformMenuActionForCommand(
            "OpenCreateSinglePlayerGameDialog" );
    assert( direct == kKisakPs4ScaleformMenuActionStartSinglePlayer );
    assert( std::strcmp( KisakPs4ScaleformElementForMenuAction( direct ),
        "StartSinglePlayer" ) == 0 );

    assert( KisakPs4ScaleformMenuActionForCommand(
        "OpenCreateSinglePlayerGameDialog_AcceptNotConnectedToLive" ) == direct );
    assert( KisakPs4ScaleformMenuActionForCommand( "OpenOptionsDialog" ) ==
        kKisakPs4ScaleformMenuActionNone );
    assert( KisakPs4ScaleformMenuActionForCommand( NULL ) ==
        kKisakPs4ScaleformMenuActionNone );
    assert( KisakPs4ScaleformElementForMenuAction(
        kKisakPs4ScaleformMenuActionNone ) == NULL );
    return 0;
}
