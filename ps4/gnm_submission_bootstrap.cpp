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
    off_t directMemory = 0;
    void *mapped = 0;
    int result = sceKernelAllocateDirectMemory( 0, (off_t)sceKernelGetDirectMemorySize(),
        kDirectMemorySize, kDirectMemoryAlignment, ORBIS_KERNEL_WC_GARLIC, &directMemory );
    if ( result < 0 )
    {
        LogResult( "allocate failed", result );
        return false;
    }

    const int protection = ORBIS_KERNEL_PROT_CPU_READ | ORBIS_KERNEL_PROT_CPU_RW |
        ORBIS_KERNEL_PROT_GPU_READ | ORBIS_KERNEL_PROT_GPU_WRITE;
    result = sceKernelMapDirectMemory( &mapped, kDirectMemorySize, protection, 0,
        directMemory, kDirectMemoryAlignment );
    if ( result < 0 )
    {
        LogResult( "map failed", result );
        sceKernelReleaseDirectMemory( directMemory, kDirectMemorySize );
        return false;
    }

    CPs4GnmDevice device;
    bool passed = device.Initialize( mapped, kDirectMemorySize );
    uint64_t completedLabel = 0;
    for ( unsigned int submit = 0; passed && submit < 3; ++submit )
    {
        passed = device.BeginFrame( completedLabel );
        void *commandMemory = passed ? device.FrameArena().Allocate( kCommandBufferSize, 256 ) : 0;
        volatile uint64_t *eopLabel = passed ? static_cast< volatile uint64_t * >(
            device.FrameArena().Allocate( sizeof( uint64_t ), 8 ) ) : 0;
        if ( !commandMemory || !eopLabel )
        {
            passed = false;
            break;
        }

        const uint64_t submittedLabel = device.SubmittedLabel() + 1;
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

        const uint64_t recordedLabel = device.EndFrame();
        passed = recordedLabel == submittedLabel && WaitForLabel( eopLabel, submittedLabel );
        if ( !passed )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: gnm submission EOP timeout" );
            break;
        }
        completedLabel = submittedLabel;
    }

    sceKernelMunmap( mapped, kDirectMemorySize );
    sceKernelReleaseDirectMemory( directMemory, kDirectMemorySize );
    KisakPs4StartupBreadcrumb( passed
        ? "kisak-ps4: gnm submission two-frame EOP passed"
        : "kisak-ps4: gnm submission self-test failed; CPU clear retained" );
    return passed;
}
