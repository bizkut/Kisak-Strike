#include "ps4_shadow_state_translate.h"

#include <assert.h>

int main()
{
    assert( Ps4TranslateDepthFunction( 0 ) == GNM_DEPTH_COMPARE_NEVER );
    assert( Ps4TranslateDepthFunction( 3 ) == GNM_DEPTH_COMPARE_LESSEQUAL );
    assert( Ps4TranslateDepthFunction( 7 ) == GNM_DEPTH_COMPARE_ALWAYS );
    assert( Ps4TranslateBlendFactor( 2 ) == GNM_BLEND_DEST_COLOR );
    assert( Ps4TranslateBlendFactor( 5 ) == GNM_BLEND_ONE_MINUS_SRC_ALPHA );
    assert( Ps4TranslateBlendOperation( 1 ) == GNM_COMB_SRC_MINUS_DST );
    assert( Ps4TranslateBlendOperation( 2 ) == GNM_COMB_DST_MINUS_SRC );

    const GnmDepthStencilControl depth = Ps4BuildDepthStencilControl(
        true, true, 3 );
    assert( depth.depthenable && depth.zwrite && depth.zfunc == GNM_DEPTH_COMPARE_LESSEQUAL );

    const GnmBlendControl blend = Ps4BuildBlendControl(
        true, 4, 5, 0, true, 1, 0, 0 );
    assert( blend.blendenabled && blend.separatealphaenable );
    assert( blend.colorsrcmult == GNM_BLEND_SRC_ALPHA );
    assert( blend.colordstmult == GNM_BLEND_ONE_MINUS_SRC_ALPHA );

    const GnmPrimitiveSetup raster = Ps4BuildPrimitiveSetup( true, 1 );
    assert( raster.cullmode == GNM_CULL_BACK );
    assert( raster.frontmode == GNM_FILL_SOLID && raster.backmode == GNM_FILL_SOLID );
    assert( raster.frontoffsetmode && raster.backoffsetmode );
    assert( Ps4BuildRenderTargetMask( true, true ) == 0xfu );
    assert( Ps4BuildRenderTargetMask( true, false ) == 0x7u );
    assert( Ps4BuildRenderTargetMask( false, true ) == 0x8u );
    return 0;
}
