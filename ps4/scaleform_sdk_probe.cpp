#include "GFx.h"
#include "GFxVersion.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"

#include <string.h>

#if !defined( SF_OS_ORBIS )
#error "Scaleform did not recognize the OpenOrbis target"
#endif

static_assert( GFX_MAJOR_VERSION == 4 && GFX_MINOR_VERSION == 2,
    "Kisak PS4 requires the Scaleform 4.2 API" );
static_assert( sizeof( Scaleform::UPInt ) == sizeof( void * ),
    "Scaleform pointer-sized integer does not match the PS4 ABI" );

namespace
{
class KisakScaleformMemoryFile final : public Scaleform::MemoryFile
{
public:
    KisakScaleformMemoryFile( const char *url, Scaleform::UByte *data, int size )
        : Scaleform::MemoryFile( url, data, size ), m_data( data )
    {
    }

    ~KisakScaleformMemoryFile() override
    {
        Scaleform::Memory::Free( m_data );
    }

private:
    Scaleform::UByte *m_data;
};

class KisakScaleformFileOpener final : public Scaleform::GFx::FileOpener
{
public:
    Scaleform::File *OpenFile( const char *url, int flags, int mode ) override
    {
        if ( g_pFullFileSystem != NULL )
        {
            CUtlBuffer sourceData;
            if ( g_pFullFileSystem->ReadFile( url, "GAME", sourceData ) )
            {
                const int size = sourceData.TellPut();
                Scaleform::UByte *data = static_cast< Scaleform::UByte * >(
                    Scaleform::Memory::Alloc( size ) );
                if ( data != NULL )
                {
                    memcpy( data, sourceData.Base(), size );
                    return new KisakScaleformMemoryFile( url, data, size );
                }
            }
        }

        return Scaleform::GFx::FileOpener::OpenFile( url, flags, mode );
    }

    Scaleform::SInt64 GetFileModifyTime( const char *url ) override
    {
        if ( g_pFullFileSystem != NULL )
            return g_pFullFileSystem->GetFileTime( url, "GAME" );
        return Scaleform::GFx::FileOpener::GetFileModifyTime( url );
    }
};
}

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
    Scaleform::Ptr< KisakScaleformFileOpener > fileOpener =
        *new KisakScaleformFileOpener();
    Scaleform::GFx::Loader loader( fileOpener );
    loader.SetAS2Support( Scaleform::Ptr< Scaleform::GFx::AS2Support >(
        *new Scaleform::GFx::AS2Support() ) );
    Scaleform::Ptr< Scaleform::GFx::MovieDef > movie = *loader.CreateMovie(
        "resource/flash/fontlib.gfx",
        Scaleform::GFx::Loader::LoadAll );
    return movie.GetPtr() != NULL;
}
