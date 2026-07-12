#include "icvar.h"
#include "common/engine_launcher_api.h"
#include "inputsystem/iinputsystem.h"
#include "scaleformui/ps4_scaleformui.h"
#include "tier1/convar.h"

#include <chrono>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <thread>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );
extern "C" bool KisakPs4VideoOutInitialize();
extern "C" bool KisakPs4VideoOutSubmitClear();
extern "C" void KisakPs4VideoOutShutdown();
extern "C" bool KisakPs4GnmSubmissionSelfTest();
extern "C" void KisakPs4GnmSubmissionShutdown();
extern "C" void KisakPs4SetSourceFrameCallback(
    void ( *callback )( void * ), void *context );
extern "C" const char *KisakPs4ScaleformSdkVersion();
extern "C" bool KisakPs4ScaleformKernelSelfTest();
extern "C" bool KisakPs4ScaleformMovieProbe();
extern "C" bool KisakPs4ScaleformMovieInstanceProbe();

namespace
{
struct Ps4SourceFrameContext
{
    IPs4ScaleformUI *scaleformUI;
    uint64_t frame;
};

void RunPs4SourceFrame( void *opaque )
{
    Ps4SourceFrameContext *context = static_cast< Ps4SourceFrameContext * >( opaque );
    if ( !context || !context->scaleformUI )
        return;
    if ( context->frame == 0 )
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame begin" );
    context->scaleformUI->RunFrame(
        static_cast< float >( context->frame ) * ( 1.0f / 60.0f ) );
    context->scaleformUI->RenderMenuFrame();
    context->scaleformUI->RenderHUDFrame();
    if ( context->frame == 0 )
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame complete" );
}

class CPs4CvarQuery final : public ICvarQuery
{
public:
    bool Connect( CreateInterfaceFn factory ) override
    {
        ICvar *cvar = static_cast<ICvar *>( factory( CVAR_INTERFACE_VERSION, NULL ) );
        if ( !cvar )
            return false;
        cvar->InstallCVarQuery( this );
        return true;
    }

    void Disconnect() override {}

    void *QueryInterface( const char *name ) override
    {
        return name && strcmp( name, CVAR_QUERY_INTERFACE_VERSION ) == 0 ? this : NULL;
    }

    InitReturnVal_t Init() override { return INIT_OK; }
    void Shutdown() override {}

