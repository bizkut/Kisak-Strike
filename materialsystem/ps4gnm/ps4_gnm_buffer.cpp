#include "ps4_gnm_buffer.h"

#include <string.h>

CPs4GnmBuffer::CPs4GnmBuffer()
    : m_memory( 0 ), m_size( 0 ), m_type( kVertexBuffer ), m_dynamic( false ),
      m_index32( false ), m_locked( false ), m_generation( 0 )
{
}

bool CPs4GnmBuffer::Initialize( void *memory, size_t size, Type type,
    bool dynamic, bool index32 )
{
    const size_t alignment = type == kIndexBuffer ? ( index32 ? 4 : 2 ) : 4;
    if ( !memory || !size || reinterpret_cast< uintptr_t >( memory ) % alignment ||
        ( type == kIndexBuffer && size % alignment ) )
        return false;

    m_memory = static_cast< uint8_t * >( memory );
    m_size = size;
    m_type = type;
    m_dynamic = dynamic;
    m_index32 = type == kIndexBuffer && index32;
    m_locked = false;
    m_generation = 1;
    return true;
}

void CPs4GnmBuffer::Reset()
{
    m_memory = 0;
    m_size = 0;
    m_type = kVertexBuffer;
    m_dynamic = false;
    m_index32 = false;
    m_locked = false;
    m_generation = 0;
}

bool CPs4GnmBuffer::Lock( size_t offset, size_t size, bool discard, void **data )
{
    if ( !data || !m_memory || m_locked || offset > m_size )
        return false;
    if ( size == 0 )
        size = m_size - offset;
    if ( !size || size > m_size - offset )
        return false;
    if ( discard && ( !m_dynamic || offset != 0 || size != m_size ) )
        return false;

    if ( discard )
        ++m_generation;
    m_locked = true;
    *data = m_memory + offset;
    return true;
}

bool CPs4GnmBuffer::Unlock()
{
    if ( !m_locked )
        return false;
    m_locked = false;
    return true;
}

bool CPs4GnmBuffer::Upload( size_t offset, const void *source, size_t size )
{
    if ( !m_memory || m_locked || !source || !size || offset > m_size ||
        size > m_size - offset )
        return false;
    memcpy( m_memory + offset, source, size );
    return true;
}
