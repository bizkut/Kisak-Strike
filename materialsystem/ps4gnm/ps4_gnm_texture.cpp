#include "ps4_gnm_texture.h"

#include <string.h>

CPs4GnmTexture::CPs4GnmTexture()
    : m_data( 0 ), m_size( 0 ), m_alignment( 0 ), m_valid( false )
{
    memset( &m_texture, 0, sizeof( m_texture ) );
}

bool CPs4GnmTexture::Initialize2D( void *memory, size_t memorySize,
    GnmDataFormat format, uint32_t width, uint32_t height, uint32_t mipCount,
    GnmTileMode tileMode, GnmGpuMode gpuMode )
{
    Reset();
    if ( !memory || !memorySize || !width || !height || !mipCount )
        return false;

    uint64_t requiredSize = 0;
    uint32_t requiredAlignment = 0;
    if ( sceGnmTexCreate2d( &m_texture, 0, format, width, height, mipCount,
        tileMode, gpuMode, &requiredSize, &requiredAlignment ) != GNM_ERROR_OK ||
        !requiredSize || !requiredAlignment )
    {
        Reset();
        return false;
    }

    const uintptr_t start = reinterpret_cast< uintptr_t >( memory );
    const uintptr_t aligned = ( start + requiredAlignment - 1 ) &
        ~static_cast< uintptr_t >( requiredAlignment - 1 );
    const size_t prefix = static_cast< size_t >( aligned - start );
    if ( prefix > memorySize || requiredSize > memorySize - prefix ||
        sceGnmTexCreate2d( &m_texture, reinterpret_cast< void * >( aligned ),
            format, width, height, mipCount, tileMode, gpuMode, 0, 0 ) != GNM_ERROR_OK )
    {
        Reset();
        return false;
    }

    m_data = reinterpret_cast< void * >( aligned );
    m_size = requiredSize;
    m_alignment = requiredAlignment;
    m_valid = true;
    return true;
}

void CPs4GnmTexture::Reset()
{
    memset( &m_texture, 0, sizeof( m_texture ) );
    m_data = 0;
    m_size = 0;
    m_alignment = 0;
    m_valid = false;
}
