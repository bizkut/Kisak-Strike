#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

#if defined( KISAK_PS4_DEV_ATTACH_GATE )
#define KISAK_PS4_DEV_ATTACH_GATE_HOLD UINT64_C( 0x4b4953414b484f4c )
#define KISAK_PS4_DEV_ATTACH_GATE_RELEASE UINT64_C( 0x4b4953414b474f21 )
#define KISAK_PS4_DEV_ATTACH_GATE_TIMEOUT UINT64_C( 0x4b4953414b54494d )
#define KISAK_PS4_DEV_ATTACH_GATE_POLL_US 10000u
#define KISAK_PS4_DEV_ATTACH_GATE_TIMEOUT_POLLS 12000u

volatile uint64_t g_KisakPs4DevAttachGate
    __attribute__((used, visibility("default"), aligned(8))) =
        KISAK_PS4_DEV_ATTACH_GATE_HOLD;
const char g_KisakPs4DevAttachGateMarker[] __attribute__((used)) =
    "kisak-ps4: dev attach gate v1";

static int KisakPs4WaitForDevAttach( void )
{
    unsigned int remainingPolls = KISAK_PS4_DEV_ATTACH_GATE_TIMEOUT_POLLS;
    while ( g_KisakPs4DevAttachGate != KISAK_PS4_DEV_ATTACH_GATE_RELEASE &&
            remainingPolls != 0 )
    {
        sceKernelUsleep( KISAK_PS4_DEV_ATTACH_GATE_POLL_US );
        --remainingPolls;
    }
    if ( g_KisakPs4DevAttachGate == KISAK_PS4_DEV_ATTACH_GATE_RELEASE )
        return 1;
    g_KisakPs4DevAttachGate = KISAK_PS4_DEV_ATTACH_GATE_TIMEOUT;
    return 0;
}
#endif

static FILE *OpenStartupLog( void )
{
    (void)mkdir( "/data/kisak-strike", 0777 );
    return fopen( "/data/kisak-strike/startup.log", "a" );
}

static void ResetStartupLog( void )
{
    (void)mkdir( "/data/kisak-strike", 0777 );
    FILE *log = fopen( "/data/kisak-strike/startup.log", "w" );
    if ( log )
        fclose( log );
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
static volatile int g_KisakPs4ScoreboardTrace;

void KisakPs4SetScoreboardTrace( int enabled )
{
    g_KisakPs4ScoreboardTrace = enabled ? 1 : 0;
}

int KisakPs4IsScoreboardTrace( void )
{
    return g_KisakPs4ScoreboardTrace != 0;
}

static int KisakPs4SuppressClearedStartupDetail( const char *line )
{
    if ( !line )
        return 1;

#define KISAK_PS4_PREFIX_MATCH( prefix ) \
    ( strncmp( line, prefix, sizeof( prefix ) - 1 ) == 0 )

    return KISAK_PS4_PREFIX_MATCH( "kisak-ps4: particle parse " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: ConVar_Register pending " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: panel init " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: panel named constructor " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: panel set keyboard " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: vpanel " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: vgui panel created " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: vgui alloc panel " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: matsurface " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: scheme font helper " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: client scheme border " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: scalable border " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: material detail " ) ||
        KISAK_PS4_PREFIX_MATCH( "kisak-ps4: vgui texture " );

#undef KISAK_PS4_PREFIX_MATCH
}

void KisakPs4StartupBreadcrumb( const char *line )
{
    if ( KisakPs4SuppressClearedStartupDetail( line ) )
        return;

    FILE *log = OpenStartupLog();
    LogLine( log, line );
    if ( log )
        fclose( log );
}
#endif

int main( int argc, char **argv )
{
#if defined( KISAK_PS4_DEV_ATTACH_GATE )
    int devAttachReleased = KisakPs4WaitForDevAttach();
#endif
    ResetStartupLog();
    FILE *log = OpenStartupLog();
    LogLine( log, "kisak-ps4: bootstrap entered" );
    LogLine( log, "kisak-ps4: build marker server_shared_protobuf_v446" );
#if defined( KISAK_PS4_DEV_ATTACH_GATE )
    LogLine( log, devAttachReleased
        ? "kisak-ps4: dev attach gate released"
        : "kisak-ps4: dev attach gate timed out; continuing" );
#endif

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
