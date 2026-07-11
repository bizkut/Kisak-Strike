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
    const bool engine = RegisterStaticModule( "engine", KisakEngineBootstrapFactory() );
    const bool rocketui = RegisterStaticModule( "rocketui", KisakRocketUIBootstrapFactory() );
    const bool inputsystem = RegisterStaticModule( "inputsystem", KisakInputSystemFactory() );
    const bool vphysics = RegisterStaticModule( "kisakvphysics", KisakVPhysicsFactory() );
    const bool shaderapiempty = RegisterStaticModule( "shaderapiempty", KisakShaderApiEmptyFactory() );
    const bool shaderapips4 = RegisterStaticModule( "shaderapips4", KisakShaderApiPs4Factory() );
    const bool materialsystem = RegisterStaticModule( "materialsystem", KisakMaterialSystemFactory() );
    const bool datacache = RegisterStaticModule( "datacache", KisakDataCacheFactory() );
    const bool studiorender = RegisterStaticModule( "studiorender", KisakStudioRenderFactory() );
    const bool soundemittersystem = RegisterStaticModule( "soundemittersystem", KisakSoundEmitterSystemFactory() );
    const bool vscript = RegisterStaticModule( "vscript", KisakVScriptFactory() );
    const bool vgui2 = RegisterStaticModule( "vgui2", KisakVGuiFactory() );
    const bool localize = RegisterStaticModule( "localize", KisakLocalizeFactory() );
    const bool vguimatsurface = RegisterStaticModule( "vguimatsurface", KisakVGuiMatSurfaceFactory() );
    return tier0 && vstdlib && launcher && filesystem && engine && rocketui && inputsystem && vphysics && shaderapiempty && shaderapips4 && materialsystem && datacache && studiorender && soundemittersystem && vscript && vgui2 && localize && vguimatsurface ? 0 : -1;
}
