#include "ps4/scaleform_gnm_hal.h"

#if defined( KISAK_PS4_MONOLITHIC )
#include "Kernel/SF_HeapNew.h"
#include "Render/Render_TreeNode.h"
#include "Render/Render_TreeShape.h"
#include "Render/Render_TreeText.h"
#include "Render/Render_Font.h"
#include "Render/Render_TessCurves.h"
#include "Render/Render_TessGen.h"
#endif

#include <algorithm>
#include <stdio.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

namespace
{
#if defined( KISAK_PS4_MONOLITHIC )
const unsigned kGradientTileSize = 32;
const unsigned kGradientAtlasColumns = 16;
const unsigned kGradientAtlasRows = 8;
const unsigned kGradientAtlasWidth = kGradientTileSize * kGradientAtlasColumns;
const unsigned kGradientAtlasHeight = kGradientTileSize * kGradientAtlasRows;
const unsigned kFontTileSize = 64;
const unsigned kFontAtlasColumns = 16;
const unsigned kFontAtlasRows = 8;
const unsigned kFontAtlasWidth = kFontTileSize * kFontAtlasColumns;
const unsigned kFontAtlasHeight = kFontTileSize * kFontAtlasRows;

bool IsComplexFill( const Scaleform::Render::ShapeDataInterface *shape,
    unsigned style )
{
    if ( !shape || style == 0 )
        return false;
    Scaleform::Render::FillStyleType fill;
    shape->GetFillStyle( style, &fill );
    return fill.pFill.GetPtr() != NULL;
}

uint32_t SampleGradient( const Scaleform::Render::GradientData *gradient, float ratio )
{
    if ( !gradient || gradient->GetRecordCount() == 0 )
        return 0;
    ratio = std::max( 0.0f, std::min( ratio, 1.0f ) );
    const Scaleform::Render::GradientRecord *records = gradient->GetRecords();
    const unsigned count = gradient->GetRecordCount();
    const float target = ratio * 255.0f;
    if ( target <= records[0].Ratio )
        return records[0].ColorV.Raw;
    for ( unsigned i = 1; i < count; ++i )
    {
        if ( target <= records[i].Ratio )
        {
            const float span = static_cast< float >( records[i].Ratio - records[i - 1].Ratio );
            const float local = span > 0.0f
                ? ( target - records[i - 1].Ratio ) / span : 0.0f;
            return Scaleform::Render::Color::Blend(
                records[i - 1].ColorV, records[i].ColorV, local ).Raw;
        }
    }
    return records[count - 1].ColorV.Raw;
}

uint32_t SampleGradientAt( const Scaleform::Render::GradientData *gradient,
    const Scaleform::Render::Matrix2F &gradientMatrix, float x, float y )
{
    gradientMatrix.Transform( &x, &y );
    float ratio = x;
    if ( gradient->GetGradientType() != Scaleform::Render::GradientLinear )
    {
        const float dx = ( x - 0.5f ) * 2.0f;
        const float dy = ( y - 0.5f ) * 2.0f;
        ratio = sqrtf( dx * dx + dy * dy );
    }
    return SampleGradient( gradient, ratio );
}

void GradientAtlasUv( const Scaleform::Render::GradientData *gradient,
    const Scaleform::Render::Matrix2F &gradientMatrix, unsigned tile,
    float x, float y, float *atlasU, float *atlasV )
{
    gradientMatrix.Transform( &x, &y );
    x = std::max( 0.0f, std::min( x, 1.0f ) );
    y = gradient->GetGradientType() == Scaleform::Render::GradientLinear
        ? 0.5f : std::max( 0.0f, std::min( y, 1.0f ) );
    const unsigned tileX = ( tile % kGradientAtlasColumns ) * kGradientTileSize;
    const unsigned tileY = ( tile / kGradientAtlasColumns ) * kGradientTileSize;
    *atlasU = ( tileX + 0.5f + x * ( kGradientTileSize - 1 ) ) /
        static_cast< float >( kGradientAtlasWidth );
    *atlasV = ( tileY + 0.5f + y * ( kGradientTileSize - 1 ) ) /
        static_cast< float >( kGradientAtlasHeight );
}

bool TessellateShapeLayer( Scaleform::Render::ShapeMeshProvider *provider,
    unsigned layer, const Scaleform::Render::Matrix2F &viewMatrix,
    const Scaleform::Render::Cxform &colorTransform,
    uint32_t *vertices, uint32_t *triangles,
    std::vector< CPs4ScaleformHal::CapturedVertex > *capturedVertices,
    std::vector< uint16_t > *capturedIndices,
    std::vector< CPs4ScaleformHal::CapturedBatch > *capturedDraws,
    std::vector< uint32_t > *gradientPixels, uint32_t *gradientTileCount )
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
        if ( tessMesh.VertexCount == 0 || triangleCount == 0 )
            continue;

        const uint32_t firstVertex = capturedVertices->size();
        std::vector< Scaleform::Render::TessVertex > meshVertices( tessMesh.VertexCount );
        const unsigned copiedVertices = generator.mTess.GetVertices(
            &tessMesh, &meshVertices[0], tessMesh.VertexCount );
        if ( copiedVertices != tessMesh.VertexCount )
            continue;
        float rawMinX = meshVertices[0].x;
        float rawMinY = meshVertices[0].y;
        float rawMaxX = rawMinX;
        float rawMaxY = rawMinY;
        for ( unsigned vertex = 1; vertex < copiedVertices; ++vertex )
        {
            rawMinX = std::min( rawMinX, meshVertices[vertex].x );
            rawMinY = std::min( rawMinY, meshVertices[vertex].y );
            rawMaxX = std::max( rawMaxX, meshVertices[vertex].x );
            rawMaxY = std::max( rawMaxY, meshVertices[vertex].y );
        }
        static bool loggedTransformSample = false;
        if ( !loggedTransformSample )
        {
            char sampleMessage[320];
            snprintf( sampleMessage, sizeof( sampleMessage ),
                "kisak-ps4: scaleform transform sample raw=%.2f,%.2f..%.2f,%.2f m=[%.5f %.5f %.2f; %.5f %.5f %.2f]",
                rawMinX, rawMinY, rawMaxX, rawMaxY,
                viewMatrix.M[0][0], viewMatrix.M[0][1], viewMatrix.M[0][3],
                viewMatrix.M[1][0], viewMatrix.M[1][1], viewMatrix.M[1][3] );
            KisakPs4StartupBreadcrumb( sampleMessage );
            loggedTransformSample = true;
        }
        const unsigned meshStyles[2] = { tessMesh.Style1, tessMesh.Style2 };
        const Scaleform::Render::GradientData *gradient = NULL;
        const Scaleform::Render::Image *image = NULL;
        Scaleform::Render::Matrix2F gradientMatrix;
        for ( unsigned styleIndex = 0; styleIndex < 2 && !gradient && !image; ++styleIndex )
        {
            if ( meshStyles[styleIndex] == 0 )
                continue;
            Scaleform::Render::FillStyleType style;
            provider->GetFillStyle( meshStyles[styleIndex], &style, 0.0f );
            if ( style.pFill && style.pFill->pGradient )
            {
                gradient = style.pFill->pGradient;
                gradientMatrix = style.pFill->ImageMatrix;
            }
            else if ( style.pFill && style.pFill->pImage )
            {
                image = style.pFill->pImage;
            }
        }
        const uint32_t requiredVertices = copiedVertices;
        const uint32_t requiredIndices = triangleCount * 3;
        if ( capturedVertices->size() + requiredVertices > 65535 ||
             capturedIndices->size() + requiredIndices > 196605 )
            continue;

