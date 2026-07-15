//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Deals with singleton  
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "datacache/imdlcache.h"
#include "utlvector.h"
#include "vprof.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#elif defined( _PS3 )
#include "materialsystem/imaterialsystem.h" // for loading fontlib
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );

static void KisakPs4TraceServerGameSystem( const char *phase, int index, const char *name, int value )
{
	char line[256];
	Q_snprintf( line, sizeof( line ),
		"kisak-ps4: server game system %s index=%d name=%s value=%d",
		phase, index, name ? name : "<null>", value );
	KisakPs4StartupBreadcrumb( line );
}
#endif

// enable this #define (here or in your vpc) to have 
// each GameSystem accounted for individually in the vprof.
// You must enable VPROF_LEVEL 1 too.
// #define VPROF_ACCOUNT_GAMESYSTEMS

// Pointer to a member method of IGameSystem
typedef void (IGameSystem::*GameSystemFunc_t)();

// Pointer to a member method of IGameSystem
typedef void (IGameSystemPerFrame::*PerFrameGameSystemFunc_t)();

// Used to invoke a method of all added Game systems in order
static void InvokeMethod( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in order
static void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in reverse order
static void InvokeMethodReverseOrder( GameSystemFunc_t f );

// Used to invoke a method of all added Game systems in order
static void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed = 0 );

static bool s_bSystemsInitted = false; 

// List of all installed Game systems
static CUtlVector<IGameSystem*> s_GameSystems( 0, 4 );
// List of all installed Game systems
static CUtlVector<IGameSystemPerFrame*> s_GameSystemsPerFrame( 0, 4 );

// The map name
static char* s_pMapName = 0;

static CBasePlayer *s_pRunCommandPlayer = NULL;
static CUserCmd *s_pRunCommandUserCmd = NULL;

//-----------------------------------------------------------------------------
// Auto-registration of game systems
//-----------------------------------------------------------------------------
static	CAutoGameSystem *s_pSystemList = NULL;

CAutoGameSystem::CAutoGameSystem( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pSystemList;
		s_pSystemList = this;
	}
}

static	CAutoGameSystemPerFrame *s_pPerFrameSystemList = NULL;

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
CAutoGameSystemPerFrame::CAutoGameSystemPerFrame( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pPerFrameSystemList;
		s_pPerFrameSystemList = this;
	}
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystem::~IGameSystem()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystemPerFrame::~IGameSystemPerFrame()
{
	Remove( this );
}


