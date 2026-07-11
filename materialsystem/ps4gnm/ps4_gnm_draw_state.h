#ifndef KISAK_PS4_GNM_DRAW_STATE_H
#define KISAK_PS4_GNM_DRAW_STATE_H

#include <stdint.h>

#include <gnm.h>

class CPs4GnmDrawState
{
public:
    enum DirtyBits
    {
        kDirtyViewport = 1u << 0,
        kDirtyScissor = 1u << 1,
        kDirtyViewportTransform = 1u << 2,
        kDirtyPrimitive = 1u << 3,
        kDirtyDepthStencil = 1u << 4,
        kDirtyDbRender = 1u << 5,
        kDirtyRenderTargetMask = 1u << 6,
        kDirtyAll = ( 1u << 7 ) - 1
    };

    CPs4GnmDrawState();

    void BeginCommand();
    void SetViewport( uint32_t index, const GnmSetViewportInfo &viewport );
    void SetScissor( uint32_t left, uint32_t top, uint32_t right, uint32_t bottom );
    void SetViewportTransform( const GnmViewportTransformControl &control );
    void SetPrimitiveSetup( const GnmPrimitiveSetup &setup );
    void SetDepthStencilControl( const GnmDepthStencilControl &control );
    void SetDbRenderControl( const GnmDbRenderControl &control );
    void SetRenderTargetMask( uint32_t mask );
    uint32_t Apply( GnmCommandBuffer *command );

    uint32_t DirtyMask() const { return m_dirtyMask; }

private:
    GnmSetViewportInfo m_viewport;
    GnmViewportTransformControl m_viewportTransform;
    GnmPrimitiveSetup m_primitiveSetup;
    GnmDepthStencilControl m_depthStencilControl;
    GnmDbRenderControl m_dbRenderControl;
    uint32_t m_viewportIndex;
    uint32_t m_scissor[4];
    uint32_t m_renderTargetMask;
    uint32_t m_dirtyMask;
};

#endif
