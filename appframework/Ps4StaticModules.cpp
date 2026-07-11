#include "appframework/Ps4StaticModules.h"

extern CreateInterfaceFn Sys_GetFactoryThis();

namespace
{
void *Tier0CreateInterface( const char *pName, int *pReturnCode )
{
    return Sys_GetFactoryThis()( pName, pReturnCode );
}

void *VstdlibCreateInterface( const char *pName, int *pReturnCode )
{
    return Sys_GetFactoryThis()( pName, pReturnCode );
}

void *LauncherCreateInterface( const char *pName, int *pReturnCode )
{
    return Sys_GetFactoryThis()( pName, pReturnCode );
}
}

CreateInterfaceFn KisakTier0Factory()
{
    return Tier0CreateInterface;
}

CreateInterfaceFn KisakVstdlibFactory()
{
    return VstdlibCreateInterface;
}

CreateInterfaceFn KisakLauncherFactory()
{
    return LauncherCreateInterface;
}

extern "C" int KisakRegisterStaticModules()
{
    const bool tier0 = RegisterStaticModule( "tier0", KisakTier0Factory() );
    const bool vstdlib = RegisterStaticModule( "vstdlib", KisakVstdlibFactory() );
    const bool launcher = RegisterStaticModule( "launcher", KisakLauncherFactory() );
    const bool filesystem = RegisterStaticModule( "filesystem_stdio", KisakFilesystemFactory() );
    return tier0 && vstdlib && launcher && filesystem ? 0 : -1;
}
