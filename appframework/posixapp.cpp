//====== Copyright  1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: Pieces of the application framework, shared between POSIX systems (Mac OS X, Linux, etc)
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
#include "appframework/AppFramework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "interface.h"
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "tier1/convar.h"
#include "vstdlib/cvar.h"
#include "togl/rendermechanism.h"

// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"

#if defined( PLATFORM_PS4 )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );
#define PS4_STEAM_BREADCRUMB( line ) KisakPs4StartupBreadcrumb( line )
#else
#define PS4_STEAM_BREADCRUMB( line ) ( (void)0 )
#endif
//asdf
//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

static CSimpleLoggingListener s_SimpleLoggingListener;
ILoggingListener *g_pDefaultLoggingListener = &s_SimpleLoggingListener;

//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance()
{
	return s_HInstance;
}


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = (HINSTANCE)hInstance;
}


//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------

int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( 0 );
	return -1;
}

static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	s_HInstance = NULL;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run( );
}



//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using Steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamApplication::CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup )
{
	m_pChildAppSystemGroup = pAppSystemGroup;
	m_pFileSystem = NULL;
}


//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create( )
{
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam create entered" );
	FileSystem_SetErrorMode( FS_ERRORMODE_NONE );
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after filesystem error mode" );

	char pFileSystemDLL[MAX_PATH];
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam before filesystem name" );
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, m_bSteam ) != FS_OK )
		return false;
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after filesystem name" );

	// Add in the cvar factory
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam before cvar load" );
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after cvar load" );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );	
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after cvar add" );

	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam before filesystem load" );
	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after filesystem load" );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	PS4_STEAM_BREADCRUMB( "kisak-ps4: steam after filesystem add" );
	if ( !m_pFileSystem )
	{
		Error( "Unable to load %s", pFileSystemDLL );
		return false;
	}

	return true;
}

bool CSteamApplication::GetFileSystemDLLName( char *pOut, int nMaxBytes, bool &bIsSteam )
{
	return FileSystem_GetFileSystemDLLName( pOut, nMaxBytes, bIsSteam ) == FS_OK;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy()
{
	m_pFileSystem = NULL;
}


//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit( )
{
	return true;
}

void CSteamApplication::PostShutdown( )
{
}


//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main( )
{
	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run( );
}


int CSteamApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetCurrentStage() != NONE )
		return nRetVal;
	
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;
	
	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}


void CSteamApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"
