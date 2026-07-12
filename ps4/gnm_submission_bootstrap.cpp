#include "materialsystem/ps4gnm/ps4_gnm_device.h"
#include "materialsystem/ps4gnm/ps4_gnm_draw_state.h"
#include "materialsystem/ps4gnm/ps4_gnm_draw_packet.h"
#include "materialsystem/ps4gnm/ps4_gnm_texture.h"
#include "materialsystem/ps4gnm/ps4_gnm_shader.h"
#include "materialsystem/ps4gnm/ps4_gnm_shader_handles.h"
#include "materialsystem/ps4gnm/ps4_shader_manifest.h"
#include "materialsystem/ps4gnm/ps4_gnm_constants.h"
#include "materialsystem/ps4gnm/ps4_gnm_runtime.h"
#include "materialsystem/ps4gnm/ps4_shadow_state_translate.h"
#include "materialsystem/ps4gnm/shaderapips4.h"

#include <gnm_commandbuffer.h>
#include <gnm_controls.h>
#include <gnm_drawcommandbuffer.h>
#include <gnm_depthrendertarget.h>
#include <gnm_helpers.h>
#include <gnm_rendertarget.h>
#include <gnm_shaderbinary.h>
#include <gnmdriver.h>
#include <gpuaddr.h>
#include <orbis/libkernel.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>

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
const size_t kDirectMemorySize = 64 * 1024 * 1024;
const size_t kDirectMemoryAlignment = 2 * 1024 * 1024;
const size_t kCommandBufferSize = 64 * 1024;
const size_t kPersistentMemorySize = 58 * 1024 * 1024;
const uint32_t kDiagnosticVertexCount = 24;
const uint32_t kDiagnosticIndexCount = 36;
off_t g_DirectMemory = 0;
void *g_Mapped = 0;
CPs4GnmDevice g_Device;
CPs4GnmDrawState g_DrawState;
uint64_t g_CompletedLabel = 0;
GnmVsShader *g_VertexShader = 0;
GnmPsShader *g_PixelShader = 0;
GnmPsShader *g_TexturePixelShader = 0;
GnmPsShader *g_DepthClearPixelShader = 0;
GnmVsShader *g_ReferenceCubeVertexShader = 0;
GnmPsShader *g_ReferenceCubePixelShader = 0;
CPs4GnmShader g_VertexShaderResource;
CPs4GnmShader g_SolidPixelShaderResource;
CPs4GnmShader g_TexturePixelShaderResource;
CPs4GnmShader g_DepthClearPixelShaderResource;
CPs4GnmShader g_ReferenceCubeVertexShaderResource;
CPs4GnmShader g_ReferenceCubePixelShaderResource;
CPs4GnmShaderHandleTable g_ShaderHandles;
Ps4GnmShaderHandle g_VertexShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
Ps4GnmShaderHandle g_SolidPixelShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
Ps4GnmShaderHandle g_TexturePixelShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
Ps4GnmShaderHandle g_DepthClearPixelShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
Ps4GnmShaderHandle g_ReferenceCubeVertexShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
Ps4GnmShaderHandle g_ReferenceCubePixelShaderHandle = PS4_GNM_SHADER_HANDLE_INVALID;
void *g_FetchShader = 0;
GnmBuffer *g_VertexBuffers = 0;
CPs4GnmBuffer g_DiagnosticVertexBuffer;
GnmDepthRenderTarget g_DepthTarget = {};
bool g_DepthTargetReady = false;
void *g_DepthMemory = 0;
uint32_t g_DepthMemorySize = 0;
CPs4GnmTexture g_DiagnosticTexture;
CPs4GnmTexture g_DiagnosticCopyTexture;
CPs4GnmTexture g_CubeEdgeTexture;
CPs4ShaderManifest g_ShaderManifest;
void *g_TextureSamplerTable = 0;
void *g_ReferenceCubeFetchShader = 0;
GnmBuffer *g_ReferenceCubeVertexBuffers = 0;
GnmBuffer *g_CubeEdgeVertexBuffers = 0;
CPs4GnmBuffer g_ReferenceCubeVertexBuffer;
CPs4GnmBuffer g_CubeEdgeVertexBuffer;
CPs4GnmVertexDeclaration g_ReferenceCubeVertexDeclaration;
CPs4GnmConstants g_ReferenceCubeConstants;
void *g_CubeEdgeSamplerTable = 0;
bool g_ShadersReady = false;
bool g_TriangleReadbackLogged = false;
bool g_ShadowStateApplyLogged = false;
bool g_ShadowDisplayApplyLogged = false;
bool g_FacadeDrawLogged = false;
typedef void ( *SourceFrameCallback )( void *context );
SourceFrameCallback g_SourceFrameCallback = 0;
void *g_SourceFrameContext = 0;
bool g_SourceFrameCallbackLogged = false;
bool g_TextureMemoryLogged = false;
bool g_ShaderManifestLogged = false;
uint32_t g_PendingSamplerMask = 0;
char g_ShaderDiagnostic[160] = "not attempted";

const float kReferenceCubeBaseMvp[16] = {
    0.219f, 0.059f, 0.094f, 0.0f, 0.0f, 0.423f, -0.068f, 0.0f,
    0.126f, -0.102f, -0.163f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f
};

void MultiplyMatrices( const float *left, const float *right, float *result )
{
    for ( uint32_t column = 0; column < 4; ++column )
        for ( uint32_t row = 0; row < 4; ++row )
        {
            float value = 0.0f;
            for ( uint32_t inner = 0; inner < 4; ++inner )
                value += left[inner * 4 + row] * right[column * 4 + inner];
            result[column * 4 + row] = value;
        }
}

