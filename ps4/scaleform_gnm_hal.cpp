#include "ps4/scaleform_gnm_hal.h"

#if defined( KISAK_PS4_MONOLITHIC )
#include "Render/Render_TreeNode.h"
#include "Render/Render_TreeShape.h"
#include "Render/Render_TessCurves.h"
#include "Render/Render_TessGen.h"
#endif

#include <algorithm>
#include <stdio.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

namespace
{
bool IsComplexFill( const Scaleform::Render::ShapeDataInterface *shape,
    unsigned style )
{
    if ( !shape || style == 0 )
        return false;
    Scaleform::Render::FillStyleType fill;
    shape->GetFillStyle( style, &fill );
    return fill.pFill.GetPtr() != NULL;
}

bool TessellateShapeLayer( Scaleform::Render::ShapeMeshProvider *provider,
    unsigned layer, uint32_t *vertices, uint32_t *triangles,
    std::vector< CPs4ScaleformHal::CapturedVertex > *capturedVertices,
    std::vector< uint16_t > *capturedIndices,
    std::vector< CPs4ScaleformHal::CapturedBatch > *capturedDraws )
{
    if ( !provider || !vertices || !triangles )
        return false;
    const Scaleform::Render::ShapeDataInterface *shape = provider->GetShapeData();
    if ( !shape )
        return false;

    Scaleform::Render::MeshGenerator generator( Scaleform::Memory::GetGlobalHeap() );
    Scaleform::Render::ToleranceParams tolerance;
    generator.mTess.SetFillRule( Scaleform::Render::Tessellator::FillEvenOdd );
    generator.mTess.SetToleranceParam( tolerance );

    Scaleform::Render::ShapePosInfo position( provider->GetLayerStartPos( layer ) );
    bool firstPath = true;
    float coordinates[Scaleform::Render::Edge_MaxCoord];
    unsigned styles[3];
    Scaleform::Render::ShapePathType pathType;
    while ( ( pathType = shape->ReadPathInfo( &position, coordinates, styles ) ) !=
            Scaleform::Render::Shape_EndShape )
    {
        if ( !firstPath && pathType == Scaleform::Render::Shape_NewLayer )
            break;
        firstPath = false;
        if ( styles[0] == styles[1] )
        {
            shape->SkipPathData( &position );
            continue;
        }

        generator.mTess.AddVertex( coordinates[0], coordinates[1] );
        Scaleform::Render::PathEdgeType edge;
        while ( ( edge = shape->ReadEdge( &position, coordinates ) ) !=
                Scaleform::Render::Edge_EndPath )
        {
            if ( edge == Scaleform::Render::Edge_LineTo )
                generator.mTess.AddVertex( coordinates[0], coordinates[1] );
            else if ( edge == Scaleform::Render::Edge_QuadTo )
                Scaleform::Render::TessellateQuadCurve( &generator.mTess, tolerance,
                    coordinates[0], coordinates[1], coordinates[2], coordinates[3] );
            else if ( edge == Scaleform::Render::Edge_CubicTo )
                Scaleform::Render::TessellateCubicCurve( &generator.mTess, tolerance,
                    coordinates[0], coordinates[1], coordinates[2], coordinates[3],
                    coordinates[4], coordinates[5] );
        }
        generator.mTess.FinalizePath( styles[0], styles[1],
            IsComplexFill( shape, styles[0] ), IsComplexFill( shape, styles[1] ) );
    }

    generator.mTess.Tessellate( false );
    *vertices += generator.mTess.GetVertexCount();
    for ( unsigned mesh = 0; mesh < generator.mTess.GetMeshCount(); ++mesh )
    {
        *triangles += generator.mTess.GetMeshTriangleCount( mesh );
        if ( !capturedVertices || !capturedIndices || !capturedDraws ||
             capturedDraws->size() >= 4096 )
            continue;

        Scaleform::Render::TessMesh tessMesh;
        generator.mTess.GetMesh( mesh, &tessMesh );
        const unsigned triangleCount = generator.mTess.GetMeshTriangleCount( mesh );
        if ( tessMesh.VertexCount == 0 || triangleCount == 0 ||
             capturedVertices->size() + tessMesh.VertexCount > 65535 ||
             capturedIndices->size() + triangleCount * 3 > 196605 )
            continue;

        const uint32_t firstVertex = capturedVertices->size();
        std::vector< Scaleform::Render::TessVertex > meshVertices( tessMesh.VertexCount );
        const unsigned copiedVertices = generator.mTess.GetVertices(
            &tessMesh, &meshVertices[0], tessMesh.VertexCount );
        if ( copiedVertices != tessMesh.VertexCount )
            continue;
        for ( unsigned vertex = 0; vertex < copiedVertices; ++vertex )
        {
            CPs4ScaleformHal::CapturedVertex captured;
            captured.x = meshVertices[vertex].x;
            captured.y = meshVertices[vertex].y;
            capturedVertices->push_back( captured );
        }

        const uint32_t firstIndex = capturedIndices->size();
        std::vector< Scaleform::UInt16 > meshIndices( triangleCount * 3 );
        generator.mTess.GetTrianglesI16( mesh, &meshIndices[0], 0, triangleCount );
        for ( unsigned index = 0; index < triangleCount * 3; ++index )
            capturedIndices->push_back( static_cast< uint16_t >(
                firstVertex + meshIndices[index] ) );

        const unsigned style = tessMesh.Style1 ? tessMesh.Style1 : tessMesh.Style2;
        Scaleform::Render::FillStyleType fillStyle;
        fillStyle.Color = 0xffffffffu;
        if ( style )
            provider->GetFillStyle( style, &fillStyle, 0.0f );
        CPs4ScaleformHal::CapturedBatch batch;
        batch.firstVertex = firstVertex;
        batch.vertexCount = copiedVertices;
        batch.firstIndex = firstIndex;
        batch.indexCount = triangleCount * 3;
        batch.color = fillStyle.Color;
        batch.complexFill = fillStyle.pFill.GetPtr() != NULL;
        capturedDraws->push_back( batch );
    }
    generator.Clear();
    return true;
}
}

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
                    if ( stats->collectGeometry && TessellateShapeLayer( shape, layer,
                            &stats->tessellatedVertices, &stats->tessellatedTriangles,
                            &m_capturedVertices, &m_capturedIndices, &m_capturedDraws ) )
                        ++stats->tessellatedLayers;
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

    const uint32_t statsBit = phase && phase[0] == 'h' ? 2u : 1u;
    m_lastTreeStats = TreeStats();
    m_lastTreeStats.collectGeometry = ( m_treeDrawableLoggedMask & statsBit ) == 0;
    if ( m_lastTreeStats.collectGeometry && statsBit == 1u )
    {
        m_capturedVertices.clear();
        m_capturedIndices.clear();
        m_capturedDraws.clear();
        m_capturedVertices.reserve( 4096 );
        m_capturedIndices.reserve( 12288 );
        m_capturedDraws.reserve( 256 );
    }
    CollectTreeStats( root, &m_lastTreeStats );
    if ( m_lastTreeStats.totalNodes == 0 )
        return false;

    ++m_capturedTrees;
    ++m_pendingBatches;
    if ( m_capturedTrees == 1 )
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform OpenGNM HAL tree batch queued" );

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
        char message[352];
        snprintf( message, sizeof( message ),
            "kisak-ps4: scaleform drawable tree phase=%s shapes=%u meshes=%u text=%u layers=%u solid=%u image=%u gradient=%u tess_layers=%u vertices=%u triangles=%u retained_vertices=%u retained_indices=%u retained_batches=%u",
            phase ? phase : "unknown", m_lastTreeStats.shapeNodes,
            m_lastTreeStats.meshNodes, m_lastTreeStats.textNodes,
            m_lastTreeStats.shapeLayers, m_lastTreeStats.solidFills,
            m_lastTreeStats.imageFills, m_lastTreeStats.gradientFills,
            m_lastTreeStats.tessellatedLayers, m_lastTreeStats.tessellatedVertices,
            m_lastTreeStats.tessellatedTriangles, CapturedVertexCount(),
            CapturedIndexCount(), CapturedBatchCount() );
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
