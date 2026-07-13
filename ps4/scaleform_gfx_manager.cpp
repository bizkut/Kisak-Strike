#include "ps4/scaleform_gfx_manager.h"
#include "ps4/scaleform_gnm_hal.h"
#include "ps4/scaleform_asset_path.h"

#include "GFx.h"
#include "GFxVersion.h"
#include "GFx/GFx_ImageCreator.h"
#include "Render/Render_TreeNode.h"
#include "filesystem.h"
#include "inputsystem/ButtonCode.h"
#include "interfaces/interfaces.h"
#include "tier1/keyvalues.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "vgui/ILocalize.h"
#include "zlib.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef KISAK_PS4_SCALEFORM_DIRECT_MENU
#define KISAK_PS4_SCALEFORM_DIRECT_MENU 0
#endif

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
        char packagedUrl[512];
        const bool packagedAsset = KisakPs4NormalizeScaleformAssetUrl(
            url, packagedUrl, sizeof( packagedUrl ) );
        // The authored root requests Legals.swf, but the console package also
        // contains Scaleform's optimized GFX movie and its external DDS closure.
        // The full SWF fails MovieClipLoader before frame one on the PS4 runtime;
        // prefer the console movie just as the original console URL builder did.
        if ( packagedAsset &&
             strcmp( packagedUrl, "resource/flash/legals.swf" ) == 0 )
        {
            V_strncpy( packagedUrl, "resource/flash/legals.gfx",
                sizeof( packagedUrl ) );
        }
        if ( packagedAsset && strcmp( packagedUrl, url ) != 0 )
        {
            static unsigned int loggedAliases = 0;
            if ( loggedAliases++ < 8 )
            {
                char aliasMarker[320];
                snprintf( aliasMarker, sizeof( aliasMarker ),
                    "kisak-ps4: scaleform packaged alias url=%s resolved=%s",
                    url, packagedUrl );
                KisakPs4StartupBreadcrumb( aliasMarker );
            }
        }

        // OpenOrbis' stat, fstat, ftell, and lseek end-position paths all report
        // truncated lengths for packaged /app0 files on hardware.  Uncompressed
        // SWF/GFX files carry their exact byte length in the header, so read that
        // many bytes sequentially instead of asking the runtime for a file size.
        if ( packagedAsset )
        {
            char packagedPath[512];
            snprintf( packagedPath, sizeof( packagedPath ), "/app0/%s", packagedUrl );
            FILE *packagedFile = fopen( packagedPath, "rb" );
            if ( packagedFile != NULL )
            {
                Scaleform::UByte header[128];
                const size_t headerBytes = fread( header, 1, sizeof( header ), packagedFile );
                bool directDds = false;
                const uint32_t declaredSize = KisakPs4ScaleformPackagedPayloadSize(
                    header, headerBytes, &directDds );
                if ( declaredSize >= headerBytes &&
                     declaredSize <= 64u * 1024u * 1024u )
                {
                    Scaleform::UByte *data = static_cast< Scaleform::UByte * >(
                        Scaleform::Memory::Alloc( declaredSize ) );
                    if ( data != NULL )
                    {
                        memcpy( data, header, headerBytes );
                        size_t totalBytes = headerBytes;
                        while ( totalBytes < declaredSize )
                        {
                            const size_t bytesRead = fread( data + totalBytes, 1,
                                declaredSize - totalBytes, packagedFile );
                            if ( bytesRead == 0 )
                                break;
                            totalBytes += bytesRead;
                        }
                        fclose( packagedFile );

                        static unsigned int loggedDirectReads[2] = { 0, 0 };
                        const unsigned int directReadKind = directDds ? 1u : 0u;
                        if ( loggedDirectReads[directReadKind]++ < 8 )
                        {
                            char directMarker[256];
                            snprintf( directMarker, sizeof( directMarker ),
                                "kisak-ps4: scaleform direct app0 type=%s bytes=%lu declared=%u url=%s",
                                directDds ? "dds" : "movie",
                                static_cast< unsigned long >( totalBytes ), declaredSize,
                                packagedUrl );
                            KisakPs4StartupBreadcrumb( directMarker );
                        }
                        if ( totalBytes == declaredSize )
                            return new KisakScaleformMemoryFile(
                                packagedUrl, data, static_cast< int >( declaredSize ) );
                        Scaleform::Memory::Free( data );
                    }
                    else
                    {
                        fclose( packagedFile );
                    }
                }
                else
                {
                    fclose( packagedFile );
                }
            }
        }

        if ( g_pFullFileSystem != NULL )
        {
            CUtlBuffer sourceData;
            bool loadedFromApp0 = false;
            if ( packagedAsset )
            {
                char packagedPath[512];
                snprintf( packagedPath, sizeof( packagedPath ), "/app0/%s", packagedUrl );
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
                     ( source[0] == 'F' || source[0] == 'C' || source[0] == 'G' ) &&
                     ( source[1] == 'W' || source[1] == 'F' ) &&
                     ( source[2] == 'S' || source[2] == 'X' ) )
                {
                    static unsigned int loggedMovieSizes = 0;
                    if ( loggedMovieSizes++ < 8 )
                    {
                        const uint32_t declaredSize =
                            static_cast< uint32_t >( source[4] ) |
                            ( static_cast< uint32_t >( source[5] ) << 8 ) |
                            ( static_cast< uint32_t >( source[6] ) << 16 ) |
                            ( static_cast< uint32_t >( source[7] ) << 24 );
                        char sizeMarker[256];
                        snprintf( sizeMarker, sizeof( sizeMarker ),
                            "kisak-ps4: scaleform file bytes=%d declared=%u url=%s",
                            size, declaredSize, url ? url : "" );
                        KisakPs4StartupBreadcrumb( sizeMarker );
                    }
                }
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
                    return new KisakScaleformMemoryFile(
                        loadedFromApp0 ? packagedUrl : url, data, size );
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

const wchar_t *FindLocalizedText( const char *value )
{
    if ( !value || value[0] != '#' || !g_pVGuiLocalize )
        return NULL;

    char key[1024];
    V_strncpy( key, value, sizeof( key ) );
    char *fontHint = strchr( key, '@' );
    if ( fontHint )
        *fontHint = '\0';
    const wchar_t *localized = g_pVGuiLocalize->Find( key );
#if defined( PLATFORM_PS4 )
    // Some shipped tables end with an accepted empty WIN32 override even
    // though the PS4 UI uses the console movie branch. Keep a readable,
    // controller-first fallback until every legacy conditional is normalized.
    if ( ( !localized || !localized[0] ) &&
         !V_stricmp( key, "#SFUI_MainMenu_Navigation_Root" ) )
    {
        return L"[X] SELECT";
    }
    if ( !V_stricmp( key, "#SFUI_PressStartPrompt" ) )
        return L"Press [X] to Start";
#endif
    return localized;
}

bool LoadScaleformLocalization()
{
    if ( !g_pVGuiLocalize )
    {
        KisakPs4StartupBreadcrumb(
            "kisak-ps4: scaleform localization interface unavailable" );
        return false;
    }

    const bool valve = g_pVGuiLocalize->AddFile(
        "resource/valve_%language%.txt", "GAME", true );
    const bool csgo = g_pVGuiLocalize->AddFile(
        "resource/csgo_%language%.txt", "GAME", true );
    const bool platform = g_pVGuiLocalize->AddFile(
        "resource/platform_%language%.txt" );
    const bool vgui = g_pVGuiLocalize->AddFile(
        "resource/vgui_%language%.txt" );
    const wchar_t *probe = FindLocalizedText( "#SFUI_MainMenu_PlayButton" );
    char probeUtf8[96] = {};
    if ( probe )
        V_UnicodeToUTF8( probe, probeUtf8, sizeof( probeUtf8 ) );

    char marker[224];
    snprintf( marker, sizeof( marker ),
        "kisak-ps4: scaleform localization valve=%u csgo=%u platform=%u vgui=%u probe=%s",
        valve ? 1u : 0u, csgo ? 1u : 0u,
        platform ? 1u : 0u, vgui ? 1u : 0u,
        probeUtf8[0] ? probeUtf8 : "missing" );
    KisakPs4StartupBreadcrumb( marker );
    return csgo && probe != NULL;
}

class KisakScaleformTranslator final : public Scaleform::GFx::Translator
{
public:
    KisakScaleformTranslator() : m_logged( 0 ) {}

    unsigned GetCaps() const override
    {
        return Cap_StripTrailingNewLines;
    }

    void Translate( TranslateInfo *info ) override
    {
        if ( !info || !info->GetKey() )
            return;

        char key[1024] = {};
        V_UnicodeToUTF8( info->GetKey(), key, sizeof( key ) );
        const wchar_t *translated = FindLocalizedText( key );
        if ( translated )
            info->SetResultHtml( translated );

        if ( m_logged < 8 && key[0] == '#' )
        {
            char value[128] = {};
            if ( translated )
                V_UnicodeToUTF8( translated, value, sizeof( value ) );
            char marker[256];
            snprintf( marker, sizeof( marker ),
                "kisak-ps4: scaleform translator hit=%u key=%s value=%s",
                translated ? 1u : 0u, key,
                value[0] ? value : "missing" );
            KisakPs4StartupBreadcrumb( marker );
            ++m_logged;
        }
    }

private:
    unsigned int m_logged;
};

enum ScaleformCallbackId
{
    kCallbackOnLoadFinished = 1,
    kCallbackOnReady,
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
    kCallbackGetPreviousLevel,
    kCallbackGetTrialTimeRemaining,
    kCallbackAnimationCompleted,
    kCallbackGetRatingsBoardForLegals,
    kCallbackPlayAudio
};

struct ScaleformCallbackDefinition
{
    const char *name;
    ScaleformCallbackId id;
};

static const ScaleformCallbackDefinition kScaleformCallbackDefinitions[] =
{
    { "OnLoadFinished", kCallbackOnLoadFinished },
    { "OnReady", kCallbackOnReady },
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
    { "GetPreviousLevel", kCallbackGetPreviousLevel },
    { "GetTrialTimeRemaining", kCallbackGetTrialTimeRemaining },
    { "AnimationCompleted", kCallbackAnimationCompleted },
    { "GetRatingsBoardForLegals", kCallbackGetRatingsBoardForLegals },
    { "PlayAudio", kCallbackPlayAudio }
};

class KisakScaleformFunctionHandler final : public Scaleform::GFx::FunctionHandler
{
public:
    KisakScaleformFunctionHandler()
        : m_loadFinished( 0 ), m_ready( 0 ), m_loadErrors( 0 ), m_uiEvents( 0 ),
          m_legalsReady( false ), m_legalsComplete( false ),
          m_startScreenReady( false ), m_mainMenuReady( false ),
          m_audioLogged( false )
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
        case kCallbackOnReady:
        {
            const char *elementName = "";
            Scaleform::GFx::Value elementValue;
            if ( params.pThis && params.pThis->IsObject() &&
                 params.pThis->GetMember( "__KisakElementName", &elementValue ) &&
                 elementValue.IsString() )
            {
                elementName = elementValue.GetString();
            }

            if ( strcmp( elementName, "Legals" ) == 0 )
                m_legalsReady = true;
            else if ( strcmp( elementName, "StartScreen" ) == 0 )
                m_startScreenReady = true;
            else if ( strcmp( elementName, "MainMenu" ) == 0 )
                m_mainMenuReady = true;

            bool shown = false;
            if ( params.pMovie && elementName && strcmp( elementName, "MainMenu" ) == 0 )
            {
                Scaleform::GFx::Value global;
                Scaleform::GFx::Value mainMenu;
                if ( params.pMovie->GetVariable( &global, "_global" ) &&
                     global.GetMember( "MainMenuMovie", &mainMenu ) &&
                     mainMenu.IsObject() )
                {
                    shown = mainMenu.Invoke( "showPanel" );
                }
            }

            if ( m_ready++ < 8 )
            {
                char marker[192];
                snprintf( marker, sizeof( marker ),
                    "kisak-ps4: scaleform element ready name=%s show=%u",
                    elementName && elementName[0] ? elementName : "unknown",
                    shown ? 1u : 0u );
                KisakPs4StartupBreadcrumb( marker );
            }
            break;
        }
        case kCallbackOnLoadError:
            if ( m_loadErrors++ < 8 )
            {
                const char *elementName = "unknown";
                const char *errorCode = "unknown";
                Scaleform::GFx::Value elementValue;
                if ( params.pThis && params.pThis->IsObject() &&
                     params.pThis->GetMember( "__KisakElementName", &elementValue ) &&
                     elementValue.IsString() )
                    elementName = elementValue.GetString();
                if ( params.pArgs && params.ArgCount > 1 &&
                     params.pArgs[1].IsString() )
                    errorCode = params.pArgs[1].GetString();
                char marker[224];
                snprintf( marker, sizeof( marker ),
                    "kisak-ps4: scaleform element load error name=%s code=%s args=%u",
                    elementName ? elementName : "unknown",
                    errorCode ? errorCode : "unknown",
                    static_cast< unsigned int >( params.ArgCount ) );
                KisakPs4StartupBreadcrumb( marker );
            }
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
        case kCallbackReplaceGlyphs:
        case kCallbackMakeStringSafe:
            if ( params.pRetVal )
            {
                const char *value = params.pArgs && params.ArgCount > 0
                    ? params.pArgs[0].GetString() : "";
                params.pRetVal->SetString( value ? value : "" );
            }
            break;
        case kCallbackTranslate:
            if ( params.pRetVal )
            {
                const char *value = params.pArgs && params.ArgCount > 0
                    ? params.pArgs[0].GetString() : "";
                const wchar_t *translated = FindLocalizedText( value );
                if ( translated && params.pMovie )
                    params.pMovie->CreateStringW( params.pRetVal, translated );
                else
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
        case kCallbackGetTrialTimeRemaining:
            // The retail console contract uses -1 to mean that the full game
            // is unlocked. Zero means the trial has expired.
            if ( params.pRetVal )
                params.pRetVal->SetNumber( -1.0 );
            break;
        case kCallbackAnimationCompleted:
            m_legalsComplete = true;
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: scaleform boot Legals animation completed" );
            break;
        case kCallbackGetRatingsBoardForLegals:
            if ( params.pRetVal )
                params.pRetVal->SetString( "ESRB" );
            break;
        case kCallbackPlayAudio:
            if ( !m_audioLogged )
            {
                KisakPs4StartupBreadcrumb(
                    "kisak-ps4: scaleform boot Legals audio callback (silent fallback)" );
                m_audioLogged = true;
            }
            break;
        case kCallbackSendUIEvent:
            if ( m_uiEvents++ == 0 )
                KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform GameInterface event bridge active" );
            break;
        default:
            break;
        }
    }

    bool IsLegalsReady() const { return m_legalsReady; }
    bool IsLegalsComplete() const { return m_legalsComplete; }
    bool IsStartScreenReady() const { return m_startScreenReady; }
    bool IsMainMenuReady() const { return m_mainMenuReady; }

private:
    uint32_t m_loadFinished;
    uint32_t m_ready;
    uint32_t m_loadErrors;
    uint32_t m_uiEvents;
    bool m_legalsReady;
    bool m_legalsComplete;
    bool m_startScreenReady;
    bool m_mainMenuReady;
    bool m_audioLogged;
};

