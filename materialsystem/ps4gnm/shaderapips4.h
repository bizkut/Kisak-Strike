#ifndef KISAK_SHADERAPIPS4_H
#define KISAK_SHADERAPIPS4_H

#include <gnm_commandbuffer.h>
#include <stdint.h>
#include "materialsystem/imesh.h"

extern "C" uint32_t KisakPs4ApplyShaderShadowState( GnmCommandBuffer *command );
extern "C" void KisakPs4SetShaderShadowCulling( bool enabled );
extern "C" void KisakPs4SetShaderShadowDepth( bool testEnabled,
    bool writesEnabled, int depthFunction );
extern "C" void KisakPs4SetShaderShadowBlend( bool enabled,
    int colorSource, int colorDestination, int colorOperation,
    bool separateAlpha, int alphaSource, int alphaDestination, int alphaOperation );
extern "C" int KisakPs4TextureMemoryUsed();
extern "C" bool KisakPs4SourceBufferProbe();
extern "C" bool KisakPs4DynamicSourceBufferProbe();
extern "C" bool KisakPs4ShaderDeviceDynamicBufferProbe();
extern "C" bool KisakPs4ShaderApiVertexFormatProbe();
extern "C" bool KisakPs4LockDynamicVertices( VertexFormat_t format, int count,
    bool append, VertexDesc_t *desc );
extern "C" void KisakPs4UnlockDynamicVertices( int count, VertexDesc_t *desc );
extern "C" bool KisakPs4LockDynamicIndices( int count, bool append, IndexDesc_t *desc );
extern "C" void KisakPs4UnlockDynamicIndices( int count, IndexDesc_t *desc );
extern "C" bool KisakPs4DynamicMeshBridgeProbe();
extern "C" bool KisakPs4PopulateShaderApiDynamicTriangle();

#endif
