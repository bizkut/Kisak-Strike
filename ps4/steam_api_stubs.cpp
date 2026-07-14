#include "steam/steam_api.h"
#include "steam/steam_api_internal.h"

// Offline PS4 builds deliberately do not ship Steam. These ABI-compatible
// entry points keep legacy callback helpers inert until a platform-neutral
// community-server authentication design is implemented.
bool S_CALLTYPE SteamAPI_RestartAppIfNecessary( uint32 )
{
    return false;
}

HSteamUser SteamAPI_GetHSteamUser()
{
    return 0;
}

HSteamPipe SteamAPI_GetHSteamPipe()
{
    return 0;
}

void *S_CALLTYPE SteamInternal_CreateInterface( const char * )
{
    return NULL;
}

void S_CALLTYPE SteamAPI_RegisterCallback( CCallbackBase *, int )
{
}

void S_CALLTYPE SteamAPI_UnregisterCallback( CCallbackBase * )
{
}

void S_CALLTYPE SteamAPI_RegisterCallResult( CCallbackBase *, SteamAPICall_t )
{
}

void S_CALLTYPE SteamAPI_UnregisterCallResult( CCallbackBase *, SteamAPICall_t )
{
}
