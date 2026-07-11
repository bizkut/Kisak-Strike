#include "ps4_gnm_shader.h"

#include <gnm_helpers.h>

#include <string.h>

CPs4GnmShader::CPs4GnmShader()
{
    Reset();
}

void CPs4GnmShader::Reset()
{
    memset( m_stage, 0, sizeof( m_stage ) );
    m_type = GNM_SHADER_INVALID;
    m_codeSize = 0;
    m_valid = false;
}

bool CPs4GnmShader::Initialize( const void *binary, size_t binarySize,
    GnmShaderType expectedType, void *codeMemory, size_t codeCapacity )
{
    Reset();
    if ( !binary || !binarySize || !codeMemory ||
        ( reinterpret_cast< uintptr_t >( codeMemory ) & 255u ) != 0 )
        return false;

    GnmShaderMetadata metadata = {};
    if ( sceGnmShaderBinaryGetMetadata( binary, binarySize, &metadata ) != GNM_ERROR_OK ||
        metadata.type != expectedType || !metadata.stage || !metadata.shadercode ||
        !metadata.stagesize || metadata.stagesize > sizeof( m_stage ) ||
        !metadata.shadercodesize || metadata.shadercodesize > codeCapacity )
        return false;

    memcpy( m_stage, metadata.stage, metadata.stagesize );
    memcpy( codeMemory, metadata.shadercode, metadata.shadercodesize );
    m_type = expectedType;
    m_codeSize = metadata.shadercodesize;
    if ( expectedType == GNM_SHADER_VERTEX )
        sceGnmVsRegsSetAddress( &reinterpret_cast< GnmVsShader * >( m_stage )->registers, codeMemory );
    else if ( expectedType == GNM_SHADER_PIXEL )
    {
        GnmPsShader *shader = reinterpret_cast< GnmPsShader * >( m_stage );
        sceGnmPsRegsSetAddress( &shader->registers, codeMemory );
        shader->registers.spibaryccntl = 0;
    }
    else
    {
        Reset();
        return false;
    }
    m_valid = true;
    return true;
}

const GnmShaderCommonData *CPs4GnmShader::Common() const
{
    return m_valid ? reinterpret_cast< const GnmShaderCommonData * >( m_stage ) : 0;
}

GnmVsShader *CPs4GnmShader::VertexShader()
{
    return m_valid && m_type == GNM_SHADER_VERTEX
        ? reinterpret_cast< GnmVsShader * >( m_stage ) : 0;
}

GnmPsShader *CPs4GnmShader::PixelShader()
{
    return m_valid && m_type == GNM_SHADER_PIXEL
        ? reinterpret_cast< GnmPsShader * >( m_stage ) : 0;
}
