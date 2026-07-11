#include "ps4_gnm_constants.h"

#include <string.h>

CPs4GnmConstants::CPs4GnmConstants()
{
    Reset();
}

void CPs4GnmConstants::Reset()
{
    memset( m_vertex, 0, sizeof( m_vertex ) );
    memset( m_pixel, 0, sizeof( m_pixel ) );
    m_vertexUsed = 0;
    m_pixelUsed = 0;
}

bool CPs4GnmConstants::SetFloat( Stage stage, uint32_t startRegister,
    const float *values, uint32_t registerCount )
{
    const uint32_t capacity = stage == kVertex ? kVertexRegisters : kPixelRegisters;
    if ( !values || !registerCount || startRegister > capacity ||
        registerCount > capacity - startRegister )
        return false;
    float ( *destination )[4] = stage == kVertex ? m_vertex : m_pixel;
    memcpy( destination[startRegister], values, registerCount * 4 * sizeof( float ) );
    uint32_t &used = stage == kVertex ? m_vertexUsed : m_pixelUsed;
    if ( used < startRegister + registerCount )
        used = startRegister + registerCount;
    return true;
}

bool CPs4GnmConstants::BuildBuffer( Stage stage, CPs4GnmMemory *arena,
    GnmBuffer *buffer )
{
    const uint32_t used = UsedRegisters( stage );
    if ( !arena || !buffer || !used )
        return false;
    const size_t bytes = used * 4 * sizeof( float );
    void *snapshot = arena->Allocate( bytes, 256 );
    if ( !snapshot )
        return false;
    memcpy( snapshot, stage == kVertex ? &m_vertex[0][0] : &m_pixel[0][0], bytes );
    *buffer = sceGnmCreateConstBuffer( snapshot, static_cast< uint32_t >( bytes ) );
    return sceGnmBufGetBaseAddress( buffer ) == snapshot &&
        buffer->numrecords == used;
}

uint32_t CPs4GnmConstants::UsedRegisters( Stage stage ) const
{
    return stage == kVertex ? m_vertexUsed : m_pixelUsed;
}
