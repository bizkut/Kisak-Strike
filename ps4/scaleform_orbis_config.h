#pragma once

#include <GFxConfig.h>

// This SDK snapshot has no pthread/OpenOrbis implementation of the Scaleform
// Thread primitives. Keep the first Kernel/GFx bring-up single-threaded; a
// native implementation must land before threaded loading is enabled.
#undef SF_ENABLE_THREADS

// AMP profiling pulls the renderer graph into the Kernel through SF_System.
// Keep it disabled for the console runtime and for the isolated Kernel gate.
#undef SF_AMP_SERVER

// External codec libraries enter after the core player can construct and link.
#undef SF_ENABLE_LIBJPEG
#undef SF_ENABLE_LIBPNG

// Keep runtime ActionScript failures visible in the PS4 bring-up build even
// though the shared SDK release configuration undefines this diagnostic macro.
// Opcode tracing remains disabled; KisakScaleformLog bounds the error stream.
#define GFX_AS2_VERBOSE_ERRORS
