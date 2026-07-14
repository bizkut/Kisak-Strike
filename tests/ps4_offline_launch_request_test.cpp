#include "ps4/offline_launch_request.h"

#include <assert.h>
#include <string.h>

int main()
{
    const char *query =
        "System { network offline }\n"
        "Game { type classic mode casual mapgroupname mg_de_dust2 skirmishmode 0 }\n"
        "Options { action create anytypemode 0 }\n"
        "Contexts {} Properties {}";
    KisakPs4OfflineLaunchRequest request = {};
    assert( KisakPs4ParseOfflineLaunchRequest( query, 2, &request ) );
    assert( strcmp( request.gameType, "classic" ) == 0 );
    assert( strcmp( request.gameMode, "casual" ) == 0 );
    assert( strcmp( request.mapGroup, "mg_de_dust2" ) == 0 );
    assert( request.skirmishMode == 0 );
    assert( request.botDifficulty == 2 );
    assert( strcmp( request.rawQuery, query ) == 0 );

    assert( !KisakPs4ParseOfflineLaunchRequest(
        "System { network LIVE } Game { type classic mode casual "
        "mapgroupname mg_de_dust2 skirmishmode 0 } Options { action create }",
        1, &request ) );
    assert( !KisakPs4ParseOfflineLaunchRequest(
        "System { network offline } Game { type classic mode casual } "
        "Options { action create }", 1, &request ) );
    assert( !KisakPs4ParseOfflineLaunchRequest( query, 4, &request ) );
    return 0;
}
