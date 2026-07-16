//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#pragma warning( disable : 4800  )  // disable forcing int to bool performance warning

#include "cbase.h"
#include <cdll_client_int.h>
#include <cdll_util.h>
#include <globalvars_base.h>

// VGUI panel includes
#include <vgui_controls/Panel.h>
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include <keyvalues.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui_controls/Button.h>

#include <igameresources.h>

// sub dialogs
#include "clientscoreboarddialog.h"
#include "spectatorgui.h"
#include "teammenu.h"
#include "vguitextwindow.h"
#include "IGameUIFuncs.h"
#include "mapoverview.h"
#include "hud.h"
#include "NavProgress.h"
#include "commentary_modelviewer.h"

// our definition
#include "baseviewport.h"
#include <filesystem.h>
#include <convar.h>
#include "ienginevgui.h"
#include "iclientmode.h"
#include "vgui_int.h"

#ifdef PORTAL2
#include "radialmenu.h"
#include "vgui/portal_stats_panel.h"
#endif // PORTAL2

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PLATFORM_PS4 )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );
extern "C" void KisakPs4SetScoreboardTrace( int enabled );
#define PS4_VIEWPORT_BREADCRUMB( line ) KisakPs4StartupBreadcrumb( line )
#define PS4_SCOREBOARD_TRACE( enabled ) KisakPs4SetScoreboardTrace( enabled )

static void KisakPs4ViewportPanelBreadcrumb( const char *pPhase, const char *pName )
{
	char line[256];
	V_snprintf( line, sizeof( line ), "kisak-ps4: viewport %s name=%s",
		pPhase ? pPhase : "unknown", pName ? pName : "missing" );
	KisakPs4StartupBreadcrumb( line );
}
#else
#define PS4_VIEWPORT_BREADCRUMB( line ) ((void)0)
#define PS4_SCOREBOARD_TRACE( enabled ) ((void)0)
#define KisakPs4ViewportPanelBreadcrumb( phase, name ) ((void)0)
#endif

static IViewPort *s_pFullscreenViewportInterface;
static IViewPort *s_pViewportInterfaces[ MAX_SPLITSCREEN_PLAYERS ];

