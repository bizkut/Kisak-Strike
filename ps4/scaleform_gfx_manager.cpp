#include "ps4/scaleform_gfx_manager.h"
#include "ps4/scaleform_gnm_hal.h"

#include "GFx.h"
#include "GFxVersion.h"
#include "filesystem.h"
#include "inputsystem/ButtonCode.h"
#include "tier1/utlbuffer.h"

#include <string.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

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

enum
{
    kScaleformMenuSlot = 0,
    kScaleformHudSlot = 1,
    kScaleformSlotCount = 2
};

struct ScaleformMovieSlot
{
    const char *name;
    Scaleform::Ptr< Scaleform::GFx::MovieDef > definition;
    Scaleform::Ptr< Scaleform::GFx::Movie > movie;
    bool ready;
    bool captured;
};

Scaleform::Key::Code MapButtonCode( ButtonCode_t code )
{
    if ( code >= KEY_A && code <= KEY_Z )
        return static_cast< Scaleform::Key::Code >( Scaleform::Key::A + ( code - KEY_A ) );
    if ( code >= KEY_0 && code <= KEY_9 )
        return static_cast< Scaleform::Key::Code >( Scaleform::Key::Num0 + ( code - KEY_0 ) );

    switch ( code )
    {
    case KEY_ENTER: return Scaleform::Key::Return;
    case KEY_ESCAPE: return Scaleform::Key::Escape;
    case KEY_SPACE: return Scaleform::Key::Space;
    case KEY_BACKSPACE: return Scaleform::Key::Backspace;
    case KEY_TAB: return Scaleform::Key::Tab;
    case KEY_UP: return Scaleform::Key::Up;
    case KEY_DOWN: return Scaleform::Key::Down;
    case KEY_LEFT: return Scaleform::Key::Left;
    case KEY_RIGHT: return Scaleform::Key::Right;
    case KEY_HOME: return Scaleform::Key::Home;
    case KEY_END: return Scaleform::Key::End;
    case KEY_PAGEUP: return Scaleform::Key::PageUp;
    case KEY_PAGEDOWN: return Scaleform::Key::PageDown;
    case KEY_DELETE: return Scaleform::Key::Delete;
    case KEY_INSERT: return Scaleform::Key::Insert;
    case KEY_F1: return Scaleform::Key::F1;
    case KEY_F2: return Scaleform::Key::F2;
    case KEY_F3: return Scaleform::Key::F3;
    case KEY_F4: return Scaleform::Key::F4;
    case KEY_F5: return Scaleform::Key::F5;
    case KEY_F6: return Scaleform::Key::F6;
    case KEY_F7: return Scaleform::Key::F7;
    case KEY_F8: return Scaleform::Key::F8;
    case KEY_F9: return Scaleform::Key::F9;
    case KEY_F10: return Scaleform::Key::F10;
    case KEY_F11: return Scaleform::Key::F11;
    case KEY_F12: return Scaleform::Key::F12;
    default: break;
    }

    switch ( code )
    {
    case KEY_XBUTTON_A: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_A );
    case KEY_XBUTTON_B: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_B );
    case KEY_XBUTTON_X: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_X );
    case KEY_XBUTTON_Y: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Y );
    case KEY_XBUTTON_START: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Start );
    case KEY_XBUTTON_BACK: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Back );
    case KEY_XBUTTON_UP: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Up );
    case KEY_XBUTTON_DOWN: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Down );
    case KEY_XBUTTON_LEFT: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Left );
    case KEY_XBUTTON_RIGHT: return static_cast< Scaleform::Key::Code >( Scaleform::Pad_Right );
    default: return Scaleform::Key::None;
    }
}

