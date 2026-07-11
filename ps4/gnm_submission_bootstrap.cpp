#include "materialsystem/ps4gnm/ps4_gnm_device.h"
#include "materialsystem/ps4gnm/ps4_gnm_draw_state.h"
#include "materialsystem/ps4gnm/ps4_gnm_texture.h"
#include "materialsystem/ps4gnm/ps4_gnm_shader.h"
#include "materialsystem/ps4gnm/ps4_shader_manifest.h"
#include "materialsystem/ps4gnm/shaderapips4.h"

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
GnmVsShader *g_VertexShader = 0;
GnmPsShader *g_PixelShader = 0;
GnmPsShader *g_TexturePixelShader = 0;
CPs4GnmShader g_VertexShaderResource;
CPs4GnmShader g_SolidPixelShaderResource;
CPs4GnmShader g_TexturePixelShaderResource;
void *g_FetchShader = 0;
GnmBuffer *g_VertexBuffers = 0;
GnmDepthRenderTarget g_DepthTarget = {};
bool g_DepthTargetReady = false;
void *g_DepthMemory = 0;
uint32_t g_DepthMemorySize = 0;
CPs4GnmTexture g_DiagnosticTexture;
CPs4GnmTexture g_DiagnosticCopyTexture;
CPs4ShaderManifest g_ShaderManifest;
void *g_TextureSamplerTable = 0;
bool g_ShadersReady = false;
bool g_TriangleReadbackLogged = false;
bool g_ShadowStateApplyLogged = false;
bool g_ShadowDisplayApplyLogged = false;
bool g_TextureMemoryLogged = false;
bool g_ShaderManifestLogged = false;
uint32_t g_PendingSamplerMask = 0;
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

uint32_t ShaderSamplerAvailability( const GnmPsShader *shader )
{
    uint32_t mask = 0;
    const GnmInputUsageSlot *usages = sceGnmPsShaderInputUsageSlotTable( shader );
    for ( uint32_t i = 0; i < shader->common.numinputusageslots; ++i )
    {
        if ( usages[i].usagetype == GNM_SHINPUTUSAGE_PTR_SAMPLERTABLE ||
            usages[i].usagetype == GNM_SHINPUTUSAGE_PTR_RESOURCETABLE )
            return 0xffffffffu;
        if ( usages[i].usagetype == GNM_SHINPUTUSAGE_IMM_SAMPLER && usages[i].apislot < 32 )
            mask |= 1u << usages[i].apislot;
    }
    return mask;
}

uint32_t ShaderFragmentOutputMask( const GnmPsShader *shader )
{
    uint32_t mask = 0;
    for ( uint32_t output = 0; output < 8; ++output )
    {
        if ( ( shader->registers.spishadercolformat >> ( output * 4 ) ) & 0xf )
            mask |= 1u << output;
    }
    return mask;
}

