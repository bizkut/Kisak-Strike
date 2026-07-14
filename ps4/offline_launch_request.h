#pragma once

#include <stddef.h>

struct KisakPs4OfflineLaunchRequest
{
    char gameType[32];
    char gameMode[32];
    char mapGroup[128];
    int skirmishMode;
    int botDifficulty;
    char rawQuery[2048];
};

bool KisakPs4ParseOfflineLaunchRequest( const char *query, int botDifficulty,
    KisakPs4OfflineLaunchRequest *request );

extern "C" bool KisakPs4SubmitOfflineLaunch(
    const char *query, int botDifficulty );