        int gradientTile = -1;
        if ( gradient )
        {
            if ( !gradientPixels || !gradientTileCount ||
                 *gradientTileCount >= kGradientAtlasColumns * kGradientAtlasRows )
                continue;
            if ( gradientPixels->empty() )
                gradientPixels->assign( kGradientAtlasWidth * kGradientAtlasHeight, 0 );
            gradientTile = static_cast< int >( ( *gradientTileCount )++ );
            const unsigned tileX = ( gradientTile % kGradientAtlasColumns ) * kGradientTileSize;
            const unsigned tileY = ( gradientTile / kGradientAtlasColumns ) * kGradientTileSize;
            for ( unsigned pixelY = 0; pixelY < kGradientTileSize; ++pixelY )
            {
                for ( unsigned pixelX = 0; pixelX < kGradientTileSize; ++pixelX )
                {
                    const float u = static_cast< float >( pixelX ) /
                        ( kGradientTileSize - 1 );
                    const float v = static_cast< float >( pixelY ) /
                        ( kGradientTileSize - 1 );
                    float ratio = u;
                    if ( gradient->GetGradientType() != Scaleform::Render::GradientLinear )
                    {
                        const float dx = ( u - 0.5f ) * 2.0f;
                        const float dy = ( v - 0.5f ) * 2.0f;
                        ratio = sqrtf( dx * dx + dy * dy );
                    }
                    Scaleform::Render::Color color = colorTransform.Transform(
                        Scaleform::Render::Color( SampleGradient( gradient, ratio ) ) );
                    ( *gradientPixels )[( tileY + pixelY ) * kGradientAtlasWidth +
                        tileX + pixelX] = color.GetRed() |
                        ( color.GetGreen() << 8 ) | ( color.GetBlue() << 16 ) |
                        ( color.GetAlpha() << 24 );
                }
            }
        }

        std::vector< Scaleform::UInt16 > meshIndices( triangleCount * 3 );
        generator.mTess.GetTrianglesI16( mesh, &meshIndices[0], 0, triangleCount );
        for ( unsigned vertex = 0; vertex < copiedVertices; ++vertex )
        {
            CPs4ScaleformHal::CapturedVertex captured;
            captured.x = meshVertices[vertex].x;
            captured.y = meshVertices[vertex].y;
            captured.gradientU = 0.0f;
            captured.gradientV = 0.0f;
            captured.color = 0;
            const Scaleform::Render::TessVertex &tessVertex = meshVertices[vertex];
            if ( !Scaleform::Render::TessStyleIsComplex( tessVertex.Flags ) )
            {
                const unsigned usedStyle = Scaleform::Render::TessGetUsedStyle(
                    tessVertex.Flags );
                Scaleform::Render::FillStyleType fill0;
                provider->GetFillStyle( tessVertex.Styles[usedStyle], &fill0, 0.0f );
                captured.color = fill0.Color;
                if ( Scaleform::Render::TessStyleMixesColors( tessVertex.Flags ) )
                {
                    Scaleform::Render::FillStyleType fill1;
                    provider->GetFillStyle( tessVertex.Styles[1], &fill1, 0.0f );
                    captured.color = ( ( fill0.Color & 0xfefefefeu ) >> 1 ) |
                        ( ( fill1.Color & 0xfefefefeu ) >> 1 );
                }
            }
            else if ( gradient )
            {
                captured.color = SampleGradientAt( gradient, gradientMatrix,
                    tessVertex.x, tessVertex.y );
                GradientAtlasUv( gradient, gradientMatrix, gradientTile,
                    tessVertex.x, tessVertex.y,
                    &captured.gradientU, &captured.gradientV );
            }
            captured.color = colorTransform.Transform(
                Scaleform::Render::Color( captured.color ) ).Raw;
            viewMatrix.Transform( &captured.x, &captured.y );
            capturedVertices->push_back( captured );
        }

        const uint32_t firstIndex = capturedIndices->size();
        for ( unsigned index = 0; index < triangleCount * 3; ++index )
            capturedIndices->push_back( static_cast< uint16_t >(
                firstVertex + meshIndices[index] ) );

        CPs4ScaleformHal::CapturedBatch batch;
        batch.firstVertex = firstVertex;
        batch.vertexCount = capturedVertices->size() - firstVertex;
        batch.firstIndex = firstIndex;
        batch.indexCount = capturedIndices->size() - firstIndex;
        batch.color = 0;
        batch.complexFill = !gradient && (
            Scaleform::Render::TessStyleIsComplex( tessMesh.Flags1 ) ||
            Scaleform::Render::TessStyleIsComplex( tessMesh.Flags2 ) );
        batch.gradientFill = gradientTile >= 0;
        batch.imageFill = image != NULL;
        batch.textFill = false;
        batch.packedTextFill = false;
        capturedDraws->push_back( batch );
    }
    generator.Clear();
    return true;
}

