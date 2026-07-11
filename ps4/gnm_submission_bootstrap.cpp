#include "materialsystem/ps4gnm/ps4_gnm_device.h"
#include "materialsystem/ps4gnm/ps4_gnm_draw_state.h"

#include <gnm_commandbuffer.h>
#include <gnm_controls.h>
#include <gnm_drawcommandbuffer.h>
#include <gnm_depthrendertarget.h>
#include <gnm_helpers.h>
#include <gnm_rendertarget.h>
#include <gnm_shaderbinary.h>
#include <gnmdriver.h>
#include <orbis/libkernel.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
const size_t kDirectMemorySize = 16 * 1024 * 1024;
const size_t kDirectMemoryAlignment = 2 * 1024 * 1024;
const size_t kCommandBufferSize = 64 * 1024;
const size_t kPersistentMemorySize = 10 * 1024 * 1024;
off_t g_DirectMemory = 0;
void *g_Mapped = 0;
CPs4GnmDevice g_Device;
CPs4GnmDrawState g_DrawState;
uint64_t g_CompletedLabel = 0;
uint8_t g_VsStorage[1024];
uint8_t g_PsStorage[1024];
GnmVsShader *g_VertexShader = 0;
GnmPsShader *g_PixelShader = 0;
void *g_FetchShader = 0;
GnmBuffer *g_VertexBuffers = 0;
GnmDepthRenderTarget g_DepthTarget = {};
bool g_DepthTargetReady = false;
bool g_ShadersReady = false;
bool g_TriangleReadbackLogged = false;
char g_ShaderDiagnostic[160] = "not attempted";

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

bool ReadShaderFile( const char *path, uint8_t **data, size_t *size )
{
    FILE *file = fopen( path, "rb" );
    if ( !file || fseek( file, 0, SEEK_END ) != 0 )
    {
        if ( file ) fclose( file );
        return false;
    }
    const long fileSize = ftell( file );
    if ( fileSize <= 0 || fseek( file, 0, SEEK_SET ) != 0 )
    {
        fclose( file );
        return false;
    }
    uint8_t *bytes = static_cast< uint8_t * >( malloc( static_cast< size_t >( fileSize ) ) );
    if ( !bytes || fread( bytes, 1, static_cast< size_t >( fileSize ), file ) != static_cast< size_t >( fileSize ) )
    {
        free( bytes );
        fclose( file );
        return false;
    }
    fclose( file );
    *data = bytes;
    *size = static_cast< size_t >( fileSize );
    return true;
}

