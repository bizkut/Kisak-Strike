#include "icvar.h"
#include "common/engine_launcher_api.h"
#include "inputsystem/iinputsystem.h"
#include "rocketui/rocketui.h"
#include "tier1/convar.h"

#include <chrono>
#include <string.h>
#include <thread>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );
extern "C" bool KisakPs4VideoOutInitialize();
extern "C" bool KisakPs4VideoOutSubmitClear();
extern "C" void KisakPs4VideoOutShutdown();

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
        IRocketUI *rocketUI = RocketUI();
		const bool videoOutReady = KisakPs4VideoOutInitialize();
		for ( int frame = 0; frame < 120; ++frame )
		{
			if ( g_pInputSystem )
				g_pInputSystem->PollInputState( false );
			if ( rocketUI )
			{
				if ( frame == 0 )
					KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame begin" );
				rocketUI->RunFrame( frame * ( 1.0f / 60.0f ) );
				rocketUI->RenderMenuFrame();
				if ( frame == 0 )
					KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher first frame complete" );
			}
			if ( frame == 59 )
				KisakPs4StartupBreadcrumb( "kisak-ps4: engine launcher frame 60" );
			if ( frame == 0 && videoOutReady )
				(void)KisakPs4VideoOutSubmitClear();
			std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
		}
		KisakPs4VideoOutShutdown();
        return RUN_OK;
    }
    void SetEngineWindow( void * ) override {}
    void PostConsoleCommand( const char * ) override {}
    bool IsRunningSimulation() const override { return m_bSimulationActive; }
    void ActivateSimulation( bool active ) override { m_bSimulationActive = active; }
    void SetMap( const char * ) override {}

private:
    StartupInfo_t m_StartupInfo = {};
    bool m_bSimulationActive = false;
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
