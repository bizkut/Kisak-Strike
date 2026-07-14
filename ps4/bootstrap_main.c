#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

extern int sceKernelUsleep( unsigned int microseconds );

#if defined( KISAK_PS4_MONOLITHIC )
extern int KisakRegisterStaticModules( void );
extern int LauncherMain( int argc, char **argv );
extern int KisakInitializeGlobalThreadPool( void );
typedef void (*KisakConstructor)( void );
extern KisakConstructor __kisak_priority_ctors_start[];
extern KisakConstructor __kisak_priority_ctors_end[];
extern KisakConstructor __kisak_ctors_start[];
extern KisakConstructor __kisak_ctors_end[];
#endif

static FILE *OpenStartupLog( void )
{
    (void)mkdir( "/data/kisak-strike", 0777 );
    return fopen( "/data/kisak-strike/startup.log", "a" );
}

static void LogLine( FILE *log, const char *line )
{
    if ( !log )
        return;
    fputs( line, log );
    fputc( '\n', log );
    fflush( log );
}

#if defined( KISAK_PS4_MONOLITHIC )
int g_KisakPs4TraceThreadPool;

void KisakPs4StartupBreadcrumb( const char *line )
{
    FILE *log = OpenStartupLog();
    LogLine( log, line );
    if ( log )
        fclose( log );
}
#endif

int main( int argc, char **argv )
{
    FILE *log = OpenStartupLog();
    LogLine( log, "kisak-ps4: bootstrap entered" );
    LogLine( log, "kisak-ps4: build marker filesystem_absolute_game_v429" );

#if defined( KISAK_PS4_MONOLITHIC )
    KisakConstructor *priorityConstructor = __kisak_priority_ctors_start;
    unsigned int priorityConstructorIndex = 0;
    while ( priorityConstructor != __kisak_priority_ctors_end )
    {
        char marker[64];
        snprintf( marker, sizeof( marker ), "kisak-ps4: before priority ctor %u", priorityConstructorIndex );
        LogLine( log, marker );
        (*priorityConstructor)();
        snprintf( marker, sizeof( marker ), "kisak-ps4: after priority ctor %u", priorityConstructorIndex );
        LogLine( log, marker );
        ++priorityConstructor;
        ++priorityConstructorIndex;
    }
    LogLine( log, "kisak-ps4: priority constructors complete" );

    KisakConstructor *constructor = __kisak_ctors_end;
    unsigned int constructorIndex = (unsigned int)(__kisak_ctors_end - __kisak_ctors_start);
    while ( constructor != __kisak_ctors_start )
    {
        char marker[64];
        --constructor;
        --constructorIndex;
        snprintf( marker, sizeof( marker ), "kisak-ps4: before ctor %u", constructorIndex );
        LogLine( log, marker );
        (*constructor)();
        snprintf( marker, sizeof( marker ), "kisak-ps4: after ctor %u", constructorIndex );
        LogLine( log, marker );
    }
    LogLine( log, "kisak-ps4: constructors complete" );

    LogLine( log, "kisak-ps4: before global thread pool" );
    if ( KisakInitializeGlobalThreadPool() != 0 )
    {
        LogLine( log, "kisak-ps4: global thread pool failed" );
        return 1;
    }
    LogLine( log, "kisak-ps4: after global thread pool" );

    if ( KisakRegisterStaticModules() != 0 )
    {
        LogLine( log, "kisak-ps4: static module registration failed" );
        if ( log )
            fclose( log );
        return 1;
    }
    LogLine( log, "kisak-ps4: static modules registered" );
    LogLine( log, "kisak-ps4: entering LauncherMain" );
    if ( log )
        fclose( log );
    {
        int launcherResult = LauncherMain( argc, argv );
        log = OpenStartupLog();
        LogLine( log, "kisak-ps4: LauncherMain returned" );
        if ( log )
            fclose( log );
        (void)launcherResult;
        for ( ;; )
            sceKernelUsleep( 1000000 );
    }
#else
    LogLine( log, "kisak-ps4: bootstrap-only build" );
    LogLine( log, "kisak-ps4: launcher not linked" );
    LogLine( log, "kisak-ps4: bootstrap idle" );
    if ( log )
        fclose( log );
    for ( ;; )
        sceKernelUsleep( 1000000 );
#endif
}