    bool AreConVarsLinkable( const ConVar *child, const ConVar *parent ) override
    {
        if ( !child || !parent )
            return false;
        const bool childReplicated = child->IsFlagSet( FCVAR_REPLICATED );
        const bool parentReplicated = parent->IsFlagSet( FCVAR_REPLICATED );
        if ( childReplicated != parentReplicated )
            return false;
        if ( childReplicated )
        {
            if ( child->IsFlagSet( FCVAR_PROTECTED ) || parent->IsFlagSet( FCVAR_PROTECTED ) )
                return false;
            if ( child->IsCommand() || parent->IsCommand() )
                return false;
            if ( child->IsFlagSet( FCVAR_GAMEDLL ) && !parent->IsFlagSet( FCVAR_CLIENTDLL ) )
                return false;
            if ( child->IsFlagSet( FCVAR_CLIENTDLL ) && !parent->IsFlagSet( FCVAR_GAMEDLL ) )
                return false;
            return true;
        }
        return !parent->IsFlagSet( FCVAR_CLIENTDLL | FCVAR_GAMEDLL );
    }
};

class CPs4EngineLauncher final : public IEngineAPI
{
public:
    bool Connect( CreateInterfaceFn ) override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap connect" );
        return true;
    }
    void Disconnect() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap disconnect" );
    }

    void *QueryInterface( const char *name ) override
    {
        return name && strcmp( name, VENGINE_LAUNCHER_API_VERSION ) == 0 ? this : NULL;
    }

    InitReturnVal_t Init() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap init" );
        return INIT_OK;
    }
    void Shutdown() override
    {
		m_QuitRequested.store( true );
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap shutdown" );
    }

    bool SetStartupInfo( StartupInfo_t &info ) override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap startup info" );
        m_StartupInfo = info;
        return true;
    }

    int Run() override
    {
		KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher bootstrap run" );
    KisakPs4StartupBreadcrumb( "kisak-ps4: build marker scaleform_console_gfx_roots_v351" );
		KisakPs4StartupBreadcrumb( KisakPs4ScaleformSdkVersion() );
		KisakPs4StartupBreadcrumb( KisakPs4ScaleformKernelSelfTest()
			? "kisak-ps4: scaleform kernel self-test passed"
			: "kisak-ps4: scaleform kernel self-test failed" );
		KisakPs4StartupBreadcrumb( KisakPs4ScaleformMovieProbe()
			? "kisak-ps4: scaleform fontlib movie probe passed"
			: "kisak-ps4: scaleform fontlib movie probe unavailable" );
		KisakPs4StartupBreadcrumb( KisakPs4ScaleformMovieInstanceProbe()
			? "kisak-ps4: scaleform render-tree handle probe passed"
			: "kisak-ps4: scaleform render-tree handle probe failed" );
		IPs4ScaleformUI *scaleformUI = Ps4ScaleformUI();
		const bool videoOutReady = KisakPs4VideoOutInitialize();
		KisakPs4GnmSubmissionSelfTest();
		Ps4SourceFrameContext sourceFrame = { scaleformUI, 0 };
		KisakPs4SetSourceFrameCallback( RunPs4SourceFrame, &sourceFrame );
		m_QuitRequested.store( false );
		uint64_t frame = 0;
		while ( !m_QuitRequested.load() )
		{
			if ( g_pInputSystem )
				g_pInputSystem->PollInputState( false );
			sourceFrame.frame = frame;
			const uint64_t completedFrame = frame + 1;
			if ( ( completedFrame <= 1200 && completedFrame % 60 == 0 ) ||
				completedFrame % 3600 == 0 )
			{
				char marker[80];
				snprintf( marker, sizeof( marker ),
					"kisak-ps4: engine launcher frame %llu",
					static_cast< unsigned long long >( completedFrame ) );
				KisakPs4StartupBreadcrumb( marker );
			}
			if ( videoOutReady )
			{
				if ( !KisakPs4VideoOutSubmitClear() )
				{
					KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher frame presentation failed" );
					break;
				}
			}
			if ( !videoOutReady )
				std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
			++frame;
		}
		KisakPs4SetSourceFrameCallback( 0, 0 );
		KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher quit requested" );
		KisakPs4GnmSubmissionShutdown();
		KisakPs4VideoOutShutdown();
        return RUN_OK;
    }
    void SetEngineWindow( void * ) override {}
    void PostConsoleCommand( const char *command ) override
    {
		if ( !command )
			return;
		if ( ( strncmp( command, "quit", 4 ) == 0 &&
			( command[4] == 0 || command[4] == '\n' || command[4] == ' ' ) ) ||
			( strncmp( command, "exit", 4 ) == 0 &&
			( command[4] == 0 || command[4] == '\n' || command[4] == ' ' ) ) )
		{
			m_QuitRequested.store( true );
		}
    }
    bool IsRunningSimulation() const override { return m_bSimulationActive; }
    void ActivateSimulation( bool active ) override { m_bSimulationActive = active; }
    void SetMap( const char * ) override {}

private:
    StartupInfo_t m_StartupInfo = {};
    bool m_bSimulationActive = false;
    std::atomic< bool > m_QuitRequested{ false };
};

CPs4CvarQuery g_Ps4CvarQuery;
CPs4EngineLauncher g_Ps4EngineLauncher;
}

CreateInterfaceFn KisakEngineBootstrapFactory();

namespace
{
void *EngineBootstrapCreateInterface( const char *name, int *returnCode )
{
    void *result = g_Ps4CvarQuery.QueryInterface( name );
    if ( !result )
        result = g_Ps4EngineLauncher.QueryInterface( name );
    if ( returnCode )
        *returnCode = result ? 0 : 1;
    return result;
}
}

CreateInterfaceFn KisakEngineBootstrapFactory()
{
    return EngineBootstrapCreateInterface;
}
