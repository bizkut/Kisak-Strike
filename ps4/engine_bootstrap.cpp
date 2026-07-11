#include "icvar.h"
#include "common/engine_launcher_api.h"
#include "inputsystem/iinputsystem.h"
#include "rocketui/rocketui.h"
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

namespace
{
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
		KisakPs4StartupBreadcrumb( "kisak-ps4: build marker live_shader_handles_v246" );
		IRocketUI *rocketUI = RocketUI();
		const bool videoOutReady = KisakPs4VideoOutInitialize();
		KisakPs4GnmSubmissionSelfTest();
		m_QuitRequested.store( false );
		uint64_t frame = 0;
		while ( !m_QuitRequested.load() )
		{
			if ( g_pInputSystem )
				g_pInputSystem->PollInputState( false );
			if ( rocketUI )
			{
				if ( frame == 0 )
					KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame begin" );
				rocketUI->RunFrame( static_cast< float >( frame ) * ( 1.0f / 60.0f ) );
				rocketUI->RenderMenuFrame();
				if ( frame == 0 )
					KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame complete" );
			}
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
