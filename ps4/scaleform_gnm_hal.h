#ifndef KISAK_PS4_SCALEFORM_GNM_HAL_H
#define KISAK_PS4_SCALEFORM_GNM_HAL_H

#include <stdint.h>

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
        bool hasViewport;
        bool truncated;

        TreeStats()
            : totalNodes( 0 ), visibleNodes( 0 ), containerNodes( 0 ),
              shapeNodes( 0 ), meshNodes( 0 ), textNodes( 0 ),
              shapeLayers( 0 ), solidFills( 0 ), imageFills( 0 ),
              gradientFills( 0 ),
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

private:
    enum { kMaxTreeNodes = 4096 };

    static void CollectTreeStats( const Scaleform::Render::TreeNode *node,
        TreeStats *stats );

    bool m_frameOpen;
    uint64_t m_frame;
    uint64_t m_capturedTrees;
    uint64_t m_pendingBatches;
    uint32_t m_treeStatsLoggedMask;
    uint32_t m_treeDrawableLoggedMask;
    TreeStats m_lastTreeStats;
};

CPs4ScaleformHal &KisakPs4ScaleformHal();

#endif
