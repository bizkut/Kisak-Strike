#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

extern int sceKernelUsleep( unsigned int microseconds );

#if defined( KISAK_PS4_MONOLITHIC )
extern int KisakRegisterStaticModules( void );
extern int LauncherMain( int argc, char **argv );
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

int main( int argc, char **argv )
{
    FILE *log = OpenStartupLog();
    LogLine( log, "kisak-ps4: bootstrap entered" );

#if defined( KISAK_PS4_MONOLITHIC )
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
    return LauncherMain( argc, argv );
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
