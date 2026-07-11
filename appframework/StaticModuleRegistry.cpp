#include "appframework/StaticModuleRegistry.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

namespace
{
const size_t kMaxStaticModules = 128;
const size_t kMaxModuleName = 64;

struct StaticModuleEntry
{
    char name[kMaxModuleName];
    CreateInterfaceFn factory;
};

StaticModuleEntry g_StaticModules[kMaxStaticModules];
size_t g_StaticModuleCount;

bool NormalizeModuleName( const char *input, char *output )
{
    if ( !input || !input[0] )
        return false;

    const char *base = input;
    for ( const char *cursor = input; *cursor; ++cursor )
    {
        if ( *cursor == '/' || *cursor == '\\' )
            base = cursor + 1;
    }

    size_t length = 0;
    while ( base[length] && base[length] != '.' && length + 1 < kMaxModuleName )
    {
        output[length] = (char)tolower( (unsigned char)base[length] );
        ++length;
    }
    if ( base[length] && base[length] != '.' )
        return false;
    output[length] = '\0';
    const char clientSuffix[] = "_client";
    const size_t suffixLength = sizeof( clientSuffix ) - 1;
    if ( length > suffixLength && strcmp( output + length - suffixLength, clientSuffix ) == 0 )
    {
        length -= suffixLength;
        output[length] = '\0';
    }
    return length != 0;
}
}

bool RegisterStaticModule( const char *pModuleName, CreateInterfaceFn factory )
{
    char normalized[kMaxModuleName];
    if ( !factory || !NormalizeModuleName( pModuleName, normalized ) )
        return false;

    for ( size_t i = 0; i < g_StaticModuleCount; ++i )
    {
        if ( strcmp( normalized, g_StaticModules[i].name ) == 0 )
            return g_StaticModules[i].factory == factory;
    }
    if ( g_StaticModuleCount == kMaxStaticModules )
        return false;

    StaticModuleEntry &entry = g_StaticModules[g_StaticModuleCount++];
    memcpy( entry.name, normalized, strlen( normalized ) + 1 );
    entry.factory = factory;
    return true;
}

CreateInterfaceFn FindStaticModuleFactory( const char *pModuleName )
{
    char normalized[kMaxModuleName];
    if ( !NormalizeModuleName( pModuleName, normalized ) )
        return NULL;

    for ( size_t i = 0; i < g_StaticModuleCount; ++i )
    {
        if ( strcmp( normalized, g_StaticModules[i].name ) == 0 )
            return g_StaticModules[i].factory;
    }
    return NULL;
}

void ClearStaticModuleRegistryForTesting()
{
    g_StaticModuleCount = 0;
}
