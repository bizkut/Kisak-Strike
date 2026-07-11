//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Minimal PS4 input backend used during monolithic engine bring-up.
//
// The input-system interface must be present before the engine can initialize.
// Actual DualShock 4 enumeration, sampling, and rumble will be implemented on
// this boundary with libScePad; until then, expose deterministic no-device
// behavior and keep proprietary Steam controller support out of the PS4 build.
//=============================================================================//

#include "inputsystem.h"

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
}

void CInputSystem::PollJoystick( void )
{
}

void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	(void)fLeftMotor;
	(void)fRightMotor;
	(void)userId;
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
