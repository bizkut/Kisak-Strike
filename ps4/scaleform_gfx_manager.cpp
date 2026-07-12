#include "ps4/scaleform_gfx_manager.h"
#include "ps4/scaleform_gnm_hal.h"

#include "GFx.h"
#include "GFxVersion.h"
#include "Render/Render_TreeNode.h"
#include "filesystem.h"
#include "inputsystem/ButtonCode.h"
#include "tier1/utlbuffer.h"
#include "zlib.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
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
            bool loadedFromApp0 = false;
            if ( url != NULL && strncmp( url, "resource/flash/", 15 ) == 0 )
            {
                char packagedPath[512];
                snprintf( packagedPath, sizeof( packagedPath ), "/app0/%s", url );
                loadedFromApp0 = g_pFullFileSystem->ReadFile(
                    packagedPath, NULL, sourceData );
                if ( !loadedFromApp0 )
                    sourceData.Purge();
            }
            if ( loadedFromApp0 ||
                 g_pFullFileSystem->ReadFile( url, "GAME", sourceData ) )
            {
                static bool loggedApp0 = false;
                if ( loadedFromApp0 && !loggedApp0 )
                {
                    KisakPs4StartupBreadcrumb(
                        "kisak-ps4: scaleform reading validated app0 movie" );
                    loggedApp0 = true;
                }
                const int size = sourceData.TellPut();
                const Scaleform::UByte *source =
                    static_cast< const Scaleform::UByte * >( sourceData.Base() );
                if ( size >= 8 && source != NULL &&
                     source[0] == 'C' && source[1] == 'W' && source[2] == 'S' )
                {
                    const uint32_t declaredSize =
                        static_cast< uint32_t >( source[4] ) |
                        ( static_cast< uint32_t >( source[5] ) << 8 ) |
                        ( static_cast< uint32_t >( source[6] ) << 16 ) |
                        ( static_cast< uint32_t >( source[7] ) << 24 );
                    if ( declaredSize >= 8 && declaredSize <= 64u * 1024u * 1024u )
                    {
                        Scaleform::UByte *inflated = static_cast< Scaleform::UByte * >(
                            Scaleform::Memory::Alloc( declaredSize ) );
                        if ( inflated != NULL )
                        {
                            memcpy( inflated, source, 8 );
                            inflated[0] = 'F';
                            uLongf bodySize = static_cast< uLongf >( declaredSize - 8 );
                            const int inflateResult = ::uncompress(
                                reinterpret_cast< Bytef * >( inflated + 8 ), &bodySize,
                                reinterpret_cast< const Bytef * >( source + 8 ),
                                static_cast< uLong >( size - 8 ) );
                            if ( inflateResult == Z_OK && bodySize == declaredSize - 8 )
                            {
                                static bool loggedInflate = false;
                                if ( !loggedInflate )
                                {
                                    KisakPs4StartupBreadcrumb(
                                        "kisak-ps4: scaleform CWS inflated in file opener" );
                                    loggedInflate = true;
                                }
                                return new KisakScaleformMemoryFile(
                                    url, inflated, static_cast< int >( declaredSize ) );
                            }
                            Scaleform::Memory::Free( inflated );
                            static bool loggedInflateFailure = false;
                            if ( !loggedInflateFailure )
                            {
                                char failureMarker[192];
                                snprintf( failureMarker, sizeof( failureMarker ),
                                    "kisak-ps4: scaleform CWS inflate failed result=%d input=%d declared=%u output=%lu",
                                    inflateResult, size, declaredSize,
                                    static_cast< unsigned long >( bodySize ) );
                                KisakPs4StartupBreadcrumb( failureMarker );
                                loggedInflateFailure = true;
                            }
                        }
                    }
                }
                Scaleform::UByte *data = static_cast< Scaleform::UByte * >(
                    Scaleform::Memory::Alloc( size ) );
                if ( data != NULL )
                {
                    memcpy( data, source, size );
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

class KisakScaleformLog final : public Scaleform::GFx::Log
{
public:
    KisakScaleformLog() : m_messages( 0 )
    {
    }

    void LogMessageVarg( Scaleform::LogMessageId messageId, const char *format,
        va_list arguments ) override
    {
        if ( m_messages++ >= 24 )
            return;

        char text[512];
        vsnprintf( text, sizeof( text ), format ? format : "", arguments );
        text[sizeof( text ) - 1] = '\0';
        size_t length = strlen( text );
        while ( length > 0 && ( text[length - 1] == '\n' || text[length - 1] == '\r' ) )
            text[--length] = '\0';

        char marker[640];
        snprintf( marker, sizeof( marker ),
            "kisak-ps4: scaleform log type=%u %s",
            static_cast< unsigned int >( messageId.GetMessageType() ), text );
        KisakPs4StartupBreadcrumb( marker );
    }

private:
    uint32_t m_messages;
};

enum ScaleformCallbackId
{
    kCallbackOnLoadFinished = 1,
    kCallbackOnLoadProgress,
    kCallbackOnLoadError,
    kCallbackOnUnload,
    kCallbackAddInputConsumer,
    kCallbackRemoveInputConsumer,
    kCallbackSetCursorShape,
    kCallbackShowCursor,
    kCallbackHideCursor,
    kCallbackLoadKVFile,
    kCallbackSaveKVFile,
    kCallbackSetConvar,
    kCallbackGetConvarNumber,
    kCallbackGetConvarNumberMin,
    kCallbackGetConvarNumberMax,
    kCallbackGetConvarString,
    kCallbackGetConvarBoolean,
    kCallbackGetPAXAvatarFromName,
    kCallbackGetPlayerColorObject,
    kCallbackTranslate,
    kCallbackReplaceGlyphs,
    kCallbackPlaySound,
    kCallbackConsoleCommand,
    kCallbackConsoleCommandExecute,
    kCallbackDisableAnalogStickNavigation,
    kCallbackDenyInputToGame,
    kCallbackSendUIEvent,
    kCallbackMakeStringSafe,
    kCallbackGetClipboardText,
    kCallbackSetClipboardText,
    kCallbackBasePanelRunCommand,
    kCallbackIsMultiplayerPrivilegeEnabled,
    kCallbackLaunchTraining,
    kCallbackViewMapInWorkshop,
    kCallbackGetPreviousLevel
};

struct ScaleformCallbackDefinition
{
    const char *name;
    ScaleformCallbackId id;
};

static const ScaleformCallbackDefinition kScaleformCallbackDefinitions[] =
{
    { "OnLoadFinished", kCallbackOnLoadFinished },
    { "OnLoadProgress", kCallbackOnLoadProgress },
    { "OnLoadError", kCallbackOnLoadError },
    { "OnUnload", kCallbackOnUnload },
    { "AddInputConsumer", kCallbackAddInputConsumer },
    { "RemoveInputConsumer", kCallbackRemoveInputConsumer },
    { "SetCursorShape", kCallbackSetCursorShape },
    { "ShowCursor", kCallbackShowCursor },
    { "HideCursor", kCallbackHideCursor },
    { "LoadKVFile", kCallbackLoadKVFile },
    { "SaveKVFile", kCallbackSaveKVFile },
    { "SetConvar", kCallbackSetConvar },
    { "GetConvarNumber", kCallbackGetConvarNumber },
    { "GetConvarNumberMin", kCallbackGetConvarNumberMin },
    { "GetConvarNumberMax", kCallbackGetConvarNumberMax },
    { "GetConvarString", kCallbackGetConvarString },
    { "GetConvarBoolean", kCallbackGetConvarBoolean },
    { "GetPAXAvatarFromName", kCallbackGetPAXAvatarFromName },
    { "GetPlayerColorObject", kCallbackGetPlayerColorObject },
    { "Translate", kCallbackTranslate },
    { "ReplaceGlyphs", kCallbackReplaceGlyphs },
    { "PlaySound", kCallbackPlaySound },
    { "ConsoleCommand", kCallbackConsoleCommand },
    { "ConsoleCommandExecute", kCallbackConsoleCommandExecute },
    { "DisableAnalogStickNavigation", kCallbackDisableAnalogStickNavigation },
    { "DenyInputToGame", kCallbackDenyInputToGame },
    { "SendUIEvent", kCallbackSendUIEvent },
    { "MakeStringSafe", kCallbackMakeStringSafe },
    { "GetClipboardText", kCallbackGetClipboardText },
    { "SetClipboardText", kCallbackSetClipboardText },
    { "BasePanelRunCommand", kCallbackBasePanelRunCommand },
    { "IsMultiplayerPrivilegeEnabled", kCallbackIsMultiplayerPrivilegeEnabled },
    { "LaunchTraining", kCallbackLaunchTraining },
    { "ViewMapInWorkshop", kCallbackViewMapInWorkshop },
    { "GetPreviousLevel", kCallbackGetPreviousLevel }
};

class KisakScaleformFunctionHandler final : public Scaleform::GFx::FunctionHandler
{
public:
    KisakScaleformFunctionHandler()
        : m_loadFinished( 0 ), m_loadErrors( 0 ), m_uiEvents( 0 )
    {
    }

    void Call( const Params &params ) override
    {
        const ScaleformCallbackId id = static_cast< ScaleformCallbackId >(
            reinterpret_cast< uintptr_t >( params.pUserData ) );
        if ( params.pRetVal )
            params.pRetVal->SetNull();

        switch ( id )
        {
        case kCallbackOnLoadFinished:
            if ( m_loadFinished++ == 0 )
                KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform element load finished" );
            break;
        case kCallbackOnLoadError:
            if ( m_loadErrors++ == 0 )
                KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform element load error" );
            break;
        case kCallbackOnUnload:
            if ( params.pRetVal )
                params.pRetVal->SetBoolean( true );
            break;
        case kCallbackGetPlayerColorObject:
        {
            static const int colors[5][3] =
            {
                { 240, 243, 32 }, { 150, 34, 223 }, { 0, 165, 90 },
                { 92, 168, 255 }, { 255, 155, 37 }
            };
            const int player = params.pArgs && params.ArgCount > 0
                ? static_cast< int >( params.pArgs[0].GetNumber() ) : 0;
            const int component = params.pArgs && params.ArgCount > 1
                ? static_cast< int >( params.pArgs[1].GetNumber() ) : 0;
            if ( params.pRetVal && player >= 0 && player < 5 && component >= 0 && component < 3 )
                params.pRetVal->SetNumber( colors[player][component] );
            break;
        }
        case kCallbackGetConvarNumber:
        case kCallbackGetConvarNumberMin:
        case kCallbackGetConvarNumberMax:
            if ( params.pRetVal )
                params.pRetVal->SetNumber( 0.0 );
            break;
        case kCallbackGetConvarBoolean:
            if ( params.pRetVal )
                params.pRetVal->SetBoolean( false );
            break;
        case kCallbackGetConvarString:
        case kCallbackGetClipboardText:
        case kCallbackGetPAXAvatarFromName:
            if ( params.pRetVal )
                params.pRetVal->SetString( "" );
            break;
        case kCallbackTranslate:
        case kCallbackReplaceGlyphs:
        case kCallbackMakeStringSafe:
            if ( params.pRetVal )
            {
                const char *value = params.pArgs && params.ArgCount > 0
                    ? params.pArgs[0].GetString() : "";
                params.pRetVal->SetString( value ? value : "" );
            }
            break;
        case kCallbackIsMultiplayerPrivilegeEnabled:
            if ( params.pRetVal )
                params.pRetVal->SetBoolean( true );
            break;
        case kCallbackGetPreviousLevel:
            if ( params.pRetVal )
                params.pRetVal->SetNumber( -1.0 );
            break;
        case kCallbackSendUIEvent:
            if ( m_uiEvents++ == 0 )
                KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform GameInterface event bridge active" );
            break;
        default:
            break;
        }
    }

private:
    uint32_t m_loadFinished;
    uint32_t m_loadErrors;
    uint32_t m_uiEvents;
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
    const char *element;
    Scaleform::Ptr< Scaleform::GFx::MovieDef > definition;
    Scaleform::Ptr< Scaleform::GFx::Movie > movie;
    bool ready;
    bool captured;
};

const char *ScaleformSlotLabel( int slot )
{
    return slot == kScaleformMenuSlot ? "menu" : "hud";
}

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
        // Source creates these two root movies as Scaleform slots.  MainMenu
        // is an element requested from MainUIRootMovie; GameUIRootMovie is
        // the client/HUD root and receives its level HUD elements later.
        m_slots[kScaleformMenuSlot] = {
            "resource/flash/mainuirootmovie.swf", "MainMenu", NULL, NULL, false, false };
        m_slots[kScaleformHudSlot] = {
            "resource/flash/gameuirootmovie.swf", NULL, NULL, NULL, false, false };
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
        m_callbackHandler = *new KisakScaleformFunctionHandler();
        Scaleform::Ptr< KisakScaleformFileOpener > fileOpener =
            *new KisakScaleformFileOpener();
        m_fileOpener = fileOpener;
        m_loader = new Scaleform::GFx::Loader( fileOpener );
        m_log = *new KisakScaleformLog();
        m_loader->SetLog( m_log );
        m_actionControl = *new Scaleform::GFx::ActionControl(
            Scaleform::GFx::ActionControl::Action_LogAllFilenames );
        m_actionControl->SetActionErrorSuppress( false );
        m_actionControl->SetVerboseAction( false );
        m_loader->SetActionControl( m_actionControl );
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform AS2 action diagnostics installed" );
        m_zlibSupport = *new Scaleform::GFx::ZlibSupport();
        m_loader->SetZlibSupport( m_zlibSupport );
        KisakPs4StartupBreadcrumb( m_loader->GetZlibSupport().GetPtr()
            ? "kisak-ps4: scaleform zlib support installed"
            : "kisak-ps4: scaleform zlib support unavailable" );
        m_loader->SetAS2Support( Scaleform::Ptr< Scaleform::GFx::AS2Support >(
            *new Scaleform::GFx::AS2Support() ) );

        m_fontLib = *new Scaleform::GFx::FontLib();
        m_loader->SetFontLib( m_fontLib );
        Scaleform::Ptr< Scaleform::GFx::MovieDef > fontDefinition =
            *m_loader->CreateMovie( "resource/flash/fontlib.gfx",
                Scaleform::GFx::Loader::LoadAll );
        if ( fontDefinition.GetPtr() )
        {
            m_fontLib->AddFontsFrom( fontDefinition, true );
            KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform font library initialized" );
        }
        else
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform font library unavailable" );
        }
        fontDefinition.Clear();

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
            m_callbackHandler.Clear();
            delete m_loader;
            m_loader = NULL;
            m_fontLib.Clear();
            m_fileOpener.Clear();
            m_log.Clear();
            m_actionControl.Clear();
            m_zlibSupport.Clear();
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
        m_callbackHandler.Clear();
        delete m_loader;
        m_loader = NULL;
        m_fontLib.Clear();
        m_fileOpener.Clear();
        m_log.Clear();
        m_actionControl.Clear();
        m_zlibSupport.Clear();
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
        Scaleform::Render::TreeRoot *root = displayHandle.GetRenderEntry();
        if ( m_slots[slot].captured && !m_loggedCapture )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform captured display tree" );
            m_loggedCapture = true;
        }
        if ( m_slots[slot].captured && root )
            KisakPs4ScaleformHal().QueueCapturedTree( root, phase );
        else
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

    bool RequestElement( int slot, const char *elementName )
    {
        if ( slot < 0 || slot >= kScaleformSlotCount || elementName == NULL ||
             !m_slots[slot].movie.GetPtr() )
            return false;

        Scaleform::GFx::Movie *movie = m_slots[slot].movie.GetPtr();
        Scaleform::GFx::Value global;
        if ( !movie->GetVariable( &global, "_global" ) )
            return false;

        Scaleform::GFx::Value function;
        function.SetNull();
        if ( !global.GetMember( "RequestElement", &function ) )
            return false;

        // Source creates a new callback object for every element.  Reusing the
        // root GameInterface lets one panel overwrite another panel's state.
        Scaleform::GFx::Value args[2];
        movie->CreateString( &args[0], elementName );
        CreateGameInterface( movie, &args[1] );
        if ( !args[1].IsObject() )
            return false;
        global.Invoke( "RequestElement", NULL, args, 2 );
        return true;
    }

