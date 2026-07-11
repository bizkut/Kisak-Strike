#include "appframework/Ps4StaticModules.h"

#include <assert.h>
#include <string.h>

static int g_QueryCount;

static void *TestFactory( const char *pName, int *pReturnCode )
{
    ++g_QueryCount;
    if ( pReturnCode )
        *pReturnCode = strcmp( pName, "TestInterface001" ) == 0 ? 0 : 1;
    return strcmp( pName, "TestInterface001" ) == 0 ? &g_QueryCount : 0;
}

CreateInterfaceFn Sys_GetFactoryThis()
{
    return TestFactory;
}

CreateInterfaceFn KisakFilesystemFactory() { return TestFactory; }
CreateInterfaceFn KisakEngineBootstrapFactory() { return TestFactory; }
CreateInterfaceFn KisakRocketUIBootstrapFactory() { return TestFactory; }
CreateInterfaceFn KisakInputSystemFactory() { return TestFactory; }
CreateInterfaceFn KisakVPhysicsFactory() { return TestFactory; }
CreateInterfaceFn KisakShaderApiEmptyFactory() { return TestFactory; }
CreateInterfaceFn KisakShaderApiPs4Factory() { return TestFactory; }
CreateInterfaceFn KisakMaterialSystemFactory() { return TestFactory; }
CreateInterfaceFn KisakDataCacheFactory() { return TestFactory; }
CreateInterfaceFn KisakStudioRenderFactory() { return TestFactory; }
CreateInterfaceFn KisakSoundEmitterSystemFactory() { return TestFactory; }
CreateInterfaceFn KisakVScriptFactory() { return TestFactory; }
CreateInterfaceFn KisakVGuiFactory() { return TestFactory; }
CreateInterfaceFn KisakLocalizeFactory() { return TestFactory; }
CreateInterfaceFn KisakVGuiMatSurfaceFactory() { return TestFactory; }

int main()
{
    ClearStaticModuleRegistryForTesting();
    assert( KisakRegisterStaticModules() == 0 );
    assert( FindStaticModuleFactory( "tier0_client.so" ) == KisakTier0Factory() );
    assert( FindStaticModuleFactory( "vstdlib.dll" ) == KisakVstdlibFactory() );
    assert( FindStaticModuleFactory( "bin/launcher" ) == KisakLauncherFactory() );
    assert( FindStaticModuleFactory( "shaderapips4" ) == KisakShaderApiPs4Factory() );

    int result = 1;
    assert( FindStaticModuleFactory( "launcher" )( "TestInterface001", &result ) == &g_QueryCount );
    assert( result == 0 );
    assert( g_QueryCount == 1 );
    assert( KisakRegisterStaticModules() == 0 );
    return 0;
}
