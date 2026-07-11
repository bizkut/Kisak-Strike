#ifndef KISAK_PS4_SHADOW_STATE_TRANSLATE_H
#define KISAK_PS4_SHADOW_STATE_TRANSLATE_H

#include <gnm.h>

GnmDepthCompare Ps4TranslateDepthFunction( int function );
GnmBlendOp Ps4TranslateBlendFactor( int factor );
GnmCombFunc Ps4TranslateBlendOperation( int operation );

GnmDepthStencilControl Ps4BuildDepthStencilControl(
    bool depthTest, bool depthWrites, int function );
GnmBlendControl Ps4BuildBlendControl(
    bool enabled, int colorSource, int colorDestination, int colorOperation,
    bool separateAlpha, int alphaSource, int alphaDestination, int alphaOperation );
GnmPrimitiveSetup Ps4BuildPrimitiveSetup( bool culling, int polygonOffset );
uint32_t Ps4BuildRenderTargetMask( bool colorWrites, bool alphaWrites );

#endif