bool TessellateGlyphShape( const Scaleform::Render::ShapeDataInterface *shape,
    const Scaleform::Render::Matrix2F &viewMatrix, float scale,
    float penX, float penY, uint32_t color,
    std::vector< CPs4ScaleformHal::CapturedVertex > *capturedVertices,
    std::vector< uint16_t > *capturedIndices,
    std::vector< CPs4ScaleformHal::CapturedBatch > *capturedDraws,
    uint32_t *glyphVertices, uint32_t *glyphTriangles )
{
    if ( !shape || shape->IsEmpty() || !capturedVertices || !capturedIndices ||
         !capturedDraws || scale <= 0.0f )
        return false;

    Scaleform::Render::MeshGenerator generator( Scaleform::Memory::GetGlobalHeap() );
    Scaleform::Render::ToleranceParams tolerance;
    tolerance.CurveTolerance *= 2.0f;
    tolerance.CollinearityTolerance *= 2.0f;
    generator.mTess.SetFillRule( Scaleform::Render::Tessellator::FillNonZero );
    generator.mTess.SetToleranceParam( tolerance );

    Scaleform::Render::ShapePosInfo position( shape->GetStartingPos() );
    float coordinates[Scaleform::Render::Edge_MaxCoord];
    unsigned styles[3];
    Scaleform::Render::ShapePathType pathType;
    bool firstPath = true;
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

        coordinates[0] = coordinates[0] * scale + penX;
        coordinates[1] = coordinates[1] * scale + penY;
        viewMatrix.Transform( &coordinates[0], &coordinates[1] );
        generator.mTess.AddVertex( coordinates[0], coordinates[1] );
        Scaleform::Render::PathEdgeType edge;
        while ( ( edge = shape->ReadEdge( &position, coordinates ) ) !=
                Scaleform::Render::Edge_EndPath )
        {
            const unsigned pointCount = edge == Scaleform::Render::Edge_LineTo ? 1u
                : ( edge == Scaleform::Render::Edge_QuadTo ? 2u : 3u );
            for ( unsigned point = 0; point < pointCount; ++point )
            {
                coordinates[point * 2 + 0] =
                    coordinates[point * 2 + 0] * scale + penX;
                coordinates[point * 2 + 1] =
                    coordinates[point * 2 + 1] * scale + penY;
                viewMatrix.Transform( &coordinates[point * 2 + 0],
                    &coordinates[point * 2 + 1] );
            }
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
        generator.mTess.FinalizePath( 1, 0, false, false );
    }

    generator.mTess.Tessellate( false );
    bool emitted = false;
    for ( unsigned mesh = 0; mesh < generator.mTess.GetMeshCount(); ++mesh )
    {
        Scaleform::Render::TessMesh tessMesh;
        generator.mTess.GetMesh( mesh, &tessMesh );
        const unsigned triangleCount = generator.mTess.GetMeshTriangleCount( mesh );
        if ( tessMesh.VertexCount == 0 || triangleCount == 0 ||
             capturedVertices->size() + tessMesh.VertexCount > 65535 ||
             capturedIndices->size() + triangleCount * 3 > 196605 ||
             capturedDraws->size() >= 4096 )
            continue;

        std::vector< Scaleform::Render::TessVertex > vertices( tessMesh.VertexCount );
        if ( generator.mTess.GetVertices( &tessMesh, &vertices[0],
                tessMesh.VertexCount ) != tessMesh.VertexCount )
            continue;
        const uint32_t firstVertex = capturedVertices->size();
        for ( unsigned vertex = 0; vertex < tessMesh.VertexCount; ++vertex )
        {
            CPs4ScaleformHal::CapturedVertex captured;
            captured.x = vertices[vertex].x;
            captured.y = vertices[vertex].y;
            captured.gradientU = captured.gradientV = 0.0f;
            captured.color = color;
            capturedVertices->push_back( captured );
        }
        const uint32_t firstIndex = capturedIndices->size();
        std::vector< Scaleform::UInt16 > indices( triangleCount * 3 );
        generator.mTess.GetTrianglesI16( mesh, &indices[0], 0, triangleCount );
        for ( unsigned index = 0; index < triangleCount * 3; ++index )
            capturedIndices->push_back( static_cast< uint16_t >(
                firstVertex + indices[index] ) );

        CPs4ScaleformHal::CapturedBatch batch;
        batch.firstVertex = firstVertex;
        batch.vertexCount = tessMesh.VertexCount;
        batch.firstIndex = firstIndex;
        batch.indexCount = triangleCount * 3;
        batch.color = color;
        batch.complexFill = false;
        batch.gradientFill = false;
        batch.imageFill = false;
        batch.textFill = true;
        batch.packedTextFill = false;
        capturedDraws->push_back( batch );
        if ( glyphVertices )
            *glyphVertices += tessMesh.VertexCount;
        if ( glyphTriangles )
            *glyphTriangles += triangleCount;
        emitted = true;
    }
    generator.Clear();
    return emitted;
}

