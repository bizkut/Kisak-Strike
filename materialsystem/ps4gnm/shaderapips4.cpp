#include "interface.h"

extern CreateInterfaceFn KisakShaderApiEmptyFactory();

namespace
{
CreateInterfaceFn g_EmptyFactory = 0;

void *Ps4CreateInterface( const char *interfaceName, int *returnCode )
{
    if ( !g_EmptyFactory )
    {
        if ( returnCode )
            *returnCode = 1;
        return 0;
    }
    return g_EmptyFactory( interfaceName, returnCode );
}
}

CreateInterfaceFn KisakShaderApiPs4Factory()
{
    // Keep a distinct PS4 factory identity even while individual interface
    // versions still forward to shaderapiempty. Native PS4 device, ShaderAPI,
    // shadow, and hardware-config objects can now replace one lookup at a time
    // without changing the material-system module lifecycle.
    g_EmptyFactory = KisakShaderApiEmptyFactory();
    return Ps4CreateInterface;
}
