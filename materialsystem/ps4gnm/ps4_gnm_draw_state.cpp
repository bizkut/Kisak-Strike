#include "ps4_gnm_draw_state.h"

#include <string.h>

namespace
{
template < typename T >
bool AssignIfChanged( T &destination, const T &source )
{
    if ( memcmp( &destination, &source, sizeof( T ) ) == 0 )
        return false;
    destination = source;
    return true;
}
}

CPs4GnmDrawState::CPs4GnmDrawState()
    : m_viewportIndex( 0 ), m_renderTargetMask( 0 ), m_renderTargetIndex( 0 ),
      m_vertexShaderModifier( 0 ), m_dirtyMask( kDirtyAll )
{
    memset( &m_viewport, 0, sizeof( m_viewport ) );
    memset( &m_viewportTransform, 0, sizeof( m_viewportTransform ) );
    memset( &m_primitiveSetup, 0, sizeof( m_primitiveSetup ) );
    memset( &m_depthStencilControl, 0, sizeof( m_depthStencilControl ) );
    memset( &m_dbRenderControl, 0, sizeof( m_dbRenderControl ) );
    memset( &m_renderTarget, 0, sizeof( m_renderTarget ) );
    memset( &m_vertexShader, 0, sizeof( m_vertexShader ) );
    memset( &m_pixelShader, 0, sizeof( m_pixelShader ) );
    memset( &m_blendControl, 0, sizeof( m_blendControl ) );
    memset( &m_depthTarget, 0, sizeof( m_depthTarget ) );
    m_blendIndex = 0;
    m_indexSize = GNM_INDEX_16;
    m_indexCachePolicy = GNM_POLICY_LRU;
    m_depthTargetBound = false;
    memset( m_pointerBindings, 0, sizeof( m_pointerBindings ) );
    m_pointerBindingCount = 0;
    m_vertexExports = 0;
    m_pixelInputs = 0;
    m_vertexExportCount = 0;
    m_pixelInputCount = 0;
    m_primitiveType = GNM_PT_TRILIST;
    m_vertexBufferStage = GNM_STAGE_VS;
    m_vertexBufferSlot = 0;
    memset( &m_vertexBuffer, 0, sizeof( m_vertexBuffer ) );
    memset( m_scissor, 0, sizeof( m_scissor ) );
}

void CPs4GnmDrawState::BeginCommand()
{
    m_dirtyMask = kDirtyAll;
}

void CPs4GnmDrawState::SetViewport( uint32_t index, const GnmSetViewportInfo &viewport )
{
    const bool viewportChanged = AssignIfChanged( m_viewport, viewport );
    if ( m_viewportIndex != index || viewportChanged )
    {
        m_viewportIndex = index;
        m_dirtyMask |= kDirtyViewport;
    }
}

void CPs4GnmDrawState::SetScissor( uint32_t left, uint32_t top, uint32_t right, uint32_t bottom )
{
    const uint32_t scissor[4] = { left, top, right, bottom };
    if ( memcmp( m_scissor, scissor, sizeof( scissor ) ) != 0 )
    {
        memcpy( m_scissor, scissor, sizeof( scissor ) );
        m_dirtyMask |= kDirtyScissor;
    }
}

void CPs4GnmDrawState::SetViewportTransform( const GnmViewportTransformControl &control )
{
    if ( AssignIfChanged( m_viewportTransform, control ) )
        m_dirtyMask |= kDirtyViewportTransform;
}

void CPs4GnmDrawState::SetPrimitiveSetup( const GnmPrimitiveSetup &setup )
{
    if ( AssignIfChanged( m_primitiveSetup, setup ) )
        m_dirtyMask |= kDirtyPrimitive;
}

void CPs4GnmDrawState::SetDepthStencilControl( const GnmDepthStencilControl &control )
{
    if ( AssignIfChanged( m_depthStencilControl, control ) )
        m_dirtyMask |= kDirtyDepthStencil;
}