bool UpdateReferenceCubeMvp()
{
    static uint64_t startTime = 0;
    static uint64_t updateCount = 0;
    const uint64_t now = sceKernelGetProcessTime();
    if ( !startTime )
        startTime = now;
    ++updateCount;
    const float seconds = now > startTime || updateCount == 1
        ? static_cast< float >( now - startTime ) * 0.000001f
        : static_cast< float >( updateCount - 1 ) * ( 1.0f / 60.0f );
    const float xAngle = seconds * 0.261799388f;
    const float yAngle = seconds * 1.047197551f;
    const float sinX = sinf( xAngle );
    const float cosX = cosf( xAngle );
    const float sinY = sinf( yAngle );
    const float cosY = cosf( yAngle );
    const float rotationX[16] = {
        1, 0, 0, 0, 0, cosX, sinX, 0, 0, -sinX, cosX, 0, 0, 0, 0, 1
    };
    const float rotationY[16] = {
        cosY, 0, -sinY, 0, 0, 1, 0, 0, sinY, 0, cosY, 0, 0, 0, 0, 1
    };
    float rotation[16];
    float mvp[16];
    MultiplyMatrices( rotationY, rotationX, rotation );
    MultiplyMatrices( kReferenceCubeBaseMvp, rotation, mvp );
    if ( !g_ReferenceCubeConstants.SetFloat(
        CPs4GnmConstants::kVertex, 0, mvp, 4 ) )
        return false;
    if ( updateCount == 1 || updateCount == 120 )
    {
        char message[160];
        snprintf( message, sizeof( message ),
            "kisak-ps4: reference cube MVP update=%llu time_us=%llu m00=%.4f m02=%.4f",
            static_cast< unsigned long long >( updateCount ),
            static_cast< unsigned long long >( now >= startTime ? now - startTime : 0 ),
            mvp[0], mvp[8] );
        KisakPs4StartupBreadcrumb( message );
    }
    return true;
}

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
    const Ps4ShaderManifestKey keys[6] = {
        { "kisak_diagnostic", PS4_SHADER_STAGE_VERTEX, 0, 0, 1 },
        { "kisak_diagnostic", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 },
        { "kisak_texture_sample", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 },
        { "kisak_depth_clear", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 },
        { "kisak_reference_cube", PS4_SHADER_STAGE_VERTEX, 0, 0, 2 },
        { "kisak_reference_cube", PS4_SHADER_STAGE_PIXEL, 0, 0, 0 }
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
    for ( unsigned int shader = 0; shader < 6; ++shader )
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
        const bool vertexStage = shader == 0 || shader == 4;
        const bool expectedStage = vertexStage ? metadata.type == GNM_SHADER_VERTEX : metadata.type == GNM_SHADER_PIXEL;
        const uint32_t actualVertexInputs = vertexStage && metadataResult == GNM_ERROR_OK && metadata.stage
            ? reinterpret_cast< const GnmVsShader * >( metadata.stage )->numinputsemantics : 0;
        const GnmShaderCommonData *common = metadataResult == GNM_ERROR_OK && metadata.stage
            ? reinterpret_cast< const GnmShaderCommonData * >( metadata.stage ) : 0;
        const uint32_t actualConstantBytes = common
            ? common->embeddedconstantbufferdqwords * 16u : 0;
        const GnmPsShader *pixelStage = !vertexStage && metadataResult == GNM_ERROR_OK && metadata.stage
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
            : ( shader == 1 ? g_SolidPixelShaderResource
            : ( shader == 2 ? g_TexturePixelShaderResource
            : ( shader == 3 ? g_DepthClearPixelShaderResource
            : ( shader == 4 ? g_ReferenceCubeVertexShaderResource
            : g_ReferenceCubePixelShaderResource ) ) ) );
        const GnmShaderType resourceType = vertexStage ? GNM_SHADER_VERTEX : GNM_SHADER_PIXEL;
        if ( !resource.Initialize( fileData, fileSize, resourceType,
                gpuCursor, static_cast< size_t >( gpuEnd - gpuCursor ) ) )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "shader resource failed stage=%u", shader );
            free( fileData );
            return false;
        }
        const Ps4GnmShaderHandleStage handleStage = vertexStage
            ? PS4_GNM_SHADER_HANDLE_VERTEX : PS4_GNM_SHADER_HANDLE_PIXEL;
        const Ps4GnmShaderHandle handle = g_ShaderHandles.Register( &resource, handleStage );
        CPs4GnmShader *resolved = g_ShaderHandles.Resolve( handle, handleStage );
        if ( !handle || !resolved )
        {
            snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
                "shader handle failed stage=%u", shader );
            free( fileData );
            return false;
        }
        if ( shader == 0 )
        {
            g_VertexShaderHandle = handle;
            g_VertexShader = resolved->VertexShader();
        }
        else if ( shader == 1 )
        {
            g_SolidPixelShaderHandle = handle;
            g_PixelShader = resolved->PixelShader();
        }
        else if ( shader == 2 )
        {
            g_TexturePixelShaderHandle = handle;
            g_TexturePixelShader = resolved->PixelShader();
        }
        else
        {
            if ( shader == 3 )
            {
                g_DepthClearPixelShaderHandle = handle;
                g_DepthClearPixelShader = resolved->PixelShader();
            }
            else if ( shader == 4 )
            {
                g_ReferenceCubeVertexShaderHandle = handle;
                g_ReferenceCubeVertexShader = resolved->VertexShader();
            }
            else
            {
                g_ReferenceCubePixelShaderHandle = handle;
                g_ReferenceCubePixelShader = resolved->PixelShader();
            }
        }
        const char *role = shader == 0 ? "vertex"
            : ( shader == 1 ? "solid_pixel"
            : ( shader == 2 ? "texture_pixel"
            : ( shader == 3 ? "depth_clear_pixel"
            : ( shader == 4 ? "reference_cube_vertex"
            : "reference_cube_pixel" ) ) ) );
        char message[112];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native GNM shader resource role=%s handle=0x%x codebytes=%u",
            role, handle, resource.CodeSize() );
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
    const float corners[8][3] = {
        { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
        { -1, -1,  1 }, { 1, -1,  1 }, { 1, 1,  1 }, { -1, 1,  1 }
    };
    const uint8_t faces[6][4] = {
        { 4, 5, 6, 7 }, { 1, 0, 3, 2 }, { 0, 4, 7, 3 },
        { 5, 1, 2, 6 }, { 3, 7, 6, 2 }, { 0, 1, 5, 4 }
    };
    const float faceColors[6][4] = {
        { 1.0f, 0.15f, 0.05f, 1.0f }, { 0.15f, 0.45f, 1.0f, 1.0f },
        { 0.15f, 1.0f, 0.35f, 1.0f }, { 1.0f, 0.8f, 0.05f, 1.0f },
        { 0.8f, 0.15f, 1.0f, 1.0f }, { 0.05f, 0.9f, 1.0f, 1.0f }
    };
    DiagnosticVertex vertices[kDiagnosticVertexCount] = {};
    for ( uint32_t face = 0; face < 6; ++face )
    {
        for ( uint32_t corner = 0; corner < 4; ++corner )
        {
            const float *source = corners[faces[face][corner]];
            const float rotatedX = 0.8660254f * source[0] + 0.5f * source[2];
            const float rotatedZ = -0.5f * source[0] + 0.8660254f * source[2];
            const float rotatedY = 0.8660254f * source[1] - 0.5f * rotatedZ;
            const float cameraZ = 3.5f + 0.5f * source[1] + 0.8660254f * rotatedZ;
            DiagnosticVertex &vertex = vertices[face * 4 + corner];
            vertex.position[0] = rotatedX * 0.45f;
            vertex.position[1] = rotatedY * 0.45f;
            vertex.position[2] = ( cameraZ - 1.5f ) * 0.45f;
            vertex.position[3] = cameraZ;
            memcpy( vertex.color, faceColors[face], sizeof( vertex.color ) );
        }
    }
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
    if ( !g_DiagnosticVertexBuffer.Initialize( vertexData, sizeof( vertices ),
        CPs4GnmBuffer::kVertexBuffer, false ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "vertex facade initialization failed" );
        return false;
    }
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
    const CPs4GnmVertexDeclaration::Element declarationElements[2] = {
        { 0, static_cast< uint16_t >( offsetof( DiagnosticVertex, position ) ),
            GNM_FMT_R32G32B32A32_FLOAT },
        { 0, static_cast< uint16_t >( offsetof( DiagnosticVertex, color ) ),
            GNM_FMT_R32G32B32A32_FLOAT }
    };
    CPs4GnmVertexDeclaration declaration;
    if ( !declaration.Initialize( declarationElements, 2 ) ||
        !g_Device.SetStreamSource( 0, &g_DiagnosticVertexBuffer, 0,
            sizeof( DiagnosticVertex ) ) ||
        !g_Device.BuildVertexDescriptorTable( declaration, 0, kDiagnosticVertexCount,
            g_VertexBuffers, 2 ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "vertex facade descriptor table failed" );
        return false;
    }
    gpuCursor += 2 * sizeof( GnmBuffer );

    GpaSurfaceProperties depthSurface = {};
    const GnmDataFormat depthFormat = sceGnmDfInitFromZ( GNM_Z_32_FLOAT );
    if ( sceGpaFindOptimalSurface( &depthSurface, GPA_SURFACE_DEPTH,
        sceGnmDfGetBitsPerElement( depthFormat ), 1, false,
        GNM_GPU_BASE ) != GPA_ERR_OK )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "depth tile mode selection failed" );
        return false;
    }
    const GnmDepthRenderTargetCreateInfo depthInfo = {
        1920, 1080, 1920, 1, GNM_Z_32_FLOAT, GNM_STENCIL_INVALID,
        depthSurface.tilemode, GNM_GPU_BASE, 1, {}
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
    GnmSampler *sampler = reinterpret_cast< GnmSampler * >( tableCursor + sizeof( GnmTexture ) );
    memset( sampler, 0, sizeof( *sampler ) );
    sampler->clampx = sampler->clampy = sampler->clampz = GNM_TEX_CLAMP_CLAMP_LAST_TEXEL;
    sampler->depthcomparefunc = GNM_DEPTH_COMPARE_ALWAYS;
    sampler->filtermode = GNM_FILTER_MODE_BLEND;
    sampler->xymagfilter = sampler->xyminfilter = GNM_FILTER_POINT;
    sampler->zfilter = GNM_ZFILTER_NONE;
    sampler->mipfilter = GNM_MIPFILTER_NONE;
    sampler->bordercolortype = GNM_BORDER_COLOR_OPAQUE_BLACK;
    if ( !g_DiagnosticTexture.BuildSamplerTable( *sampler, tableCursor,
            CPs4GnmTexture::SamplerTableSize(), &g_TextureSamplerTable ) )
        return false;
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
    if ( !g_DiagnosticCopyTexture.BuildSamplerTable( *sampler, copyTableCursor,
            CPs4GnmTexture::SamplerTableSize(), &g_TextureSamplerTable ) )
        return false;
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
    uint8_t *referenceCursor = copyTableCursor + sizeof( GnmTexture ) + sizeof( GnmSampler );
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    if ( !g_CubeEdgeTexture.Initialize2D( referenceCursor,
            static_cast< size_t >( gpuEnd - referenceCursor ), GNM_FMT_R8G8B8A8_UNORM,
            1, 1, 1, GNM_TM_DISPLAY_LINEAR_ALIGNED, GNM_GPU_BASE ) )
        return false;
    const uint32_t edgeColor = 0xff411f10;
    if ( !g_CubeEdgeTexture.UploadLinear( &edgeColor, sizeof( edgeColor ), 1 ) )
        return false;
    referenceCursor = static_cast< uint8_t * >( g_CubeEdgeTexture.Data() ) +
        g_CubeEdgeTexture.Size();
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    if ( sizeof( GnmTexture ) + sizeof( GnmSampler ) >
        static_cast< size_t >( gpuEnd - referenceCursor ) )
        return false;
    if ( !g_CubeEdgeTexture.BuildSamplerTable( *sampler, referenceCursor,
            CPs4GnmTexture::SamplerTableSize(), &g_CubeEdgeSamplerTable ) )
        return false;
    referenceCursor += sizeof( GnmTexture ) + sizeof( GnmSampler );
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    const size_t referenceVertexBytes = 36 * 8 * sizeof( float );
    if ( referenceVertexBytes + 2 * sizeof( GnmBuffer ) >
        static_cast< size_t >( gpuEnd - referenceCursor ) )
        return false;
    float *referenceVertices = reinterpret_cast< float * >( referenceCursor );
    const float cubeCorners[8][3] = {
        {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}
    };
    const uint8_t cubeFaces[6][6] = {
        {0,1,2,2,3,0},{4,5,6,6,7,4},{7,3,0,0,4,7},
        {6,2,1,1,5,6},{0,1,5,5,4,0},{3,2,6,6,7,3}
    };
    const float cubeUvs[6][2] = {{0,1},{1,1},{1,0},{1,0},{0,0},{0,1}};
    for ( uint32_t face = 0; face < 6; ++face )
        for ( uint32_t vertex = 0; vertex < 6; ++vertex )
        {
            float *destination = referenceVertices + ( face * 6 + vertex ) * 8;
            memcpy( destination, cubeCorners[cubeFaces[face][vertex]], 3 * sizeof( float ) );
            memset( destination + 3, 0, 3 * sizeof( float ) );
            memcpy( destination + 6, cubeUvs[vertex], 2 * sizeof( float ) );
        }
    referenceCursor += referenceVertexBytes;
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    g_ReferenceCubeVertexBuffers = reinterpret_cast< GnmBuffer * >( referenceCursor );
    const CPs4GnmVertexDeclaration::Element referenceElements[2] = {
        { 0, 0, GNM_FMT_R32G32B32_FLOAT },
        { 0, static_cast< uint16_t >( 6 * sizeof( float ) ), GNM_FMT_R32G32_FLOAT }
    };
    if ( !g_ReferenceCubeVertexBuffer.Initialize( referenceVertices,
            referenceVertexBytes, CPs4GnmBuffer::kVertexBuffer, false ) ||
        !g_ReferenceCubeVertexDeclaration.Initialize( referenceElements, 2 ) ||
        !g_Device.SetStreamSource( 0, &g_ReferenceCubeVertexBuffer, 0,
            8 * sizeof( float ) ) ||
        !g_Device.BuildVertexDescriptorTable( g_ReferenceCubeVertexDeclaration,
            0, 36, g_ReferenceCubeVertexBuffers, 2 ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "reference cube facade descriptor table failed" );
        return false;
    }
    referenceCursor += 2 * sizeof( GnmBuffer );
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    const size_t edgeVertexBytes = 24 * 8 * sizeof( float );
    if ( edgeVertexBytes + 2 * sizeof( GnmBuffer ) >
        static_cast< size_t >( gpuEnd - referenceCursor ) )
        return false;
    float *edgeVertices = reinterpret_cast< float * >( referenceCursor );
    const uint8_t cubeEdges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for ( uint32_t edge = 0; edge < 12; ++edge )
        for ( uint32_t endpoint = 0; endpoint < 2; ++endpoint )
        {
            float *destination = edgeVertices + ( edge * 2 + endpoint ) * 8;
            const float *source = cubeCorners[cubeEdges[edge][endpoint]];
            destination[0] = source[0] * 1.012f;
            destination[1] = source[1] * 1.012f;
            destination[2] = source[2] * 1.012f;
            memset( destination + 3, 0, 3 * sizeof( float ) );
            destination[6] = destination[7] = 0.5f;
        }
    referenceCursor += edgeVertexBytes;
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 15 ) & ~static_cast< uintptr_t >( 15 ) );
    g_CubeEdgeVertexBuffers = reinterpret_cast< GnmBuffer * >( referenceCursor );
    if ( !g_CubeEdgeVertexBuffer.Initialize( edgeVertices, edgeVertexBytes,
            CPs4GnmBuffer::kVertexBuffer, false ) ||
        !g_Device.SetStreamSource( 0, &g_CubeEdgeVertexBuffer, 0, 8 * sizeof( float ) ) ||
        !g_Device.BuildVertexDescriptorTable( g_ReferenceCubeVertexDeclaration,
            0, 24, g_CubeEdgeVertexBuffers, 2 ) )
        return false;
    referenceCursor += 2 * sizeof( GnmBuffer );
    GnmFetchShaderCreateInfo referenceFetchInfo = {};
    referenceFetchInfo.regs = &g_ReferenceCubeVertexShader->registers;
    referenceFetchInfo.inputusages = sceGnmVsShaderInputUsageSlotTable( g_ReferenceCubeVertexShader );
    referenceFetchInfo.numinputusages = g_ReferenceCubeVertexShader->common.numinputusageslots;
    referenceFetchInfo.vtxinputs = sceGnmVsShaderInputSemanticTable( g_ReferenceCubeVertexShader );
    referenceFetchInfo.numvtxinputs = g_ReferenceCubeVertexShader->numinputsemantics;
    uint32_t referenceFetchSize = 0;
    if ( sceGnmFetchShaderCalcSize( &referenceFetchSize, &referenceFetchInfo ) != GNM_ERROR_OK )
        return false;
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    GnmFetchShaderResults referenceFetchResults = {};
    if ( sceGnmCreateFetchShader( referenceCursor, referenceFetchSize,
        &referenceFetchInfo, &referenceFetchResults ) != GNM_ERROR_OK )
        return false;
    g_ReferenceCubeFetchShader = referenceCursor;
    sceGnmVsRegsSetFetchShaderModifier( &g_ReferenceCubeVertexShader->registers,
        &referenceFetchResults );
    referenceCursor += referenceFetchSize;
    referenceCursor = reinterpret_cast< uint8_t * >(
        ( reinterpret_cast< uintptr_t >( referenceCursor ) + 255 ) & ~static_cast< uintptr_t >( 255 ) );
    if ( referenceCursor >= gpuEnd || !KisakPs4GnmRuntime().Register(
        &g_Device, referenceCursor, static_cast< size_t >( gpuEnd - referenceCursor ) ) )
    {
        snprintf( g_ShaderDiagnostic, sizeof( g_ShaderDiagnostic ),
            "native runtime arena registration failed" );
        return false;
    }
    {
        char message[128];
        snprintf( message, sizeof( message ),
            "kisak-ps4: native runtime persistent arena bytes=%llu",
            static_cast< unsigned long long >(
                KisakPs4GnmRuntime().PersistentArena().Capacity() ) );
        KisakPs4StartupBreadcrumb( message );
    }
    if ( !KisakPs4SourceBufferProbe() )
        return false;
    KisakPs4StartupBreadcrumb( "kisak-ps4: native Source buffer probe passed" );
    if ( !KisakPs4ShaderApiVertexFormatProbe() )
        return false;
    KisakPs4StartupBreadcrumb(
        "kisak-ps4: ShaderAPI vertex format bridge passed" );
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

