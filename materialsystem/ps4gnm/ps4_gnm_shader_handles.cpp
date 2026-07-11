#include "ps4_gnm_shader_handles.h"

#include <string.h>

namespace
{
Ps4GnmShaderHandle MakeHandle( uint32_t index, uint16_t generation )
{
    return ( static_cast< uint32_t >( generation ) << 16 ) | ( index + 1 );
}
}

CPs4GnmShaderHandleTable::CPs4GnmShaderHandleTable() : m_count( 0 )
{
    memset( m_entries, 0, sizeof( m_entries ) );
    for ( uint32_t i = 0; i < kMaxHandles; ++i )
        m_entries[i].generation = 1;
}

Ps4GnmShaderHandle CPs4GnmShaderHandleTable::Register(
    CPs4GnmShader *shader, Ps4GnmShaderHandleStage stage )
{
    if ( !shader )
        return PS4_GNM_SHADER_HANDLE_INVALID;
    for ( uint32_t i = 0; i < kMaxHandles; ++i )
    {
        Entry &entry = m_entries[i];
        if ( entry.shader == shader && entry.stage == stage )
            return MakeHandle( i, entry.generation );
    }
    for ( uint32_t i = 0; i < kMaxHandles; ++i )
    {
        Entry &entry = m_entries[i];
        if ( !entry.shader )
        {
            entry.shader = shader;
            entry.stage = static_cast< uint8_t >( stage );
            ++m_count;
            return MakeHandle( i, entry.generation );
        }
    }
    return PS4_GNM_SHADER_HANDLE_INVALID;
}

CPs4GnmShader *CPs4GnmShaderHandleTable::Resolve(
    Ps4GnmShaderHandle handle, Ps4GnmShaderHandleStage stage ) const
{
    const uint32_t encodedIndex = handle & 0xffffu;
    const uint16_t generation = static_cast< uint16_t >( handle >> 16 );
    if ( !encodedIndex || encodedIndex > kMaxHandles || !generation )
        return 0;
    const Entry &entry = m_entries[encodedIndex - 1];
    return entry.shader && entry.generation == generation && entry.stage == stage
        ? entry.shader : 0;
}

bool CPs4GnmShaderHandleTable::Destroy(
    Ps4GnmShaderHandle handle, Ps4GnmShaderHandleStage stage )
{
    const uint32_t encodedIndex = handle & 0xffffu;
    if ( !encodedIndex || encodedIndex > kMaxHandles ||
        Resolve( handle, stage ) == 0 )
        return false;
    Entry &entry = m_entries[encodedIndex - 1];
    entry.shader = 0;
    entry.stage = 0;
    if ( ++entry.generation == 0 )
        entry.generation = 1;
    --m_count;
    return true;
}
