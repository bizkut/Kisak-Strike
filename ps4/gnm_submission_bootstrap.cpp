#include "materialsystem/ps4gnm/ps4_gnm_device.h"

#include <gnm_commandbuffer.h>
#include <gnm_drawcommandbuffer.h>
#include <gnmdriver.h>
#include <orbis/libkernel.h>

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );
extern "C" int sceKernelUsleep( unsigned int microseconds );
extern "C" int32_t sceKernelMunmap( void *address, uint64_t size );
extern "C" int32_t sceKernelReleaseDirectMemory( int64_t start, uint64_t size );

#ifndef ORBIS_KERNEL_WC_GARLIC
#define ORBIS_KERNEL_WC_GARLIC 3
#endif
#ifndef ORBIS_KERNEL_PROT_CPU_READ
#define ORBIS_KERNEL_PROT_CPU_READ 0x01
#endif
#ifndef ORBIS_KERNEL_PROT_CPU_RW
#define ORBIS_KERNEL_PROT_CPU_RW 0x02
#endif
#ifndef ORBIS_KERNEL_PROT_GPU_READ
#define ORBIS_KERNEL_PROT_GPU_READ 0x10
#endif
#ifndef ORBIS_KERNEL_PROT_GPU_WRITE
#define ORBIS_KERNEL_PROT_GPU_WRITE 0x20
#endif

namespace
{
const size_t kDirectMemorySize = 2 * 1024 * 1024;
const size_t kDirectMemoryAlignment = 2 * 1024 * 1024;
const size_t kCommandBufferSize = 64 * 1024;
off_t g_DirectMemory = 0;
void *g_Mapped = 0;
CPs4GnmDevice g_Device;
uint64_t g_CompletedLabel = 0;

void LogResult( const char *stage, int result )
{
    char message[112];
    snprintf( message, sizeof( message ), "kisak-ps4: gnm submission %s result=%d", stage, result );
    KisakPs4StartupBreadcrumb( message );
}

bool WaitForLabel( volatile uint64_t *label, uint64_t expected )
{
    for ( unsigned int poll = 0; poll < 40000; ++poll )
    {
        if ( *label == expected )
            return true;
        sceKernelUsleep( 50 );
    }
    return false;
}
}

extern "C" bool KisakPs4GnmSubmissionSelfTest()
{
    if ( g_Mapped )
        return true;
    int result = sceKernelAllocateDirectMemory( 0, (off_t)sceKernelGetDirectMemorySize(),
        kDirectMemorySize, kDirectMemoryAlignment, ORBIS_KERNEL_WC_GARLIC, &g_DirectMemory );
    if ( result < 0 )
    {
        LogResult( "allocate failed", result );
        return false;
    }

    const int protection = ORBIS_KERNEL_PROT_CPU_READ | ORBIS_KERNEL_PROT_CPU_RW |
        ORBIS_KERNEL_PROT_GPU_READ | ORBIS_KERNEL_PROT_GPU_WRITE;
    result = sceKernelMapDirectMemory( &g_Mapped, kDirectMemorySize, protection, 0,
        g_DirectMemory, kDirectMemoryAlignment );
    if ( result < 0 )
    {
        LogResult( "map failed", result );
        sceKernelReleaseDirectMemory( g_DirectMemory, kDirectMemorySize );
        g_DirectMemory = 0;
        return false;
    }

    bool passed = g_Device.Initialize( g_Mapped, kDirectMemorySize );
    for ( unsigned int submit = 0; passed && submit < 3; ++submit )
    {
        passed = g_Device.BeginFrame( g_CompletedLabel );
        void *commandMemory = passed ? g_Device.FrameArena().Allocate( kCommandBufferSize, 256 ) : 0;
        volatile uint64_t *eopLabel = passed ? static_cast< volatile uint64_t * >(
            g_Device.FrameArena().Allocate( sizeof( uint64_t ), 8 ) ) : 0;
        if ( !commandMemory || !eopLabel )
        {
            passed = false;
            break;
        }

        const uint64_t submittedLabel = g_Device.SubmittedLabel() + 1;
        *eopLabel = 0;
        GnmCommandBuffer command = sceGnmCmdInit( commandMemory, kCommandBufferSize, 0, 0 );
        sceGnmDrawCmdInitDefaultHardwareState( &command );
        sceGnmDrawCmdDrawIndexAuto( &command, 0 );
        sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
            (uint64_t)(uintptr_t)eopLabel, GNM_DATA_SEL_SEND_DATA64, submittedLabel );

        void *dcbAddresses[1] = { command.beginptr };
        uint32_t dcbSizes[1] = {
            static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
                reinterpret_cast< uintptr_t >( command.beginptr ) )
        };
        result = sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 );
        if ( result < 0 )
        {
            LogResult( "submit failed", result );
            passed = false;
            break;
        }
        result = sceGnmSubmitDone();
        if ( result < 0 )
        {
            LogResult( "submit done failed", result );
            passed = false;
            break;
        }

        const uint64_t recordedLabel = g_Device.EndFrame();
        passed = recordedLabel == submittedLabel && WaitForLabel( eopLabel, submittedLabel );
        if ( !passed )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: gnm submission EOP timeout" );
            break;
        }
        g_CompletedLabel = submittedLabel;
    }

    KisakPs4StartupBreadcrumb( passed
        ? "kisak-ps4: gnm submission two-frame EOP passed"
        : "kisak-ps4: gnm submission self-test failed; CPU clear retained" );
    if ( !passed )
    {
        sceKernelMunmap( g_Mapped, kDirectMemorySize );
        sceKernelReleaseDirectMemory( g_DirectMemory, kDirectMemorySize );
        g_Mapped = 0;
        g_DirectMemory = 0;
    }
    return passed;
}