void CPs4GnmDrawState::SetDbRenderControl( const GnmDbRenderControl &control )
{
    if ( AssignIfChanged( m_dbRenderControl, control ) )
        m_dirtyMask |= kDirtyDbRender;
}

void CPs4GnmDrawState::SetRenderTargetMask( uint32_t mask )
{
    if ( m_renderTargetMask != mask )
    {
        m_renderTargetMask = mask;
        m_dirtyMask |= kDirtyRenderTargetMask;
    }
}

void CPs4GnmDrawState::SetRenderTarget( uint32_t index, const GnmRenderTarget &target )
{
    const bool targetChanged = AssignIfChanged( m_renderTarget, target );
    if ( m_renderTargetIndex != index || targetChanged )
    {
        m_renderTargetIndex = index;
        m_dirtyMask |= kDirtyRenderTarget;
    }
}

void CPs4GnmDrawState::SetVertexShader( const GnmVsStageRegisters &registers, uint32_t modifier )
{
    const bool registersChanged = AssignIfChanged( m_vertexShader, registers );
    if ( m_vertexShaderModifier != modifier || registersChanged )
    {
        m_vertexShaderModifier = modifier;
        m_dirtyMask |= kDirtyVertexShader;
    }
}

void CPs4GnmDrawState::SetPixelShader( const GnmPsStageRegisters &registers )
{
    if ( AssignIfChanged( m_pixelShader, registers ) )
        m_dirtyMask |= kDirtyPixelShader;
}

void CPs4GnmDrawState::SetBlendControl( uint32_t index, const GnmBlendControl &control )
{
    const bool controlChanged = AssignIfChanged( m_blendControl, control );
    if ( m_blendIndex != index || controlChanged )
    {
        m_blendIndex = index;
        m_dirtyMask |= kDirtyBlend;
    }
}

void CPs4GnmDrawState::SetIndexSize( GnmIndexSize size, GnmCachePolicy policy )
{
    if ( m_indexSize != size || m_indexCachePolicy != policy )
    {
        m_indexSize = size;
        m_indexCachePolicy = policy;
        m_dirtyMask |= kDirtyIndexSize;
    }
}

void CPs4GnmDrawState::SetDepthRenderTarget( const GnmDepthRenderTarget &target )
{
    const bool targetChanged = AssignIfChanged( m_depthTarget, target );
    if ( !m_depthTargetBound || targetChanged )
    {
        m_depthTargetBound = true;
        m_dirtyMask |= kDirtyDepthTarget;
    }
}

void CPs4GnmDrawState::ClearDepthRenderTarget()
{
    if ( m_depthTargetBound )
    {
        m_depthTargetBound = false;
        m_dirtyMask |= kDirtyDepthTarget;
    }
}

bool CPs4GnmDrawState::SetPointerUserData( GnmShaderStage stage, uint32_t startSlot, void *pointer )
{
    for ( uint32_t i = 0; i < m_pointerBindingCount; ++i )
    {
        PointerBinding &binding = m_pointerBindings[i];
        if ( binding.stage == stage && binding.startSlot == startSlot )
        {
            if ( binding.pointer != pointer )
            {
                binding.pointer = pointer;
                m_dirtyMask |= kDirtyPointerUserData;
            }
            return true;
        }
    }
    if ( m_pointerBindingCount >= kMaxPointerBindings )
        return false;
    PointerBinding &binding = m_pointerBindings[m_pointerBindingCount++];
    binding.stage = stage;
    binding.startSlot = startSlot;
    binding.pointer = pointer;
    m_dirtyMask |= kDirtyPointerUserData;
    return true;
}

void CPs4GnmDrawState::SetPsInputUsage(
    const GnmVertexExportSemantic *vertexExports, uint32_t vertexExportCount,
    const GnmPixelInputSemantic *pixelInputs, uint32_t pixelInputCount )
{
    if ( m_vertexExports != vertexExports || m_vertexExportCount != vertexExportCount ||
        m_pixelInputs != pixelInputs || m_pixelInputCount != pixelInputCount )
    {
        m_vertexExports = vertexExports;
        m_vertexExportCount = vertexExportCount;
        m_pixelInputs = pixelInputs;
        m_pixelInputCount = pixelInputCount;
        m_dirtyMask |= kDirtyPsInputUsage;
    }
}

