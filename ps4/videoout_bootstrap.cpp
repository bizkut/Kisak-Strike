#include <gnm_helpers.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );
extern "C" bool KisakPs4GnmFillAndWait( void *destination, uint32_t size, uint32_t value );

namespace
{
GnmVideoOut g_VideoOut = {};
bool g_VideoOutReady = false;
bool g_GpuClearLogged = false;
}

extern "C" bool KisakPs4VideoOutInitialize()
{
    if ( g_VideoOutReady )
        return true;

    GnmVideoOutCreateInfo info;
    sceGnmVideoOutInitDefaultCreateInfo( &info, 1920, 1080 );
    info.numbuffers = 2;
	const GnmError layoutResult = sceGnmVideoOutCalcBufferLayout( &info, NULL, NULL );
	if ( layoutResult != GNM_ERROR_OK )
	{
		if ( layoutResult == GNM_ERROR_INVALID_ARGS )
			KisakPs4StartupBreadcrumb( "kisak-ps4: videoout layout invalid args" );
		else if ( layoutResult == GNM_ERROR_OVERFLOW )
			KisakPs4StartupBreadcrumb( "kisak-ps4: videoout layout overflow" );
		else
			KisakPs4StartupBreadcrumb( "kisak-ps4: videoout layout unsupported" );
		return false;
	}
	KisakPs4StartupBreadcrumb( "kisak-ps4: videoout layout valid" );
	KisakPs4StartupBreadcrumb( "kisak-ps4: videoout helper diagnostics v1.74" );
    KisakPs4StartupBreadcrumb( "kisak-ps4: videoout before open" );
    if ( sceGnmVideoOutOpen( &g_VideoOut, &info ) != GNM_ERROR_OK )
    {
		char diagnostic[96];
		snprintf( diagnostic, sizeof( diagnostic ),
		    "kisak-ps4: videoout open stage=%u code=%d",
		    g_VideoOut.last_error_stage, g_VideoOut.last_error_code );
		KisakPs4StartupBreadcrumb( diagnostic );
		if ( g_VideoOut.last_error_stage == 1 && g_VideoOut.last_error_code == GNM_ERROR_INVALID_ARGS )
			KisakPs4StartupBreadcrumb( "kisak-ps4: videoout open layout invalid args" );
		else if ( g_VideoOut.last_error_stage == 1 && g_VideoOut.last_error_code == GNM_ERROR_OVERFLOW )
			KisakPs4StartupBreadcrumb( "kisak-ps4: videoout open layout overflow" );
		else switch ( g_VideoOut.last_error_stage )
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

    if ( !KisakPs4GnmFillAndWait( buffer, (uint32_t)g_VideoOut.buffersize, 0xff201008 ) )
    {
        memset( buffer, 0, (size_t)g_VideoOut.buffersize );
        if ( !g_GpuClearLogged )
            KisakPs4StartupBreadcrumb( "kisak-ps4: GPU VideoOut clear failed; CPU fallback active" );
    }
    else if ( !g_GpuClearLogged )
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: GPU VideoOut clear and EOP passed" );
    }
    g_GpuClearLogged = true;
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
