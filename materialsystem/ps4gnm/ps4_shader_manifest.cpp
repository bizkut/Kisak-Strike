#include "ps4_shader_manifest.h"

#include <string.h>

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