class CPs4ScaleformMovieManager final
{
public:
    CPs4ScaleformMovieManager()
        : m_system( NULL ), m_loader( NULL ), m_initialized( false ),
          m_loggedCapture( false ), m_lastTime( -1.0f ), m_frame( 0 )
    {
        m_slots[kScaleformMenuSlot] = { "resource/flash/mainmenu.gfx", NULL, NULL, false, false };
        m_slots[kScaleformHudSlot] = { "resource/flash/mainuirootmovie.gfx", NULL, NULL, false, false };
    }

    ~CPs4ScaleformMovieManager()
    {
        Shutdown();
    }

    bool Initialize()
    {
        if ( m_initialized )
            return true;

        m_system = new Scaleform::System();
        Scaleform::Ptr< KisakScaleformFileOpener > fileOpener =
            *new KisakScaleformFileOpener();
        m_fileOpener = fileOpener;
        m_loader = new Scaleform::GFx::Loader( fileOpener );
        m_loader->SetAS2Support( Scaleform::Ptr< Scaleform::GFx::AS2Support >(
            *new Scaleform::GFx::AS2Support() ) );

        const bool menu = LoadSlot( kScaleformMenuSlot );
        const bool hud = LoadSlot( kScaleformHudSlot );
        m_initialized = menu || hud;
        KisakPs4StartupBreadcrumb( menu
            ? "kisak-ps4: scaleform main menu movie loaded"
            : "kisak-ps4: scaleform main menu movie unavailable" );
        KisakPs4StartupBreadcrumb( hud
            ? "kisak-ps4: scaleform HUD movie loaded"
            : "kisak-ps4: scaleform HUD movie unavailable" );
        if ( !m_initialized )
        {
            delete m_loader;
            m_loader = NULL;
            m_fileOpener.Clear();
            delete m_system;
            m_system = NULL;
        }
        return m_initialized;
    }

    void Shutdown()
    {
        if ( !m_system )
            return;
        for ( unsigned int i = 0; i < kScaleformSlotCount; ++i )
        {
            if ( m_slots[i].movie.GetPtr() )
                m_slots[i].movie->ShutdownRendering( true );
            m_slots[i].movie.Clear();
            m_slots[i].definition.Clear();
            m_slots[i].ready = false;
            m_slots[i].captured = false;
        }
        delete m_loader;
        m_loader = NULL;
        m_fileOpener.Clear();
        delete m_system;
        m_system = NULL;
        m_initialized = false;
        m_lastTime = -1.0f;
        m_frame = 0;
    }

    void Advance( float time )
    {
        float delta = 1.0f / 60.0f;
        if ( m_lastTime >= 0.0f && time > m_lastTime )
        {
            delta = time - m_lastTime;
            if ( delta > 0.25f )
                delta = 0.25f;
        }
        m_lastTime = time;
        KisakPs4ScaleformHal().BeginFrame( ++m_frame );
        for ( unsigned int i = 0; i < kScaleformSlotCount; ++i )
        {
            if ( m_slots[i].movie.GetPtr() )
            {
                m_slots[i].movie->Advance( delta );
                m_slots[i].captured = false;
            }
        }
    }