bool LoadDiagnosticShaders()
{
    const char *paths[2] = {
        "/app0/kisak_diagnostic.vert.sb",
        "/app0/kisak_diagnostic.frag.sb"
    };
    uint8_t *gpuCursor = static_cast< uint8_t * >( g_Mapped );
    uint8_t *gpuEnd = gpuCursor + kPersistentMemorySize;
    for ( unsigned int shader = 0; shader < 2; ++shader )
    {
        uint8_t *fileData = 0;
        size_t fileSize = 0;
        if ( !ReadShaderFile( paths[shader], &fileData, &fileSize ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "file read failed stage=%u path=%s", shader, paths[shader] );
            return false;
        }
        GnmShaderMetadata metadata = {};
        const GnmError metadataResult = sceGnmShaderBinaryGetMetadata( fileData, fileSize, &metadata );
        const bool expectedStage = shader == 0 ? metadata.type == GNM_SHADER_VERTEX : metadata.type == GNM_SHADER_PIXEL;
        uint8_t *storage = shader == 0 ? g_VsStorage : g_PsStorage;
        if ( metadataResult != GNM_ERROR_OK || !expectedStage || metadata.stagesize > 1024 )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "metadata failed stage=%u result=%d type=%u stagebytes=%u filebytes=%llu",
                shader, metadataResult, metadata.type, metadata.stagesize,
                static_cast< unsigned long long >( fileSize ) );
            free( fileData );
            return false;
        }
        gpuCursor = reinterpret_cast< uint8_t * >(
            ( reinterpret_cast< uintptr_t >( gpuCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
        if ( metadata.shadercodesize > static_cast< uint32_t >( gpuEnd - gpuCursor ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "code allocation failed stage=%u codebytes=%u available=%llu",
                shader, metadata.shadercodesize,
                static_cast< unsigned long long >( gpuEnd - gpuCursor ) );
            free( fileData );
            return false;
        }
        memcpy( storage, metadata.stage, metadata.stagesize );
        memcpy( gpuCursor, metadata.shadercode, metadata.shadercodesize );
        if ( shader == 0 )
        {
            g_VertexShader = reinterpret_cast< GnmVsShader * >( storage );
            sceGnmVsRegsSetAddress( &g_VertexShader->registers, gpuCursor );
        }
        else
        {
            g_PixelShader = reinterpret_cast< GnmPsShader * >( storage );
            sceGnmPsRegsSetAddress( &g_PixelShader->registers, gpuCursor );
            g_PixelShader->registers.spibaryccntl = 0;
        }
        gpuCursor += metadata.shadercodesize;
        free( fileData );
    }

    GnmFetchShaderCreateInfo fetchInfo = {};
    fetchInfo.regs = &g_VertexShader->registers;
    fetchInfo.inputusages = sceGnmVsShaderInputUsageSlotTable( g_VertexShader );
    fetchInfo.numinputusages = g_VertexShader->common.numinputusageslots;
    fetchInfo.vtxinputs = sceGnmVsShaderInputSemanticTable( g_VertexShader );
    fetchInfo.numvtxinputs = g_VertexShader->numinputsemantics;
    if ( fetchInfo.numvtxinputs == 0 )
    {
        g_FetchShader = 0;
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "ready vsbytes=%u psbytes=%u fetchbytes=0 procedural=1",
            g_VertexShader->common.shadersize, g_PixelShader->common.shadersize );
        return true;
    }
    uint32_t fetchSize = 0;
    if ( sceGnmFetchShaderCalcSize( &fetchSize, &fetchInfo ) != GNM_ERROR_OK )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "fetch size failed inputs=%u usages=%u", fetchInfo.numvtxinputs,
            fetchInfo.numinputusages );
        return false;
    }
    gpuCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( gpuCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    if ( fetchSize > static_cast< uint32_t >( gpuEnd - gpuCursor ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "fetch allocation failed bytes=%u", fetchSize );
        return false;
    }
    GnmFetchShaderResults fetchResults = {};
    if ( sceGnmCreateFetchShader( gpuCursor, fetchSize, &fetchInfo, &fetchResults ) != GNM_ERROR_OK )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "fetch creation failed bytes=%u", fetchSize );
        return false;
    }
    g_FetchShader = gpuCursor;
    sceGnmVsRegsSetFetchShaderModifier( &g_VertexShader->registers, &fetchResults );
    gpuCursor += fetchSize;

    struct DiagnosticVertex
    {
        float position[4];
        float color[4];
    };
    static const DiagnosticVertex vertices[3] = {
        { { -0.5f, -0.4f, 0.0f, 1.0f }, { 1.0f, 0.1f, 0.0f, 1.0f } },
        { {  0.0f,  0.6f, 0.0f, 1.0f }, { 1.0f, 0.5f, 0.0f, 1.0f } },
        { {  0.5f, -0.4f, 0.0f, 1.0f }, { 1.0f, 0.9f, 0.0f, 1.0f } }
    };
    gpuCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( gpuCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    if ( sizeof( vertices ) + 2 * sizeof( GnmBuffer ) > static_cast< size_t >( gpuEnd - gpuCursor ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "vertex allocation failed bytes=%u", static_cast< unsigned int >( sizeof( vertices ) ) );
        return false;
    }
    DiagnosticVertex *vertexData = reinterpret_cast< DiagnosticVertex * >( gpuCursor );
    memcpy( vertexData, vertices, sizeof( vertices ) );
    gpuCursor += sizeof( vertices );
    gpuCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( gpuCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    if ( 2 * sizeof( GnmBuffer ) > static_cast< size_t >( gpuEnd - gpuCursor ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "vertex descriptor allocation failed" );
        return false;
    }
    g_VertexBuffers = reinterpret_cast< GnmBuffer * >( gpuCursor );
    g_VertexBuffers[0] = sceGnmCreateVertexBuffer( vertexData[0].position,
        GNM_FMT_R32G32B32A32_FLOAT, sizeof( DiagnosticVertex ), 3 );
    g_VertexBuffers[1] = sceGnmCreateVertexBuffer( vertexData[0].color,
        GNM_FMT_R32G32B32A32_FLOAT, sizeof( DiagnosticVertex ), 3 );
    gpuCursor += 2 * sizeof( GnmBuffer );

    const GnmDepthRenderTargetCreateInfo depthInfo = {
        1920, 1080, 1920, 1, GNM_Z_32_FLOAT, GNM_STENCIL_INVALID,
        GNM_TM_DEPTH_1D_THIN, GNM_GPU_BASE, 1, {}
    };
    if ( sceGnmCreateDepthRenderTarget( &g_DepthTarget, &depthInfo ) != GNM_ERROR_OK )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "depth target create failed" );
        return false;
    }
    uint64_t depthSize = 0;
    uint32_t depthAlignment = 0;
    if ( sceGnmDrtCalcByteSize( &depthSize, &depthAlignment, &g_DepthTarget ) != GNM_ERROR_OK ||
        !depthAlignment )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "depth layout failed" );
        return false;
    }
    gpuCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( gpuCursor ) + depthAlignment - 1 ) &
        ~static_cast< uintptr_t >( depthAlignment - 1 ) );
    if ( depthSize > static_cast< uint64_t >( gpuEnd - gpuCursor ) ||
        sceGnmDrtSetZReadAddress( &g_DepthTarget, gpuCursor ) != GNM_ERROR_OK ||
        sceGnmDrtSetZWriteAddress( &g_DepthTarget, gpuCursor ) != GNM_ERROR_OK )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "depth allocation failed bytes=%llu align=%u",
            static_cast< unsigned long long >( depthSize ), depthAlignment );
        return false;
    }
    g_DepthTargetReady = true;
    snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
        "ready vsbytes=%u psbytes=%u fetchbytes=%u vertexinputs=%u depthbytes=%llu",
        g_VertexShader->common.shadersize, g_PixelShader->common.shadersize,
        fetchSize, g_VertexShader->numinputsemantics,
        static_cast< unsigned long long >( depthSize ) );
    return true;
}

