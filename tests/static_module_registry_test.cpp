#include "appframework/StaticModuleRegistry.h"

#include <assert.h>
#include <stdio.h>

static void *FactoryA( const char *, int * ) { return NULL; }
static void *FactoryB( const char *, int * ) { return NULL; }

int main()
{
    ClearStaticModuleRegistryForTesting();
    assert( !RegisterStaticModule( NULL, FactoryA ) );
    assert( !RegisterStaticModule( "launcher", NULL ) );
    assert( RegisterStaticModule( "launcher", FactoryA ) );
    assert( RegisterStaticModule( "LAUNCHER.DLL", FactoryA ) );
    assert( !RegisterStaticModule( "launcher", FactoryB ) );
    assert( FindStaticModuleFactory( "bin/LAUNCHER_client.so" ) == FactoryA );
    assert( FindStaticModuleFactory( "missing" ) == NULL );
    puts( "static module registry tests passed" );
    return 0;
}
