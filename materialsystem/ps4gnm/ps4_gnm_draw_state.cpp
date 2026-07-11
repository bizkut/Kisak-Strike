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
    : m_viewportIndex( 0 ), m_renderTargetMask( 0 ), m_dirtyMask( kDirtyAll )
{
    memset( &m_viewport, 0, sizeof( m_viewport ) );
    memset( &m_viewportTransform, 0, sizeof( m_viewportTransform ) );
    memset( &m_primitiveSetup, 0, sizeof( m_primitiveSetup ) );
    memset( &m_depthStencilControl, 0, sizeof( m_depthStencilControl ) );
    memset( &m_dbRenderControl, 0, sizeof( m_dbRenderControl ) );
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
    if ( emitted & kDirtyRenderTargetMask )
        sceGnmDrawCmdSetRenderTargetMask( command, m_renderTargetMask );
    m_dirtyMask = 0;
    return emitted;
}
