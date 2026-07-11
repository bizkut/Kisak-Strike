#include "ps4_gnm_texture.h"

#include <string.h>

CPs4GnmTexture::CPs4GnmTexture()
    : m_data( 0 ), m_size( 0 ), m_capacity( 0 ), m_alignment( 0 ), m_width( 0 ), m_height( 0 ),
      m_bytesPerElement( 0 ), m_valid( false )
{
    memset( &m_texture, 0, sizeof( m_texture ) );
    memset( &m_colorTarget, 0, sizeof( m_colorTarget ) );
    m_colorTargetValid = false;
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
    m_capacity = memorySize - prefix;
    m_alignment = requiredAlignment;
    m_width = width;
    m_height = height;
    m_bytesPerElement = sceGnmDfGetBytesPerElement( format );
    if ( !m_bytesPerElement )
    {
        Reset();
        return false;
    }
    m_valid = true;
    return true;
}

bool CPs4GnmTexture::UploadLinear( const void *source, size_t sourceRowBytes,
    uint32_t rowCount )
{
    if ( !m_valid || !source || !sourceRowBytes || !rowCount ||
        rowCount > m_height )
        return false;
    const size_t pitchBytes = static_cast< size_t >( sceGnmTexGetPitch( &m_texture ) ) *
        m_bytesPerElement;
    if ( sourceRowBytes > pitchBytes || pitchBytes > m_size / rowCount )
        return false;
    const uint8_t *sourceBytes = static_cast< const uint8_t * >( source );
    uint8_t *destination = static_cast< uint8_t * >( m_data );
    for ( uint32_t row = 0; row < rowCount; ++row )
    {
        memcpy( destination + row * pitchBytes, sourceBytes + row * sourceRowBytes,
            sourceRowBytes );
        if ( sourceRowBytes < pitchBytes )
            memset( destination + row * pitchBytes + sourceRowBytes, 0,
                pitchBytes - sourceRowBytes );
    }
    return true;
}

bool CPs4GnmTexture::CreateColorTargetView( GnmDataFormat format,
    uint32_t width, uint32_t height, GnmTileMode tileMode, GnmGpuMode gpuMode )
{
    memset( &m_colorTarget, 0, sizeof( m_colorTarget ) );
    m_colorTargetValid = false;
    if ( !m_valid || width != m_width || height != m_height )
        return false;
    uint64_t requiredSize = 0;
    uint32_t requiredAlignment = 0;
    if ( sceGnmRtCreateColorTarget( &m_colorTarget, m_data, format, width, height,
        1, 1, 1, tileMode, gpuMode, &requiredSize, &requiredAlignment ) != GNM_ERROR_OK ||
        requiredSize > m_capacity ||
        ( reinterpret_cast< uintptr_t >( m_data ) & ( requiredAlignment - 1 ) ) != 0 )
    {
        memset( &m_colorTarget, 0, sizeof( m_colorTarget ) );
        return false;
    }
    if ( requiredSize > m_size )
        m_size = requiredSize;
    m_colorTargetValid = true;
    return true;
}

void CPs4GnmTexture::Reset()
{
    memset( &m_texture, 0, sizeof( m_texture ) );
    memset( &m_colorTarget, 0, sizeof( m_colorTarget ) );
    m_data = 0;
    m_size = 0;
    m_capacity = 0;
    m_alignment = 0;
    m_width = 0;
    m_height = 0;
    m_bytesPerElement = 0;
    m_valid = false;
    m_colorTargetValid = false;
}