IViewPort *GetViewPortInterface()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return s_pViewportInterfaces[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

IViewPort *GetFullscreenViewPortInterface()
{
	return s_pFullscreenViewportInterface;
}

vgui::Panel *g_lastPanel = NULL; // used for mouseover buttons, keeps track of the last active panel
vgui::Button *g_lastButton = NULL; // used for mouseover buttons, keeps track of the last active button
using namespace vgui;

ConVar hud_autoreloadscript("hud_autoreloadscript", "0", FCVAR_NONE, "Automatically reloads the animation script each time one is ran");

static ConVar cl_leveloverviewmarker( "cl_leveloverviewmarker", "0", FCVAR_CHEAT );

CON_COMMAND( showpanel, "Shows a viewport panel <name>" )
{
	if ( !GetViewPortInterface() )
		return;

	if ( args.ArgC() != 2 )
		return;

	GetViewPortInterface()->ShowPanel( args[ 1 ], true );
}

CON_COMMAND( hidepanel, "Hides a viewport panel <name>" )
{
	if ( !GetViewPortInterface() )
		return;

	if ( args.ArgC() != 2 )
		return;

	GetViewPortInterface()->ShowPanel( args[ 1 ], false );
}

/* global helper functions

bool Helper_LoadFile( IBaseFileSystem *pFileSystem, const char *pFilename, CUtlVector<char> &buf )
{
FileHandle_t hFile = pFileSystem->Open( pFilename, "rt" );
if ( hFile == FILESYSTEM_INVALID_HANDLE )
{
Warning( "Helper_LoadFile: missing %s\n", pFilename );
return false;
}

unsigned long len = pFileSystem->Size( hFile );
buf.SetSize( len );
pFileSystem->Read( buf.Base(), buf.Count(), hFile );
pFileSystem->Close( hFile );

return true;
} */

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseViewport::LoadHudAnimations( void )
{
	const char *HUDANIMATION_MANIFEST_FILE = "scripts/hudanimations_manifest.txt";
	KeyValues *manifest = new KeyValues( HUDANIMATION_MANIFEST_FILE );
	if ( manifest->LoadFromFile( g_pFullFileSystem, HUDANIMATION_MANIFEST_FILE, "GAME" ) == false )
	{
		manifest->deleteThis();
		return false;
	}

	bool bClearScript = true;

	// Load each file defined in the text
	for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
	{
		if ( !Q_stricmp( sub->GetName(), "file" ) )
		{
			// Add it
			if ( m_pAnimController->SetScriptFile( GetVPanel(), sub->GetString(), bClearScript ) == false )
			{
				Assert( 0 );
			}

			bClearScript = false;
			continue;
		}
	}

	manifest->deleteThis();
	return true;
}

//================================================================
CBaseViewport::CBaseViewport() : vgui::EditablePanel( NULL, "CBaseViewport" )
{	
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor body entered" );
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor size ready" );
	m_bInitialized = false;
	m_bFullscreenViewport = false;

	m_GameuiFuncs = NULL;
	m_GameEventManager = NULL;
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor before keyboard input" );
	SetKeyBoardInputEnabled( false );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor keyboard input ready" );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor before mouse input" );
	SetMouseInputEnabled( false );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor mouse input ready" );

	m_pBackGround = NULL;

	m_bHasParent = false;
	m_pActivePanel = NULL;

#if !defined( CSTRIKE15 )
	m_pLastActivePanel = NULL;
#endif

	g_lastPanel = NULL;

	m_OldSize[ 0 ] = m_OldSize[ 1 ] = -1;
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport constructor complete" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::VPANEL CBaseViewport::GetSchemeSizingVPanel( void )
{
	return VGui_GetFullscreenRootVPANEL();
}

//-----------------------------------------------------------------------------
// Purpose: Updates hud to handle the new screen size
//-----------------------------------------------------------------------------
void CBaseViewport::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);

	IViewPortPanel* pSpecGuiPanel = FindPanelByName(PANEL_SPECGUI);
	bool bSpecGuiWasVisible = pSpecGuiPanel && pSpecGuiPanel->IsVisible();

	// reload the script file, so the screen positions in it are correct for the new resolution
	ReloadScheme( NULL );

	// recreate all the default panels
	RemoveAllPanels();

	m_pBackGround = new CBackGroundPanel( NULL );
	m_pBackGround->SetZPos( -20 ); // send it to the back 
	m_pBackGround->SetVisible( false );

	if ( !IsFullscreenViewport() )
	{
		CreateDefaultPanels();
	}

	vgui::ipanel()->MoveToBack( m_pBackGround->GetVPanel() ); // really send it to the back

	// hide all panels when reconnecting 
	ShowPanel( PANEL_ALL, false );

	// re-enable the spectator gui if it was previously visible
	if ( bSpecGuiWasVisible )
	{
		ShowPanel( PANEL_SPECGUI, true );
	}
}

void CBaseViewport::CreateDefaultPanels( void )
{
#ifdef PORTAL2
	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_RADIAL_MENU );
	AddNewPanel( CreatePanelByName( PANEL_RADIAL_MENU ), "PANEL_RADIAL_MENU" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_RADIAL_MENU );