bool LoadDiagnosticShaders()
{
    g_PendingSamplerMask = 0;
    const Ps4ShaderManifestKey keys[3] = {
        { "kisak_diagnostic", PS4_SHADER_STAGE_VERTEX, 0, 0, 1 },
        { "kisak_diagnostic", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 },
        { "kisak_texture_sample", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 }
    };
    uint8_t *manifestData = 0;
    size_t manifestSize = 0;
    if ( !ReadShaderFile( "/app0/kisak_diagnostic.manifest", &manifestData, &manifestSize ) ||
        !g_ShaderManifest.LoadText( reinterpret_cast< const char * >( manifestData ), manifestSize ) )
    {
        free( manifestData );
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "manifest file load failed" );
        return false;
    }
    free( manifestData );
    if ( !g_ShaderManifestLogged )
    {
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native shader manifest entries=%u",
            static_cast< unsigned int >( g_ShaderManifest.Count() ) );
        KisakPs4StartupBreadcrumb( message );
        g_ShaderManifestLogged = true;
    }
    uint8_t *gpuCursor = static_cast< uint8_t * >( g_Mapped );
    uint8_t *gpuEnd = gpuCursor + kPersistentMemorySize;
    for ( unsigned int shader = 0; shader < 3; ++shader )
    {
        const Ps4ShaderManifestEntry *entry = g_ShaderManifest.Find( keys[shader] );
        if ( !entry )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "manifest lookup failed stage=%u", shader );
            return false;
        }
        uint8_t *fileData = 0;
        size_t fileSize = 0;
        if ( !ReadShaderFile( entry->path, &fileData, &fileSize ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "file read failed stage=%u path=%s", shader, entry->path );
            return false;
        }
        GnmShaderMetadata metadata = {};
        const GnmError metadataResult = sceGnmShaderBinaryGetMetadata( fileData, fileSize, &metadata );
        const bool expectedStage = shader == 0 ? metadata.type == GNM_SHADER_VERTEX : metadata.type == GNM_SHADER_PIXEL;
        const uint32_t actualVertexInputs = shader == 0 && metadataResult == GNM_ERROR_OK && metadata.stage
            ? reinterpret_cast< const GnmVsShader * >( metadata.stage )->numinputsemantics : 0;
        const GnmShaderCommonData *common = metadataResult == GNM_ERROR_OK && metadata.stage
            ? reinterpret_cast< const GnmShaderCommonData * >( metadata.stage ) : 0;
        const uint32_t actualConstantBytes = common
            ? common->embeddedconstantbufferdqwords * 16u : 0;
        const GnmPsShader *pixelStage = shader != 0 && metadataResult == GNM_ERROR_OK && metadata.stage
            ? reinterpret_cast< const GnmPsShader * >( metadata.stage ) : 0;
        const uint32_t availableSamplers = pixelStage ? ShaderSamplerAvailability( pixelStage ) : 0;
        const uint32_t actualFragmentOutputs = pixelStage ? ShaderFragmentOutputMask( pixelStage ) : 0;
        if ( metadataResult != GNM_ERROR_OK || !expectedStage || metadata.stagesize > 1024 ||
            ( entry->vertexInputCount && entry->vertexInputCount != actualVertexInputs ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "metadata failed stage=%u result=%d type=%u stagebytes=%u inputs=%u expected=%u filebytes=%llu",
                shader, metadataResult, metadata.type, metadata.stagesize,
                actualVertexInputs, entry->vertexInputCount,
                static_cast< unsigned long long >( fileSize ) );
            free( fileData );
            return false;
        }
        if ( entry->constantBytes != actualConstantBytes ||
            ( availableSamplers && ( entry->samplerMask & ~availableSamplers ) != 0 ) ||
            entry->fragmentOutputMask != actualFragmentOutputs )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "bindings failed stage=%u constants=%u/%u samplers=0x%x/0x%x outputs=0x%x/0x%x",
                shader, actualConstantBytes, entry->constantBytes,
                availableSamplers, entry->samplerMask,
                actualFragmentOutputs, entry->fragmentOutputMask );
            free( fileData );
            return false;
        }
        if ( pixelStage && entry->samplerMask && !availableSamplers )
            g_PendingSamplerMask |= entry->samplerMask;
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
        CPs4GnmShader &resource = shader == 0 ? g_VertexShaderResource
            : ( shader == 1 ? g_SolidPixelShaderResource : g_TexturePixelShaderResource );
        const GnmShaderType resourceType = shader == 0 ? GNM_SHADER_VERTEX : GNM_SHADER_PIXEL;
        if ( !resource.Initialize( fileData, fileSize, resourceType,
                gpuCursor, static_cast< size_t >( gpuEnd - gpuCursor ) ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "shader resource failed stage=%u", shader );
            free( fileData );
            return false;
        }
        if ( shader == 0 )
            g_VertexShader = resource.VertexShader();
        else if ( shader == 1 )
            g_PixelShader = resource.PixelShader();
        else
            g_TexturePixelShader = resource.PixelShader();
        const char *role = shader == 0 ? "vertex"
            : ( shader == 1 ? "solid_pixel" : "texture_pixel" );
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native GNM shader resource role=%s codebytes=%u",
            role, resource.CodeSize() );
        KisakPs4StartupBreadcrumb( message );
        gpuCursor += resource.CodeSize();
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
    g_DepthMemory = gpuCursor;
    g_DepthMemorySize = static_cast< uint32_t >( depthSize );
    gpuCursor += depthSize;
    if ( !g_DiagnosticTexture.Initialize2D( gpuCursor,
        static_cast< size_t >( gpuEnd - gpuCursor ), GNM_FMT_R8G8B8A8_UNORM,
        4, 4, 1, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "diagnostic texture allocation failed" );
        return false;
    }
    const uint32_t texturePixels[16] = {
        0xff2020ff, 0xff20ff20, 0xffff2020, 0xffffffff,
        0xff20ff20, 0xffff2020, 0xffffffff, 0xff2020ff,
        0xffff2020, 0xffffffff, 0xff2020ff, 0xff20ff20,
        0xffffffff, 0xff2020ff, 0xff20ff20, 0xffff2020
    };
    if ( !g_DiagnosticTexture.UploadLinear( texturePixels, 4 * sizeof( uint32_t ), 4 ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "texture upload failed" );
        return false;
    }
    if ( !g_DiagnosticTexture.CreateColorTargetView( GNM_FMT_R8G8B8A8_UNORM,
        4, 4, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "texture color target view failed" );
        return false;
    }
    uint8_t *tableCursor = static_cast< uint8_t * >( g_DiagnosticTexture.Data() ) +
        g_DiagnosticTexture.Size();
    tableCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( tableCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    if ( sizeof( GnmTexture ) + sizeof( GnmSampler ) > static_cast< size_t >( gpuEnd - tableCursor ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "texture table allocation failed" );
        return false;
    }
    GnmTexture *textureTable = reinterpret_cast< GnmTexture * >( tableCursor );
    *textureTable = g_DiagnosticTexture.Descriptor();
    GnmSampler *sampler = reinterpret_cast< GnmSampler * >( tableCursor + sizeof( GnmTexture ) );
    memset( sampler, 0, sizeof( *sampler ) );
    sampler->clampx = sampler->clampy = sampler->clampz = GNM_TEX_CLAMP_CLAMP_LAST_TEXEL;
    sampler->depthcomparefunc = GNM_DEPTH_COMPARE_ALWAYS;
    sampler->filtermode = GNM_FILTER_MODE_BLEND;
    sampler->xymagfilter = sampler->xyminfilter = GNM_FILTER_POINT;
    sampler->zfilter = GNM_ZFILTER_NONE;
    sampler->mipfilter = GNM_MIPFILTER_NONE;
    sampler->bordercolortype = GNM_BORDER_COLOR_OPAQUE_BLACK;
    g_TextureSamplerTable = tableCursor;
    uint8_t *copyCursor = tableCursor + sizeof( GnmTexture ) + sizeof( GnmSampler );
    copyCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( copyCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    if ( !g_DiagnosticCopyTexture.Initialize2D( copyCursor,
        static_cast< size_t >( gpuEnd - copyCursor ), GNM_FMT_R8G8B8A8_UNORM,
        4, 4, 1, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE ) ||
        !g_DiagnosticCopyTexture.CreateColorTargetView( GNM_FMT_R8G8B8A8_UNORM,
            4, 4, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "copy texture allocation failed" );
        return false;
    }
    uint8_t *copyTableCursor = static_cast< uint8_t * >( g_DiagnosticCopyTexture.Data() ) +
        g_DiagnosticCopyTexture.Size();
    copyTableCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( copyTableCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    if ( sizeof( GnmTexture ) + sizeof( GnmSampler ) >
        static_cast< size_t >( gpuEnd - copyTableCursor ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ), "copy texture table failed" );
        return false;
    }
    *reinterpret_cast< GnmTexture * >( copyTableCursor ) = g_DiagnosticCopyTexture.Descriptor();
    *reinterpret_cast< GnmSampler * >( copyTableCursor + sizeof( GnmTexture ) ) = *sampler;
    g_TextureSamplerTable = copyTableCursor;
    const uint32_t combinedTableSamplerMask = 1u;
    if ( ( g_PendingSamplerMask & ~combinedTableSamplerMask ) != 0 )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "combined sampler table missing declared=0x%x available=0x%x",
            g_PendingSamplerMask, combinedTableSamplerMask );
        return false;
    }
    if ( g_PendingSamplerMask )
    {
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native shader sampler binding mask=0x%x source=combined_table",
            g_PendingSamplerMask );
        KisakPs4StartupBreadcrumb( message );
    }
    snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
        "ready vsbytes=%u psbytes=%u fetchbytes=%u vertexinputs=%u depthbytes=%llu texturebytes=%llu",
        g_VertexShader->common.shadersize, g_PixelShader->common.shadersize,
        fetchSize, g_VertexShader->numinputsemantics,
        static_cast< unsigned long long >( depthSize ),
        static_cast< unsigned long long >( g_DiagnosticTexture.Size() ) );
    if ( !g_TextureMemoryLogged )
    {
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native debug texture memory bytes=%d", KisakPs4TextureMemoryUsed() );
        KisakPs4StartupBreadcrumb( message );
        g_TextureMemoryLogged = true;
    }
    return true;
}

