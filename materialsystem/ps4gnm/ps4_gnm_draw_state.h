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
        kDirtyRenderTarget = 1u << 7,
        kDirtyVertexShader = 1u << 8,
        kDirtyPixelShader = 1u << 9,
        kDirtyBlend = 1u << 10,
        kDirtyIndexSize = 1u << 11,
        kDirtyDepthTarget = 1u << 12,
        kDirtyPointerUserData = 1u << 13,
        kDirtyPsInputUsage = 1u << 14,
        kDirtyPrimitiveType = 1u << 15,
        kDirtyVertexBuffer = 1u << 16,
        kDirtyAll = ( 1u << 17 ) - 1
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
    void SetRenderTarget( uint32_t index, const GnmRenderTarget &target );
    void SetVertexShader( const GnmVsStageRegisters &registers, uint32_t modifier );
    void SetPixelShader( const GnmPsStageRegisters &registers );
    void SetBlendControl( uint32_t index, const GnmBlendControl &control );
    void SetIndexSize( GnmIndexSize size, GnmCachePolicy policy );
    void SetDepthRenderTarget( const GnmDepthRenderTarget &target );
    void ClearDepthRenderTarget();
    bool SetPointerUserData( GnmShaderStage stage, uint32_t startSlot, void *pointer );
    void SetPsInputUsage( const GnmVertexExportSemantic *vertexExports,
        uint32_t vertexExportCount, const GnmPixelInputSemantic *pixelInputs,
        uint32_t pixelInputCount );
    void SetPrimitiveType( GnmPrimitiveType primitiveType );
    void SetVertexBuffer( GnmShaderStage stage, uint32_t startSlot,
        const GnmBuffer &buffer );
    uint32_t Apply( GnmCommandBuffer *command );
    void RetainDirtyMask( uint32_t mask ) { m_dirtyMask &= mask; }
    void Invalidate( uint32_t mask ) { m_dirtyMask |= mask & kDirtyAll; }

    uint32_t DirtyMask() const { return m_dirtyMask; }

private:
    struct PointerBinding
    {
        GnmShaderStage stage;
        uint32_t startSlot;
        void *pointer;
    };
    enum { kMaxPointerBindings = 8 };
    GnmSetViewportInfo m_viewport;
    GnmViewportTransformControl m_viewportTransform;
    GnmPrimitiveSetup m_primitiveSetup;
    GnmDepthStencilControl m_depthStencilControl;
    GnmDbRenderControl m_dbRenderControl;
    GnmRenderTarget m_renderTarget;
    GnmVsStageRegisters m_vertexShader;
    GnmPsStageRegisters m_pixelShader;
    GnmBlendControl m_blendControl;
    GnmDepthRenderTarget m_depthTarget;
    uint32_t m_viewportIndex;
    uint32_t m_scissor[4];
    uint32_t m_renderTargetMask;
    uint32_t m_renderTargetIndex;
    uint32_t m_vertexShaderModifier;
    uint32_t m_blendIndex;
    GnmIndexSize m_indexSize;
    GnmCachePolicy m_indexCachePolicy;
    bool m_depthTargetBound;
    PointerBinding m_pointerBindings[kMaxPointerBindings];
    uint32_t m_pointerBindingCount;
    const GnmVertexExportSemantic *m_vertexExports;
    const GnmPixelInputSemantic *m_pixelInputs;
    uint32_t m_vertexExportCount;
    uint32_t m_pixelInputCount;
    GnmPrimitiveType m_primitiveType;
    GnmShaderStage m_vertexBufferStage;
    uint32_t m_vertexBufferSlot;
    GnmBuffer m_vertexBuffer;
    bool m_vertexBufferBound;
    uint32_t m_dirtyMask;
};

#endif
