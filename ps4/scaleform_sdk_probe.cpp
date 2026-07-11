#include "GFxVersion.h"
#include "Kernel/SF_System.h"

#if !defined( SF_OS_ORBIS )
#error "Scaleform did not recognize the OpenOrbis target"
#endif

static_assert( GFX_MAJOR_VERSION == 4 && GFX_MINOR_VERSION == 2,
    "Kisak PS4 requires the Scaleform 4.2 API" );
static_assert( sizeof( Scaleform::UPInt ) == sizeof( void * ),
    "Scaleform pointer-sized integer does not match the PS4 ABI" );

extern "C" const char *KisakPs4ScaleformSdkVersion()
{
    return "kisak-ps4: scaleform sdk " GFX_VERSION_STRING;
}

extern "C" bool KisakPs4ScaleformKernelSelfTest()
{
    Scaleform::System system;
    return Scaleform::Memory::GetGlobalHeap() != NULL;
}