enum
{
    kScaleformMenuSlot = 0,
    kScaleformHudSlot = 1,
    kScaleformSlotCount = 2
};

enum ScaleformBootStage
{
    kBootInactive = 0,
    kBootLegalsLoading,
    kBootLegalsPlaying,
    kBootStartScreenLoading,
    kBootStartScreenWaiting,
    kBootMainMenuLoading,
    kBootReady
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

struct CsgoFontMapDefinition
{
    const char *exportedName;
    const char *fontName;
    Scaleform::GFx::FontMap::MapFontFlags flags;
};

const CsgoFontMapDefinition kFallbackFontMap[] = {
    { "$BigButtonFont", "Stratum2 Bold", Scaleform::GFx::FontMap::MFF_Original },
    { "$CounterStrike", "Counter-Strike", Scaleform::GFx::FontMap::MFF_Original },
    { "$TextFontLight", "Stratum2 Regular", Scaleform::GFx::FontMap::MFF_Original },
    { "$BodyText", "PF DinText Pro", Scaleform::GFx::FontMap::MFF_Original },
    { "$TextFont", "Stratum2 Regular", Scaleform::GFx::FontMap::MFF_Original },
    { "$TextFontBold", "Stratum2 Bold", Scaleform::GFx::FontMap::MFF_Original },
    { "$cs", "cs", Scaleform::GFx::FontMap::MFF_Original },
    { "$ThaiTest", "iannnnnPDF 2008", Scaleform::GFx::FontMap::MFF_Original },
    { "$ArialDefault", "Arial Unicode MS", Scaleform::GFx::FontMap::MFF_Original }
};

Scaleform::GFx::FontMap::MapFontFlags FontMapFlagsForStyle( const char *style )
{
    if ( style && !V_stricmp( style, "bold" ) )
        return Scaleform::GFx::FontMap::MFF_Bold;
    if ( style && !V_stricmp( style, "italic" ) )
        return Scaleform::GFx::FontMap::MFF_Italic;
    if ( style && ( !V_stricmp( style, "bolditalic" ) ||
                    !V_stricmp( style, "italicbold" ) ) )
        return Scaleform::GFx::FontMap::MFF_BoldItalic;
    return Scaleform::GFx::FontMap::MFF_Original;
}

bool LoadPackagedFontMappings( KeyValues *config )
{
    if ( !config )
        return false;

    const char *path = "/app0/resource/flash/fontmapping.cfg";
    FILE *file = fopen( path, "rb" );
    if ( !file )
        return false;

    const size_t maxConfigBytes = 256u * 1024u;
    CUtlBuffer contents( 0, 4096, CUtlBuffer::TEXT_BUFFER );
    char chunk[4096];
    size_t totalBytes = 0;
    bool valid = true;
    for ( ;; )
    {
        const size_t bytesRead = fread( chunk, 1, sizeof( chunk ), file );
        if ( bytesRead > 0 )
        {
            if ( totalBytes + bytesRead > maxConfigBytes )
            {
                valid = false;
                break;
            }
            contents.Put( chunk, static_cast< int >( bytesRead ) );
            totalBytes += bytesRead;
        }
        if ( bytesRead < sizeof( chunk ) )
        {
            valid = ferror( file ) == 0;
            break;
        }
    }
    fclose( file );

    if ( !valid || totalBytes == 0 )
        return false;
    contents.PutChar( '\0' );
    return config->LoadFromBuffer( path, contents, g_pFullFileSystem );
}

unsigned InstallFontMappings( Scaleform::GFx::FontMap *fontMap, KeyValues *config )
{
    if ( !fontMap )
        return 0;

    unsigned mapped = 0;
    if ( config )
    {
        for ( KeyValues *entry = config->GetFirstTrueSubKey(); entry;
              entry = entry->GetNextTrueSubKey() )
        {
            const char *exportedName = entry->GetName();
            const char *fontName = entry->GetString( "font", "" );
            if ( !exportedName || exportedName[0] != '$' || !fontName || !fontName[0] )
                continue;
            if ( fontMap->MapFont( exportedName, fontName,
                    FontMapFlagsForStyle( entry->GetString( "style", "normal" ) ) ) )
                ++mapped;
        }
    }

    for ( unsigned i = 0; i < sizeof( kFallbackFontMap ) / sizeof( kFallbackFontMap[0] );
          ++i )
    {
        Scaleform::GFx::FontMap::MapEntry existing;
        if ( fontMap->GetFontMapping( &existing, kFallbackFontMap[i].exportedName ) )
            continue;
        if ( fontMap->MapFont( kFallbackFontMap[i].exportedName,
                kFallbackFontMap[i].fontName, kFallbackFontMap[i].flags ) )
            ++mapped;
    }
    return mapped;
}

class CPs4ScaleformMovieManager final
{
public:
    CPs4ScaleformMovieManager()
        : m_system( NULL ), m_loader( NULL ), m_initialized( false ),
          m_loggedCapture( false ), m_lastTime( -1.0f ), m_frame( 0 ),
          m_bootStage( kBootInactive ), m_bootStageFrame( 0 )
    {
        // Source creates these two root movies as Scaleform slots.  MainMenu
        // is an element requested from MainUIRootMovie; GameUIRootMovie is
        // the client/HUD root and receives its level HUD elements later.  The
        // console runtime consumes Scaleform's optimized GFX versions.
        m_slots[kScaleformMenuSlot] = {
            "resource/flash/mainuirootmovie.gfx", NULL, NULL, NULL, false, false };
        m_slots[kScaleformHudSlot] = {
            "resource/flash/gameuirootmovie.gfx", NULL, NULL, NULL, false, false };
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
        m_imageHandlers = *new Scaleform::GFx::ImageFileHandlerRegistry(
            Scaleform::GFx::ImageFileHandlerRegistry::AddDefaultHandlers );
        m_imageCreator = *new Scaleform::GFx::ImageCreator();
        m_loader->SetImageFileHandlerRegistry( m_imageHandlers );
        m_loader->SetImageCreator( m_imageCreator );
        KisakPs4StartupBreadcrumb(
            m_loader->GetImageCreator().GetPtr() &&
            m_loader->GetImageFileHandlerRegistry().GetPtr()
                ? "kisak-ps4: scaleform CPU image creator and DDS handler installed"
                : "kisak-ps4: scaleform image creation state unavailable" );
        LoadScaleformLocalization();
        m_translator = *new KisakScaleformTranslator();
        m_loader->SetTranslator( m_translator );
        m_actionControl = *new Scaleform::GFx::ActionControl(
            Scaleform::GFx::ActionControl::Action_LogAllFilenames );
        m_actionControl->SetActionErrorSuppress( false );
        m_actionControl->SetVerboseAction( false );
        m_loader->SetActionControl( m_actionControl );
        m_zlibSupport = *new Scaleform::GFx::ZlibSupport();
        m_loader->SetZlibSupport( m_zlibSupport );
        KisakPs4StartupBreadcrumb( m_loader->GetZlibSupport().GetPtr()
            ? "kisak-ps4: scaleform zlib support installed"
            : "kisak-ps4: scaleform zlib support unavailable" );
        m_loader->SetAS2Support( Scaleform::Ptr< Scaleform::GFx::AS2Support >(
            *new Scaleform::GFx::AS2Support() ) );
        KisakPs4StartupBreadcrumb( m_loader->GetAS2Support().GetPtr()
            ? "kisak-ps4: scaleform AS2 support and action diagnostics installed"
            : "kisak-ps4: scaleform AS2 support unavailable" );

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

        Scaleform::Ptr< Scaleform::GFx::MovieDef > extraFontDefinition =
            *m_loader->CreateMovie( "resource/flash/fontlib_extra.swf",
                Scaleform::GFx::Loader::LoadAll );
        if ( extraFontDefinition.GetPtr() )
        {
            m_fontLib->AddFontsFrom( extraFontDefinition, true );
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: scaleform extra font library initialized" );
        }
        else
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: scaleform extra font library unavailable" );
        }
        extraFontDefinition.Clear();

