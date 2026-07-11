#include "ps4_gnm_memory.h"

#include <limits.h>

CPs4GnmMemory::CPs4GnmMemory() : m_base( 0 ), m_size( 0 ), m_offset( 0 )
{
}

bool CPs4GnmMemory::Initialize( void *base, size_t size )
{
    if ( !base || !size )
        return false;

    m_base = static_cast< uint8_t * >( base );
    m_size = size;
    m_offset = 0;
    return true;
}

void CPs4GnmMemory::Reset()
{
    m_offset = 0;
}

void *CPs4GnmMemory::Allocate( size_t size, size_t alignment )
{
    if ( !m_base || !size || !alignment || ( alignment & ( alignment - 1 ) ) )
        return 0;
    const uintptr_t baseAddress = reinterpret_cast< uintptr_t >( m_base );
    if ( m_offset > UINTPTR_MAX - baseAddress )
        return 0;

    const uintptr_t currentAddress = baseAddress + m_offset;
    if ( alignment - 1 > UINTPTR_MAX - currentAddress )
        return 0;

    const uintptr_t alignedAddress = ( currentAddress + alignment - 1 ) & ~( alignment - 1 );
    const size_t aligned = static_cast< size_t >( alignedAddress - baseAddress );
    if ( aligned > m_size || size > m_size - aligned )
        return 0;

    void *result = m_base + aligned;
    m_offset = aligned + size;
    return result;
}