void EmitDiagnosticTriangle( GnmCommandBuffer *command, void *destination, const uint16_t *indices )
{
    GnmRenderTarget renderTarget = {};
    if ( sceGnmRtCreateColorTarget( &renderTarget, destination, GNM_FMT_R8G8B8A8_SRGB,
        1920, 1080, 1, 1, 1, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE, 0, 0 ) != GNM_ERROR_OK )
        return;
    sceGnmDrawCmdInitDefaultHardwareState( command );
    KisakPs4SetShaderShadowCulling( false );
    KisakPs4SetShaderShadowDepth( false, false, 3 );
    KisakPs4SetShaderShadowBlend( false, 1, 0, 0, false, 1, 0, 0 );
    const uint32_t shadowStateMask = KisakPs4ApplyShaderShadowState( command );
    if ( !g_ShadowStateApplyLogged )
    {
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native shader shadow applied mask=0x%08x", shadowStateMask );
        KisakPs4StartupBreadcrumb( message );
        g_ShadowStateApplyLogged = true;
    }
    const GnmSetViewportInfo viewport = {
        0.0f, 1.0f,
        { 960.0f, -540.0f, 0.5f },
        { 960.0f, 540.0f, 0.25f }
    };
    const GnmSetViewportInfo offscreenViewport = {
        0.0f, 1.0f,
        { 2.0f, -2.0f, 0.5f },
        { 2.0f, 2.0f, 0.5f }
    };
    g_DrawState.BeginCommand();
    g_DrawState.RetainDirtyMask( ~static_cast< uint32_t >(
        CPs4GnmDrawState::kDirtyRenderTargetMask |
        CPs4GnmDrawState::kDirtyPrimitive |
        CPs4GnmDrawState::kDirtyDepthStencil |
        CPs4GnmDrawState::kDirtyBlend ) );
    g_DrawState.SetViewport( 0, offscreenViewport );
    g_DrawState.SetScissor( 0, 0, 4, 4 );
    sceGnmDrawCmdSetHwScreenOffset( command, 0, 0 );
    sceGnmDrawCmdSetGuardBands( command, 100.0f, 100.0f, 1.0f, 1.0f );
    GnmViewportTransformControl viewportControl = {};
    viewportControl.scalex = viewportControl.offsetx = 1;
    viewportControl.scaley = viewportControl.offsety = 1;
    viewportControl.scalez = viewportControl.offsetz = 1;
    viewportControl.invertw = 1;
    g_DrawState.SetViewportTransform( viewportControl );
    if ( !sceGnmDrawCmdFillMemory( command,
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticTexture.Data() ) ),
        static_cast< uint32_t >( g_DiagnosticTexture.Size() ), 0xff000000 ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    g_DrawState.ClearDepthRenderTarget();
    GnmDbRenderControl dbControl = {};
    g_DrawState.SetDbRenderControl( dbControl );
    g_DrawState.SetRenderTarget( 0, g_DiagnosticTexture.ColorTarget() );
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
    g_DrawState.SetPrimitiveType( GNM_PT_TRILIST );
    g_DrawState.Apply( command );
    sceGnmDrawCmdDrawIndex( command, 3, indices );
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    if ( !sceGnmDrawCmdCopyMemory( command,
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticCopyTexture.Data() ) ),
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticTexture.Data() ) ),
        static_cast< uint32_t >( g_DiagnosticTexture.Size() ) ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );

    g_DrawState.SetViewport( 0, viewport );
    g_DrawState.SetScissor( 0, 0, 1920, 1080 );
    sceGnmDrawCmdSetHwScreenOffset( command, 60, 32 );
    sceGnmDrawCmdSetGuardBands( command, 33.0f, 59.0f, 1.0f, 1.0f );
    KisakPs4SetShaderShadowDepth( g_DepthTargetReady, g_DepthTargetReady, 3 );
    KisakPs4SetShaderShadowBlend( true, 4, 5, 0, true, 1, 0, 0 );
    const uint32_t displayStateMask = KisakPs4ApplyShaderShadowState( command );
    if ( !g_ShadowDisplayApplyLogged )
    {
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native shader shadow display state mask=0x%08x", displayStateMask );
        KisakPs4StartupBreadcrumb( message );
        g_ShadowDisplayApplyLogged = true;
    }
    if ( g_DepthTargetReady )
        g_DrawState.SetDepthRenderTarget( g_DepthTarget );
    else
        g_DrawState.ClearDepthRenderTarget();
    g_DrawState.SetDbRenderControl( dbControl );
    g_DrawState.SetRenderTarget( 0, renderTarget );
    g_DrawState.SetVertexShader( g_VertexShader->registers, 0 );
    g_DrawState.SetPixelShader( g_TexturePixelShader->registers );
    g_DrawState.SetIndexSize( GNM_INDEX_16, GNM_POLICY_LRU );
    if ( g_FetchShader )
    {
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 0, g_FetchShader );
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, g_VertexBuffers );
    }
    g_DrawState.SetPointerUserData( GNM_STAGE_PS, 0, g_TextureSamplerTable );
    g_DrawState.SetPsInputUsage(
        sceGnmVsShaderExportSemanticTable( g_VertexShader ), g_VertexShader->numexportsemantics,
        sceGnmPsShaderInputSemanticTable( g_TexturePixelShader ), g_TexturePixelShader->numinputsemantics );
    g_DrawState.SetPrimitiveType( GNM_PT_TRILIST );
    g_DrawState.Apply( command );
    sceGnmDrawCmdDrawIndex( command, 3, indices );
    GnmSetViewportInfo farViewport = viewport;
    farViewport.offset[2] = 0.75f;
    g_DrawState.SetViewport( 0, farViewport );
    g_DrawState.Apply( command );
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
    if ( g_DepthTargetReady )
    {
        if ( !sceGnmDrawCmdFillMemory( &command,
            static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DepthMemory ) ),
            g_DepthMemorySize, 0x3f800000 ) )
        {
            g_Device.CancelFrame();
            return false;
        }
        sceGnmDrawCmdWaitGraphicsWrite( &command, GNM_ACQUIRE_TARGET_DB );
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