#endif // PORTAL2

	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_SCOREBOARD );
	AddNewPanel( CreatePanelByName( PANEL_SCOREBOARD ), "PANEL_SCOREBOARD" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_SCOREBOARD );
	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_INFO );
	AddNewPanel( CreatePanelByName( PANEL_INFO ), "PANEL_INFO" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_INFO );
	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_SPECGUI );
	AddNewPanel( CreatePanelByName( PANEL_SPECGUI ), "PANEL_SPECGUI" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_SPECGUI );
	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_SPECMENU );
	AddNewPanel( CreatePanelByName( PANEL_SPECMENU ), "PANEL_SPECMENU" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_SPECMENU );
	KisakPs4ViewportPanelBreadcrumb( "default panel before create and add", PANEL_NAV_PROGRESS );
	AddNewPanel( CreatePanelByName( PANEL_NAV_PROGRESS ), "PANEL_NAV_PROGRESS" );
	KisakPs4ViewportPanelBreadcrumb( "default panel create and add ready", PANEL_NAV_PROGRESS );
}

void CBaseViewport::UpdateAllPanels( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( vgui::ipanel()->GetMessageContextId( GetVPanel() ) );

	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];

		if ( p->IsVisible() )
		{
			p->Update();
		}
	}
}

IViewPortPanel* CBaseViewport::CreatePanelByName(const char *szPanelName)
{
	KisakPs4ViewportPanelBreadcrumb( "panel factory entered", szPanelName );
	IViewPortPanel* newpanel = NULL;

	if ( Q_strcmp(PANEL_SCOREBOARD, szPanelName) == 0 )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel factory before scoreboard allocation", szPanelName );
		PS4_SCOREBOARD_TRACE( 1 );
		newpanel = new CClientScoreBoardDialog( this );
		PS4_SCOREBOARD_TRACE( 0 );
		KisakPs4ViewportPanelBreadcrumb( "panel factory scoreboard allocation ready", szPanelName );
	}
	else if ( Q_strcmp(PANEL_INFO, szPanelName) == 0 )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel factory before info allocation", szPanelName );
		newpanel = new CTextWindow( this );
		KisakPs4ViewportPanelBreadcrumb( "panel factory info allocation ready", szPanelName );
	}
	/*	else if ( Q_strcmp(PANEL_OVERVIEW, szPanelName) == 0 )
	{
	newpanel = new CMapOverview( this );
	}
	*/
	//else if ( Q_strcmp(PANEL_TEAM, szPanelName) == 0 )
	//{
	//	newpanel = new CTeamMenu( this );
	//}
	else if ( Q_strcmp(PANEL_NAV_PROGRESS, szPanelName) == 0 )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel factory before nav allocation", szPanelName );
		newpanel = new CNavProgress( this );
		KisakPs4ViewportPanelBreadcrumb( "panel factory nav allocation ready", szPanelName );
	}
#ifdef PORTAL2
	else if ( Q_strcmp( PANEL_RADIAL_MENU, szPanelName ) == 0 )
	{
		newpanel = new CRadialMenuPanel( this );
	}
#endif // PORTAL2

	if ( Q_strcmp(PANEL_COMMENTARY_MODELVIEWER, szPanelName) == 0 )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel factory before commentary allocation", szPanelName );
		newpanel = new CCommentaryModelViewer( this );
		KisakPs4ViewportPanelBreadcrumb( "panel factory commentary allocation ready", szPanelName );
	}

	KisakPs4ViewportPanelBreadcrumb( newpanel
		? "panel factory complete ready"
		: "panel factory complete missing", szPanelName );
	return newpanel;
}


