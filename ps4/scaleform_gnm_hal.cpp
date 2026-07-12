#include "ps4/scaleform_gnm_hal.h"

#if defined( KISAK_PS4_MONOLITHIC )
#include "Render/Render_TreeNode.h"
#include "Render/Render_TreeShape.h"
#endif

#include <algorithm>
#include <stdio.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

CPs4ScaleformHal::CPs4ScaleformHal()
    : m_frameOpen( false ), m_frame( 0 ), m_capturedTrees( 0 ), m_pendingBatches( 0 ),
      m_treeStatsLoggedMask( 0 ), m_treeDrawableLoggedMask( 0 )
{
}

void CPs4ScaleformHal::BeginFrame( uint64_t frame )
{
    m_frameOpen = true;
    m_frame = frame;
    m_pendingBatches = 0;
    m_lastTreeStats = TreeStats();
}

void CPs4ScaleformHal::EndFrame()
{
    m_frameOpen = false;
}

bool CPs4ScaleformHal::QueueCapturedTree( bool captured, const char *phase )
{
    if ( !m_frameOpen || !captured )
        return false;

    ++m_capturedTrees;
    ++m_pendingBatches;
    if ( m_capturedTrees == 1 )
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform OpenGNM HAL capture queued" );
    (void)phase;
    return true;
}