bool ClearDiagnosticDepth( GnmCommandBuffer *command )
{
    if ( !command || !g_DepthTargetReady || !g_DepthClearPixelShader )
        return false;
    GnmDbRenderControl db = {};
    db.depthclearenable = true;
    GnmDepthStencilControl depth = {};
    depth.depthenable = true;
    depth.zwrite = true;
    depth.zfunc = GNM_DEPTH_COMPARE_ALWAYS;
    sceGnmDrawCmdSetDbRenderControl( command, &db );
    sceGnmDrawCmdSetDepthStencilControl( command, &depth );
    sceGnmDrawCmdSetDepthClearValue( command, 1.0f );
    sceGnmDrawCmdSetRenderTargetMask( command, 0 );
    sceGnmDrawCmdSetEmbeddedVsShader( command, GNM_EMBEDDED_VSH_FULLSCREEN, 0 );
    sceGnmDrawCmdSetPsShader( command, &g_DepthClearPixelShader->registers );
    float *color = static_cast< float * >(
        sceGnmCmdAllocInside( command, 4 * sizeof( float ), 4 ) );
    GnmBuffer *descriptor = static_cast< GnmBuffer * >(
        sceGnmCmdAllocInside( command, sizeof( GnmBuffer ), 4 ) );
    if ( !color || !descriptor )
        return false;
    memset( color, 0, 4 * sizeof( float ) );
    *descriptor = sceGnmCreateConstBuffer( color, 4 * sizeof( float ) );
    sceGnmDrawCmdSetPointerUserData( command, GNM_STAGE_PS, 0, descriptor );
    const GnmSetViewportInfo viewport = {
        0.0f, 1.0f, { 960.0f, -540.0f, 0.5f }, { 960.0f, 540.0f, 0.5f }
    };
    sceGnmDrawCmdSetViewport( command, 0, &viewport );
    sceGnmDrawCmdSetScreenScissor( command, 0, 0, 1920, 1080 );
    sceGnmDrawCmdSetDepthRenderTarget( command, &g_DepthTarget );
    sceGnmDrawCmdSetPrimitiveType( command, GNM_PT_RECTLIST );
    sceGnmDrawCmdSetIndexSize( command, GNM_INDEX_16, GNM_POLICY_LRU );
    sceGnmDrawCmdDrawIndexAuto( command, 3 );
    return true;
}

