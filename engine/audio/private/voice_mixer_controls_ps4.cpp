#include "audio_pch.h"
#include "voice_mixer_controls.h"

// Voice capture and mixer controls remain unavailable until the PS4 AudioOut
// playback path is stable. Source's existing null checks disable microphone
// controls deterministically through this small console implementation.
IMixerControls *g_pMixerControls = NULL;

void InitMixerControls()
{
}

void ShutdownMixerControls()
{
}