bool CBaseViewport::AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName )
{
	KisakPs4ViewportPanelBreadcrumb( "panel add entered", pchDebugName );
	if ( !pPanel )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel add skipped missing", pchDebugName );
		return false;
	}

	// we created a new panel, initialize it
	KisakPs4ViewportPanelBreadcrumb( "panel add before duplicate lookup", pchDebugName );
	if ( FindPanelByName( pPanel->GetName() ) != NULL )
	{
		KisakPs4ViewportPanelBreadcrumb( "panel add duplicate found", pchDebugName );
		DevMsg("CBaseViewport::AddNewPanel: panel with name '%s' already exists.\n", pPanel->GetName() );
		return false;
	}
	KisakPs4ViewportPanelBreadcrumb( "panel add duplicate lookup ready", pchDebugName );

	KisakPs4ViewportPanelBreadcrumb( "panel add before dictionary insert", pchDebugName );
	m_Panels.Insert( pPanel->GetName(), pPanel );
	KisakPs4ViewportPanelBreadcrumb( "panel add dictionary insert ready", pchDebugName );
	KisakPs4ViewportPanelBreadcrumb( "panel add before parent set", pchDebugName );
	pPanel->SetParent( GetVPanel() );
	KisakPs4ViewportPanelBreadcrumb( "panel add parent set ready", pchDebugName );
	KisakPs4ViewportPanelBreadcrumb( "panel add before ordered append", pchDebugName );
	m_UnorderedPanels.AddToTail( pPanel );
	KisakPs4ViewportPanelBreadcrumb( "panel add complete", pchDebugName );

	return true;
}

IViewPortPanel* CBaseViewport::FindPanelByName(const char *szPanelName)
{
	int idx = m_Panels.Find( szPanelName );
	if ( idx == m_Panels.InvalidIndex() )
		return NULL;

	return m_Panels[ idx ];
}

void CBaseViewport::PostMessageToPanel( IViewPortPanel* pPanel, KeyValues *pKeyValues )
{			   
	PostMessage( pPanel->GetVPanel(), pKeyValues );
}

void CBaseViewport::PostMessageToPanel( const char *pName, KeyValues *pKeyValues )
{
	if ( Q_strcmp( pName, PANEL_ALL ) == 0 )
	{
		for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
		{
			IViewPortPanel *p = m_UnorderedPanels[i];
			PostMessageToPanel( p, pKeyValues );
		}

		return;
	}

	IViewPortPanel * panel = NULL;

	if ( Q_strcmp( pName, PANEL_ACTIVE ) == 0 )
	{
		panel = m_pActivePanel;
	}
	else
	{
		panel = FindPanelByName( pName );
	}

	if ( !panel	)
		return;

	PostMessageToPanel( panel, pKeyValues );
}


void CBaseViewport::ShowPanel( const char *pName, bool state, KeyValues *data, bool autoDeleteData )
{
	if ( !data )
	{
		ShowPanel( pName, state );
		return;
	}

	// Also try to show the panel in the full screen viewport
	if ( this != s_pFullscreenViewportInterface )
	{
		GetFullscreenViewPortInterface()->ShowPanel( pName, state, data, false );
	}

	IViewPortPanel *panel = FindPanelByName( pName );
	if ( panel )
	{
		panel->SetData( data );
		GetViewPortInterface()->ShowPanel( panel, state );
	}

	if ( autoDeleteData )
	{
		data->deleteThis();
	}
}


