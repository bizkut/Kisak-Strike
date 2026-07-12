#ifndef KISAK_PS4_SCALEFORM_GNM_HAL_H
#define KISAK_PS4_SCALEFORM_GNM_HAL_H

#include <stdint.h>
#include <vector>

#include <gnm.h>

namespace Scaleform
{
namespace Render
{
class TreeNode;
class TreeRoot;
}
}

// OpenGNM state translation boundary for Scaleform batches. The movie manager
// owns GFx lifetime and capture; this class owns PS4 blend/scissor semantics
// and will become the Render::HAL adapter once tree batches are wired through.
class CPs4ScaleformHal
{
public:
    struct CapturedVertex
    {
        float x;
        float y;
        uint32_t color;
    };

    struct CapturedBatch
    {
        uint32_t firstVertex;
        uint32_t vertexCount;
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t color;
        bool complexFill;
    };

    struct TreeStats
    {
        uint32_t totalNodes;
        uint32_t visibleNodes;
        uint32_t containerNodes;
        uint32_t shapeNodes;
        uint32_t meshNodes;
        uint32_t textNodes;
        uint32_t shapeLayers;
        uint32_t solidFills;
        uint32_t imageFills;
        uint32_t gradientFills;
        uint32_t tessellatedLayers;
        uint32_t tessellatedVertices;
        uint32_t tessellatedTriangles;
        uint32_t degenerateTransforms;
        bool collectGeometry;
        bool hasViewport;
        bool truncated;

        TreeStats()
            : totalNodes( 0 ), visibleNodes( 0 ), containerNodes( 0 ),
              shapeNodes( 0 ), meshNodes( 0 ), textNodes( 0 ),
              shapeLayers( 0 ), solidFills( 0 ), imageFills( 0 ),
              gradientFills( 0 ), tessellatedLayers( 0 ),
              tessellatedVertices( 0 ), tessellatedTriangles( 0 ),
              degenerateTransforms( 0 ),
              collectGeometry( false ),
              hasViewport( false ), truncated( false )
        {
        }
    };

    enum BlendMode
    {
        kBlendNormal,
        kBlendAdd,
        kBlendMultiply,
        kBlendScreen
    };

    CPs4ScaleformHal();

    void BeginFrame( uint64_t frame );
    void EndFrame();
    bool QueueCapturedTree( bool captured, const char *phase );
    bool QueueCapturedTree( Scaleform::Render::TreeRoot *root, const char *phase );

    bool TranslateBlend( BlendMode mode, GnmBlendControl *control ) const;
    bool TranslateScissor( int left, int top, int right, int bottom,
        uint32_t width, uint32_t height, uint32_t scissor[4] ) const;

    uint64_t CapturedTrees() const { return m_capturedTrees; }
    uint64_t PendingBatches() const { return m_pendingBatches; }
    const TreeStats &LastTreeStats() const { return m_lastTreeStats; }
    uint32_t CapturedVertexCount() const { return m_capturedVertices.size(); }
    uint32_t CapturedIndexCount() const { return m_capturedIndices.size(); }
    uint32_t CapturedBatchCount() const { return m_capturedDraws.size(); }
    const std::vector< CapturedVertex > &CapturedVertices() const
        { return m_capturedVertices; }
    const std::vector< uint16_t > &CapturedIndices() const
        { return m_capturedIndices; }
    const std::vector< CapturedBatch > &CapturedDraws() const
        { return m_capturedDraws; }

private:
    enum { kMaxTreeNodes = 4096 };

    void CollectTreeStats( const Scaleform::Render::TreeNode *node,
        TreeStats *stats );

    bool m_frameOpen;
    uint64_t m_frame;
    uint64_t m_capturedTrees;
    uint64_t m_pendingBatches;
    uint32_t m_treeStatsLoggedMask;
    uint32_t m_treeDrawableLoggedMask;
    TreeStats m_lastTreeStats;
    std::vector< CapturedVertex > m_capturedVertices;
    std::vector< uint16_t > m_capturedIndices;
    std::vector< CapturedBatch > m_capturedDraws;
};

CPs4ScaleformHal &KisakPs4ScaleformHal();

#endif