        KeyValues *fontConfig = new KeyValues( "english" );
        KeyValues::AutoDelete fontConfigDelete( fontConfig );
        const bool loadedFontConfig = fontConfig &&
            ( LoadPackagedFontMappings( fontConfig ) ||
              ( g_pFullFileSystem && fontConfig->LoadFromFile( g_pFullFileSystem,
                  "resource/flash/fontmapping.cfg", "GAME" ) ) );
        m_fontMap = *new Scaleform::GFx::FontMap();
        const unsigned mappedFonts = InstallFontMappings(
            m_fontMap.GetPtr(), loadedFontConfig ? fontConfig : NULL );
        m_loader->SetFontMap( m_fontMap );
        char fontMapMarker[160];
        snprintf( fontMapMarker, sizeof( fontMapMarker ),
            "kisak-ps4: scaleform font map config=%u aliases=%u",
            loadedFontConfig ? 1u : 0u, mappedFonts );
        KisakPs4StartupBreadcrumb( fontMapMarker );

        const bool menu = LoadSlot( kScaleformMenuSlot );
        const bool hud = LoadSlot( kScaleformHudSlot );
        m_initialized = menu || hud;
        if ( menu )
            BeginBootSequence();
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
            m_fontMap.Clear();
            m_imageCreator.Clear();
            m_imageHandlers.Clear();
            m_translator.Clear();
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
        m_fontMap.Clear();
        m_imageCreator.Clear();
        m_imageHandlers.Clear();
        m_translator.Clear();
        m_fileOpener.Clear();
        m_log.Clear();
        m_actionControl.Clear();
        m_zlibSupport.Clear();
        delete m_system;
        m_system = NULL;
        m_initialized = false;
        m_lastTime = -1.0f;
        m_frame = 0;
        m_bootStage = kBootInactive;
        m_bootStageFrame = 0;
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
        AdvanceBootSequence();
    }

