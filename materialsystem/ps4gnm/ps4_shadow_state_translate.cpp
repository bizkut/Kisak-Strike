#include "ps4_shadow_state_translate.h"

GnmDepthCompare Ps4TranslateDepthFunction( int function )
{
    static const GnmDepthCompare functions[] = {
        GNM_DEPTH_COMPARE_NEVER,
        GNM_DEPTH_COMPARE_LESS,
        GNM_DEPTH_COMPARE_EQUAL,
        GNM_DEPTH_COMPARE_LESSEQUAL,
        GNM_DEPTH_COMPARE_GREATER,
        GNM_DEPTH_COMPARE_NOTEQUAL,
        GNM_DEPTH_COMPARE_GREATEREQUAL,
        GNM_DEPTH_COMPARE_ALWAYS
    };
    const unsigned int index = static_cast< unsigned int >( function );
    return index < sizeof( functions ) / sizeof( functions[0] )
        ? functions[index] : GNM_DEPTH_COMPARE_ALWAYS;
}

GnmBlendOp Ps4TranslateBlendFactor( int factor )
{
    static const GnmBlendOp factors[] = {
        GNM_BLEND_ZERO,
        GNM_BLEND_ONE,
        GNM_BLEND_DEST_COLOR,
        GNM_BLEND_ONE_MINUS_DEST_COLOR,
        GNM_BLEND_SRC_ALPHA,
        GNM_BLEND_ONE_MINUS_SRC_ALPHA,
        GNM_BLEND_DEST_ALPHA,
        GNM_BLEND_ONE_MINUS_DEST_ALPHA,
        GNM_BLEND_SRC_ALPHA_SATURATE,
        GNM_BLEND_SRC_COLOR,
        GNM_BLEND_ONE_MINUS_SRC_COLOR
    };
    const unsigned int index = static_cast< unsigned int >( factor );
    return index < sizeof( factors ) / sizeof( factors[0] )
        ? factors[index] : GNM_BLEND_ONE;
}

GnmCombFunc Ps4TranslateBlendOperation( int operation )
{
    static const GnmCombFunc operations[] = {
        GNM_COMB_DST_PLUS_SRC,
        GNM_COMB_SRC_MINUS_DST,
        GNM_COMB_DST_MINUS_SRC,
        GNM_COMB_MIN_DST_SRC,
        GNM_COMB_MAX_DST_SRC
    };
    const unsigned int index = static_cast< unsigned int >( operation );
    return index < sizeof( operations ) / sizeof( operations[0] )
        ? operations[index] : GNM_COMB_DST_PLUS_SRC;
}

GnmDepthStencilControl Ps4BuildDepthStencilControl(
    bool depthTest, bool depthWrites, int function )
{
    GnmDepthStencilControl control = {};
    control.depthenable = depthTest;
    control.zwrite = depthWrites;
    control.zfunc = Ps4TranslateDepthFunction( function );
    return control;
}

GnmBlendControl Ps4BuildBlendControl(
    bool enabled, int colorSource, int colorDestination, int colorOperation,
    bool separateAlpha, int alphaSource, int alphaDestination, int alphaOperation )
{
    GnmBlendControl control = {};
    control.blendenabled = enabled;
    control.colorfunc = Ps4TranslateBlendOperation( colorOperation );
    control.colorsrcmult = Ps4TranslateBlendFactor( colorSource );
    control.colordstmult = Ps4TranslateBlendFactor( colorDestination );
    control.alphafunc = Ps4TranslateBlendOperation( alphaOperation );
    control.alphasrcmult = Ps4TranslateBlendFactor( alphaSource );
    control.alphadstmult = Ps4TranslateBlendFactor( alphaDestination );
    control.separatealphaenable = separateAlpha;
    return control;
}

GnmPrimitiveSetup Ps4BuildPrimitiveSetup( bool culling, int polygonOffset )
{
    GnmPrimitiveSetup setup = {};
    setup.cullmode = culling ? GNM_CULL_BACK : GNM_CULL_NONE;
    setup.frontface = GNM_FACE_CCW;
    setup.frontmode = GNM_FILL_SOLID;
    setup.backmode = GNM_FILL_SOLID;
    setup.frontoffsetmode = polygonOffset != 0;
    setup.backoffsetmode = polygonOffset != 0;
    return setup;
}

uint32_t Ps4BuildRenderTargetMask( bool colorWrites, bool alphaWrites )
{
    return ( colorWrites ? 0x7u : 0u ) | ( alphaWrites ? 0x8u : 0u );
}