bool CapturePackedGlyph( const Scaleform::Render::TextureGlyph *glyph,
    float fontSize, float textureHeight, float penX, float penY, uint32_t color,
    const Scaleform::Render::Matrix2F &viewMatrix,
    std::vector< CPs4ScaleformHal::CapturedVertex > *capturedVertices,
    std::vector< uint16_t > *capturedIndices,
    std::vector< CPs4ScaleformHal::CapturedBatch > *capturedDraws,
    std::vector< uint32_t > *atlasPixels,
    std::vector< const void * > *glyphKeys,
    std::vector< uint32_t > *glyphColors )
{
    if ( !glyph || !glyph->pImage || fontSize <= 0.0f || textureHeight <= 0.0f ||
         !capturedVertices || !capturedIndices || !capturedDraws ||
         !atlasPixels || !glyphKeys || !glyphColors ||
         capturedVertices->size() + 4 > 65535 ||
         capturedIndices->size() + 6 > 196605 || capturedDraws->size() >= 4096 )
        return false;

    unsigned tile = 0;
    for ( ; tile < glyphKeys->size(); ++tile )
        if ( ( *glyphKeys )[tile] == glyph && ( *glyphColors )[tile] == color )
            break;
    const Scaleform::Render::ImageSize imageSize = glyph->pImage->GetSize();
    if ( tile == glyphKeys->size() )
    {
        if ( tile >= kFontAtlasColumns * kFontAtlasRows )
            return false;

        // File-loaded GFx font images are commonly wrapped in ImageDelegate.
        // GetImageType() deliberately forwards the wrapped type, so casting the
        // outer object after checking that value corrupts the RawImage access.
        Scaleform::Render::Image *resolvedImage = glyph->pImage.GetPtr();
        const bool delegatedImage = resolvedImage && resolvedImage->IsDelegate();
        for ( unsigned depth = 0; resolvedImage && resolvedImage->IsDelegate() && depth < 8;
              ++depth )
        {
            Scaleform::Render::Image *next = resolvedImage->GetAsImage();
            if ( !next || next == resolvedImage )
                break;
            resolvedImage = next;
        }
        if ( !resolvedImage ||
             resolvedImage->GetImageType() != Scaleform::Render::ImageBase::Type_RawImage )
        {
            static bool loggedUnsupportedFontImage = false;
            if ( !loggedUnsupportedFontImage )
            {
                const Scaleform::Render::ImageSize resolvedSize = resolvedImage
                    ? resolvedImage->GetSize() : Scaleform::Render::ImageSize();
                char imageMessage[224];
                snprintf( imageMessage, sizeof( imageMessage ),
                    "kisak-ps4: scaleform font image rejected delegate=%u type=%u format=%u size=%ux%u",
                    delegatedImage ? 1u : 0u,
                    resolvedImage ? static_cast< unsigned int >(
                        resolvedImage->GetImageType() ) : ~0u,
                    resolvedImage ? static_cast< unsigned int >(
                        resolvedImage->GetFormatNoConv() ) : ~0u,
                    resolvedSize.Width, resolvedSize.Height );
                KisakPs4StartupBreadcrumb( imageMessage );
                loggedUnsupportedFontImage = true;
            }
            return false;
        }
        Scaleform::Render::ImageData imageData;
        Scaleform::Render::RawImage *rawImage =
            static_cast< Scaleform::Render::RawImage * >( resolvedImage );
        if ( !rawImage->GetImageData( &imageData ) )
            return false;
        if ( atlasPixels->empty() )
            atlasPixels->assign( kFontAtlasWidth * kFontAtlasHeight, 0 );

        const unsigned sourceX = static_cast< unsigned >( std::max( 0.0f,
            floorf( glyph->UvBounds.x1 * imageSize.Width ) ) );
        const unsigned sourceY = static_cast< unsigned >( std::max( 0.0f,
            floorf( glyph->UvBounds.y1 * imageSize.Height ) ) );
        const unsigned sourceX2 = static_cast< unsigned >( std::min(
            static_cast< float >( imageSize.Width ),
            ceilf( glyph->UvBounds.x2 * imageSize.Width ) ) );
        const unsigned sourceY2 = static_cast< unsigned >( std::min(
            static_cast< float >( imageSize.Height ),
            ceilf( glyph->UvBounds.y2 * imageSize.Height ) ) );
        const unsigned copyWidth = std::min( kFontTileSize,
            sourceX2 > sourceX ? sourceX2 - sourceX : 0u );
        const unsigned copyHeight = std::min( kFontTileSize,
            sourceY2 > sourceY ? sourceY2 - sourceY : 0u );
        if ( copyWidth == 0 || copyHeight == 0 )
            return false;
        const unsigned tileX = ( tile % kFontAtlasColumns ) * kFontTileSize;
        const unsigned tileY = ( tile / kFontAtlasColumns ) * kFontTileSize;
        Scaleform::Render::Color layoutColor( color );
        unsigned nonzeroCoverage = 0;
        unsigned maxCoverage = 0;
        for ( unsigned y = 0; y < copyHeight; ++y )
            for ( unsigned x = 0; x < copyWidth; ++x )
            {
                const unsigned sampleX = sourceX + x;
                const unsigned sampleY = sourceY + y;
                const Scaleform::UByte *scanline = imageData.GetScanline( sampleY );
                unsigned coverage = 255;
                switch ( imageData.GetFormatNoConv() )
                {
                case Scaleform::Render::Image_A8:
                    // ImageData::GetPixel historically indexes A8 as x * 4;
                    // packed font atlases are one byte per pixel.
                    coverage = scanline[sampleX];
                    break;
                case Scaleform::Render::Image_R8G8B8A8:
                case Scaleform::Render::Image_B8G8R8A8:
                    coverage = scanline[sampleX * 4 + 3];
                    break;
                default:
                    coverage = imageData.GetPixel( sampleX, sampleY ).GetAlpha();
                    break;
                }
                if ( coverage )
                    ++nonzeroCoverage;
                maxCoverage = std::max( maxCoverage, coverage );
                const unsigned alpha = coverage * layoutColor.GetAlpha() / 255;
                ( *atlasPixels )[( tileY + y ) * kFontAtlasWidth + tileX + x] =
                    layoutColor.GetRed() | ( layoutColor.GetGreen() << 8 ) |
                    ( layoutColor.GetBlue() << 16 ) | ( alpha << 24 );
            }
        static bool loggedFontImage = false;
        if ( !loggedFontImage )
        {
            char imageMessage[256];
            snprintf( imageMessage, sizeof( imageMessage ),
                "kisak-ps4: scaleform font image resolved delegate=%u type=%u format=%u size=%ux%u copy=%ux%u nonzero=%u max=%u",
                delegatedImage ? 1u : 0u,
                static_cast< unsigned int >( resolvedImage->GetImageType() ),
                static_cast< unsigned int >( imageData.GetFormatNoConv() ),
                imageData.GetWidth(), imageData.GetHeight(), copyWidth, copyHeight,
                nonzeroCoverage, maxCoverage );
            KisakPs4StartupBreadcrumb( imageMessage );
            loggedFontImage = true;
        }
        glyphKeys->push_back( glyph );
        glyphColors->push_back( color );
    }

    const unsigned sourceWidth = std::max( 1u, static_cast< unsigned >(
        ceilf( ( glyph->UvBounds.x2 - glyph->UvBounds.x1 ) * imageSize.Width ) ) );
    const unsigned sourceHeight = std::max( 1u, static_cast< unsigned >(
        ceilf( ( glyph->UvBounds.y2 - glyph->UvBounds.y1 ) * imageSize.Height ) ) );
    const unsigned tileX = ( tile % kFontAtlasColumns ) * kFontTileSize;
    const unsigned tileY = ( tile / kFontAtlasColumns ) * kFontTileSize;
    const float u1 = ( tileX + 0.5f ) / static_cast< float >( kFontAtlasWidth );
    const float v1 = ( tileY + 0.5f ) / static_cast< float >( kFontAtlasHeight );
    const float u2 = ( tileX + std::min( kFontTileSize, sourceWidth ) - 0.5f ) /
        static_cast< float >( kFontAtlasWidth );
    const float v2 = ( tileY + std::min( kFontTileSize, sourceHeight ) - 0.5f ) /
        static_cast< float >( kFontAtlasHeight );

    const float scaleX = fontSize / textureHeight * imageSize.Width;
    const float scaleY = fontSize / textureHeight * imageSize.Height;
    float positions[8] = {
        ( glyph->UvBounds.x1 - glyph->UvOrigin.x ) * scaleX + penX,
        ( glyph->UvBounds.y1 - glyph->UvOrigin.y ) * scaleY + penY,
        ( glyph->UvBounds.x2 - glyph->UvOrigin.x ) * scaleX + penX,
        ( glyph->UvBounds.y1 - glyph->UvOrigin.y ) * scaleY + penY,
        ( glyph->UvBounds.x2 - glyph->UvOrigin.x ) * scaleX + penX,
        ( glyph->UvBounds.y2 - glyph->UvOrigin.y ) * scaleY + penY,
        ( glyph->UvBounds.x1 - glyph->UvOrigin.x ) * scaleX + penX,
        ( glyph->UvBounds.y2 - glyph->UvOrigin.y ) * scaleY + penY
    };
    const float uvs[8] = { u1, v1, u2, v1, u2, v2, u1, v2 };
    const uint32_t firstVertex = capturedVertices->size();
    for ( unsigned vertex = 0; vertex < 4; ++vertex )
    {
        CPs4ScaleformHal::CapturedVertex captured;
        captured.x = positions[vertex * 2 + 0];
        captured.y = positions[vertex * 2 + 1];
        viewMatrix.Transform( &captured.x, &captured.y );
        captured.gradientU = uvs[vertex * 2 + 0];
        captured.gradientV = uvs[vertex * 2 + 1];
        captured.color = color;
        capturedVertices->push_back( captured );
    }
    const uint16_t quad[6] = {
        static_cast< uint16_t >( firstVertex + 0 ),
        static_cast< uint16_t >( firstVertex + 1 ),
        static_cast< uint16_t >( firstVertex + 2 ),
        static_cast< uint16_t >( firstVertex + 2 ),
        static_cast< uint16_t >( firstVertex + 3 ),
        static_cast< uint16_t >( firstVertex + 0 )
    };
    const uint32_t firstIndex = capturedIndices->size();
    capturedIndices->insert( capturedIndices->end(), quad, quad + 6 );
    CPs4ScaleformHal::CapturedBatch batch;
    batch.firstVertex = firstVertex;
    batch.vertexCount = 4;
    batch.firstIndex = firstIndex;
    batch.indexCount = 6;
    batch.color = color;
    batch.complexFill = false;
    batch.gradientFill = false;
    batch.imageFill = false;
    batch.textFill = false;
    batch.packedTextFill = true;
    capturedDraws->push_back( batch );
    return true;
}
#endif
}

