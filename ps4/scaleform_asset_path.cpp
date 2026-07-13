#include "ps4/scaleform_asset_path.h"

#include <ctype.h>
#include <string.h>

namespace
{
bool StartsWithNoCase( const char *value, const char *prefix )
{
    while ( *prefix )
    {
        if ( !*value || tolower( static_cast< unsigned char >( *value ) ) !=
             tolower( static_cast< unsigned char >( *prefix ) ) )
            return false;
        ++value;
        ++prefix;
    }
    return true;
}
}

bool KisakPs4NormalizeScaleformAssetUrl( const char *url, char *normalized,
    size_t normalizedCapacity )
{
    if ( !url || !url[0] || !normalized || normalizedCapacity == 0 )
        return false;
    normalized[0] = '\0';

    const char *relative = url;
    if ( StartsWithNoCase( relative, "/app0/" ) )
        relative += 6;
    while ( relative[0] == '.' &&
            ( relative[1] == '/' || relative[1] == '\\' ) )
        relative += 2;
    if ( relative[0] == '/' || relative[0] == '\\' || strstr( relative, "://" ) )
        return false;

    const char prefix[] = "resource/flash/";
    size_t output = 0;
    if ( !StartsWithNoCase( relative, prefix ) )
    {
        if ( sizeof( prefix ) > normalizedCapacity )
            return false;
        memcpy( normalized, prefix, sizeof( prefix ) - 1 );
        output = sizeof( prefix ) - 1;
    }

    unsigned componentLength = 0;
    bool componentOnlyDots = true;
    for ( const char *input = relative; *input && *input != '?' && *input != '#'; ++input )
    {
        char value = *input == '\\' ? '/' : *input;
        if ( value == '/' )
        {
            if ( componentLength == 2 && componentOnlyDots )
                return false;
            componentLength = 0;
            componentOnlyDots = true;
        }
        else
        {
            ++componentLength;
            componentOnlyDots = componentOnlyDots && value == '.';
        }
        if ( output + 1 >= normalizedCapacity )
            return false;
        normalized[output++] = static_cast< char >(
            tolower( static_cast< unsigned char >( value ) ) );
    }
    if ( ( componentLength == 2 && componentOnlyDots ) || output == 0 ||
         normalized[output - 1] == '/' )
        return false;
    normalized[output] = '\0';
    return true;
}
