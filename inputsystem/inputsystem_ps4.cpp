//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: DualShock 4 input backend for the monolithic PS4 client.
//=============================================================================//

#include "inputsystem.h"

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include <algorithm>
#include <stdio.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *message );

namespace
{
int g_ps4PadHandle = -1;
uint32_t g_ps4PadButtons = 0;
bool g_ps4PadConnected = false;

struct Ps4ButtonMapping
{
	uint32_t mask;
	ButtonCode_t code;
};

const Ps4ButtonMapping kPs4ButtonMappings[] = {
	{ ORBIS_PAD_BUTTON_CROSS, KEY_XBUTTON_A },
	{ ORBIS_PAD_BUTTON_CIRCLE, KEY_XBUTTON_B },
	{ ORBIS_PAD_BUTTON_SQUARE, KEY_XBUTTON_X },
	{ ORBIS_PAD_BUTTON_TRIANGLE, KEY_XBUTTON_Y },
	{ ORBIS_PAD_BUTTON_L1, KEY_XBUTTON_LEFT_SHOULDER },
	{ ORBIS_PAD_BUTTON_R1, KEY_XBUTTON_RIGHT_SHOULDER },
	{ ORBIS_PAD_BUTTON_L2, KEY_XBUTTON_LTRIGGER },
	{ ORBIS_PAD_BUTTON_R2, KEY_XBUTTON_RTRIGGER },
	{ ORBIS_PAD_BUTTON_L3, KEY_XBUTTON_STICK1 },
	{ ORBIS_PAD_BUTTON_R3, KEY_XBUTTON_STICK2 },
	{ ORBIS_PAD_BUTTON_OPTIONS, KEY_XBUTTON_START },
	{ ORBIS_PAD_BUTTON_TOUCH_PAD, KEY_XBUTTON_BACK },
	{ ORBIS_PAD_BUTTON_UP, KEY_XBUTTON_UP },
	{ ORBIS_PAD_BUTTON_RIGHT, KEY_XBUTTON_RIGHT },
	{ ORBIS_PAD_BUTTON_DOWN, KEY_XBUTTON_DOWN },
	{ ORBIS_PAD_BUTTON_LEFT, KEY_XBUTTON_LEFT },
};

int Ps4StickSample( uint8_t sample )
{
	if ( sample >= 116 && sample <= 140 )
		return 0;
	return std::max( -MAX_BUTTONSAMPLE,
		std::min( MAX_BUTTONSAMPLE - 1,
			( static_cast< int >( sample ) - 128 ) * 256 ) );
}
}

void CInputSystem::PressX360Button( const CCommand &args )
{
	(void)args;
}

void CInputSystem::PollPressX360Button( void )
{
}

void CInputSystem::InitializeJoysticks( void )
{
	m_nJoystickCount = 0;
	g_ps4PadHandle = -1;
	g_ps4PadButtons = 0;
	g_ps4PadConnected = false;

	const int userServiceResult = sceUserServiceInitialize( NULL );
	int userId = -1;
	const int userResult = sceUserServiceGetInitialUser( &userId );
	const int padInitResult = scePadInit();
	if ( padInitResult == 0 || padInitResult == 1 )
	{
		if ( userResult != 0 || userId < 0 )
			userId = 0;
		g_ps4PadHandle = scePadOpen( userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL );
		if ( g_ps4PadHandle < 0 && userId != 0xff )
			g_ps4PadHandle = scePadOpen( 0xff,
				ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL );
	}

	if ( g_ps4PadHandle >= 0 )
	{
		m_nJoystickCount = 1;
		JoystickInfo_t &info = m_pJoystickInfo[0];
		info.m_pDevice = reinterpret_cast< void * >(
			static_cast< intptr_t >( g_ps4PadHandle ) );
		info.m_fCurrentRumble = 0.0f;
		info.m_bRumbleEnabled = true;
		info.m_nButtonCount = sizeof( kPs4ButtonMappings ) /
			sizeof( kPs4ButtonMappings[0] );
		info.m_nAxisFlags = ( 1 << JOY_AXIS_X ) | ( 1 << JOY_AXIS_Y ) |
			( 1 << JOY_AXIS_U ) | ( 1 << JOY_AXIS_R );
		info.m_nDeviceId = 0;
		info.m_bHasPOVControl = true;
	}

	char message[224];
	snprintf( message, sizeof( message ),
		"kisak-ps4: pad init usersvc=%d user_result=%d user=%d pad_init=%d handle=%d count=%d",
		userServiceResult, userResult, userId, padInitResult,
		g_ps4PadHandle, m_nJoystickCount );
	KisakPs4StartupBreadcrumb( message );
}

