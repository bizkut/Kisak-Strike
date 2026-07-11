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

int main()
{
    ClearStaticModuleRegistryForTesting();
    assert( KisakRegisterStaticModules() == 0 );
    assert( FindStaticModuleFactory( "tier0_client.so" ) == KisakTier0Factory() );
    assert( FindStaticModuleFactory( "vstdlib.dll" ) == KisakVstdlibFactory() );
    assert( FindStaticModuleFactory( "bin/launcher" ) == KisakLauncherFactory() );

    int result = 1;
    assert( FindStaticModuleFactory( "launcher" )( "TestInterface001", &result ) == &g_QueryCount );
    assert( result == 0 );
    assert( g_QueryCount == 1 );
    assert( KisakRegisterStaticModules() == 0 );
    return 0;
}
