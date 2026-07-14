#include "ps4/offline_launch_request.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace
{
const char *SkipWhitespace( const char *cursor, const char *end )
{
    while ( cursor < end && isspace( static_cast< unsigned char >( *cursor ) ) )
        ++cursor;
    return cursor;
}

bool FindSection( const char *query, const char *name,
    const char **sectionBegin, const char **sectionEnd )
{
    if ( !query || !name || !sectionBegin || !sectionEnd )
        return false;
    const size_t nameLength = strlen( name );
    const char *cursor = query;
    while ( ( cursor = strstr( cursor, name ) ) != NULL )
    {
        const bool leftBoundary = cursor == query ||
            isspace( static_cast< unsigned char >( cursor[-1] ) ) || cursor[-1] == '}';
        const char *afterName = cursor + nameLength;
        const bool rightBoundary = *afterName == '\0' ||
            isspace( static_cast< unsigned char >( *afterName ) ) || *afterName == '{';
        if ( leftBoundary && rightBoundary )
        {
            while ( *afterName && isspace( static_cast< unsigned char >( *afterName ) ) )
                ++afterName;
            if ( *afterName != '{' )
                return false;
            const char *body = afterName + 1;
            const char *close = strchr( body, '}' );
            if ( !close )
                return false;
            *sectionBegin = body;
            *sectionEnd = close;
            return true;
        }
        cursor += nameLength;
    }
    return false;
}

bool FindValue( const char *begin, const char *end, const char *key,
    char *value, size_t valueSize )
{
    if ( !begin || !end || begin >= end || !key || !value || valueSize == 0 )
        return false;
    const size_t keyLength = strlen( key );
    const char *cursor = begin;
    while ( cursor < end )
    {
        cursor = SkipWhitespace( cursor, end );
        const char *tokenBegin = cursor;
        while ( cursor < end && !isspace( static_cast< unsigned char >( *cursor ) ) )
            ++cursor;
        if ( static_cast< size_t >( cursor - tokenBegin ) == keyLength &&
             strncmp( tokenBegin, key, keyLength ) == 0 )
        {
            cursor = SkipWhitespace( cursor, end );
            const char *valueBegin = cursor;
            while ( cursor < end && !isspace( static_cast< unsigned char >( *cursor ) ) )
                ++cursor;
            const size_t length = static_cast< size_t >( cursor - valueBegin );
            if ( length == 0 || length >= valueSize )
                return false;
            memcpy( value, valueBegin, length );
            value[length] = '\0';
            return true;
        }
    }
    return false;
}

void CopyBounded( char *destination, size_t destinationSize, const char *source )
{
    if ( !destination || destinationSize == 0 )
        return;
    if ( !source )
        source = "";
    const size_t length = strlen( source );
    const size_t copyLength = length < destinationSize - 1
        ? length : destinationSize - 1;
    memcpy( destination, source, copyLength );
    destination[copyLength] = '\0';
}
}

bool KisakPs4ParseOfflineLaunchRequest( const char *query, int botDifficulty,
    KisakPs4OfflineLaunchRequest *request )
{
    // GameModes.txt defines six console-facing choices (0 through 5),
    // including harmless/dumb presets before the normal difficulty levels.
    if ( !query || !request || botDifficulty < 0 || botDifficulty > 5 )
        return false;
    memset( request, 0, sizeof( *request ) );

    const char *systemBegin = NULL;
    const char *systemEnd = NULL;
    const char *gameBegin = NULL;
    const char *gameEnd = NULL;
    const char *optionsBegin = NULL;
    const char *optionsEnd = NULL;
    char network[32];
    char action[32];
    char skirmish[32];
    if ( !FindSection( query, "System", &systemBegin, &systemEnd ) ||
         !FindSection( query, "Game", &gameBegin, &gameEnd ) ||
         !FindSection( query, "Options", &optionsBegin, &optionsEnd ) ||
         !FindValue( systemBegin, systemEnd, "network", network, sizeof( network ) ) ||
         !FindValue( gameBegin, gameEnd, "type", request->gameType,
             sizeof( request->gameType ) ) ||
         !FindValue( gameBegin, gameEnd, "mode", request->gameMode,
             sizeof( request->gameMode ) ) ||
         !FindValue( gameBegin, gameEnd, "mapgroupname", request->mapGroup,
             sizeof( request->mapGroup ) ) ||
         !FindValue( optionsBegin, optionsEnd, "action", action, sizeof( action ) ) )
    {
        return false;
    }
    if ( strcmp( network, "offline" ) != 0 || strcmp( action, "create" ) != 0 )
        return false;

    // Normal map groups omit skirmishmode. It is only present for a skirmish
    // selection, and Source treats the absent setting as zero.
    request->skirmishMode = FindValue( gameBegin, gameEnd, "skirmishmode",
        skirmish, sizeof( skirmish ) ) ? atoi( skirmish ) : 0;
    request->botDifficulty = botDifficulty;
    CopyBounded( request->rawQuery, sizeof( request->rawQuery ), query );
    return true;
}
