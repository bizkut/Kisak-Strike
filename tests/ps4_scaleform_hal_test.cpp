#include "../ps4/scaleform_gnm_hal.h"

#include <assert.h>

extern "C" void KisakPs4StartupBreadcrumb( const char * )
{
}

int main()
{
    CPs4ScaleformHal hal;
    GnmBlendControl blend;
    assert( hal.TranslateBlend( CPs4ScaleformHal::kBlendNormal, &blend ) );
    assert( blend.blendenabled );
    assert( blend.colorsrcmult == GNM_BLEND_SRC_ALPHA );
    assert( blend.colordstmult == GNM_BLEND_ONE_MINUS_SRC_ALPHA );

    uint32_t scissor[4] = { 0, 0, 0, 0 };
    assert( hal.TranslateScissor( -10, 20, 2000, 1100, 1920, 1080, scissor ) );
    assert( scissor[0] == 0 && scissor[1] == 20 &&
        scissor[2] == 1920 && scissor[3] == 1080 );
    assert( !hal.TranslateScissor( 10, 10, 10, 100, 1920, 1080, scissor ) );

    CPs4ScaleformHal::CapturedBatch solid = {
        0, 4, 0, 6, 0xffffffffu, false, false, false, false, false
    };
    assert( CPs4ScaleformHal::IsOrderedAtlasBatch( solid ) );
    CPs4ScaleformHal::CapturedBatch gradient = solid;
    gradient.gradientFill = true;
    assert( CPs4ScaleformHal::IsOrderedAtlasBatch( gradient ) );
    CPs4ScaleformHal::CapturedBatch text = solid;
    text.textFill = true;
    assert( CPs4ScaleformHal::IsOrderedAtlasBatch( text ) );
    CPs4ScaleformHal::CapturedBatch packed = text;
    packed.packedTextFill = true;
    assert( !CPs4ScaleformHal::IsOrderedAtlasBatch( packed ) );
    CPs4ScaleformHal::CapturedBatch complex = solid;
    complex.complexFill = true;
    assert( !CPs4ScaleformHal::IsOrderedAtlasBatch( complex ) );
    CPs4ScaleformHal::CapturedBatch image = solid;
    image.complexFill = true;
    image.imageFill = true;
    assert( !CPs4ScaleformHal::IsOrderedAtlasBatch( image ) );
    assert( CPs4ScaleformHal::IsDeferredImageBatch( image ) );
    CPs4ScaleformHal::CapturedBatch empty = solid;
    empty.indexCount = 0;
    assert( !CPs4ScaleformHal::IsOrderedAtlasBatch( empty ) );

    hal.BeginFrame( 1 );
    assert( !hal.QueueCapturedTree( static_cast< Scaleform::Render::TreeRoot * >( nullptr ), "menu" ) );
    assert( hal.QueueCapturedTree( true, "menu" ) );
    assert( hal.PendingBatches() == 1 );
    assert( hal.LastTreeStats().totalNodes == 0 );
    assert( hal.LastTreeStats().maskOwnerNodes == 0 );
    assert( hal.LastTreeStats().maskTreeNodes == 0 );
    assert( hal.LastTreeStats().maskViewBounds == 0 );
    assert( !hal.LastTreeStats().truncated );
    hal.EndFrame();
    assert( hal.CapturedTrees() == 1 );
    return 0;
}