//-----------------------------------------------------------------------------
// Adds a system to the list of systems to run
//-----------------------------------------------------------------------------
void IGameSystem::Add( IGameSystem* pSys )
{
	s_GameSystems.AddToTail( pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != NULL )
	{
		s_GameSystemsPerFrame.AddToTail( static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}


//-----------------------------------------------------------------------------
// Removes a system from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::Remove( IGameSystem* pSys )
{
	s_GameSystems.FindAndRemove( pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != NULL )
	{
		s_GameSystemsPerFrame.FindAndRemove( static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes *all* systems from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::RemoveAll(  )
{
	s_GameSystems.RemoveAll();
	s_GameSystemsPerFrame.RemoveAll();
}


//-----------------------------------------------------------------------------
// Client systems can use this to get at the map name
//-----------------------------------------------------------------------------
char const*	IGameSystem::MapName()
{
	return s_pMapName;
}

#ifndef CLIENT_DLL
CBasePlayer *IGameSystem::RunCommandPlayer()
{
	return s_pRunCommandPlayer;
}

CUserCmd *IGameSystem::RunCommandUserCmd()
{
	return s_pRunCommandUserCmd;
}
#endif

//-----------------------------------------------------------------------------
// Invokes methods on all installed game systems
//-----------------------------------------------------------------------------
bool IGameSystem::InitAllSystems()
{
	int i;

#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
	KisakPs4StartupBreadcrumb( "kisak-ps4: server game system init all entered" );
#endif

	{
		// first add any auto systems to the end
		CAutoGameSystem *pSystem = s_pSystemList;
		int autoSystemIndex = 0;
		while ( pSystem )
		{
#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
			if ( autoSystemIndex < 256 )
				KisakPs4TraceServerGameSystem( "auto before add", autoSystemIndex, pSystem->Name(), 0 );
#endif
			if ( s_GameSystems.Find( pSystem ) == s_GameSystems.InvalidIndex() )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}
			pSystem = pSystem->m_pNext;
			++autoSystemIndex;
		}
		s_pSystemList = NULL;
#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
		KisakPs4TraceServerGameSystem( "auto list ready", autoSystemIndex, NULL, s_GameSystems.Count() );
#endif
	}

	{
		CAutoGameSystemPerFrame *pSystem = s_pPerFrameSystemList;
		int perFrameSystemIndex = 0;
		while ( pSystem )
		{
#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
			if ( perFrameSystemIndex < 256 )
				KisakPs4TraceServerGameSystem( "per-frame before add", perFrameSystemIndex, pSystem->Name(), 0 );
#endif
			if ( s_GameSystems.Find( pSystem ) == s_GameSystems.InvalidIndex() )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}

			pSystem = pSystem->m_pNext;
			++perFrameSystemIndex;
		}
		s_pSystemList = NULL;
#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
		KisakPs4TraceServerGameSystem( "per-frame list ready", perFrameSystemIndex, NULL, s_GameSystems.Count() );
#endif
	}
	// Now remember that we are initted so new CAutoGameSystems will add themselves automatically.
	s_bSystemsInitted = true;

	// PS3: haul the fontlib into memory; some systems (eg vgui) need it.
	for ( i = 0; i < s_GameSystems.Count(); ++i )
	{
#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
		KisakPs4TraceServerGameSystem( "before mdlcache lock", i, NULL, s_GameSystems.Count() );
#endif
		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();

		IGameSystem *sys = s_GameSystems[i];

#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
		KisakPs4TraceServerGameSystem( "before init", i, sys->Name(), s_GameSystems.Count() );
#endif

#if defined( _GAMECONSOLE )
		char sz[128];
		Q_snprintf( sz, sizeof( sz ), "%s->Init():Start", sys->Name() );
		COM_TimestampedLog( sz );
#endif
		bool valid = sys->Init();

#if defined( PLATFORM_PS4 ) && !defined( CLIENT_DLL )
		KisakPs4TraceServerGameSystem( "after init", i, sys->Name(), valid ? 1 : 0 );
#endif

#if defined( _GAMECONSOLE )
		Q_snprintf( sz, sizeof( sz ), "%s->Init():Finish", sys->Name() );
		COM_TimestampedLog( sz );
#endif
		if ( !valid )
		{
			DevWarning( 1, "Failed to load %s\n", sys->Name() );
			return false;
		}
	}

	return true;
}

void IGameSystem::PostInitAllSystems( void )
{
	InvokeMethod( &IGameSystem::PostInit, "PostInit" );
}

void IGameSystem::ShutdownAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::Shutdown );
}

void IGameSystem::LevelInitPreEntityAllSystems( char const* pMapName )
{
	// Store off the map name
	if ( s_pMapName )
	{
		delete[] s_pMapName;
	}

	int len = Q_strlen(pMapName) + 1;
	s_pMapName = new char [ len ];
	Q_strncpy( s_pMapName, pMapName, len );
	
	InvokeMethodTickProgress( &IGameSystem::LevelInitPreEntity, "LevelInitPreEntity" );
}

void IGameSystem::LevelInitPostEntityAllSystems( void )
{
	InvokeMethod( &IGameSystem::LevelInitPostEntity, "LevelInitPostEntity" );
}

void IGameSystem::LevelShutdownPreEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPreEntity );
}

void IGameSystem::LevelShutdownPostEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPostEntity );

	if ( s_pMapName )
	{
		delete[] s_pMapName;
		s_pMapName = 0;
	}
}

void IGameSystem::OnSaveAllSystems()
{
	InvokeMethod( &IGameSystem::OnSave );
}