private:
    void LogMovieInfoProbe( int slot, const char *kind, const char *url )
    {
        Scaleform::GFx::MovieInfo info;
        info.Clear();
        const bool available = m_loader->GetMovieInfo( url, &info, false,
            Scaleform::GFx::Loader::LoadKeepBindData );
        char marker[256];
        snprintf( marker, sizeof( marker ),
            "kisak-ps4: scaleform %s %s info ok=%u url=%s version=%u flags=0x%x frames=%u size=%dx%d",
            ScaleformSlotLabel( slot ), kind, available ? 1u : 0u, url,
            info.Version, info.Flags, info.FrameCount, info.Width, info.Height );
        KisakPs4StartupBreadcrumb( marker );
    }

    void CreateGameInterface( Scaleform::GFx::Movie *movie,
        Scaleform::GFx::Value *gameInterface )
    {
        if ( !movie || !gameInterface )
            return;

        movie->CreateObject( gameInterface );
        if ( !gameInterface->IsObject() || !m_callbackHandler.GetPtr() )
            return;

        for ( unsigned int i = 0;
              i < sizeof( kScaleformCallbackDefinitions ) / sizeof( kScaleformCallbackDefinitions[0] );
              ++i )
        {
            Scaleform::GFx::Value function;
            movie->CreateFunction( &function, m_callbackHandler.GetPtr(),
                reinterpret_cast< void * >( static_cast< uintptr_t >(
                    kScaleformCallbackDefinitions[i].id ) ) );
            gameInterface->SetMember( kScaleformCallbackDefinitions[i].name, function );
        }
    }

    bool LoadSlot( int slot )
    {
        ScaleformMovieSlot &movieSlot = m_slots[slot];
        const char *swfRoot = slot == kScaleformMenuSlot
            ? "resource/flash/mainuirootmovie.swf"
            : "resource/flash/gameuirootmovie.swf";
        const char *gfxRoot = slot == kScaleformMenuSlot
            ? "resource/flash/mainuirootmovie.gfx"
            : "resource/flash/gameuirootmovie.gfx";
        LogMovieInfoProbe( slot, "swf", swfRoot );
        LogMovieInfoProbe( slot, "gfx", gfxRoot );
        Scaleform::GFx::MovieDef *loadedDefinition = m_loader->CreateMovie(
            movieSlot.name, Scaleform::GFx::Loader::LoadWaitFrame1 );
        if ( loadedDefinition == NULL )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: scaleform wait-frame1 failed; using async fallback" );
            loadedDefinition = m_loader->CreateMovie( movieSlot.name,
                Scaleform::GFx::Loader::LoadAll );
        }
        if ( loadedDefinition == NULL )
            return false;
        movieSlot.definition = *loadedDefinition;

        movieSlot.movie = *movieSlot.definition->CreateInstance(
            true, 0, m_actionControl.GetPtr() );
        if ( !movieSlot.movie.GetPtr() )
            return false;

        const char *movieUrl = movieSlot.definition->GetFileURL();
        char metadataMarker[256];
        snprintf( metadataMarker, sizeof( metadataMarker ),
            "kisak-ps4: scaleform %s metadata url=%s version=%u frames=%u size=%ux%u avm=%d current=%u",
            ScaleformSlotLabel( slot ), movieUrl ? movieUrl : "",
            movieSlot.definition->GetVersion(), movieSlot.definition->GetFrameCount(),
            static_cast< unsigned int >( movieSlot.definition->GetWidth() ),
            static_cast< unsigned int >( movieSlot.definition->GetHeight() ),
            movieSlot.movie->GetAVMVersion(), movieSlot.movie->GetCurrentFrame() );
        KisakPs4StartupBreadcrumb( metadataMarker );

        movieSlot.movie->SetViewAlignment( Scaleform::GFx::Movie::Align_TopLeft );
        // Keep authored coordinates intact; ResizeManager applies the console
        // resolution/safe-zone scale in ActionScript, matching BaseSlot::Init.
        movieSlot.movie->SetViewScaleMode( Scaleform::GFx::Movie::SM_NoScale );
        movieSlot.movie->SetBackgroundAlpha( 0.0f );
        movieSlot.movie->SetVisible( true );

        Scaleform::GFx::Value global;
        Scaleform::GFx::Value gameInterface;
        const bool globalReady = movieSlot.movie->GetVariable( &global, "_global" );
        if ( globalReady )
        {
            Scaleform::GFx::Value platformCode;
            platformCode.SetNumber( 2 ); // PS3/PS4 console ActionScript convention.
            global.SetMember( "PlatformCode", platformCode );

            Scaleform::GFx::Value controllerUI;
            controllerUI.SetBoolean( true );
            global.SetMember( "wantControllerShown", controllerUI );

            Scaleform::GFx::Value uiSlot;
            uiSlot.SetNumber( slot );
            global.SetMember( "UISlot", uiSlot );

            CreateGameInterface( movieSlot.movie.GetPtr(), &gameInterface );
            if ( gameInterface.IsObject() )
                global.SetMember( "GameInterface", gameInterface );
        }

        if ( slot == kScaleformMenuSlot )
            movieSlot.movie->HandleEvent( Scaleform::GFx::Event::SetFocus );

        // Match the original BaseSlot::Init bootstrap: a zero-delta advance
        // flushes frame-0 actions after PlatformCode/GameInterface are set.
        movieSlot.movie->Advance( 0.0f );

        char scriptMarker[224];
        snprintf( scriptMarker, sizeof( scriptMarker ),
            "kisak-ps4: scaleform %s script avm=%d current=%u init=%u request=%u globalInit=%u globalRequest=%u",
            ScaleformSlotLabel( slot ), movieSlot.movie->GetAVMVersion(),
            movieSlot.movie->GetCurrentFrame(),
            movieSlot.movie->IsAvailable( "InitSlot" ) ? 1u : 0u,
            movieSlot.movie->IsAvailable( "RequestElement" ) ? 1u : 0u,
            movieSlot.movie->IsAvailable( "_global.InitSlot" ) ? 1u : 0u,
            movieSlot.movie->IsAvailable( "_global.RequestElement" ) ? 1u : 0u );
        KisakPs4StartupBreadcrumb( scriptMarker );

        // BaseSlot::Init invokes these methods on _global, not on Movie.
        // Calling Movie::Invoke skips the ActionScript global object and made
        // the old direct movie bootstrap report false for both hooks.
        Scaleform::GFx::Value function;
        function.SetNull();
        const bool initSlot = globalReady && global.GetMember( "InitSlot", &function );
        if ( initSlot )
            global.Invoke( "InitSlot" );

        movieSlot.movie->SetViewport( 1920, 1080, 0, 0, 1920, 1080 );
        function.SetNull();
        const bool forceResize = globalReady && global.GetMember( "ForceResize", &function );
        if ( forceResize )
            global.Invoke( "ForceResize" );

        const bool requestElement = movieSlot.element != NULL &&
            RequestElement( slot, movieSlot.element );
        char marker[160];
        snprintf( marker, sizeof( marker ),
            "kisak-ps4: scaleform %s init global=%u InitSlot=%u ForceResize=%u RequestElement=%u",
            ScaleformSlotLabel( slot ), globalReady ? 1u : 0u,
            initSlot ? 1u : 0u, forceResize ? 1u : 0u,
            requestElement ? 1u : 0u );
        KisakPs4StartupBreadcrumb( marker );
        movieSlot.ready = true;
        return true;
    }

    Scaleform::System *m_system;
    Scaleform::GFx::Loader *m_loader;
    Scaleform::Ptr< KisakScaleformFileOpener > m_fileOpener;
    Scaleform::Ptr< KisakScaleformLog > m_log;
    Scaleform::Ptr< Scaleform::GFx::ActionControl > m_actionControl;
    Scaleform::Ptr< Scaleform::GFx::ZlibSupportBase > m_zlibSupport;
    Scaleform::Ptr< Scaleform::GFx::FontLib > m_fontLib;
    Scaleform::Ptr< KisakScaleformFunctionHandler > m_callbackHandler;
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

bool KisakPs4ScaleformUiRequestElement( int slot, const char *elementName )
{
    return g_scaleformMovieManager.RequestElement( slot, elementName );
}
