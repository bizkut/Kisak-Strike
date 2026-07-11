#include "ps4_shader_manifest.h"

#include <string.h>
#include <stdio.h>

namespace
{
bool KeyMatches( const Ps4ShaderManifestEntry &entry, const Ps4ShaderManifestKey &key )
{
    return key.name && strcmp( entry.name, key.name ) == 0 &&
        entry.stage == key.stage && entry.staticCombo == key.staticCombo &&
        entry.dynamicCombo == key.dynamicCombo && entry.vertexFormat == key.vertexFormat;
}
}

CPs4ShaderManifest::CPs4ShaderManifest() : m_count( 0 )
{
    memset( m_entries, 0, sizeof( m_entries ) );
}

void CPs4ShaderManifest::Clear()
{
    memset( m_entries, 0, sizeof( m_entries ) );
    m_count = 0;
}

bool CPs4ShaderManifest::Register( const Ps4ShaderManifestKey &key, const char *path )
{
    if ( !key.name || !key.name[0] || !path || !path[0] ||
        strlen( key.name ) >= sizeof( m_entries[0].name ) ||
        strlen( path ) >= sizeof( m_entries[0].path ) || m_count >= kMaxEntries )
        return false;
    if ( Find( key ) )
        return false;

    Ps4ShaderManifestEntry &entry = m_entries[m_count++];
    strcpy( entry.name, key.name );
    strcpy( entry.path, path );
    entry.stage = key.stage;
    entry.staticCombo = key.staticCombo;
    entry.dynamicCombo = key.dynamicCombo;
    entry.vertexFormat = key.vertexFormat;
    return true;
}

const Ps4ShaderManifestEntry *CPs4ShaderManifest::Find( const Ps4ShaderManifestKey &key ) const
{
    for ( size_t i = 0; i < m_count; ++i )
    {
        if ( KeyMatches( m_entries[i], key ) )
            return &m_entries[i];
    }
    return 0;
}

bool CPs4ShaderManifest::LoadText( const char *text, size_t length )
{
    Clear();
    if ( !text || !length )
        return false;

    size_t offset = 0;
    while ( offset < length )
    {
        size_t end = offset;
        while ( end < length && text[end] != '\n' )
            ++end;
        size_t lineLength = end - offset;
        if ( lineLength && text[offset + lineLength - 1] == '\r' )
            --lineLength;
        if ( lineLength && text[offset] != '#' )
        {
            char line[384];
            char name[64];
            char stageName[16];
            char path[128];
            unsigned int staticCombo = 0;
            unsigned int dynamicCombo = 0;
            unsigned long long vertexFormat = 0;
            if ( lineLength >= sizeof( line ) )
            {
                Clear();
                return false;
            }
            memcpy( line, text + offset, lineLength );
            line[lineLength] = 0;
            if ( sscanf( line, "%63[^|]|%15[^|]|%u|%u|%llu|%127[^\r\n]",
                    name, stageName, &staticCombo, &dynamicCombo, &vertexFormat, path ) != 6 )
            {
                Clear();
                return false;
            }
            Ps4ShaderStage stage;
            if ( strcmp( stageName, "vertex" ) == 0 )
                stage = PS4_SHADER_STAGE_VERTEX;
            else if ( strcmp( stageName, "pixel" ) == 0 )
                stage = PS4_SHADER_STAGE_PIXEL;
            else
            {
                Clear();
                return false;
            }
            const Ps4ShaderManifestKey key = {
                name, stage, staticCombo, dynamicCombo,
                static_cast< uint64_t >( vertexFormat )
            };
            if ( !Register( key, path ) )
            {
                Clear();
                return false;
            }
        }
        offset = end < length ? end + 1 : end;
    }
    return m_count != 0;
}