void IGameSystem::OnRestoreAllSystems()
{
	InvokeMethod( &IGameSystem::OnRestore );
}

void IGameSystem::SafeRemoveIfDesiredAllSystems()
{
    SNPROF("SafeRemoveIfDesiredAllSystems");
	InvokeMethodReverseOrder( &IGameSystem::SafeRemoveIfDesired );
}

#ifdef CLIENT_DLL

void IGameSystem::PreRenderAllSystems()
{
	VPROF("IGameSystem::PreRenderAllSystems");
	InvokePerFrameMethod( &IGameSystemPerFrame::PreRender );
}

void IGameSystem::UpdateAllSystems( float frametime )
{
	SafeRemoveIfDesiredAllSystems();

	int i;
	int c = s_GameSystemsPerFrame.Count();
	MDLCACHE_CRITICAL_SECTION();
	for ( i = 0; i < c; ++i )
	{
		IGameSystemPerFrame *sys = s_GameSystemsPerFrame[i];
		sys->Update( frametime );
	}
}

void IGameSystem::PostRenderAllSystems()
{
	InvokePerFrameMethod( &IGameSystemPerFrame::PostRender );
}

#else

void IGameSystem::FrameUpdatePreEntityThinkAllSystems()
{
	SNPROF("FrameUpdatePreEntityThinkAllSystems");
	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePreEntityThink );
}

void IGameSystem::FrameUpdatePostEntityThinkAllSystems()
{
	SNPROF("FrameUpdatePostEntityThinkAllSystems");
	SafeRemoveIfDesiredAllSystems();

	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePostEntityThink );
}

void IGameSystem::PreClientUpdateAllSystems() 
{
	SNPROF("PreClientUpdateAllSystems");
	InvokePerFrameMethod( &IGameSystemPerFrame::PreClientUpdate );
}

#endif


//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethod( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	NOTE_UNUSED( timed );

	MDLCACHE_COARSE_LOCK();
	MDLCACHE_CRITICAL_SECTION();
	int i;
	int c = s_GameSystems.Count();
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	NOTE_UNUSED( timed );

	int i;
	int c = s_GameSystems.Count();
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();
#if defined( CLIENT_DLL )
		engine->TickProgressBar();
#endif
		(sys->*f)();
	}
}
//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed /*=0*/ )
{
	NOTE_UNUSED( timed );

	int i;
	int c = s_GameSystemsPerFrame.Count();
	for ( i = 0; i < c ; ++i )
	{
		IGameSystemPerFrame *sys  = s_GameSystemsPerFrame[i];
#if (VPROF_LEVEL > 0) && defined(VPROF_ACCOUNT_GAMESYSTEMS)   // make sure each game system is individually attributed
		// because vprof nodes must really be constructed with a pointer to a static
		// string, we can't create a temporary char[] here and sprintf a distinctive
		// V_snprintf( buf, 63, "gamesys_preframe_%s", sys->Name() ). We'll have to
		// settle for just the system name, and distinguish between pre and post frame
		// in hierarchy.
		VPROF( sys->Name() );
#endif
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in reverse order
//-----------------------------------------------------------------------------
void InvokeMethodReverseOrder( GameSystemFunc_t f )
{
	int i;
	int c = s_GameSystems.Count();
	MDLCACHE_CRITICAL_SECTION();
	for ( i = c; --i >= 0; )
	{
		IGameSystem *sys = s_GameSystems[i];
#if (VPROF_LEVEL > 0) && defined(VPROF_ACCOUNT_GAMESYSTEMS)   // make sure each game system is individually attributed
		// because vprof nodes must really be constructed with a pointer to a static
		// string, we can't create a temporary char[] here and sprintf a distinctive
		// V_snprintf( buf, 63, "gamesys_preframe_%s", sys->Name() ). We'll have to
		// settle for just the system name, and distinguish between pre and post frame
		// in hierarchy.
		VPROF( sys->Name() );
#endif
		(sys->*f)();
	}
}

