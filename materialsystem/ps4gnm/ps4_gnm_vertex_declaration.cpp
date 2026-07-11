#include "ps4_gnm_vertex_declaration.h"

#include <string.h>

CPs4GnmVertexDeclaration::CPs4GnmVertexDeclaration() : m_count( 0 )
{
    memset( m_elements, 0, sizeof( m_elements ) );
}

bool CPs4GnmVertexDeclaration::Initialize( const Element *elements, uint32_t count )
{
    if ( !elements || !count || count > kMaxElements )
        return false;
    for ( uint32_t i = 0; i < count; ++i )
    {
        if ( elements[i].stream >= 8 || !sceGnmDfGetBytesPerElement( elements[i].format ) )
            return false;
    }
    memcpy( m_elements, elements, count * sizeof( Element ) );
    m_count = count;
    return true;
}

void CPs4GnmVertexDeclaration::Reset()
{
    memset( m_elements, 0, sizeof( m_elements ) );
    m_count = 0;
}