void CPs4GnmDrawState::SetPrimitiveType( GnmPrimitiveType primitiveType )
{
    if ( m_primitiveType != primitiveType )
    {
        m_primitiveType = primitiveType;
        m_dirtyMask |= kDirtyPrimitiveType;
    }
}

void CPs4GnmDrawState::SetVertexBuffer( GnmShaderStage stage, uint32_t startSlot,
    const GnmBuffer &buffer )
{
    const bool bufferChanged = AssignIfChanged( m_vertexBuffer, buffer );
    if ( m_vertexBufferStage != stage || m_vertexBufferSlot != startSlot ||
        bufferChanged )
    {
        m_vertexBufferStage = stage;
        m_vertexBufferSlot = startSlot;
        m_dirtyMask |= kDirtyVertexBuffer;
    }
}

uint32_t CPs4GnmDrawState::Apply( GnmCommandBuffer *command )
{
    if ( !command )
        return 0;
    const uint32_t emitted = m_dirtyMask;
    if ( emitted & kDirtyViewport )
        sceGnmDrawCmdSetViewport( command, m_viewportIndex, &m_viewport );
    if ( emitted & kDirtyScissor )
        sceGnmDrawCmdSetScreenScissor( command, m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3] );
    if ( emitted & kDirtyViewportTransform )
        sceGnmDrawCmdSetViewportTransformControl( command, &m_viewportTransform );
    if ( emitted & kDirtyPrimitive )
        sceGnmDrawCmdSetPrimitiveSetup( command, &m_primitiveSetup );
    if ( emitted & kDirtyDepthStencil )
        sceGnmDrawCmdSetDepthStencilControl( command, &m_depthStencilControl );
    if ( emitted & kDirtyDbRender )
        sceGnmDrawCmdSetDbRenderControl( command, &m_dbRenderControl );
    if ( emitted & kDirtyRenderTarget )
        sceGnmDrawCmdSetRenderTarget( command, m_renderTargetIndex, &m_renderTarget );
    if ( emitted & kDirtyRenderTargetMask )
        sceGnmDrawCmdSetRenderTargetMask( command, m_renderTargetMask );
    if ( emitted & kDirtyVertexShader )
        sceGnmDrawCmdSetVsShader( command, &m_vertexShader, m_vertexShaderModifier );
    if ( emitted & kDirtyPixelShader )
        sceGnmDrawCmdSetPsShader( command, &m_pixelShader );
    if ( emitted & kDirtyBlend )
        sceGnmDrawCmdSetBlendControl( command, m_blendIndex, &m_blendControl );
    if ( emitted & kDirtyIndexSize )
        sceGnmDrawCmdSetIndexSize( command, m_indexSize, m_indexCachePolicy );
    if ( emitted & kDirtyDepthTarget )
        sceGnmDrawCmdSetDepthRenderTarget( command,
            m_depthTargetBound ? &m_depthTarget : 0 );
    if ( emitted & kDirtyPointerUserData )
    {
        for ( uint32_t i = 0; i < m_pointerBindingCount; ++i )
        {
            const PointerBinding &binding = m_pointerBindings[i];
            sceGnmDrawCmdSetPointerUserData( command, binding.stage,
                binding.startSlot, binding.pointer );
        }
    }
    if ( emitted & kDirtyPsInputUsage )
        sceGnmDrawCmdSetPsInputUsage( command, m_vertexExports,
            m_vertexExportCount, m_pixelInputs, m_pixelInputCount );
    if ( emitted & kDirtyPrimitiveType )
        sceGnmDrawCmdSetPrimitiveType( command, m_primitiveType );
    if ( emitted & kDirtyVertexBuffer )
        sceGnmDrawCmdSetVsharpUserData( command, m_vertexBufferStage,
            m_vertexBufferSlot, &m_vertexBuffer );
    m_dirtyMask = 0;
    return emitted;
}