#if defined( KISAK_PS4_MONOLITHIC )
void CPs4ScaleformHal::CollectTreeStats( const Scaleform::Render::TreeNode *node,
    TreeStats *stats )
{
    if ( !node || !stats )
        return;

    if ( stats->totalNodes >= kMaxTreeNodes )
    {
        stats->truncated = true;
        return;
    }

    ++stats->totalNodes;
    if ( node->IsVisible() )
        ++stats->visibleNodes;

    switch ( node->GetReadOnlyData()->GetType() )
    {
    case Scaleform::Render::Context::EntryData::ET_Root:
        stats->hasViewport = static_cast< const Scaleform::Render::TreeRoot * >( node )
            ->HasViewport();
        ++stats->containerNodes;
        break;
    case Scaleform::Render::Context::EntryData::ET_Container:
        ++stats->containerNodes;
        break;
    case Scaleform::Render::Context::EntryData::ET_Shape:
        ++stats->shapeNodes;
        {
            const Scaleform::Render::TreeShape *shapeNode =
                static_cast< const Scaleform::Render::TreeShape * >( node );
            Scaleform::Render::ShapeMeshProvider *shape = shapeNode->GetShape();
            if ( shape )
            {
                const unsigned layerCount = shape->GetLayerCount();
                stats->shapeLayers += layerCount;
                for ( unsigned layer = 0; layer < layerCount; ++layer )
                {
                    const unsigned fillCount = shape->GetFillCount( layer, 0 );
                    for ( unsigned fill = 0; fill < fillCount; ++fill )
                    {
                        Scaleform::Render::FillData fillData;
                        shape->GetFillData( &fillData, layer, fill, 0 );
                        switch ( fillData.Type )
                        {
                        case Scaleform::Render::Fill_SolidColor:
                        case Scaleform::Render::Fill_VColor:
                            ++stats->solidFills;
                            break;
                        case Scaleform::Render::Fill_Image:
                            ++stats->imageFills;
                            break;
                        case Scaleform::Render::Fill_Gradient:
                            ++stats->gradientFills;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
        break;
    case Scaleform::Render::Context::EntryData::ET_Mesh:
        ++stats->meshNodes;
        break;
    case Scaleform::Render::Context::EntryData::ET_Text:
        ++stats->textNodes;
        break;
    default:
        break;
    }

    const Scaleform::Render::Context::EntryData::EntryType type =
        node->GetReadOnlyData()->GetType();
    if ( type != Scaleform::Render::Context::EntryData::ET_Root &&
         type != Scaleform::Render::Context::EntryData::ET_Container )
    {
        return;
    }

    const Scaleform::Render::TreeContainer *container =
        static_cast< const Scaleform::Render::TreeContainer * >( node );
    for ( Scaleform::UPInt i = 0; i < container->GetSize(); ++i )
        CollectTreeStats( container->GetAt( i ), stats );
}

bool CPs4ScaleformHal::QueueCapturedTree( Scaleform::Render::TreeRoot *root,
    const char *phase )
{
    if ( !m_frameOpen || !root )
        return false;

    m_lastTreeStats = TreeStats();
    CollectTreeStats( root, &m_lastTreeStats );
    if ( m_lastTreeStats.totalNodes == 0 )
        return false;

    ++m_capturedTrees;
    ++m_pendingBatches;
    if ( m_capturedTrees == 1 )
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform OpenGNM HAL tree batch queued" );

    const uint32_t statsBit = phase && phase[0] == 'h' ? 2u : 1u;
    if ( ( m_treeStatsLoggedMask & statsBit ) == 0 )
    {
        m_treeStatsLoggedMask |= statsBit;
        char message[192];
        snprintf( message, sizeof( message ),
            "kisak-ps4: scaleform tree stats phase=%s total=%u visible=%u containers=%u shapes=%u meshes=%u text=%u viewport=%u truncated=%u",
            phase ? phase : "unknown",
            m_lastTreeStats.totalNodes, m_lastTreeStats.visibleNodes,
            m_lastTreeStats.containerNodes, m_lastTreeStats.shapeNodes,
            m_lastTreeStats.meshNodes, m_lastTreeStats.textNodes,
            m_lastTreeStats.hasViewport ? 1u : 0u,
            m_lastTreeStats.truncated ? 1u : 0u );
        KisakPs4StartupBreadcrumb( message );
    }

    if ( ( m_lastTreeStats.shapeNodes || m_lastTreeStats.meshNodes || m_lastTreeStats.textNodes ) &&
         ( m_treeDrawableLoggedMask & statsBit ) == 0 )
    {
        m_treeDrawableLoggedMask |= statsBit;
        char message[224];
        snprintf( message, sizeof( message ),
            "kisak-ps4: scaleform drawable tree phase=%s shapes=%u meshes=%u text=%u layers=%u solid=%u image=%u gradient=%u",
            phase ? phase : "unknown", m_lastTreeStats.shapeNodes,
            m_lastTreeStats.meshNodes, m_lastTreeStats.textNodes,
            m_lastTreeStats.shapeLayers, m_lastTreeStats.solidFills,
            m_lastTreeStats.imageFills, m_lastTreeStats.gradientFills );
        KisakPs4StartupBreadcrumb( message );
    }
    (void)phase;
    return true;
}
#else
bool CPs4ScaleformHal::QueueCapturedTree( Scaleform::Render::TreeRoot *root,
    const char *phase )
{
    (void)root;
    (void)phase;
    return false;
}
#endif

bool CPs4ScaleformHal::TranslateBlend( BlendMode mode, GnmBlendControl *control ) const
{
    if ( !control )
        return false;

    *control = GnmBlendControl();
    control->blendenabled = true;
    control->colorfunc = GNM_COMB_DST_PLUS_SRC;
    control->alphafunc = GNM_COMB_DST_PLUS_SRC;
    control->separatealphaenable = true;
    control->alphasrcmult = GNM_BLEND_ONE;
    control->alphadstmult = GNM_BLEND_ONE_MINUS_SRC_ALPHA;

    switch ( mode )
    {
    case kBlendNormal:
        control->colorsrcmult = GNM_BLEND_SRC_ALPHA;
        control->colordstmult = GNM_BLEND_ONE_MINUS_SRC_ALPHA;
        return true;
    case kBlendAdd:
        control->colorsrcmult = GNM_BLEND_SRC_ALPHA;
        control->colordstmult = GNM_BLEND_ONE;
        return true;
    case kBlendMultiply:
        control->colorsrcmult = GNM_BLEND_DEST_COLOR;
        control->colordstmult = GNM_BLEND_ZERO;
        return true;
    case kBlendScreen:
        control->colorsrcmult = GNM_BLEND_ONE;
        control->colordstmult = GNM_BLEND_ONE_MINUS_SRC_COLOR;
        return true;
    default:
        return false;
    }
}

bool CPs4ScaleformHal::TranslateScissor( int left, int top, int right, int bottom,
    uint32_t width, uint32_t height, uint32_t scissor[4] ) const
{
    if ( !scissor || width == 0 || height == 0 || right <= left || bottom <= top )
        return false;

    left = std::max( 0, std::min( left, static_cast< int >( width ) ) );
    right = std::max( 0, std::min( right, static_cast< int >( width ) ) );
    top = std::max( 0, std::min( top, static_cast< int >( height ) ) );
    bottom = std::max( 0, std::min( bottom, static_cast< int >( height ) ) );
    if ( right <= left || bottom <= top )
        return false;

    scissor[0] = static_cast< uint32_t >( left );
    scissor[1] = static_cast< uint32_t >( top );
    scissor[2] = static_cast< uint32_t >( right );
    scissor[3] = static_cast< uint32_t >( bottom );
    return true;
}

CPs4ScaleformHal &KisakPs4ScaleformHal()
{
    static CPs4ScaleformHal hal;
    return hal;
}
