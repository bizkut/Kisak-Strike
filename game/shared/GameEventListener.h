//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAME_EVENT_LISTENER_H
#define GAME_EVENT_LISTENER_H
#ifdef _WIN32
#pragma once
#endif

#include "igameevents.h"
extern IGameEventManager2 *gameeventmanager;

#if defined( PLATFORM_PS4 )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );

static inline bool KisakPs4IsHltvStatusEvent( const char *name )
{
	if ( !name )
		return false;

	static const char target[] = "hltv_status";
	for ( int i = 0; ; ++i )
	{
		if ( name[i] != target[i] )
			return false;
		if ( target[i] == '\0' )
			return true;
	}
}
#endif

// A safer method than inheriting straight from IGameEventListener2.
// Avoids requiring the user to remove themselves as listeners in 
// their deconstructor, and sets the serverside variable based on
// our dll location.
class CGameEventListener : public IGameEventListener2
{
public:
	CGameEventListener() : m_bRegisteredForEvents(false)
	{
		m_nDebugID = EVENT_DEBUG_ID_INIT;
	}

	~CGameEventListener()
	{
		m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
		StopListeningForAllEvents();
	}

	void ListenForGameEvent( const char *name )
	{
#if defined( PLATFORM_PS4 )
		const bool bTraceHltvStatus = KisakPs4IsHltvStatusEvent( name );
		if ( bTraceHltvStatus )
		{
			KisakPs4StartupBreadcrumb( "kisak-ps4: game event listener hltv inline entered" );
#ifdef CLIENT_DLL
			KisakPs4StartupBreadcrumb( "kisak-ps4: game event listener hltv client inline selected" );
#else
			KisakPs4StartupBreadcrumb( "kisak-ps4: game event listener hltv server inline selected" );
#endif
		}

		if ( !gameeventmanager )
		{
			if ( bTraceHltvStatus )
				KisakPs4StartupBreadcrumb( "kisak-ps4: game event listener hltv manager missing" );
			return;
		}

		if ( bTraceHltvStatus )
			KisakPs4StartupBreadcrumb( "kisak-ps4: game event listener hltv manager ready" );
#endif

#ifdef CLIENT_DLL
		bool bServerSide = false;
#else
		bool bServerSide = true;
#endif

		const bool bAdded = gameeventmanager->AddListener( this, name, bServerSide );
		if ( bAdded )
			m_bRegisteredForEvents = true;

#if defined( PLATFORM_PS4 )
		if ( bTraceHltvStatus )
		{
			KisakPs4StartupBreadcrumb( bAdded
				? "kisak-ps4: game event listener hltv add returned true"
				: "kisak-ps4: game event listener hltv add returned false" );
		}
#endif
	}

	void ListenForAllGameEvents()
	{

#ifdef CLIENT_DLL
	bool bServerSide = false;
#else
	bool bServerSide = true;
#endif

		gameeventmanager->AddListenerGlobal( this, bServerSide );
	}


	void StopListeningForAllEvents()
	{
		// remove me from list
		if ( m_bRegisteredForEvents )
		{
			if ( gameeventmanager )
				gameeventmanager->RemoveListener( this );

			m_bRegisteredForEvents = false;
		}
	}

	// Intentionally abstract
	virtual void FireGameEvent( IGameEvent *event ) = 0;
	int m_nDebugID;
	virtual int GetEventDebugID( void )			{ return m_nDebugID; }

private:

	// Have we registered for any events?
	bool m_bRegisteredForEvents;
};

#endif
