#include "game/client/cstrike15/gameui/sys_utils.h"

// The console UI has no desktop windows, registry messages, or named Win32
// mutexes. Keep the legacy embedded GameUI ABI inert for monolithic linking.
const unsigned int SYS_NO_ERROR = 0;
const unsigned int SYS_ERROR_INVALID_HANDLE = 6;
const unsigned int SYS_WAIT_OBJECT_0 = 0;
const unsigned int SYS_WAIT_ABANDONED = 0x80;

void Sys_SetLastError( unsigned long ) {}
unsigned long Sys_GetLastError() { return SYS_NO_ERROR; }
WHANDLE Sys_CreateMutex( const char * ) { return 0; }
void Sys_ReleaseMutex( WHANDLE ) {}
unsigned int Sys_WaitForSingleObject( WHANDLE, int ) { return SYS_WAIT_OBJECT_0; }
unsigned int Sys_RegisterWindowMessage( const char * ) { return 0; }
WHANDLE Sys_FindWindow( const char *, const char * ) { return 0; }
void Sys_EnumWindows( void *, int ) {}
void Sys_GetWindowText( WHANDLE, char *buffer, int bufferSize )
{
    if ( buffer && bufferSize > 0 )
        buffer[0] = '\0';
}
void Sys_PostMessage( WHANDLE, unsigned int, unsigned int, unsigned int ) {}
WHANDLE Sys_CreateWindowEx( const char * ) { return 0; }
void Sys_DestroyWindow( WHANDLE ) {}
void Sys_SetCursorPos( int, int ) {}