    bool Render( int slot, const char *phase )
    {
        if ( slot < 0 || slot >= kScaleformSlotCount || !m_slots[slot].movie.GetPtr() )
            return false;

        Scaleform::GFx::Movie *movie = m_slots[slot].movie.GetPtr();
        movie->Capture( false );
        Scaleform::GFx::MovieDisplayHandle displayHandle = movie->GetDisplayHandle();
        m_slots[slot].captured = displayHandle.NextCapture( NULL );
        if ( m_slots[slot].captured && !m_loggedCapture )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform captured display tree" );
            m_loggedCapture = true;
        }
        KisakPs4ScaleformHal().QueueCapturedTree( m_slots[slot].captured, phase );
        if ( slot == kScaleformHudSlot )
            KisakPs4ScaleformHal().EndFrame();
        if ( m_slots[slot].captured && phase )
        {
            (void)phase;
        }
        return m_slots[slot].captured;
    }

    bool HandleInput( const InputEvent_t &event )
    {
        if ( !m_initialized )
            return false;

        bool handled = false;
        for ( unsigned int i = 0; i < kScaleformSlotCount; ++i )
        {
            Scaleform::GFx::Movie *movie = m_slots[i].movie.GetPtr();
            if ( !movie )
                continue;

            if ( event.m_nType == IE_ButtonPressed || event.m_nType == IE_ButtonReleased )
            {
                const Scaleform::Key::Code key = MapButtonCode(
                    static_cast< ButtonCode_t >( event.m_nData ) );
                if ( key != Scaleform::Key::None )
                {
                    const Scaleform::GFx::Event::EventType type =
                        event.m_nType == IE_ButtonPressed
                            ? Scaleform::GFx::Event::KeyDown
                            : Scaleform::GFx::Event::KeyUp;
                    Scaleform::GFx::KeyEvent keyEvent( type, key );
                    handled = movie->HandleEvent( keyEvent ) !=
                        Scaleform::GFx::Movie::HE_NotHandled || handled;
                }
            }
            else if ( event.m_nType == IE_AnalogValueChanged )
            {
                const float value = static_cast< float >( event.m_nData2 ) /
                    static_cast< float >( MAX_BUTTONSAMPLE );
                Scaleform::GFx::GamePadAnalogEvent analogEvent(
                    static_cast< Scaleform::UInt32 >( event.m_nData ), value );
                handled = movie->HandleEvent( analogEvent ) !=
                    Scaleform::GFx::Movie::HE_NotHandled || handled;
            }
        }
        return handled;
    }

    bool IsReady( int slot ) const
    {
        return slot >= 0 && slot < kScaleformSlotCount && m_slots[slot].ready;
    }

private:
    bool LoadSlot( int slot )
    {
        ScaleformMovieSlot &movieSlot = m_slots[slot];
        movieSlot.definition = *m_loader->CreateMovie( movieSlot.name,
            Scaleform::GFx::Loader::LoadAll );
        if ( !movieSlot.definition.GetPtr() )
            return false;

        movieSlot.movie = *movieSlot.definition->CreateInstance( true );
        if ( !movieSlot.movie.GetPtr() )
            return false;

        movieSlot.movie->SetViewport( 1920, 1080, 0, 0, 1920, 1080 );
        movieSlot.movie->SetViewScaleMode( Scaleform::GFx::Movie::SM_ExactFit );
        movieSlot.movie->SetBackgroundAlpha( 0.0f );
        movieSlot.ready = true;
        return true;
    }

    Scaleform::System *m_system;
    Scaleform::GFx::Loader *m_loader;
    Scaleform::Ptr< KisakScaleformFileOpener > m_fileOpener;
    ScaleformMovieSlot m_slots[kScaleformSlotCount];
    bool m_initialized;
    bool m_loggedCapture;
    float m_lastTime;
    uint64_t m_frame;
};

CPs4ScaleformMovieManager g_scaleformMovieManager;
}

bool KisakPs4ScaleformUiInitialize()
{
    return g_scaleformMovieManager.Initialize();
}

void KisakPs4ScaleformUiShutdown()
{
    g_scaleformMovieManager.Shutdown();
}

void KisakPs4ScaleformUiAdvance( float time )
{
    g_scaleformMovieManager.Advance( time );
}

bool KisakPs4ScaleformUiRenderMenu()
{
    return g_scaleformMovieManager.Render( kScaleformMenuSlot, "menu" );
}

bool KisakPs4ScaleformUiRenderHud()
{
    return g_scaleformMovieManager.Render( kScaleformHudSlot, "hud" );
}

bool KisakPs4ScaleformUiHandleInput( const InputEvent_t &event )
{
    return g_scaleformMovieManager.HandleInput( event );
}

bool KisakPs4ScaleformUiMovieReady( int slot )
{
    return g_scaleformMovieManager.IsReady( slot );
}
