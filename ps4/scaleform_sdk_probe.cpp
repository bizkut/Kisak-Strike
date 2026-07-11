#include "GFx.h"
#include "GFxVersion.h"

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
    if ( Scaleform::Memory::GetGlobalHeap() == NULL )
        return false;

    Scaleform::Ptr< Scaleform::GFx::AS2Support > as2 =
        *new Scaleform::GFx::AS2Support();
    Scaleform::GFx::Loader loader;
    loader.SetAS2Support( as2 );
    return loader.GetAS2Support() != NULL;
}

extern "C" bool KisakPs4ScaleformMovieProbe()
{
    Scaleform::System system;
    Scaleform::GFx::Loader loader;
    loader.SetAS2Support( Scaleform::Ptr< Scaleform::GFx::AS2Support >(
        *new Scaleform::GFx::AS2Support() ) );
    Scaleform::Ptr< Scaleform::GFx::MovieDef > movie = *loader.CreateMovie(
        "/data/kisak-strike/csgo/resource/flash/fontlib.gfx",
        Scaleform::GFx::Loader::LoadAll );
    return movie.GetPtr() != NULL;
}