void CInputSystem::PollJoystick( void )
{
	if ( g_ps4PadHandle < 0 )
		return;

	OrbisPadData data = {};
	const int readResult = scePadRead( g_ps4PadHandle, &data, 1 );
	if ( readResult <= 0 )
		return;

	const bool connected = data.connected != 0;
	const uint32_t buttons = connected ? data.buttons : 0;
	const uint32_t changed = buttons ^ g_ps4PadButtons;
	for ( unsigned int i = 0;
		i < sizeof( kPs4ButtonMappings ) / sizeof( kPs4ButtonMappings[0] ); ++i )
	{
		const Ps4ButtonMapping &mapping = kPs4ButtonMappings[i];
		if ( ( changed & mapping.mask ) == 0 )
			continue;
		if ( buttons & mapping.mask )
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick,
				mapping.code, mapping.code );
		else
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick,
				mapping.code, mapping.code );
	}

	InputState_t &state = m_InputState[INPUT_STATE_CURRENT];
	const int samples[4] = {
		connected ? Ps4StickSample( data.leftStick.x ) : 0,
		connected ? Ps4StickSample( data.leftStick.y ) : 0,
		connected ? Ps4StickSample( data.rightStick.x ) : 0,
		connected ? Ps4StickSample( data.rightStick.y ) : 0,
	};
	const JoystickAxis_t axes[4] = {
		JOY_AXIS_X, JOY_AXIS_Y, JOY_AXIS_U, JOY_AXIS_R
	};
	for ( unsigned int i = 0; i < 4; ++i )
	{
		const AnalogCode_t code = JOYSTICK_AXIS( 0, axes[i] );
		const int previous = state.m_pAnalogValue[code];
		if ( samples[i] == previous )
			continue;
		state.m_pAnalogValue[code] = samples[i];
		state.m_pAnalogDelta[code] = samples[i] - previous;
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code,
			state.m_pAnalogValue[code], state.m_pAnalogDelta[code] );
	}

	static unsigned int loggedTransitions = 0;
	if ( connected != g_ps4PadConnected || changed != 0 )
	{
		if ( loggedTransitions < 24 )
		{
			char message[192];
			snprintf( message, sizeof( message ),
				"kisak-ps4: pad sample connected=%u buttons=0x%08x changed=0x%08x left=%u,%u right=%u,%u",
				connected ? 1u : 0u, buttons, changed,
				data.leftStick.x, data.leftStick.y,
				data.rightStick.x, data.rightStick.y );
			KisakPs4StartupBreadcrumb( message );
			++loggedTransitions;
		}
	}
	g_ps4PadButtons = buttons;
	g_ps4PadConnected = connected;
}

void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	(void)userId;
	if ( g_ps4PadHandle < 0 )
		return;
	OrbisPadVibeParam vibration = {};
	vibration.lgMotor = static_cast< uint8_t >(
		std::max( 0.0f, std::min( 1.0f, fLeftMotor ) ) * 255.0f );
	vibration.smMotor = static_cast< uint8_t >(
		std::max( 0.0f, std::min( 1.0f, fRightMotor ) ) * 255.0f );
	scePadSetVibration( g_ps4PadHandle, &vibration );
}

bool CInputSystem::InitializeSteamControllers( void )
{
	return false;
}

bool CInputSystem::PollSteamControllers( void )
{
	return false;
}

bool CInputSystem::IsSteamControllerActive() const
{
	return false;
}

void CInputSystem::SetSteamControllerMode( const char *pSteamControllerMode, const void *obj )
{
	(void)pSteamControllerMode;
	(void)obj;
}
