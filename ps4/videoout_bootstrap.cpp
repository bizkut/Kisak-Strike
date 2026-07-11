#include <gnm_helpers.h>

#include <stdbool.h>
#include <string.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

namespace
{
GnmVideoOut g_VideoOut = {};
bool g_VideoOutReady = false;
}

extern "C" bool KisakPs4VideoOutInitialize()
{
    if ( g_VideoOutReady )
        return true;

    GnmVideoOutCreateInfo info;
    sceGnmVideoOutInitDefaultCreateInfo( &info, 1920, 1080 );
    info.numbuffers = 2;
    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout before open" );
    if ( sceGnmVideoOutOpen( &g_VideoOut, &info ) != GNM_ERROR_OK )
    {
		switch ( g_VideoOut.last_error_stage )
		{
		case 2: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout sce open failed" ); break;
		case 3: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout direct memory failed" ); break;
		case 4: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout register buffers failed" ); break;
		case 5: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout equeue failed" ); break;
		case 6: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout flip event failed" ); break;
		default: KisakPs4StartupBreadcrumb( "kisak-ps4: videoout layout failed" ); break;
		}
        return false;
    }

    g_VideoOutReady = true;
    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout opened" );
    return true;
}

extern "C" bool KisakPs4VideoOutSubmitClear()
{
    if ( !g_VideoOutReady )
        return false;

    void *buffer = sceGnmVideoOutGetBuffer( &g_VideoOut, g_VideoOut.currentbuffer );
    if ( !buffer )
        return false;

    memset( buffer, 0, (size_t)g_VideoOut.buffersize );
    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout before flip" );
    if ( sceGnmVideoOutSubmitFlipAndWait( &g_VideoOut, g_VideoOut.currentbuffer, 0, GNM_VIDEO_OUT_FLIP_VSYNC ) != GNM_ERROR_OK )
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: videoout flip failed" );
        return false;
    }

    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout flip complete" );
    return true;
}

extern "C" void KisakPs4VideoOutShutdown()
{
    if ( !g_VideoOutReady )
        return;

    sceGnmVideoOutClose( &g_VideoOut );
    g_VideoOutReady = false;
    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout closed" );
}
