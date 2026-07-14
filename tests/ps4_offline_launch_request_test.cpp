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

    const char *actionScriptQuery =
        "System {\n network offline \n}\n"
        "Game {\n type classic\n mode casual\n mapgroupname mg_bomb\n"
        " skirmishmode 0\n}\n"
        "Options {\n action create \n anytypemode 0 \n}\n"
        "Contexts {\n\n}\nProperties {\n\n}\n";
    assert( KisakPs4ParseOfflineLaunchRequest(
        actionScriptQuery, 0, &request ) );
    assert( strcmp( request.gameType, "classic" ) == 0 );
    assert( strcmp( request.gameMode, "casual" ) == 0 );
    assert( strcmp( request.mapGroup, "mg_bomb" ) == 0 );

    const char *normalMapGroupQuery =
        "System { network offline } "
        "Game { type classic mode casual mapgroupname mg_cs_office } "
        "Options { action create } Contexts {} Properties {}";
    assert( KisakPs4ParseOfflineLaunchRequest(
        normalMapGroupQuery, 5, &request ) );
    assert( request.skirmishMode == 0 );
    assert( request.botDifficulty == 5 );

    assert( !KisakPs4ParseOfflineLaunchRequest(
        "System { network LIVE } Game { type classic mode casual "
        "mapgroupname mg_de_dust2 skirmishmode 0 } Options { action create }",
        1, &request ) );
    assert( !KisakPs4ParseOfflineLaunchRequest(
        "System { network offline } Game { type classic mode casual } "
        "Options { action create }", 1, &request ) );
    assert( !KisakPs4ParseOfflineLaunchRequest( query, 6, &request ) );
    return 0;
}