    bool Render( int slot, const char *phase )
    {
        if ( slot < 0 || slot >= kScaleformSlotCount || !m_slots[slot].movie.GetPtr() )
            return false;

        Scaleform::GFx::Movie *movie = m_slots[slot].movie.GetPtr();
        // Advance() captures modified trees. Forcing Capture(false) here clones
        // an unchanged snapshot for both slots every frame and serializes work
        // that the retained OpenGNM geometry does not consume.
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
        if ( handled )
            KisakPs4ScaleformHal().RequestDynamicRefresh( 30 );
        if ( event.m_nType == IE_ButtonPressed &&
             m_bootStage == kBootStartScreenWaiting &&
             ( event.m_nData == KEY_XBUTTON_A || event.m_nData == KEY_ENTER ||
               event.m_nData == KEY_SPACE ) )
        {
            CompleteStartScreen( "controller" );
            handled = true;
        }
        if ( event.m_nType == IE_ButtonPressed ||
             event.m_nType == IE_ButtonReleased )
        {
            static unsigned int loggedInputs = 0;
            if ( loggedInputs < 24 )
            {
                char message[160];
                snprintf( message, sizeof( message ),
                    "kisak-ps4: scaleform input type=%d button=%d key=%u handled=%u",
                    event.m_nType, event.m_nData,
                    static_cast< unsigned int >( MapButtonCode(
                        static_cast< ButtonCode_t >( event.m_nData ) ) ),
                    handled ? 1u : 0u );
                KisakPs4StartupBreadcrumb( message );
                ++loggedInputs;
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
        CreateGameInterface( movie, &args[1], elementName );
        if ( !args[1].IsObject() )
            return false;
        global.Invoke( "RequestElement", NULL, args, 2 );
        return true;
    }

private:
    void SetBootStage( ScaleformBootStage stage, const char *reason )
    {
        m_bootStage = stage;
        m_bootStageFrame = m_frame;
        char marker[192];
        snprintf( marker, sizeof( marker ),
            "kisak-ps4: scaleform boot stage=%u frame=%llu reason=%s",
            static_cast< unsigned int >( stage ),
            static_cast< unsigned long long >( m_frame ),
            reason ? reason : "none" );
        KisakPs4StartupBreadcrumb( marker );
    }

    bool RemoveBootElement( const char *globalMember )
    {
        Scaleform::GFx::Movie *movie = m_slots[kScaleformMenuSlot].movie.GetPtr();
        if ( !movie || !globalMember )
            return false;
        Scaleform::GFx::Value global;
        Scaleform::GFx::Value element;
        Scaleform::GFx::Value removeFunction;
        if ( !movie->GetVariable( &global, "_global" ) ||
             !global.GetMember( globalMember, &element ) || !element.IsObject() ||
             !global.GetMember( "RemoveElement", &removeFunction ) )
            return false;
        return global.Invoke( "RemoveElement", NULL, &element, 1 );
    }

    void RequestStartScreen( const char *reason )
    {
        RemoveBootElement( "LegalsMovie" );
        SetBootStage( kBootStartScreenLoading, reason );
        if ( !RequestElement( kScaleformMenuSlot, "StartScreen" ) )
        {
            SetBootStage( kBootMainMenuLoading, "StartScreen request fallback" );
            RequestElement( kScaleformMenuSlot, "MainMenu" );
        }
    }

    void CompleteStartScreen( const char *reason )
    {
        if ( m_bootStage != kBootStartScreenWaiting &&
             m_bootStage != kBootStartScreenLoading )
            return;
        RemoveBootElement( "StartScreenMovie" );
        SetBootStage( kBootMainMenuLoading, reason );
        if ( !RequestElement( kScaleformMenuSlot, "MainMenu" ) )
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: scaleform boot MainMenu request failed" );
    }

    void BeginBootSequence()
    {
#if KISAK_PS4_SCALEFORM_DIRECT_MENU
        SetBootStage( kBootMainMenuLoading, "direct development mode" );
        RequestElement( kScaleformMenuSlot, "MainMenu" );
#else
        SetBootStage( kBootLegalsLoading, "classic sequence" );
        if ( !RequestElement( kScaleformMenuSlot, "Legals" ) )
            RequestStartScreen( "Legals request fallback" );
#endif
    }

    void AdvanceBootSequence()
    {
        if ( !m_callbackHandler.GetPtr() )
            return;
        const uint64_t elapsed = m_frame - m_bootStageFrame;
        switch ( m_bootStage )
        {
        case kBootLegalsLoading:
            if ( m_callbackHandler->IsLegalsReady() )
                SetBootStage( kBootLegalsPlaying, "Legals ready" );
            else if ( elapsed >= 600 )
                RequestStartScreen( "Legals load timeout" );
            break;
        case kBootLegalsPlaying:
            if ( m_callbackHandler->IsLegalsComplete() )
                RequestStartScreen( "Legals completed" );
            else if ( elapsed >= 360 )
                RequestStartScreen( "Legals animation timeout" );
            break;
        case kBootStartScreenLoading:
            if ( m_callbackHandler->IsStartScreenReady() )
            {
                Scaleform::GFx::Movie *movie =
                    m_slots[kScaleformMenuSlot].movie.GetPtr();
                Scaleform::GFx::Value startScreen;
                bool shown = false;
                if ( movie && movie->GetVariable( &startScreen,
                         "_global.StartScreenMovie" ) && startScreen.IsObject() )
                    shown = startScreen.Invoke( "ShowStartLogo" );
                SetBootStage( kBootStartScreenWaiting,
                    shown ? "StartScreen ready" : "StartScreen ready without logo hook" );
            }
            else if ( elapsed >= 600 )
                CompleteStartScreen( "StartScreen load timeout" );
            break;
        case kBootStartScreenWaiting:
            // Keep development boot non-blocking until the post-open PS4 pad
            // polling gap is fixed. Controller confirmation takes this path
            // immediately when input is available.
            if ( elapsed >= 180 )
                CompleteStartScreen( "offline development timeout" );
            break;
        case kBootMainMenuLoading:
            if ( m_callbackHandler->IsMainMenuReady() )
                SetBootStage( kBootReady, "MainMenu ready" );
            break;
        default:
            break;
        }
    }

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
        Scaleform::GFx::Value *gameInterface, const char *elementName )
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

        if ( elementName && elementName[0] )
            gameInterface->SetMember( "__KisakElementName", elementName );
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

        // LoadWaitFrame1 has already made frame one available.  Defer the
        // instance's implicit first advance until after PlatformCode and the
        // GameInterface object are installed below.
        movieSlot.movie = *movieSlot.definition->CreateInstance(
            false, 0, m_actionControl.GetPtr() );
        if ( !movieSlot.movie.GetPtr() )
            return false;

        const char *movieUrl = movieSlot.definition->GetFileURL();
        char metadataMarker[256];
        snprintf( metadataMarker, sizeof( metadataMarker ),
            "kisak-ps4: scaleform %s metadata url=%s version=%u frames=%u loading=%u size=%ux%u avm=%d current=%u",
            ScaleformSlotLabel( slot ), movieUrl ? movieUrl : "",
            movieSlot.definition->GetVersion(), movieSlot.definition->GetFrameCount(),
            movieSlot.definition->GetLoadingFrame(),
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

            CreateGameInterface( movieSlot.movie.GetPtr(), &gameInterface, NULL );
            if ( gameInterface.IsObject() )
                global.SetMember( "GameInterface", gameInterface );
        }

        if ( slot == kScaleformMenuSlot )
            movieSlot.movie->HandleEvent( Scaleform::GFx::Event::SetFocus );

        // Match the original BaseSlot::Init bootstrap: a zero-delta advance
        // flushes frame-0 actions after PlatformCode/GameInterface are set.
        movieSlot.movie->Advance( 0.0f );
        // LoadWaitFrame1 makes the root resident, but some GFx builds queue
        // frame actions until one complete movie interval has elapsed.
        const float frameRate = movieSlot.movie->GetFrameRate();
        if ( frameRate > 0.0f )
            movieSlot.movie->Advance( 1.0f / frameRate );

        Scaleform::GFx::Value scriptProbe;
        scriptProbe.SetNull();
        const bool globalGfxExtensions = globalReady &&
            global.GetMember( "gfxExtensions", &scriptProbe );
        scriptProbe.SetNull();
        const bool globalElementLoaders = globalReady &&
            global.GetMember( "ElementLoaders", &scriptProbe );
        scriptProbe.SetNull();
        const bool globalResizeManager = globalReady &&
            global.GetMember( "resizeManager", &scriptProbe );
        char scriptMarker[320];
        snprintf( scriptMarker, sizeof( scriptMarker ),
            "kisak-ps4: scaleform %s script avm=%d current=%u gfx=%u elements=%u resize=%u init=%u request=%u globalInit=%u globalRequest=%u",
            ScaleformSlotLabel( slot ), movieSlot.movie->GetAVMVersion(),
            movieSlot.movie->GetCurrentFrame(),
            globalGfxExtensions ? 1u : 0u, globalElementLoaders ? 1u : 0u,
            globalResizeManager ? 1u : 0u,
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

        // Construct the GFx viewport explicitly.  The six-argument inline
        // helper reached hardware with AspectRatio == 0 and Scale == 2/3,
        // collapsing the root X transform to the screen center.  These fields
        // are part of GFx::Viewport beyond the Render::Viewport base and must
        // remain initialized for SM_NoScale.
        Scaleform::GFx::Viewport movieViewport( 1920, 1080, 0, 0, 1920, 1080 );
        movieViewport.Scale = 1.0f;
        movieViewport.AspectRatio = 1.0f;
        movieSlot.movie->SetViewport( movieViewport );
        Scaleform::GFx::Viewport appliedViewport;
        movieSlot.movie->GetViewport( &appliedViewport );
        static bool loggedViewport = false;
        if ( !loggedViewport )
        {
            const Scaleform::Render::RectF visibleFrame =
                movieSlot.movie->GetVisibleFrameRect();
            char viewportMarker[320];
            snprintf( viewportMarker, sizeof( viewportMarker ),
                "kisak-ps4: scaleform viewport width=%d height=%d scale=%.4f ratio=%.4f mode=%u visible=%.2f,%.2f..%.2f,%.2f",
                appliedViewport.Width, appliedViewport.Height,
                appliedViewport.Scale, appliedViewport.AspectRatio,
                static_cast< unsigned int >( movieSlot.movie->GetViewScaleMode() ),
                visibleFrame.x1, visibleFrame.y1,
                visibleFrame.x2, visibleFrame.y2 );
            KisakPs4StartupBreadcrumb( viewportMarker );
            loggedViewport = true;
        }
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
    Scaleform::Ptr< Scaleform::GFx::FontMap > m_fontMap;
    Scaleform::Ptr< Scaleform::GFx::ImageCreator > m_imageCreator;
    Scaleform::Ptr< Scaleform::GFx::ImageFileHandlerRegistry > m_imageHandlers;
    Scaleform::Ptr< KisakScaleformTranslator > m_translator;
    Scaleform::Ptr< KisakScaleformFunctionHandler > m_callbackHandler;
    ScaleformMovieSlot m_slots[kScaleformSlotCount];
    bool m_initialized;
    bool m_loggedCapture;
    float m_lastTime;
    uint64_t m_frame;
    ScaleformBootStage m_bootStage;
    uint64_t m_bootStageFrame;
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