void CBaseViewport::ShowPanel( const char *pName, bool state )
{
	// Also try to show the panel in the full screen viewport
	if ( this != s_pFullscreenViewportInterface )
	{
		GetFullscreenViewPortInterface()->ShowPanel( pName, state );
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( Q_strcmp( pName, PANEL_ALL ) == 0 )
	{
		for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
		{
			IViewPortPanel *p = m_UnorderedPanels[i];
			ShowPanel( p, state );
		}

		return;
	}

	IViewPortPanel * panel = NULL;

	if ( Q_strcmp( pName, PANEL_ACTIVE ) == 0 )
	{
		panel = m_pActivePanel;
	}
	else
	{
		panel = FindPanelByName( pName );
	}

	if ( !panel	)
		return;

	ShowPanel( panel, state );
}

void CBaseViewport::ShowPanel( IViewPortPanel* pPanel, bool state )
{
	// Extra guard to get layout stuff correct
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( GET_ACTIVE_SPLITSCREEN_SLOT() );

	if ( state )
	{
		// if this is an 'active' panel, deactivate old active panel
		if ( pPanel->HasInputElements() )
		{
			// don't show input panels during normal demo playback
			if ( engine->IsPlayingDemo() && !g_bEngineIsHLTV
#if defined( REPLAY_ENABLED )
				&& !engine->IsReplay()
#endif
				)
				return;

			if ( (m_pActivePanel != NULL) && (m_pActivePanel != pPanel) && (m_pActivePanel->IsVisible()) )
			{
				// store a pointer to the currently active panel
				// so we can restore it later
				if ( pPanel->CanReplace( m_pActivePanel->GetName() ) )
				{
#if !defined( CSTRIKE15 )
					m_pLastActivePanel = m_pActivePanel;
#endif

#ifdef CSTRIKE15 
					// in cs, if the scoreboard tries to hide the spectator via this method, just skip it
					IViewPortPanel* pSpecGuiPanel = FindPanelByName(PANEL_SPECGUI);
					if ( pSpecGuiPanel != m_pActivePanel )
					{
						Msg("CBaseViewport::ShowPanel(0) %s\n", m_pActivePanel->GetName());
						m_pActivePanel->ShowPanel( false );
					}
#else
					Msg("CBaseViewport::ShowPanel(0) %s\n", m_pActivePanel->GetName());
					m_pActivePanel->ShowPanel( false );
#endif
				}
				else
				{
#if !defined( CSTRIKE15 )
					m_pLastActivePanel = pPanel;
#endif
					return;
				}
			}

			m_pActivePanel = pPanel;
		}
	}
	else
	{
		// if this is our current active panel
		// update m_pActivePanel pointer
		if ( m_pActivePanel == pPanel )
		{
			m_pActivePanel = NULL;
		}

#if !defined( CSTRIKE15 )
		// restore the previous active panel if it exists
		if( m_pLastActivePanel )
		{
			m_pActivePanel = m_pLastActivePanel;
			m_pLastActivePanel = NULL;

			Msg("CBaseViewport::ShowPanel(1) %s\n", m_pActivePanel->GetName());
			m_pActivePanel->ShowPanel( true );
		}
#endif
	}

	// just show/hide panel
	Msg("CBaseViewport::ShowPanel(%d) %s\n", (int)state, pPanel->GetName());
	pPanel->ShowPanel( state );

	UpdateAllPanels(); // let other panels rearrange
}

IViewPortPanel* CBaseViewport::GetActivePanel( void )
{
	return m_pActivePanel;
}

void CBaseViewport::RecreatePanel( const char *szPanelName )
{
	IViewPortPanel *panel = FindPanelByName( szPanelName );
	if ( panel )
	{
		m_Panels.Remove( szPanelName );
		for ( int i = m_UnorderedPanels.Count() - 1; i >= 0; --i )
		{
			if ( m_UnorderedPanels[ i ] == panel )
			{
				m_UnorderedPanels.Remove( i );
				break;
			}
		}

		vgui::VPANEL vPanel = panel->GetVPanel();
		if ( vPanel )
		{
			vgui::ipanel()->DeletePanel( vPanel );
		}
		else
		{
			delete panel;
		}

		if ( m_pActivePanel == panel )
		{
			m_pActivePanel = NULL;
		}

#if !defined( CSTRIKE15 )
		if ( m_pLastActivePanel == panel )
		{
			m_pLastActivePanel = NULL;
		}
#endif

		AddNewPanel( CreatePanelByName( szPanelName ), szPanelName );
	}
}


void CBaseViewport::RemoveAllPanels( void)
{
	g_lastPanel = NULL;
	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		vgui::VPANEL vPanel = p->GetVPanel();
		if ( vPanel )
		{
			vgui::ipanel()->DeletePanel( vPanel );
		}
		else
		{
			delete p;
		}

	}

	if ( m_pBackGround )
	{
		m_pBackGround->MarkForDeletion();
		m_pBackGround = NULL;
	}

	m_Panels.RemoveAll();
	m_UnorderedPanels.RemoveAll();
	m_pActivePanel = NULL;
#if !defined( CSTRIKE15 )
	m_pLastActivePanel = NULL;
#endif

}