void EmitDiagnosticTriangle( GnmCommandBuffer *command, void *destination, const uint16_t *indices )
{
    GnmRenderTarget renderTarget = {};
    if ( sceGnmRtCreateColorTarget( &renderTarget, destination, GNM_FMT_R8G8B8A8_SRGB,
        1920, 1080, 1, 1, 1, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE, 0, 0 ) != GNM_ERROR_OK )
        return;
    sceGnmDrawCmdInitDefaultHardwareState( command );
    const GnmSetViewportInfo viewport = {
        0.0f, 1.0f,
        { 960.0f, -540.0f, 0.5f },
        { 960.0f, 540.0f, 0.5f }
    };
    g_DrawState.BeginCommand();
    g_DrawState.SetViewport( 0, viewport );
    g_DrawState.SetScissor( 0, 0, 1920, 1080 );
    sceGnmDrawCmdSetHwScreenOffset( command, 60, 32 );
    sceGnmDrawCmdSetGuardBands( command, 33.0f, 59.0f, 1.0f, 1.0f );
    GnmViewportTransformControl viewportControl = {};
    viewportControl.scalex = viewportControl.offsetx = 1;
    viewportControl.scaley = viewportControl.offsety = 1;
    viewportControl.scalez = viewportControl.offsetz = 1;
    viewportControl.invertw = 1;
    g_DrawState.SetViewportTransform( viewportControl );
    GnmPrimitiveSetup primitiveSetup = {};
    primitiveSetup.cullmode = GNM_CULL_NONE;
    primitiveSetup.frontface = GNM_FACE_CCW;
    primitiveSetup.frontmode = primitiveSetup.backmode = GNM_FILL_SOLID;
    g_DrawState.SetPrimitiveSetup( primitiveSetup );
    GnmDepthStencilControl depthControl = {};
    g_DrawState.SetDepthStencilControl( depthControl );
    if ( g_DepthTargetReady )
        g_DrawState.SetDepthRenderTarget( g_DepthTarget );
    else
        g_DrawState.ClearDepthRenderTarget();
    GnmDbRenderControl dbControl = {};
    g_DrawState.SetDbRenderControl( dbControl );
    GnmBlendControl blendControl = {};
    blendControl.blendenabled = true;
    blendControl.colorfunc = GNM_COMB_DST_PLUS_SRC;
    blendControl.colorsrcmult = GNM_BLEND_SRC_ALPHA;
    blendControl.colordstmult = GNM_BLEND_ONE_MINUS_SRC_ALPHA;
    blendControl.alphafunc = GNM_COMB_DST_PLUS_SRC;
    blendControl.alphasrcmult = GNM_BLEND_ONE;
    blendControl.alphadstmult = GNM_BLEND_ZERO;
    blendControl.separatealphaenable = true;
    g_DrawState.SetBlendControl( 0, blendControl );
    g_DrawState.SetRenderTarget( 0, renderTarget );
    g_DrawState.SetRenderTargetMask( 0xf );
    g_DrawState.SetVertexShader( g_VertexShader->registers, 0 );
    g_DrawState.SetPixelShader( g_PixelShader->registers );
    g_DrawState.SetIndexSize( GNM_INDEX_16, GNM_POLICY_LRU );
    if ( g_FetchShader )
    {
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 0, g_FetchShader );
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, g_VertexBuffers );
    }
    g_DrawState.SetPsInputUsage(
        sceGnmVsShaderExportSemanticTable( g_VertexShader ), g_VertexShader->numexportsemantics,
        sceGnmPsShaderInputSemanticTable( g_PixelShader ), g_PixelShader->numinputsemantics );
    g_DrawState.Apply( command );
    sceGnmDrawCmdSetPrimitiveType( command, GNM_PT_TRILIST );
    sceGnmDrawCmdDrawIndex( command, 3, indices );
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

    g_ShadersReady = LoadDiagnosticShaders();
    {
        char message[208];
        snprintf( message, sizeof( message ), "kisak-ps4: diagnostic shader detail %s",
            g_ShaderDiagnostic );
        KisakPs4StartupBreadcrumb( message );
    }
    KisakPs4StartupBreadcrumb( g_ShadersReady
        ? "kisak-ps4: diagnostic shader binaries loaded"
        : "kisak-ps4: diagnostic shader load failed; DMA bars retained" );
    bool passed = g_Device.Initialize( static_cast< uint8_t * >( g_Mapped ) + kPersistentMemorySize,
        kDirectMemorySize - kPersistentMemorySize );
    for ( unsigned int submit = 0; passed && submit < 3; ++submit )
    {
        CPs4GnmDevice::SubmissionFrame submission = {};
        passed = g_Device.BeginSubmission( g_CompletedLabel, kCommandBufferSize, 256, &submission );
        if ( !passed )
        {
            break;
        }

        GnmCommandBuffer command = sceGnmCmdInit( submission.commandMemory, kCommandBufferSize, 0, 0 );
        sceGnmDrawCmdInitDefaultHardwareState( &command );
        sceGnmDrawCmdDrawIndexAuto( &command, 0 );
        sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
            (uint64_t)(uintptr_t)submission.completionLabel, GNM_DATA_SEL_SEND_DATA64,
            submission.submittedLabel );

        void *dcbAddresses[1] = { command.beginptr };
        uint32_t dcbSizes[1] = {
            static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
                reinterpret_cast< uintptr_t >( command.beginptr ) )
        };
        result = sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 );
        if ( result < 0 )
        {
            LogResult( "submit failed", result );
            g_Device.CancelFrame();
            passed = false;
            break;
        }
        result = sceGnmSubmitDone();
        if ( result < 0 )
        {
            LogResult( "submit done failed", result );
            g_Device.CancelFrame();
            passed = false;
            break;
        }

        passed = g_Device.CommitSubmission( submission ) &&
            WaitForLabel( submission.completionLabel, submission.submittedLabel );
        if ( !passed )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: gnm submission EOP timeout" );
            break;
        }
        g_CompletedLabel = submission.submittedLabel;
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
    if ( !g_Mapped || !destination || !size )
        return false;

    CPs4GnmDevice::SubmissionFrame submission = {};
    if ( !g_Device.BeginSubmission( g_CompletedLabel, kCommandBufferSize, 256, &submission ) )
        return false;

    GnmCommandBuffer command = sceGnmCmdInit( submission.commandMemory, kCommandBufferSize, 0, 0 );
    if ( !sceGnmDrawCmdFillMemory( &command, (uint64_t)(uintptr_t)destination, size, value ) )
    {
        g_Device.CancelFrame();
        return false;
    }
    sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)submission.completionLabel, GNM_DATA_SEL_SEND_DATA64,
        submission.submittedLabel );

    void *dcbAddresses[1] = { command.beginptr };
    uint32_t dcbSizes[1] = {
        static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
            reinterpret_cast< uintptr_t >( command.beginptr ) )
    };
    const int submitResult = sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 );
    if ( submitResult < 0 || sceGnmSubmitDone() < 0 )
    {
        g_Device.CancelFrame();
        return false;
    }
    if ( !g_Device.CommitSubmission( submission ) ||
        !WaitForLabel( submission.completionLabel, submission.submittedLabel ) )
        return false;
    g_CompletedLabel = submission.submittedLabel;
    return true;
}

