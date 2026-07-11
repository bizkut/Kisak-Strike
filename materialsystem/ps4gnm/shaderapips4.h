#ifndef KISAK_SHADERAPIPS4_H
#define KISAK_SHADERAPIPS4_H

#include <gnm_commandbuffer.h>
#include <stdint.h>

extern "C" uint32_t KisakPs4ApplyShaderShadowState( GnmCommandBuffer *command );
extern "C" void KisakPs4SetShaderShadowCulling( bool enabled );

#endif