CBaseViewport::~CBaseViewport()
{
	m_bInitialized = false;

	if ( !m_bHasParent && m_pBackGround )
	{
		m_pBackGround->MarkForDeletion();
	}
	m_pBackGround = NULL;

	RemoveAllPanels();
}

void CBaseViewport::InitViewportSingletons( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	s_pViewportInterfaces[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = this;
}

//-----------------------------------------------------------------------------
// Purpose: called when the VGUI subsystem starts up
//			Creates the sub panels and initialises them
//-----------------------------------------------------------------------------
void CBaseViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start entered" );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before singleton init" );
	InitViewportSingletons();
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start singleton ready" );

	m_GameuiFuncs = pGameUIFuncs;
	m_GameEventManager = pGameEventManager;
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start dependencies assigned" );

	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before background allocation" );
	m_pBackGround = new CBackGroundPanel( NULL );
	PS4_VIEWPORT_BREADCRUMB( m_pBackGround
		? "kisak-ps4: base viewport start background allocation ready"
		: "kisak-ps4: base viewport start background allocation missing" );
	m_pBackGround->SetZPos( -20 ); // send it to the back 
	m_pBackGround->SetVisible( false );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start background configured" );

	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before event listen" );
	ListenForGameEvent( "game_newmap" );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start event listen ready" );

	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before scheme set" );
	SetScheme( "ClientScheme" );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start scheme set ready" );
	SetProportional( true );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start proportional ready" );

	if ( !IsFullscreenViewport() )
	{
		PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before default panels" );
		CreateDefaultPanels();
		PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start default panels ready" );
	}

	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before animation controller allocation" );
	m_pAnimController = new vgui::AnimationController(this);
	PS4_VIEWPORT_BREADCRUMB( m_pAnimController
		? "kisak-ps4: base viewport start animation controller ready"
		: "kisak-ps4: base viewport start animation controller missing" );
	// create our animation controller
	m_pAnimController->SetScheme( GetScheme() );
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start animation scheme ready" );
	m_pAnimController->SetProportional(true);
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start animation proportional ready" );

	// Attempt to load all hud animations
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before hud animation load" );
	if ( LoadHudAnimations() == false )
	{
		PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start hud animation manifest failed" );
		// Fall back to just the main
		PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start before fallback animation load" );
		if ( m_pAnimController->SetScriptFile( GetVPanel(), "scripts/HudAnimations.txt", true ) == false )
		{
			Assert(0);
		}
		PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start fallback animation load returned" );
	}
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start hud animation load ready" );

	m_bInitialized = true;
	PS4_VIEWPORT_BREADCRUMB( "kisak-ps4: base viewport start complete" );
}

/*

//-----------------------------------------------------------------------------
// Purpose: Updates the spectator panel with new player info
//-----------------------------------------------------------------------------
void CBaseViewport::UpdateSpectatorPanel()
{
char bottomText[128];
int player = -1;
const char *name;
Q_snprintf(bottomText,sizeof( bottomText ), "#Spec_Mode%d", m_pClientDllInterface->SpectatorMode() );

m_pClientDllInterface->CheckSettings();
// check if we're locked onto a target, show the player's name
if ( (m_pClientDllInterface->SpectatorTarget() > 0) && (m_pClientDllInterface->SpectatorTarget() <= m_pClientDllInterface->GetMaxPlayers()) && (m_pClientDllInterface->SpectatorMode() != OBS_ROAMING) )
{
player = m_pClientDllInterface->SpectatorTarget();
}

// special case in free map and inset off, don't show names
if ( ((m_pClientDllInterface->SpectatorMode() == OBS_MAP_FREE) && !m_pClientDllInterface->PipInsetOff()) || player == -1 )
name = NULL;
else
name = m_pClientDllInterface->GetPlayerInfo(player).name;

// create player & health string
if ( player && name )
{
Q_strncpy( bottomText, name, sizeof( bottomText ) );
}
char szMapName[64];
Q_FileBase( const_cast<char *>(m_pClientDllInterface->GetLevelName()), szMapName );

m_pSpectatorGUI->Update(bottomText, player, m_pClientDllInterface->SpectatorMode(), m_pClientDllInterface->IsSpectateOnly(), m_pClientDllInterface->SpectatorNumber(), szMapName );
m_pSpectatorGUI->UpdateSpectatorPlayerList();
}  */