extern "C" bool KisakPs4GnmFillAndWait( void *destination, uint32_t size, uint32_t value )
{
    if ( !g_Mapped || !destination || !size || !g_Device.BeginFrame( g_CompletedLabel ) )
        return false;

    void *commandMemory = g_Device.FrameArena().Allocate( kCommandBufferSize, 256 );
    volatile uint64_t *eopLabel = static_cast< volatile uint64_t * >(
        g_Device.FrameArena().Allocate( sizeof( uint64_t ), 8 ) );
    if ( !commandMemory || !eopLabel )
    {
        g_Device.CancelFrame();
        return false;
    }

    const uint64_t submittedLabel = g_Device.SubmittedLabel() + 1;
    *eopLabel = 0;
    GnmCommandBuffer command = sceGnmCmdInit( commandMemory, kCommandBufferSize, 0, 0 );
    if ( !sceGnmDrawCmdFillMemory( &command, (uint64_t)(uintptr_t)destination, size, value ) )
    {
        g_Device.CancelFrame();
        return false;
    }
    sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)eopLabel, GNM_DATA_SEL_SEND_DATA64, submittedLabel );

    void *dcbAddresses[1] = { command.beginptr };
    uint32_t dcbSizes[1] = {
        static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
            reinterpret_cast< uintptr_t >( command.beginptr ) )
    };
    const int submitResult = sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 );
    if ( submitResult < 0 || sceGnmSubmitDone() < 0 )
        return false;
    if ( g_Device.EndFrame() != submittedLabel || !WaitForLabel( eopLabel, submittedLabel ) )
        return false;
    g_CompletedLabel = submittedLabel;
    return true;
}

extern "C" bool KisakPs4GnmColorBarsAndWait( void *destination, uint32_t size )
{
    if ( !g_Mapped || !destination || size < 16 || ( size & 15 ) ||
        !g_Device.BeginFrame( g_CompletedLabel ) )
        return false;

    void *commandMemory = g_Device.FrameArena().Allocate( kCommandBufferSize, 256 );
    volatile uint64_t *eopLabel = static_cast< volatile uint64_t * >(
        g_Device.FrameArena().Allocate( sizeof( uint64_t ), 8 ) );
    if ( !commandMemory || !eopLabel )
    {
        g_Device.CancelFrame();
        return false;
    }

    const uint32_t bandSize = size / 4;
    const uint32_t colors[4] = {
        0xff0000ff, // red in little-endian A8B8G8R8 memory
        0xff00ff00, // green
        0xffff0000, // blue
        0xffffffff  // white
    };
    GnmCommandBuffer command = sceGnmCmdInit( commandMemory, kCommandBufferSize, 0, 0 );
    for ( unsigned int band = 0; band < 4; ++band )
    {
        const uintptr_t address = reinterpret_cast< uintptr_t >( destination ) + band * bandSize;
        if ( !sceGnmDrawCmdFillMemory( &command, address, bandSize, colors[band] ) )
        {
            g_Device.CancelFrame();
            return false;
        }
    }

    const uint64_t submittedLabel = g_Device.SubmittedLabel() + 1;
    *eopLabel = 0;
    sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)eopLabel, GNM_DATA_SEL_SEND_DATA64, submittedLabel );
    void *dcbAddresses[1] = { command.beginptr };
    uint32_t dcbSizes[1] = {
        static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
            reinterpret_cast< uintptr_t >( command.beginptr ) )
    };
    if ( sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 ) < 0 ||
        sceGnmSubmitDone() < 0 )
        return false;
    if ( g_Device.EndFrame() != submittedLabel || !WaitForLabel( eopLabel, submittedLabel ) )
        return false;
    g_CompletedLabel = submittedLabel;
    return true;
}

extern "C" void KisakPs4GnmSubmissionShutdown()
{
    if ( !g_Mapped )
        return;
    sceKernelMunmap( g_Mapped, kDirectMemorySize );
    sceKernelReleaseDirectMemory( g_DirectMemory, kDirectMemorySize );
    g_Mapped = 0;
    g_DirectMemory = 0;
    KisakPs4StartupBreadcrumb( "kisak-ps4: gnm submission pool released" );
}