extern "C" bool KisakPs4GnmColorBarsAndWait( void *destination, uint32_t size )
{
    if ( !g_Mapped || !destination || size < 16 || ( size & 15 ) )
        return false;

    CPs4GnmDevice::SubmissionFrame submission = {};
    if ( !g_Device.BeginSubmission( g_CompletedLabel, kCommandBufferSize, 256, &submission ) )
        return false;

    const uint32_t bandSize = size / 4;
    const uint32_t colors[4] = {
        0xff0000ff, // red in little-endian A8B8G8R8 memory
        0xff00ff00, // green
        0xffff0000, // blue
        0xffffffff  // white
    };
    GnmCommandBuffer command = sceGnmCmdInit( submission.commandMemory, kCommandBufferSize, 0, 0 );
    for ( unsigned int band = 0; band < 4; ++band )
    {
        const uintptr_t address = reinterpret_cast< uintptr_t >( destination ) + band * bandSize;
        if ( !sceGnmDrawCmdFillMemory( &command, address, bandSize, colors[band] ) )
        {
            g_Device.CancelFrame();
            return false;
        }
    }
    if ( g_ShadersReady )
    {
        uint16_t *indices = static_cast< uint16_t * >(
            g_Device.FrameArena().Allocate( 3 * sizeof( uint16_t ), 8 ) );
        if ( !indices )
        {
            g_Device.CancelFrame();
            return false;
        }
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        sceGnmDrawCmdWaitGraphicsWrite( &command, GNM_ACQUIRE_TARGET_CB0 );
        EmitDiagnosticTriangle( &command, destination, indices );
    }

    sceGnmDrawCmdEventWriteEop( &command, GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)submission.completionLabel, GNM_DATA_SEL_SEND_DATA64,
        submission.submittedLabel );
    void *dcbAddresses[1] = { command.beginptr };
    uint32_t dcbSizes[1] = {
        static_cast< uint32_t >( reinterpret_cast< uintptr_t >( command.cmdptr ) -
            reinterpret_cast< uintptr_t >( command.beginptr ) )
    };
    if ( sceGnmSubmitCommandBuffers( 1, dcbAddresses, dcbSizes, 0, 0 ) < 0 ||
        sceGnmSubmitDone() < 0 )
    {
        g_Device.CancelFrame();
        return false;
    }
    if ( !g_Device.CommitSubmission( submission ) ||
        !WaitForLabel( submission.completionLabel, submission.submittedLabel ) )
        return false;
    g_CompletedLabel = submission.submittedLabel;
    if ( g_ShadersReady && !g_TriangleReadbackLogged )
    {
        const uint32_t centerPixel = static_cast< const volatile uint32_t * >( destination )[540 * 1920 + 960];
        char message[96];
        snprintf( message, sizeof( message ),
            "kisak-ps4: diagnostic center pixel 0x%08x", centerPixel );
        KisakPs4StartupBreadcrumb( message );
        g_TriangleReadbackLogged = true;
    }
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

extern "C" bool KisakPs4GnmDiagnosticShadersReady()
{
    return g_ShadersReady;
}