// Return TRUE if the HUD's allowed to print text messages
bool CBaseViewport::AllowedToPrintText( void )
{

	/* int iId = GetCurrentMenuID();
	if ( iId == MENU_TEAM || iId == MENU_CLASS || iId == MENU_INTRO || iId == MENU_CLASSHELP )
	return false; */
	// TODO ask every aktive elemet if it allows to draw text while visible

	return ( m_pActivePanel == NULL);
} 

void CBaseViewport::OnThink()
{
	ABS_QUERY_GUARD( true );

	// Clear our active panel pointer if the panel has made
	// itself invisible. Need this so we don't bring up dead panels
	// if they are stored as the last active panel
	if( m_pActivePanel && !m_pActivePanel->IsVisible() )
	{

#if !defined( CSTRIKE15 )
		if( m_pLastActivePanel )
		{
			if ( m_pLastActivePanel->CanBeReopened() )
			{
				m_pActivePanel = m_pLastActivePanel;
				ShowPanel( m_pActivePanel, true );
			}
			else
			{
				m_pActivePanel = NULL;
			}
			m_pLastActivePanel = NULL;
		}
		else
#endif
			m_pActivePanel = NULL;
	}

	m_pAnimController->UpdateAnimations( gpGlobals->curtime );

	// check the auto-reload cvar
	m_pAnimController->SetAutoReloadScript(hud_autoreloadscript.GetBool());

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( vgui::ipanel()->GetMessageContextId( GetVPanel() ) );

	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		if ( p )
		{
			p->UpdateVisibility();
			if ( p->IsVisible() )
			{
				if ( p->NeedsUpdate() )
				{
					p->Update();
				}
				p->ViewportThink();
			}
		}
	}

	int w, h;
	vgui::ipanel()->GetSize( VGui_GetClientDLLRootPanel(), w, h );

	if ( m_OldSize[ 0 ] != w || m_OldSize[ 1 ] != h )
	{
		m_OldSize[ 0 ] = w;
		m_OldSize[ 1 ] = h;

		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		GetClientMode()->Layout();
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the parent for each panel to use
//-----------------------------------------------------------------------------
void CBaseViewport::SetParent(vgui::VPANEL parent)
{
	EditablePanel::SetParent( parent );
	// force ourselves to be proportional - when we set our parent above, if our new
	// parent happened to be non-proportional (such as the vgui root panel), we got
	// slammed to be nonproportional
	EditablePanel::SetProportional( true );

	m_pBackGround->SetParent( (vgui::VPANEL)parent );

	// set proportionality on animation controller
	m_pAnimController->SetProportional( true );

	m_bHasParent = (parent != 0);
}

//-----------------------------------------------------------------------------
// Purpose: called when the engine shows the base client VGUI panel (i.e when entering a new level or exiting GameUI )
//-----------------------------------------------------------------------------
void CBaseViewport::ActivateClientUI() 
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the engine hides the base client VGUI panel (i.e when the GameUI is comming up ) 
//-----------------------------------------------------------------------------
void CBaseViewport::HideClientUI()
{
}

//-----------------------------------------------------------------------------
// Purpose: passes death msgs to the scoreboard to display specially
//-----------------------------------------------------------------------------
void CBaseViewport::FireGameEvent( IGameEvent * event)
{
	const char * type = event->GetName();

	if ( Q_strcmp(type, "game_newmap") == 0 )
	{
		// hide all panels when reconnecting 
		ShowPanel( PANEL_ALL, false );

		if ( g_bEngineIsHLTV
#if defined( REPLAY_ENABLED )
			|| engine->IsReplay()
#endif
			)
		{
			ShowPanel( PANEL_SPECGUI, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewport::ReloadScheme(const char *fromFile)
{
	// See if scheme should change

	if ( fromFile != NULL )
	{
		// "resource/ClientScheme.res"

		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( GetSchemeSizingVPanel(), fromFile, "HudScheme" );

		SetScheme(scheme);
		SetProportional( true );
		m_pAnimController->SetScheme(scheme);
	}

	// Force a reload
	if ( LoadHudAnimations() == false )
	{
		// Fall back to just the main
		if ( m_pAnimController->SetScriptFile( GetVPanel(), "scripts/HudAnimations.txt", true ) == false )
		{
			Assert(0);
		}
	}

	SetProportional( true );

	LoadHudLayout();

	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		HudIcons().RefreshHudTextures();
	}

	InvalidateLayout( true, true );

	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		p->ReloadScheme();
	}

	// reset the hud
	GetHud().ResetHUD();
}

extern ConVar ss_verticalsplit;

void AddSubKeyNamed( KeyValues *pKeys, const char *pszName )
{
	KeyValues *pNewKey = new KeyValues( pszName );
	if ( pNewKey )
	{
		pKeys->AddSubKey( pNewKey );
	}	
}

void CBaseViewport::LoadHudLayout( void )
{
	VGUI_ABSPOS_SPLITSCREEN_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );

	// reload the .res file from disk
	KeyValues *pConditions = NULL;
	
	if ( engine->IsSplitScreenActive() )
	{
		pConditions = new KeyValues( "conditions" );
		if ( pConditions )
		{
			AddSubKeyNamed( pConditions, "if_split_screen_active" );

			ConVarRef ss_verticalsplit( "ss_verticalsplit" );
			
			if ( ss_verticalsplit.IsValid() && ss_verticalsplit.GetBool() )
			{
				AddSubKeyNamed( pConditions, "if_split_screen_vertical" );

				if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
				{
					AddSubKeyNamed( pConditions, "if_split_screen_left" );
				}
				else
				{
					AddSubKeyNamed( pConditions, "if_split_screen_right" );
				}
			}
			else
			{
				AddSubKeyNamed( pConditions, "if_split_screen_horizontal" );

				if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
				{
					AddSubKeyNamed( pConditions, "if_split_screen_top" );
				}
				else
				{
					AddSubKeyNamed( pConditions, "if_split_screen_bottom" );
				}
			}
		}	
	}

	LoadControlSettings( "scripts/HudLayout.res", NULL, NULL, pConditions );

	if ( pConditions )
	{
		pConditions->deleteThis();
	}
}

int CBaseViewport::GetDeathMessageStartHeight( void )
{
	return YRES(2);
}

void CBaseViewport::Paint()
{
	if ( cl_leveloverviewmarker.GetInt() > 0 )
	{
		int size = cl_leveloverviewmarker.GetInt();
		// draw a 1024x1024 pixel box
		vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
		vgui::surface()->DrawLine( size, 0, size, size );
		vgui::surface()->DrawLine( 0, size, size, size );
	}
}

void CBaseViewport::SetAsFullscreenViewportInterface( void )
{
	s_pFullscreenViewportInterface = this;
	m_bFullscreenViewport = true;
}

bool CBaseViewport::IsFullscreenViewport() const
{
	return m_bFullscreenViewport;
}

void CBaseViewport::LevelInit( void )
{
	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		if ( p )
		{
			p->LevelInit();
		}
	}
}