bool DrawDiagnosticColorBars( GnmCommandBuffer *command,
    const GnmRenderTarget &renderTarget )
{
    if ( !command || !g_DepthClearPixelShader )
        return false;

    GnmDbRenderControl db = {};
    GnmDepthStencilControl depth = {};
    depth.zfunc = GNM_DEPTH_COMPARE_NEVER;
    depth.stencilfunc = GNM_DEPTH_COMPARE_NEVER;
    depth.stencilbackfunc = GNM_DEPTH_COMPARE_NEVER;
    GnmBlendControl blend = {};
    sceGnmDrawCmdSetDbRenderControl( command, &db );
    sceGnmDrawCmdSetDepthStencilControl( command, &depth );
    sceGnmDrawCmdSetBlendControl( command, 0, &blend );
    sceGnmDrawCmdSetRenderTarget( command, 0, &renderTarget );
    sceGnmDrawCmdSetRenderTargetMask( command, 0xf );
    sceGnmDrawCmdSetEmbeddedVsShader( command, GNM_EMBEDDED_VSH_FULLSCREEN, 0 );
    sceGnmDrawCmdSetPsShader( command, &g_DepthClearPixelShader->registers );
    const GnmSetViewportInfo viewport = {
        0.0f, 1.0f, { 960.0f, -540.0f, 0.5f }, { 960.0f, 540.0f, 0.5f }
    };
    sceGnmDrawCmdSetViewport( command, 0, &viewport );
    sceGnmDrawCmdSetPrimitiveType( command, GNM_PT_RECTLIST );
    sceGnmDrawCmdSetIndexSize( command, GNM_INDEX_16, GNM_POLICY_LRU );

    const float colors[4][4] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };
    for ( uint32_t band = 0; band < 4; ++band )
    {
        float *color = static_cast< float * >(
            sceGnmCmdAllocInside( command, 4 * sizeof( float ), 4 ) );
        GnmBuffer *descriptor = static_cast< GnmBuffer * >(
            sceGnmCmdAllocInside( command, sizeof( GnmBuffer ), 4 ) );
        if ( !color || !descriptor )
            return false;
        memcpy( color, colors[band], sizeof( colors[band] ) );
        *descriptor = sceGnmCreateConstBuffer( color, 4 * sizeof( float ) );
        sceGnmDrawCmdSetPointerUserData( command, GNM_STAGE_PS, 0, descriptor );
        const int32_t top = static_cast< int32_t >( band * 270 );
        sceGnmDrawCmdSetScreenScissor( command, 0, top, 1920, top + 270 );
        sceGnmDrawCmdDrawIndexAuto( command, 3 );
    }
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    return true;
}

