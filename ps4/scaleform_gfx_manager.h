#ifndef KISAK_PS4_SCALEFORM_GFX_MANAGER_H
#define KISAK_PS4_SCALEFORM_GFX_MANAGER_H

#include "inputsystem/InputEnums.h"

bool KisakPs4ScaleformUiInitialize();
void KisakPs4ScaleformUiShutdown();
void KisakPs4ScaleformUiAdvance( float time );
bool KisakPs4ScaleformUiRenderMenu();
bool KisakPs4ScaleformUiRenderHud();
bool KisakPs4ScaleformUiHandleInput( const InputEvent_t &event );
bool KisakPs4ScaleformUiMovieReady( int slot );
bool KisakPs4ScaleformUiRequestElement( int slot, const char *elementName );

#endif