CPs4ScaleformHal::CPs4ScaleformHal()
    : m_frameOpen( false ), m_frame( 0 ), m_capturedTrees( 0 ), m_pendingBatches( 0 ),
      m_treeStatsLoggedMask( 0 ), m_treeDrawableLoggedMask( 0 ),
      m_menuVisibilitySignature( 0 ), m_menuTopologySignature( 0 ),
      m_visibilityRebuilds( 0 ), m_menuVisibilityValid( false ),
      m_menuTopologyValid( false ), m_dynamicRefreshUntilFrame( 0 ),
      m_gradientTileCount( 0 )
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

void CPs4ScaleformHal::RequestDynamicRefresh( uint32_t frames )
{
    const uint64_t requested = m_frame + frames;
    if ( requested > m_dynamicRefreshUntilFrame )
        m_dynamicRefreshUntilFrame = requested;
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
    TreeStats *stats, bool parentVisible,
    const Scaleform::Render::Cxform &parentCxform )
{
    if ( !node || !stats )
        return;

    if ( stats->totalNodes >= kMaxTreeNodes )
    {
        stats->truncated = true;
        return;
    }

    ++stats->totalNodes;
    const bool visible = parentVisible && node->IsVisible();
    if ( !visible )
    {
        ++stats->hiddenSubtrees;
        return;
    }
    ++stats->visibleNodes;

    Scaleform::Render::Cxform cumulativeCxform = node->GetCxform();
    cumulativeCxform.Append( parentCxform );
    if ( !node->GetCxform().IsIdentity() )
        ++stats->colorTransforms;

    if ( node->IsMaskNode() )
        ++stats->maskTreeNodes;
    if ( node->HasMask() )
    {
        ++stats->maskOwnerNodes;
        const Scaleform::Render::TreeNode *mask = node->GetMaskNode();
        if ( mask && !mask->Is3D() )
        {
            Scaleform::Render::Matrix2F maskViewMatrix;
            mask->CalcViewMatrix( &maskViewMatrix );
            const Scaleform::Render::RectF maskBounds =
                maskViewMatrix.EncloseTransform( mask->GetAproxLocalBounds() );
            if ( maskBounds.Width() > 0.0f && maskBounds.Height() > 0.0f )
            {
                ++stats->maskViewBounds;
                static unsigned int loggedMasks = 0;
                if ( loggedMasks < 8 )
                {
                    char maskMessage[224];
                    snprintf( maskMessage, sizeof( maskMessage ),
                        "kisak-ps4: scaleform mask owner_type=%u mask_type=%u bounds=%.2f,%.2f..%.2f,%.2f",
                        static_cast< unsigned int >(
                            node->GetReadOnlyData()->GetType() ),
                        static_cast< unsigned int >(
                            mask->GetReadOnlyData()->GetType() ),
                        maskBounds.x1, maskBounds.y1,
                        maskBounds.x2, maskBounds.y2 );
                    KisakPs4StartupBreadcrumb( maskMessage );
                    ++loggedMasks;
                }
            }
        }
    }

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
                static bool loggedAncestorChain = false;
                if ( !loggedAncestorChain )
                {
                    const Scaleform::Render::TreeNode *ancestor = shapeNode;
                    unsigned depth = 0;
                    while ( ancestor && depth < 12 )
                    {
                        const Scaleform::Render::Matrix2F &matrix = ancestor->M2D();
                        char chainMessage[256];
                        snprintf( chainMessage, sizeof( chainMessage ),
                            "kisak-ps4: scaleform ancestor depth=%u type=%u m=[%.5f %.5f %.2f; %.5f %.5f %.2f]",
                            depth,
                            static_cast< unsigned int >(
                                ancestor->GetReadOnlyData()->GetType() ),
                            matrix.M[0][0], matrix.M[0][1], matrix.M[0][3],
                            matrix.M[1][0], matrix.M[1][1], matrix.M[1][3] );
                        KisakPs4StartupBreadcrumb( chainMessage );
                        ancestor = ancestor->GetParent();
                        ++depth;
                    }
                    loggedAncestorChain = true;
                }
                Scaleform::Render::Matrix2F viewMatrix;
                shapeNode->CalcViewMatrix( &viewMatrix );
                const float determinant = viewMatrix.M[0][0] * viewMatrix.M[1][1] -
                    viewMatrix.M[0][1] * viewMatrix.M[1][0];
                if ( determinant > -0.000001f && determinant < 0.000001f )
                    ++stats->degenerateTransforms;
                const unsigned layerCount = shape->GetLayerCount();
                stats->shapeLayers += layerCount;
                for ( unsigned layer = 0; layer < layerCount; ++layer )
                {
                    if ( stats->collectGeometry && TessellateShapeLayer( shape, layer,
                            viewMatrix, cumulativeCxform,
                             &stats->tessellatedVertices, &stats->tessellatedTriangles,
                             &m_capturedVertices, &m_capturedIndices, &m_capturedDraws,
                             &m_gradientPixels, &m_gradientTileCount ) )
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
    {
        ++stats->textNodes;
        if ( stats->collectGeometry )
        {
            Scaleform::Render::TreeText *textNode =
                const_cast< Scaleform::Render::TreeText * >(
                    static_cast< const Scaleform::Render::TreeText * >( node ) );
            const Scaleform::Render::TextLayout *layout = textNode->GetLayout();
            if ( layout )
            {
                Scaleform::Render::Matrix2F viewMatrix;
                textNode->CalcViewMatrix( &viewMatrix );
                Scaleform::Render::Font *font = NULL;
                float fontSize = 0.0f;
                float penX = 0.0f;
                float penY = 0.0f;
                uint32_t color = 0xffffffffu;
                Scaleform::Ptr< Scaleform::Render::GlyphShape > temporaryShape =
                    *SF_HEAP_NEW( Scaleform::Memory::GetGlobalHeap() )
                        Scaleform::Render::GlyphShape;
                Scaleform::Render::TextLayout::Record record;
                Scaleform::UPInt position = 0;
                while ( ( position = layout->ReadNext( position, &record ) ) != 0 )
                {
                    switch ( layout->GetRecordType( record ) )
                    {
                    case Scaleform::Render::TextLayout::Record_Font:
                        font = record.mFont.pFont;
                        fontSize = record.mFont.mSize;
                        if ( font )
                        {
                            static const Scaleform::Render::Font *loggedFonts[8] = {};
                            static unsigned loggedFontCount = 0;
                            bool alreadyLogged = false;
                            for ( unsigned i = 0; i < loggedFontCount; ++i )
                                alreadyLogged = alreadyLogged || loggedFonts[i] == font;
                            if ( !alreadyLogged && loggedFontCount < 8 )
                            {
                                loggedFonts[loggedFontCount++] = font;
                                const char *fontName = font->GetName();
                                const bool emptyFont =
                                    ( !fontName || !fontName[0] ) &&
                                    !font->HasVectorOrRasterGlyphs() &&
                                    !font->HasTextureGlyphs() &&
                                    font->GetGlyphShapeCount() == 0;
                                char fontMessage[256];
                                snprintf( fontMessage, sizeof( fontMessage ),
                                    "kisak-ps4: scaleform text font name=%s flags=0x%x resolved=%u empty=%u stripped=%u glyphs=%u vector=%u texture=%u texheight=%.2f nominal=%.2f",
                                    fontName ? fontName : "",
                                    font->GetFontFlags(), font->IsResolved() ? 1u : 0u,
                                    emptyFont ? 1u : 0u,
                                    font->GlyphShapesStripped() ? 1u : 0u,
                                    font->GetGlyphShapeCount(),
                                    font->HasVectorOrRasterGlyphs() ? 1u : 0u,
                                    font->HasTextureGlyphs() ? 1u : 0u,
                                    font->GetTextureGlyphHeight(),
                                    font->GetNominalGlyphHeight() );
                                KisakPs4StartupBreadcrumb( fontMessage );
                            }
                        }
                        break;
                    case Scaleform::Render::TextLayout::Record_Color:
                        color = record.mColor.mColor;
                        break;
                    case Scaleform::Render::TextLayout::Record_NewLine:
                        penX = record.mLine.x;
                        penY = record.mLine.y;
                        break;
                    case Scaleform::Render::TextLayout::Record_Char:
                    {
                        ++stats->textGlyphRecords;
                        if ( font && fontSize > 0.0f &&
                             ( record.mChar.Flags &
                               Scaleform::Render::TextLayout::Flag_Invisible ) == 0 )
                        {
                            static bool tracedFirstVectorGlyph = false;
                            const bool traceVectorGlyph = !tracedFirstVectorGlyph;
                            if ( traceVectorGlyph )
                            {
                                char glyphMessage[192];
                                snprintf( glyphMessage, sizeof( glyphMessage ),
                                    "kisak-ps4: scaleform vector glyph begin index=%u size=%.2f advance=%.2f",
                                    record.mChar.GlyphIndex, fontSize,
                                    record.mChar.Advance );
                                KisakPs4StartupBreadcrumb( glyphMessage );
                            }
                            const Scaleform::Render::TextureGlyph *textureGlyph =
                                font->GetTextureGlyph( record.mChar.GlyphIndex );
                            if ( traceVectorGlyph )
                                KisakPs4StartupBreadcrumb( textureGlyph
                                    ? "kisak-ps4: scaleform vector glyph texture lookup found"
                                    : "kisak-ps4: scaleform vector glyph texture lookup empty" );
                            const bool packedCaptured = textureGlyph &&
                                CapturePackedGlyph( textureGlyph,
                                    fontSize, font->GetTextureGlyphHeight(), penX, penY,
                                    cumulativeCxform.Transform(
                                        Scaleform::Render::Color( color ) ).Raw,
                                    viewMatrix, &m_capturedVertices,
                                    &m_capturedIndices, &m_capturedDraws,
                                    &m_fontAtlasPixels, &m_fontGlyphKeys,
                                    &m_fontGlyphColors );
                            if ( packedCaptured )
                                ++stats->packedTextGlyphs;
                            else
                            {
                                const Scaleform::Render::ShapeDataInterface *glyphShape =
                                    font->GetPermanentGlyphShape( record.mChar.GlyphIndex );
                                if ( traceVectorGlyph )
                                    KisakPs4StartupBreadcrumb( glyphShape
                                        ? "kisak-ps4: scaleform vector glyph permanent shape found"
                                        : "kisak-ps4: scaleform vector glyph temporary shape begin" );
                                const bool temporaryReady = !glyphShape && temporaryShape &&
                                    font->GetTemporaryGlyphShape(
                                        record.mChar.GlyphIndex,
                                        static_cast< unsigned >( fontSize + 0.5f ),
                                        temporaryShape.GetPtr() );
                                if ( temporaryReady )
                                    glyphShape = temporaryShape.GetPtr();
                                if ( traceVectorGlyph )
                                    KisakPs4StartupBreadcrumb( glyphShape && !glyphShape->IsEmpty()
                                        ? "kisak-ps4: scaleform vector glyph shape ready"
                                        : "kisak-ps4: scaleform vector glyph shape unavailable" );
                                const float nominalHeight = font->GetNominalGlyphHeight();
                                const bool glyphCaptured = glyphShape && nominalHeight > 0.0f &&
                                    TessellateGlyphShape( glyphShape, viewMatrix,
                                        fontSize / nominalHeight, penX, penY,
                                        cumulativeCxform.Transform(
                                            Scaleform::Render::Color( color ) ).Raw,
                                        &m_capturedVertices, &m_capturedIndices,
                                        &m_capturedDraws, &stats->textGlyphVertices,
                                        &stats->textGlyphTriangles );
                                if ( glyphCaptured )
                                    ++stats->textGlyphShapes;
                                if ( traceVectorGlyph )
                                {
                                    KisakPs4StartupBreadcrumb( glyphCaptured
                                        ? "kisak-ps4: scaleform vector glyph tessellation complete"
                                        : "kisak-ps4: scaleform vector glyph tessellation empty" );
                                    tracedFirstVectorGlyph = true;
                                }
                            }
                        }
                        penX += record.mChar.Advance;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
        break;
    }
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
        CollectTreeStats( container->GetAt( i ), stats, visible, cumulativeCxform );
}

void CPs4ScaleformHal::AccumulateTreeSignature(
    const Scaleform::Render::TreeNode *node, uint64_t *signature,
    uint32_t *visited ) const
{
    if ( !node || !signature || !visited || *visited >= kMaxTreeNodes )
        return;

    ++*visited;
    const uint64_t value =
        ( static_cast< uint64_t >( node->GetReadOnlyData()->GetType() ) << 3 ) |
        ( node->IsVisible() ? 1u : 0u ) |
        ( node->HasMask() ? 2u : 0u ) |
        ( node->IsMaskNode() ? 4u : 0u );
    *signature ^= value + 0x9e3779b97f4a7c15ull +
        ( *signature << 6 ) + ( *signature >> 2 );

    if ( !node->IsVisible() )
        return;

    const Scaleform::Render::Matrix2F &matrix = node->M2D();
    const Scaleform::Render::Cxform &cxform = node->GetCxform();
    for ( unsigned row = 0; row < 2; ++row )
    {
        for ( unsigned column = 0; column < 4; ++column )
        {
            uint32_t matrixBits = 0;
            uint32_t cxformBits = 0;
            memcpy( &matrixBits, &matrix.M[row][column], sizeof( matrixBits ) );
            memcpy( &cxformBits, &cxform.M[row][column], sizeof( cxformBits ) );
            *signature ^= matrixBits + 0x9e3779b97f4a7c15ull +
                ( *signature << 6 ) + ( *signature >> 2 );
            *signature ^= cxformBits + 0x9e3779b97f4a7c15ull +
                ( *signature << 6 ) + ( *signature >> 2 );
        }
    }

    const Scaleform::Render::Context::EntryData::EntryType type =
        node->GetReadOnlyData()->GetType();
    if ( type != Scaleform::Render::Context::EntryData::ET_Root &&
         type != Scaleform::Render::Context::EntryData::ET_Container )
        return;

    const Scaleform::Render::TreeContainer *container =
        static_cast< const Scaleform::Render::TreeContainer * >( node );
    *signature ^= static_cast< uint64_t >( container->GetSize() ) +
        0x9e3779b97f4a7c15ull + ( *signature << 6 ) + ( *signature >> 2 );
    for ( Scaleform::UPInt i = 0; i < container->GetSize(); ++i )
        AccumulateTreeSignature( container->GetAt( i ), signature, visited );
}

void CPs4ScaleformHal::AccumulateTopologySignature(
    const Scaleform::Render::TreeNode *node, uint64_t *signature,
    uint32_t *visited ) const
{
    if ( !node || !signature || !visited || *visited >= kMaxTreeNodes )
        return;

    ++*visited;
    const uint64_t value =
        ( static_cast< uint64_t >( node->GetReadOnlyData()->GetType() ) << 3 ) |
        ( node->IsVisible() ? 1u : 0u ) |
        ( node->HasMask() ? 2u : 0u ) |
        ( node->IsMaskNode() ? 4u : 0u );
    *signature ^= value + 0x9e3779b97f4a7c15ull +
        ( *signature << 6 ) + ( *signature >> 2 );
    if ( !node->IsVisible() )
        return;

    const Scaleform::Render::Context::EntryData::EntryType type =
        node->GetReadOnlyData()->GetType();
    if ( type != Scaleform::Render::Context::EntryData::ET_Root &&
         type != Scaleform::Render::Context::EntryData::ET_Container )
        return;

    const Scaleform::Render::TreeContainer *container =
        static_cast< const Scaleform::Render::TreeContainer * >( node );
    *signature ^= static_cast< uint64_t >( container->GetSize() ) +
        0x9e3779b97f4a7c15ull + ( *signature << 6 ) + ( *signature >> 2 );
    for ( Scaleform::UPInt i = 0; i < container->GetSize(); ++i )
        AccumulateTopologySignature( container->GetAt( i ), signature, visited );
}

bool CPs4ScaleformHal::QueueCapturedTree( Scaleform::Render::TreeRoot *root,
    const char *phase )
{
    if ( !m_frameOpen || !root )
        return false;

    const uint32_t statsBit = phase && phase[0] == 'h' ? 2u : 1u;
    uint64_t visibilitySignature = 0xcbf29ce484222325ull;
    uint32_t visibilityNodes = 0;
    AccumulateTreeSignature( root, &visibilitySignature, &visibilityNodes );
    const bool menuVisualChanged = statsBit == 1u &&
        ( !m_menuVisibilityValid || visibilitySignature != m_menuVisibilitySignature );
    uint64_t topologySignature = 0xcbf29ce484222325ull;
    uint32_t topologyNodes = 0;
    AccumulateTopologySignature( root, &topologySignature, &topologyNodes );
    const bool menuTopologyChanged = statsBit == 1u &&
        ( !m_menuTopologyValid || topologySignature != m_menuTopologySignature );
    if ( statsBit == 1u )
    {
        m_menuVisibilitySignature = visibilitySignature;
        m_menuVisibilityValid = true;
        m_menuTopologySignature = topologySignature;
        m_menuTopologyValid = true;
        if ( menuTopologyChanged )
            RequestDynamicRefresh( 30 );
    }

    const bool dynamicRefreshActive = m_frame <= 90 ||
        m_frame <= m_dynamicRefreshUntilFrame;

    m_lastTreeStats = TreeStats();
    m_lastTreeStats.collectGeometry = statsBit == 1u
        ? ( menuTopologyChanged || ( menuVisualChanged && dynamicRefreshActive ) )
        : ( m_treeDrawableLoggedMask & statsBit ) == 0;
    if ( m_lastTreeStats.collectGeometry && statsBit == 1u )
    {
        m_capturedVertices.clear();
        m_capturedIndices.clear();
        m_capturedDraws.clear();
        m_gradientPixels.clear();
        m_gradientTileCount = 0;
        m_fontAtlasPixels.clear();
        m_fontGlyphKeys.clear();
        m_fontGlyphColors.clear();
        m_capturedVertices.reserve( 4096 );
        m_capturedIndices.reserve( 12288 );
        m_capturedDraws.reserve( 256 );
        if ( m_treeDrawableLoggedMask & statsBit )
        {
            ++m_visibilityRebuilds;
            if ( m_visibilityRebuilds <= 8 )
            {
                char rebuildMessage[192];
                snprintf( rebuildMessage, sizeof( rebuildMessage ),
                    "kisak-ps4: scaleform visibility rebuild=%u nodes=%u signature=%llx",
                    m_visibilityRebuilds, visibilityNodes,
                    static_cast< unsigned long long >( visibilitySignature ) );
                KisakPs4StartupBreadcrumb( rebuildMessage );
            }
        }
    }
    if ( statsBit == 1u &&
         ( m_frame == 60 || m_frame == 120 || m_frame == 300 || m_frame == 600 ) )
    {
        char rebuildHeartbeat[176];
        snprintf( rebuildHeartbeat, sizeof( rebuildHeartbeat ),
            "kisak-ps4: scaleform rebuild heartbeat frame=%llu total=%u changed=%u nodes=%u",
            static_cast< unsigned long long >( m_frame ), m_visibilityRebuilds,
            ( menuVisualChanged && dynamicRefreshActive ) ? 1u : 0u,
            visibilityNodes );
        KisakPs4StartupBreadcrumb( rebuildHeartbeat );
    }
    CollectTreeStats( root, &m_lastTreeStats, true,
        Scaleform::Render::Cxform::Identity );
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
            "kisak-ps4: scaleform tree stats phase=%s total=%u visible=%u hidden_subtrees=%u containers=%u shapes=%u meshes=%u text=%u mask_owners=%u mask_nodes=%u mask_bounds=%u viewport=%u truncated=%u",
            phase ? phase : "unknown",
            m_lastTreeStats.totalNodes, m_lastTreeStats.visibleNodes,
            m_lastTreeStats.hiddenSubtrees,
            m_lastTreeStats.containerNodes, m_lastTreeStats.shapeNodes,
            m_lastTreeStats.meshNodes, m_lastTreeStats.textNodes,
            m_lastTreeStats.maskOwnerNodes, m_lastTreeStats.maskTreeNodes,
            m_lastTreeStats.maskViewBounds,
            m_lastTreeStats.hasViewport ? 1u : 0u,
            m_lastTreeStats.truncated ? 1u : 0u );
        KisakPs4StartupBreadcrumb( message );

        if ( !m_capturedVertices.empty() && statsBit == 1u )
        {
            float minX = m_capturedVertices[0].x;
            float minY = m_capturedVertices[0].y;
            float maxX = minX;
            float maxY = minY;
            for ( size_t i = 1; i < m_capturedVertices.size(); ++i )
            {
                minX = std::min( minX, m_capturedVertices[i].x );
                minY = std::min( minY, m_capturedVertices[i].y );
                maxX = std::max( maxX, m_capturedVertices[i].x );
                maxY = std::max( maxY, m_capturedVertices[i].y );
            }
            char boundsMessage[192];
            snprintf( boundsMessage, sizeof( boundsMessage ),
                "kisak-ps4: scaleform transformed bounds min=%.2f,%.2f max=%.2f,%.2f",
                minX, minY, maxX, maxY );
            KisakPs4StartupBreadcrumb( boundsMessage );
        }
    }

    if ( ( m_lastTreeStats.shapeNodes || m_lastTreeStats.meshNodes || m_lastTreeStats.textNodes ) &&
         ( m_treeDrawableLoggedMask & statsBit ) == 0 )
    {
        m_treeDrawableLoggedMask |= statsBit;
        char message[384];
        snprintf( message, sizeof( message ),
            "kisak-ps4: scaleform drawable tree phase=%s shapes=%u meshes=%u text=%u layers=%u solid=%u image=%u gradient=%u tess_layers=%u vertices=%u triangles=%u retained_vertices=%u retained_indices=%u retained_batches=%u degenerate=%u cxforms=%u",
            phase ? phase : "unknown", m_lastTreeStats.shapeNodes,
            m_lastTreeStats.meshNodes, m_lastTreeStats.textNodes,
            m_lastTreeStats.shapeLayers, m_lastTreeStats.solidFills,
            m_lastTreeStats.imageFills, m_lastTreeStats.gradientFills,
            m_lastTreeStats.tessellatedLayers, m_lastTreeStats.tessellatedVertices,
            m_lastTreeStats.tessellatedTriangles, CapturedVertexCount(),
            CapturedIndexCount(), CapturedBatchCount(),
            m_lastTreeStats.degenerateTransforms,
            m_lastTreeStats.colorTransforms );
        KisakPs4StartupBreadcrumb( message );
        if ( m_lastTreeStats.textNodes )
        {
            char textMessage[192];
            snprintf( textMessage, sizeof( textMessage ),
                "kisak-ps4: scaleform text capture nodes=%u records=%u packed=%u shapes=%u vertices=%u triangles=%u atlas_glyphs=%u",
                m_lastTreeStats.textNodes, m_lastTreeStats.textGlyphRecords,
                m_lastTreeStats.packedTextGlyphs, m_lastTreeStats.textGlyphShapes,
                m_lastTreeStats.textGlyphVertices,
                m_lastTreeStats.textGlyphTriangles, FontAtlasGlyphCount() );
            KisakPs4StartupBreadcrumb( textMessage );
        }
        if ( statsBit == 1u && !m_capturedDraws.empty() )
        {
            uint32_t kinds[5] = {};
            uint32_t runs = 0;
            int previousKind = -1;
            for ( size_t i = 0; i < m_capturedDraws.size(); ++i )
            {
                const CapturedBatch &batch = m_capturedDraws[i];
                const int kind = batch.imageFill ? 4 :
                    ( batch.packedTextFill ? 3 :
                    ( batch.gradientFill ? 1 : ( batch.textFill ? 2 : 0 ) ) );
                ++kinds[kind];
                if ( kind != previousKind )
                {
                    ++runs;
                    previousKind = kind;
                }
            }
            char orderMessage[224];
            snprintf( orderMessage, sizeof( orderMessage ),
                "kisak-ps4: scaleform ordered diagnostics batches=%u runs=%u solid=%u gradient=%u text=%u packed=%u image=%u",
                CapturedBatchCount(), runs, kinds[0], kinds[1], kinds[2],
                kinds[3], kinds[4] );
            KisakPs4StartupBreadcrumb( orderMessage );
        }
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

bool CPs4ScaleformHal::IsOrderedAtlasBatch( const CapturedBatch &batch )
{
    return !batch.complexFill && !batch.imageFill && !batch.packedTextFill &&
        batch.vertexCount > 0 && batch.indexCount > 0;
}

bool CPs4ScaleformHal::IsDeferredImageBatch( const CapturedBatch &batch )
{
    return batch.imageFill && batch.vertexCount > 0 && batch.indexCount > 0;
}

CPs4ScaleformHal &KisakPs4ScaleformHal()
{
    static CPs4ScaleformHal hal;
    return hal;
}