bool BuildSourceDynamicTriangle( CPs4GnmDevice::IndexedDrawPacket *packet,
    GnmBuffer *descriptors )
{
    if ( !packet || !descriptors )
        return false;
    if ( !KisakPs4PopulateShaderApiDynamicTriangle() )
        return false;
    int firstIndex = 0;
    int indexCount = 0;
    if ( !KisakPs4TakeDynamicMeshDraw( &firstIndex, &indexCount ) )
        return false;
    const CPs4GnmVertexDeclaration::Element elements[2] = {
        { 0, 0, GNM_FMT_R32G32B32A32_FLOAT },
        { 0, 16, GNM_FMT_R32G32B32A32_FLOAT }
    };
    CPs4GnmVertexDeclaration declaration;
    return declaration.Initialize( elements, 2 ) &&
        g_Device.BuildVertexDescriptorTable( declaration, 0, 3, descriptors, 2 ) &&
        g_Device.BuildIndexedDrawPacket( GNM_FMT_R32G32B32A32_FLOAT,
            static_cast< uint32_t >( firstIndex ),
            static_cast< uint32_t >( indexCount ), 0, 3, packet );
}

void EmitDiagnosticTriangle( GnmCommandBuffer *command, void *destination,
    const CPs4GnmDevice::IndexedDrawPacket &packet,
    const CPs4GnmDevice::IndexedDrawPacket &sourcePacket,
    GnmBuffer *sourceDescriptors,
    const CPs4GnmDevice::PrimitiveDrawPacket &cubePacket,
    const CPs4GnmDevice::PrimitiveDrawPacket &edgePacket,
    GnmBuffer *cubeVsDescriptor )
{
    GnmRenderTarget renderTarget = {};
    if ( !g_Device.BuildDisplayRenderTarget(
        destination, 1920, 1080, 1920, &renderTarget ) )
        return;
    sceGnmDrawCmdInitDefaultHardwareState( command );
    if ( !DrawDiagnosticColorBars( command, renderTarget ) )
        return;
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
        { 960.0f, 540.0f, 0.5f }
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
        static_cast< uint32_t >( g_DiagnosticTexture.Size() ), 0xff00a5ff ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    g_DrawState.ClearDepthRenderTarget();
    GnmDbRenderControl dbControl = {};
    g_DrawState.SetDbRenderControl( dbControl );
    g_DrawState.SetRenderTarget( 0, g_DiagnosticTexture.ColorTarget() );
    g_DrawState.SetVertexShader( g_VertexShader->registers, 0 );
    g_DrawState.SetPixelShader( g_PixelShader->registers );
    if ( g_FetchShader )
    {
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 0, g_FetchShader );
        g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, g_VertexBuffers );
    }
    g_DrawState.SetPsInputUsage(
        sceGnmVsShaderExportSemanticTable( g_VertexShader ), g_VertexShader->numexportsemantics,
        sceGnmPsShaderInputSemanticTable( g_PixelShader ), g_PixelShader->numinputsemantics );
    Ps4EmitIndexedDraw( command, &g_DrawState, packet, UINT32_MAX );
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    if ( !sceGnmDrawCmdCopyMemory( command,
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticCopyTexture.Data() ) ),
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticTexture.Data() ) ),
        static_cast< uint32_t >( g_DiagnosticTexture.Size() ) ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );

    // Isolate blending from the display-linear VideoOut target. Preserve the
    // original texture in the copy above for the cube, then clear this small
    // ordinary RGBA8 target and run the same alpha-exporting pixel shader.
    if ( !sceGnmDrawCmdFillMemory( command,
        static_cast< uint64_t >( reinterpret_cast< uintptr_t >( g_DiagnosticTexture.Data() ) ),
        static_cast< uint32_t >( g_DiagnosticTexture.Size() ), 0 ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );
    GnmBlendControl offscreenBlend = {};
    offscreenBlend.blendenabled = true;
    offscreenBlend.colorfunc = GNM_COMB_DST_PLUS_SRC;
    offscreenBlend.colorsrcmult = GNM_BLEND_SRC_ALPHA;
    offscreenBlend.colordstmult = GNM_BLEND_ONE_MINUS_SRC_ALPHA;
    offscreenBlend.alphafunc = GNM_COMB_DST_PLUS_SRC;
    offscreenBlend.alphasrcmult = GNM_BLEND_ONE;
    offscreenBlend.alphadstmult = GNM_BLEND_ZERO;
    g_DrawState.SetBlendControl( 0, offscreenBlend );
    g_DrawState.Invalidate( CPs4GnmDrawState::kDirtyBlend );
    if ( !Ps4EmitIndexedDraw( command, &g_DrawState, packet, UINT32_MAX ) )
        return;
    sceGnmDrawCmdWaitGraphicsWrite( command, GNM_ACQUIRE_TARGET_CB0 );

    g_DrawState.SetViewport( 0, viewport );
    g_DrawState.SetScissor( 0, 0, 1920, 1080 );
    sceGnmDrawCmdSetHwScreenOffset( command, 60, 32 );
    sceGnmDrawCmdSetGuardBands( command, 33.0f, 59.0f, 1.0f, 1.0f );
    KisakPs4SetShaderShadowDepth( g_DepthTargetReady, g_DepthTargetReady, 3 );
    KisakPs4SetShaderShadowBlend( false, 1, 0, 0, false, 1, 0, 0 );
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
    g_DrawState.SetVertexShader( g_ReferenceCubeVertexShader->registers, 0 );
    g_DrawState.SetPixelShader( g_ReferenceCubePixelShader->registers );
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 0, g_ReferenceCubeFetchShader );
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, g_ReferenceCubeVertexBuffers );
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 6, cubeVsDescriptor );
    g_DrawState.SetPointerUserData( GNM_STAGE_PS, 0, g_TextureSamplerTable );
    g_DrawState.SetPsInputUsage(
        sceGnmVsShaderExportSemanticTable( g_ReferenceCubeVertexShader ),
        g_ReferenceCubeVertexShader->numexportsemantics,
        sceGnmPsShaderInputSemanticTable( g_ReferenceCubePixelShader ),
        g_ReferenceCubePixelShader->numinputsemantics );
    GnmDbRenderControl referenceDbControl = {};
    GnmDepthStencilControl referenceDepthControl = {};
    referenceDepthControl.zwrite = true;
    referenceDepthControl.zfunc = GNM_DEPTH_COMPARE_LESSEQUAL;
    referenceDepthControl.stencilfunc = GNM_DEPTH_COMPARE_NEVER;
    referenceDepthControl.stencilbackfunc = GNM_DEPTH_COMPARE_NEVER;
    referenceDepthControl.depthenable = true;
    g_DrawState.SetDbRenderControl( referenceDbControl );
    g_DrawState.SetDepthStencilControl( referenceDepthControl );
    g_DrawState.Invalidate( CPs4GnmDrawState::kDirtyDbRender |
        CPs4GnmDrawState::kDirtyDepthStencil );
    if ( !Ps4EmitPrimitiveDraw( command, &g_DrawState, cubePacket ) )
        return;
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, g_CubeEdgeVertexBuffers );
    g_DrawState.SetPointerUserData( GNM_STAGE_PS, 0, g_CubeEdgeSamplerTable );
    Ps4EmitPrimitiveDraw( command, &g_DrawState, edgePacket );
    g_DrawState.SetVertexShader( g_VertexShader->registers, 0 );
    g_DrawState.SetPixelShader( g_PixelShader->registers );
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 0, g_FetchShader );
    g_DrawState.SetPointerUserData( GNM_STAGE_VS, 2, sourceDescriptors );
    // Intentionally clip the right side of the Source triangle. This makes
    // hardware validation of per-draw scissor emission visually unambiguous.
    g_DrawState.SetScissor( 80, 0, 330, 1080 );
    g_DrawState.SetPsInputUsage(
        sceGnmVsShaderExportSemanticTable( g_VertexShader ),
        g_VertexShader->numexportsemantics,
        sceGnmPsShaderInputSemanticTable( g_PixelShader ),
        g_PixelShader->numinputsemantics );
    GnmDepthStencilControl sourceDepth = {};
    sourceDepth.zfunc = GNM_DEPTH_COMPARE_ALWAYS;
    sourceDepth.stencilfunc = GNM_DEPTH_COMPARE_NEVER;
    sourceDepth.stencilbackfunc = GNM_DEPTH_COMPARE_NEVER;
    g_DrawState.SetDepthStencilControl( sourceDepth );
    GnmBlendControl sourceBlend = {};
    sourceBlend.blendenabled = true;
    sourceBlend.colorfunc = GNM_COMB_DST_PLUS_SRC;
    sourceBlend.colorsrcmult = GNM_BLEND_SRC_ALPHA;
    sourceBlend.colordstmult = GNM_BLEND_ONE_MINUS_SRC_ALPHA;
    sourceBlend.alphafunc = GNM_COMB_DST_PLUS_SRC;
    sourceBlend.alphasrcmult = GNM_BLEND_ONE;
    sourceBlend.alphadstmult = GNM_BLEND_ZERO;
    g_DrawState.SetBlendControl( 0, sourceBlend );
    g_DrawState.Invalidate( CPs4GnmDrawState::kDirtyDepthStencil );
    uint32_t sourceDirtyMask = 0;
    Ps4EmitIndexedDraw( command, &g_DrawState, sourcePacket, UINT32_MAX,
        &sourceDirtyMask );
    static bool sourceBlendStateLogged = false;
    if ( !sourceBlendStateLogged )
    {
        char message[128];
        snprintf( message, sizeof( message ),
            "kisak-ps4: Source blend state dirty=0x%08x color_info=0x%08x gpu_mode=%u",
            sourceDirtyMask, renderTarget.info.asuint,
            static_cast< unsigned int >( sceGnmGpuMode() ) );
        KisakPs4StartupBreadcrumb( message );
        sourceBlendStateLogged = true;
    }
}
}

static void LogBlendPm4State( const uint32_t *begin, const uint32_t *end )
{
    static bool logged = false;
    if ( logged || !begin || !end || begin >= end )
        return;

    // SET_CONTEXT_REG stores a dword register offset from 0x28000 followed by
    // one or more values. Scan all candidate packets instead of walking packet
    // lengths so header-only padding packets cannot hide the diagnostic state.
    const uint32_t kSetContextRegOpcode = 0x69;
    const uint32_t kContextRegBase = 0x28000;
    const uint32_t kBlendControl = 0x28780;
    const uint32_t kColorControl = 0x28808;
    const uint32_t kBlendAlpha = 0x28420;
    const uint32_t kShaderMask = 0x2823c;
    const uint32_t kShaderColorFormat = 0x28714;
    uint32_t blendControl = 0;
    uint32_t colorControl = 0;
    uint32_t blendAlpha = 0;
    uint32_t blendWrites = 0;
    uint32_t colorWrites = 0;
    uint32_t alphaWrites = 0;
    uint32_t shaderMask = 0;
    uint32_t shaderColorFormat = 0;

    for ( const uint32_t *word = begin; word + 2 < end; ++word )
    {
        const uint32_t header = word[0];
        if ( ( header >> 30 ) != 3 || ( ( header >> 8 ) & 0xff ) != kSetContextRegOpcode )
            continue;
        const uint32_t valueCount = ( header >> 16 ) & 0x3fff;
        if ( valueCount == 0 || word + 2 + valueCount > end )
            continue;
        const uint32_t firstRegister = kContextRegBase + ( word[1] << 2 );
        for ( uint32_t i = 0; i < valueCount; ++i )
        {
            const uint32_t reg = firstRegister + i * sizeof( uint32_t );
            if ( reg == kBlendControl )
            {
                blendControl = word[2 + i];
                ++blendWrites;
            }
            else if ( reg == kColorControl )
            {
                colorControl = word[2 + i];
                ++colorWrites;
            }
            else if ( reg == kBlendAlpha )
            {
                blendAlpha = word[2 + i];
                ++alphaWrites;
            }
            else if ( reg == kShaderMask )
                shaderMask = word[2 + i];
            else if ( reg == kShaderColorFormat )
                shaderColorFormat = word[2 + i];
        }
    }

    char message[192];
    snprintf( message, sizeof( message ),
        "kisak-ps4: PM4 blend=0x%08x writes=%u color=0x%08x writes=%u alpha=0x%08x writes=%u",
        blendControl, blendWrites, colorControl, colorWrites, blendAlpha, alphaWrites );
    KisakPs4StartupBreadcrumb( message );
    snprintf( message, sizeof( message ),
        "kisak-ps4: PM4 source shader_mask=0x%08x color_format=0x%08x",
        shaderMask, shaderColorFormat );
    KisakPs4StartupBreadcrumb( message );
    logged = true;
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

    bool passed = g_Device.Initialize(
        static_cast< uint8_t * >( g_Mapped ) + kPersistentMemorySize,
        kDirectMemorySize - kPersistentMemorySize );
    g_ShadersReady = passed && LoadDiagnosticShaders();
    {
        char message[208];
        snprintf( message, sizeof( message ), "kisak-ps4: diagnostic shader detail %s",
            g_ShaderDiagnostic );
        KisakPs4StartupBreadcrumb( message );
    }
    KisakPs4StartupBreadcrumb( g_ShadersReady
        ? "kisak-ps4: diagnostic shader binaries loaded"
        : "kisak-ps4: diagnostic shader load failed; DMA bars retained" );
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

extern "C" void KisakPs4SetSourceFrameCallback(
    void ( *callback )( void * ), void *context )
{
    g_SourceFrameCallback = callback;
    g_SourceFrameContext = context;
}

extern "C" bool KisakPs4GnmColorBarsAndWait( void *destination, uint32_t size )
{
    if ( !g_Mapped || !destination || size < 16 || ( size & 15 ) )
        return false;

    CPs4GnmDevice::SubmissionFrame submission = {};
    if ( !g_Device.BeginSubmission( g_CompletedLabel, kCommandBufferSize, 256, &submission ) )
        return false;
    if ( g_SourceFrameCallback )
    {
        g_SourceFrameCallback( g_SourceFrameContext );
        if ( !g_SourceFrameCallbackLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: Source frame callback ran inside GPU frame" );
            g_SourceFrameCallbackLogged = true;
        }
    }
    static bool dynamicBufferProbePassed = false;
    if ( !dynamicBufferProbePassed )
    {
        if ( !KisakPs4ShaderDeviceDynamicBufferProbe() )
        {
            g_Device.CancelFrame();
            return false;
        }
        KisakPs4StartupBreadcrumb(
            "kisak-ps4: shader device dynamic buffers passed" );
        dynamicBufferProbePassed = true;
    }
    static bool dynamicMeshBridgeProbePassed = false;
    if ( !dynamicMeshBridgeProbePassed )
    {
        if ( !KisakPs4DynamicMeshBridgeProbe() )
        {
            g_Device.CancelFrame();
            return false;
        }
        KisakPs4StartupBreadcrumb(
            "kisak-ps4: ShaderAPI dynamic mesh bridge passed" );
        dynamicMeshBridgeProbePassed = true;
    }

    const uint32_t bandSize = size / 4;
    const uint32_t colors[4] = {
        0xff0000ff, // red in little-endian A8B8G8R8 memory
        0xff00ff00, // green
        0xffff0000, // blue
        0xffffffff  // white
    };
    GnmCommandBuffer command = sceGnmCmdInit( submission.commandMemory, kCommandBufferSize, 0, 0 );
    // Retired frames can leave dirty CB lines for this reused VideoOut address.
    // Flush them before DMA_DATA writes the new bars, otherwise a later CB
    // acquire can write the previous triangle back over the fresh destination.
    sceGnmDrawCmdWaitGraphicsWrite( &command, GNM_ACQUIRE_TARGET_CB0 );
    for ( unsigned int band = 0; band < 4; ++band )
    {
        const uintptr_t address = reinterpret_cast< uintptr_t >( destination ) + band * bandSize;
        if ( !sceGnmDrawCmdFillMemory( &command, address, bandSize, colors[band] ) )
        {
            g_Device.CancelFrame();
            return false;
        }
    }
    if ( g_DepthTargetReady && !ClearDiagnosticDepth( &command ) )
    {
        g_Device.CancelFrame();
        return false;
    }
    if ( g_ShadersReady )
    {
        if ( !UpdateReferenceCubeMvp() )
        {
            g_Device.CancelFrame();
            return false;
        }
        GnmBuffer *cubeVsDescriptor = static_cast< GnmBuffer * >(
            g_Device.FrameArena().Allocate( sizeof( GnmBuffer ), 16 ) );
        if ( !cubeVsDescriptor || !g_ReferenceCubeConstants.BuildBuffer(
            CPs4GnmConstants::kVertex, &g_Device.FrameArena(), cubeVsDescriptor ) )
        {
            g_Device.CancelFrame();
            return false;
        }
        uint16_t *indices = static_cast< uint16_t * >(
            g_Device.FrameArena().Allocate( kDiagnosticIndexCount * sizeof( uint16_t ), 8 ) );
        if ( !indices )
        {
            g_Device.CancelFrame();
            return false;
        }
        for ( uint32_t face = 0; face < 6; ++face )
        {
            const uint16_t base = static_cast< uint16_t >( face * 4 );
            const uint32_t first = face * 6;
            indices[first + 0] = base + 0;
            indices[first + 1] = base + 1;
            indices[first + 2] = base + 2;
            indices[first + 3] = base + 0;
            indices[first + 4] = base + 2;
            indices[first + 5] = base + 3;
        }
        CPs4GnmBuffer indexBuffer;
        if ( !indexBuffer.Initialize( indices,
            kDiagnosticIndexCount * sizeof( uint16_t ),
            CPs4GnmBuffer::kIndexBuffer, true ) || !g_Device.BeginScene() )
        {
            g_Device.CancelFrame();
            return false;
        }
        if ( !g_Device.SetStreamSource( 0, &g_DiagnosticVertexBuffer, 0,
                8 * sizeof( float ) ) || !g_Device.SetIndices( &indexBuffer ) )
        {
            g_Device.EndScene();
            g_Device.CancelFrame();
            return false;
        }
        g_Device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
        CPs4GnmDevice::IndexedDrawPacket packet = {};
        CPs4GnmDevice::IndexedDrawPacket sourcePacket = {};
        CPs4GnmDevice::PrimitiveDrawPacket cubePacket = {};
        CPs4GnmDevice::PrimitiveDrawPacket edgePacket = {};
        GnmBuffer *sourceDescriptors = static_cast< GnmBuffer * >(
            g_Device.FrameArena().Allocate( 2 * sizeof( GnmBuffer ), 16 ) );
        if ( !g_Device.BuildIndexedDrawPacket( GNM_FMT_R32G32B32A32_FLOAT,
                0, kDiagnosticIndexCount, 0, kDiagnosticVertexCount, &packet ) ||
            !g_Device.SetStreamSource( 0, &g_ReferenceCubeVertexBuffer, 0,
                8 * sizeof( float ) ) ||
            !g_Device.BuildVertexDescriptorTable( g_ReferenceCubeVertexDeclaration,
                0, 36, g_ReferenceCubeVertexBuffers, 2 ) ||
            !g_Device.BuildPrimitiveDrawPacket( 0, 12, &cubePacket ) )
        {
            g_Device.EndScene();
            g_Device.CancelFrame();
            return false;
        }
        g_Device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveLines );
        if ( !g_Device.SetStreamSource( 0, &g_CubeEdgeVertexBuffer, 0,
                8 * sizeof( float ) ) ||
            !g_Device.BuildVertexDescriptorTable( g_ReferenceCubeVertexDeclaration,
                0, 24, g_CubeEdgeVertexBuffers, 2 ) ||
            !g_Device.BuildPrimitiveDrawPacket( 0, 12, &edgePacket ) )
        {
            g_Device.EndScene();
            g_Device.CancelFrame();
            return false;
        }
        g_Device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
        if ( !sourceDescriptors ||
            !BuildSourceDynamicTriangle( &sourcePacket, sourceDescriptors ) )
        {
            g_Device.EndScene();
            g_Device.CancelFrame();
            return false;
        }
        sceGnmDrawCmdWaitGraphicsWrite( &command, GNM_ACQUIRE_TARGET_CB0 );
        EmitDiagnosticTriangle( &command, destination, packet, sourcePacket,
            sourceDescriptors, cubePacket, edgePacket, cubeVsDescriptor );
        if ( !g_Device.EndScene() )
        {
            g_Device.CancelFrame();
            return false;
        }
        LogBlendPm4State( command.beginptr, command.cmdptr );
        if ( !g_FacadeDrawLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: D3D9 facade indexed diagnostic draw emitted" );
            g_FacadeDrawLogged = true;
        }
        static bool sourceDynamicDrawLogged = false;
        if ( !sourceDynamicDrawLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: IMesh Draw command emitted" );
            sourceDynamicDrawLogged = true;
        }
        static bool sourceScissorLogged = false;
        if ( !sourceScissorLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: Source dynamic mesh scissor emitted" );
            sourceScissorLogged = true;
        }
        static bool sourceBlendLogged = false;
        if ( !sourceBlendLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: Source dynamic mesh alpha blend emitted" );
            sourceBlendLogged = true;
        }
        static bool threeDimensionalDrawLogged = false;
        if ( !threeDimensionalDrawLogged )
        {
            KisakPs4StartupBreadcrumb(
                "kisak-ps4: reference cube pipeline diagnostic emitted" );
            threeDimensionalDrawLogged = true;
        }
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
        const volatile uint32_t *offscreenPixels =
            static_cast< const volatile uint32_t * >( g_DiagnosticTexture.Data() );
        snprintf( message, sizeof( message ),
            "kisak-ps4: offscreen blend pixels %08x %08x %08x %08x",
            offscreenPixels[5], offscreenPixels[6],
            offscreenPixels[9], offscreenPixels[10] );
        KisakPs4StartupBreadcrumb( message );
        g_TriangleReadbackLogged = true;
    }
    static bool sourceBlendReadbackLogged = false;
    if ( g_ShadersReady && !sourceBlendReadbackLogged )
    {
        const volatile uint32_t *pixels =
            static_cast< const volatile uint32_t * >( destination );
        char message[128];
        snprintf( message, sizeof( message ),
            "kisak-ps4: Source blend pixels blue=0x%08x white=0x%08x",
            pixels[570 * 1920 + 200], pixels[650 * 1920 + 200] );
        KisakPs4StartupBreadcrumb( message );
        sourceBlendReadbackLogged = true;
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
