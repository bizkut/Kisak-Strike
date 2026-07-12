#include "interface.h"
#include "shaderapi/IShaderDevice.h"
#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/idebugtextureinfo.h"
#include "interfaces/interfaces.h"
#include "tier1/keyvalues.h"
#include "ps4_gnm_draw_state.h"
#include "ps4_gnm_texture.h"
#include "ps4_gnm_runtime.h"
#include "ps4_source_buffers.h"
#include "ps4_shadow_state_translate.h"
#include "shaderapips4.h"

#include <string.h>
#include <limits.h>

extern CreateInterfaceFn KisakShaderApiEmptyFactory();

namespace
{
CreateInterfaceFn g_EmptyFactory = 0;
void *Ps4CreateInterface( const char *interfaceName, int *returnCode );
IVertexBuffer *g_LockedDynamicVertexBuffer = 0;
IIndexBuffer *g_LockedDynamicIndexBuffer = 0;
int g_QueuedDynamicFirstIndex = 0;
int g_QueuedDynamicIndexCount = 0;
bool g_DynamicMeshDrawQueued = false;

class CShaderDeviceMgrPs4 : public IShaderDeviceMgr
{
public:
    CShaderDeviceMgrPs4() : m_delegate( 0 ) {}
    void SetDelegate( IShaderDeviceMgr *delegate ) { m_delegate = delegate; }

    bool Connect( CreateInterfaceFn factory ) { return m_delegate && m_delegate->Connect( factory ); }
    void Disconnect() { if ( m_delegate ) m_delegate->Disconnect(); }
    void *QueryInterface( const char *name )
    {
        if ( name && strcmp( name, SHADER_DEVICE_MGR_INTERFACE_VERSION ) == 0 )
            return this;
        return m_delegate ? m_delegate->QueryInterface( name ) : 0;
    }
    InitReturnVal_t Init() { return m_delegate ? m_delegate->Init() : INIT_FAILED; }
    void Shutdown() { if ( m_delegate ) m_delegate->Shutdown(); }

    int GetAdapterCount() const { return 1; }
    void GetAdapterInfo( int adapter, MaterialAdapterInfo_t &info ) const
    {
        memset( &info, 0, sizeof( info ) );
        if ( adapter != 0 )
            return;
        strncpy( info.m_pDriverName, "OpenGNM Liverpool",
            sizeof( info.m_pDriverName ) - 1 );
        info.m_VendorID = 0x1002;
        info.m_nDXSupportLevel = 95;
        info.m_nMinDXSupportLevel = 90;
        info.m_nMaxDXSupportLevel = 95;
    }
    bool GetRecommendedConfigurationInfo( int adapter, int dxLevel, KeyValues *configuration )
    {
        if ( adapter != 0 || !configuration || dxLevel < 90 || dxLevel > 95 )
            return false;
        configuration->SetInt( "setting.dxlevel", dxLevel );
        configuration->SetInt( "setting.defaultres", 1920 );
        configuration->SetInt( "setting.defaultresheight", 1080 );
        configuration->SetInt( "setting.fullscreen", 1 );
        configuration->SetInt( "setting.nowindowborder", 0 );
        configuration->SetInt( "setting.mat_vsync", 1 );
        configuration->SetInt( "setting.mat_triplebuffered", 0 );
        configuration->SetInt( "setting.mat_antialias", 0 );
        configuration->SetInt( "setting.gpu_mem_level", 2 );
        return true;
    }
    int GetModeCount( int adapter ) const { return adapter == 0 ? 1 : 0; }
    void GetModeInfo( ShaderDisplayMode_t *info, int adapter, int mode ) const
    {
        if ( !info )
            return;
        *info = ShaderDisplayMode_t();
        if ( adapter != 0 || mode != 0 )
            return;
        info->m_nWidth = 1920;
        info->m_nHeight = 1080;
        info->m_Format = IMAGE_FORMAT_BGRA8888;
        info->m_nRefreshRateNumerator = 60;
        info->m_nRefreshRateDenominator = 1;
    }
    void GetCurrentModeInfo( ShaderDisplayMode_t *info, int adapter ) const
    { GetModeInfo( info, adapter, 0 ); }
    bool SetAdapter( int adapter, int flags )
    {
        if ( adapter != 0 )
            return false;
        if ( m_delegate )
            m_delegate->SetAdapter( adapter, flags );
        return true;
    }
    CreateInterfaceFn SetMode( void *window, int adapter, const ShaderDeviceInfo_t &mode )
    {
        return m_delegate && m_delegate->SetMode( window, adapter, mode )
            ? Ps4CreateInterface : 0;
    }
    void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t callback )
    { if ( m_delegate ) m_delegate->AddModeChangeCallback( callback ); }
    void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t callback )
    { if ( m_delegate ) m_delegate->RemoveModeChangeCallback( callback ); }
    bool GetRecommendedVideoConfig( int adapter, KeyValues *configuration )
    { return GetRecommendedConfigurationInfo( adapter, 95, configuration ); }
    void AddDeviceDependentObject( IShaderDeviceDependentObject *object )
    { if ( m_delegate ) m_delegate->AddDeviceDependentObject( object ); }
    void RemoveDeviceDependentObject( IShaderDeviceDependentObject *object )
    { if ( m_delegate ) m_delegate->RemoveDeviceDependentObject( object ); }

private:
    IShaderDeviceMgr *m_delegate;
};

CShaderDeviceMgrPs4 g_DeviceMgrPs4;

class CShaderDevicePs4 : public IShaderDevice
{
public:
    CShaderDevicePs4() : m_delegate( 0 )
    {
        memset( m_nativeVertexBuffers, 0, sizeof( m_nativeVertexBuffers ) );
        memset( m_nativeIndexBuffers, 0, sizeof( m_nativeIndexBuffers ) );
        memset( m_dynamicVertexBuffers, 0, sizeof( m_dynamicVertexBuffers ) );
        memset( m_dynamicVertexFormats, 0, sizeof( m_dynamicVertexFormats ) );
        m_dynamicIndexBuffer = 0;
    }
    void SetDelegate( IShaderDevice *delegate ) { m_delegate = delegate; }

    void ReleaseResources( bool releaseManagedResources = true )
    { if ( m_delegate ) m_delegate->ReleaseResources( releaseManagedResources ); }
    void ReacquireResources() { if ( m_delegate ) m_delegate->ReacquireResources(); }
    ImageFormat GetBackBufferFormat() const { return IMAGE_FORMAT_BGRA8888; }
    void GetBackBufferDimensions( int &width, int &height ) const
    { width = 1920; height = 1080; }
    const AspectRatioInfo_t &GetAspectRatioInfo() const
    {
        static AspectRatioInfo_t empty;
        return m_delegate ? m_delegate->GetAspectRatioInfo() : empty;
    }
    int GetCurrentAdapter() const { return m_delegate ? m_delegate->GetCurrentAdapter() : 0; }
    bool IsUsingGraphics() const { return true; }
    void SpewDriverInfo() const { if ( m_delegate ) m_delegate->SpewDriverInfo(); }
    int StencilBufferBits() const { return 0; }
    bool IsAAEnabled() const { return m_delegate && m_delegate->IsAAEnabled(); }
    void Present() { if ( m_delegate ) m_delegate->Present(); }
    void GetWindowSize( int &width, int &height ) const
    { width = 1920; height = 1080; }
    void SetHardwareGammaRamp( float gamma, float tvMin, float tvMax, float tvExponent, bool tvEnabled )
    { if ( m_delegate ) m_delegate->SetHardwareGammaRamp( gamma, tvMin, tvMax, tvExponent, tvEnabled ); }
    bool AddView( void *window ) { return m_delegate && m_delegate->AddView( window ); }
    void RemoveView( void *window ) { if ( m_delegate ) m_delegate->RemoveView( window ); }
    void SetView( void *window ) { if ( m_delegate ) m_delegate->SetView( window ); }
    IShaderBuffer *CompileShader( const char *program, size_t length, const char *version )
    { return m_delegate ? m_delegate->CompileShader( program, length, version ) : 0; }
    VertexShaderHandle_t CreateVertexShader( IShaderBuffer *buffer )
    { return m_delegate ? m_delegate->CreateVertexShader( buffer ) : VERTEX_SHADER_HANDLE_INVALID; }
    void DestroyVertexShader( VertexShaderHandle_t shader )
    { if ( m_delegate ) m_delegate->DestroyVertexShader( shader ); }
    GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer *buffer )
    { return m_delegate ? m_delegate->CreateGeometryShader( buffer ) : GEOMETRY_SHADER_HANDLE_INVALID; }
    void DestroyGeometryShader( GeometryShaderHandle_t shader )
    { if ( m_delegate ) m_delegate->DestroyGeometryShader( shader ); }
    PixelShaderHandle_t CreatePixelShader( IShaderBuffer *buffer )
    { return m_delegate ? m_delegate->CreatePixelShader( buffer ) : PIXEL_SHADER_HANDLE_INVALID; }
    void DestroyPixelShader( PixelShaderHandle_t shader )
    { if ( m_delegate ) m_delegate->DestroyPixelShader( shader ); }
    IMesh *CreateStaticMesh( VertexFormat_t format, const char *budgetGroup, IMaterial *material = 0,
                            VertexStreamSpec_t *streamSpec = 0 )
    { return m_delegate ? m_delegate->CreateStaticMesh( format, budgetGroup, material, streamSpec ) : 0; }
    void DestroyStaticMesh( IMesh *mesh ) { if ( m_delegate ) m_delegate->DestroyStaticMesh( mesh ); }
    IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t format, int count,
                                      const char *budgetGroup )
    {
        if ( KisakPs4GnmRuntime().IsReady() )
        {
            CPs4SourceVertexBuffer *buffer = new CPs4SourceVertexBuffer;
            if ( buffer && buffer->Initialize( type, format, count ) &&
                AddNativePointer( m_nativeVertexBuffers, buffer ) )
                return buffer;
            delete buffer;
        }
        return m_delegate ? m_delegate->CreateVertexBuffer(
            type, format, count, budgetGroup ) : 0;
    }
    void DestroyVertexBuffer( IVertexBuffer *buffer )
    {
        if ( RemoveNativePointer( m_nativeVertexBuffers, buffer ) )
            delete static_cast< CPs4SourceVertexBuffer * >( buffer );
        else if ( m_delegate )
            m_delegate->DestroyVertexBuffer( buffer );
    }
    IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t type, MaterialIndexFormat_t format, int count,
                                    const char *budgetGroup )
    {
        if ( KisakPs4GnmRuntime().IsReady() )
        {
            CPs4SourceIndexBuffer *buffer = new CPs4SourceIndexBuffer;
            if ( buffer && buffer->Initialize( type, format, count ) &&
                AddNativePointer( m_nativeIndexBuffers, buffer ) )
                return buffer;
            delete buffer;
        }
        return m_delegate ? m_delegate->CreateIndexBuffer(
            type, format, count, budgetGroup ) : 0;
    }
    void DestroyIndexBuffer( IIndexBuffer *buffer )
    {
        if ( RemoveNativePointer( m_nativeIndexBuffers, buffer ) )
            delete static_cast< CPs4SourceIndexBuffer * >( buffer );
        else if ( m_delegate )
            m_delegate->DestroyIndexBuffer( buffer );
    }
    IVertexBuffer *GetDynamicVertexBuffer( int stream, VertexFormat_t format, bool buffered = true )
    {
        if ( !KisakPs4GnmRuntime().IsReady() || stream < 0 || stream >= 8 )
            return m_delegate ? m_delegate->GetDynamicVertexBuffer(
                stream, format, buffered ) : 0;
        if ( m_dynamicVertexBuffers[stream] &&
            m_dynamicVertexFormats[stream] != format )
        {
            delete m_dynamicVertexBuffers[stream];
            m_dynamicVertexBuffers[stream] = 0;
        }
        if ( !m_dynamicVertexBuffers[stream] )
        {
            CPs4SourceVertexBuffer *buffer = new CPs4SourceVertexBuffer;
            if ( !buffer || !buffer->Initialize(
                    SHADER_BUFFER_TYPE_DYNAMIC, format, 16384 ) )
            {
                delete buffer;
                return 0;
            }
            m_dynamicVertexBuffers[stream] = buffer;
            m_dynamicVertexFormats[stream] = format;
        }
        return m_dynamicVertexBuffers[stream];
    }
    IIndexBuffer *GetDynamicIndexBuffer()
    {
        if ( !KisakPs4GnmRuntime().IsReady() )
            return m_delegate ? m_delegate->GetDynamicIndexBuffer() : 0;
        if ( !m_dynamicIndexBuffer )
        {
            CPs4SourceIndexBuffer *buffer = new CPs4SourceIndexBuffer;
            if ( !buffer || !buffer->Initialize( SHADER_BUFFER_TYPE_DYNAMIC,
                    MATERIAL_INDEX_FORMAT_16BIT, 131072 ) )
            {
                delete buffer;
                return 0;
            }
            m_dynamicIndexBuffer = buffer;
        }
        return m_dynamicIndexBuffer;
    }
    void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *info = 0 )
    { if ( m_delegate ) m_delegate->EnableNonInteractiveMode( mode, info ); }
    void RefreshFrontBufferNonInteractive()
    { if ( m_delegate ) m_delegate->RefreshFrontBufferNonInteractive(); }
    void HandleThreadEvent( uint32 event )
    { if ( m_delegate ) m_delegate->HandleThreadEvent( event ); }

private:
    template < typename T, size_t N >
    static bool AddNativePointer( T *( &pointers )[N], T *pointer )
    {
        for ( size_t i = 0; i < N; ++i )
            if ( !pointers[i] )
            {
                pointers[i] = pointer;
                return true;
            }
        return false;
    }

    template < typename T, size_t N, typename U >
    static bool RemoveNativePointer( T *( &pointers )[N], U *pointer )
    {
        for ( size_t i = 0; i < N; ++i )
            if ( pointers[i] == pointer )
            {
                pointers[i] = 0;
                return true;
            }
        return false;
    }

    IShaderDevice *m_delegate;
    CPs4SourceVertexBuffer *m_nativeVertexBuffers[256];
    CPs4SourceIndexBuffer *m_nativeIndexBuffers[256];
    CPs4SourceVertexBuffer *m_dynamicVertexBuffers[8];
    VertexFormat_t m_dynamicVertexFormats[8];
    CPs4SourceIndexBuffer *m_dynamicIndexBuffer;
};

CShaderDevicePs4 g_DevicePs4;

class CPs4SourceShadowState
{
public:
    enum DirtyBits
    {
        DIRTY_DEPTH = 1 << 0,
        DIRTY_COLOR = 1 << 1,
        DIRTY_BLEND = 1 << 2,
        DIRTY_RASTER = 1 << 3,
        DIRTY_TEXTURE = 1 << 4,
        DIRTY_SHADER = 1 << 5,
        DIRTY_ALL = ( 1 << 6 ) - 1
    };

    CPs4SourceShadowState() { Reset(); }

    void Reset()
    {
        depthFunction = SHADER_DEPTHFUNC_NEAREROREQUAL;
        depthWrites = true;
        depthTest = true;
        polygonOffset = SHADER_POLYOFFSET_DISABLE;
        colorWrites = true;
        alphaWrites = true;
        blending = false;
        forceOpaque = false;
        sourceBlend = SHADER_BLEND_ONE;
        destinationBlend = SHADER_BLEND_ZERO;
        separateAlphaBlend = false;
        sourceAlphaBlend = SHADER_BLEND_ONE;
        destinationAlphaBlend = SHADER_BLEND_ZERO;
        blendOperation = SHADER_BLEND_OP_ADD;
        alphaBlendOperation = SHADER_BLEND_OP_ADD;
        alphaTest = false;
        alphaFunction = SHADER_ALPHAFUNC_ALWAYS;
        alphaReference = 0.0f;
        culling = true;
        sRgbWrite = false;
        textureEnableMask = 0;
        sRgbReadMask = 0;
        vertexTextureEnableMask = 0;
        vertexFormatFlags = 0;
        textureCoordinateCount = 0;
        vertexUserDataSize = 0;
        vertexShaderIndex = 0;
        pixelShaderIndex = 0;
        dirty = DIRTY_ALL;
    }

    ShaderDepthFunc_t depthFunction;
    bool depthWrites;
    bool depthTest;
    PolygonOffsetMode_t polygonOffset;
    bool colorWrites;
    bool alphaWrites;
    bool blending;
    bool forceOpaque;
    ShaderBlendFactor_t sourceBlend;
    ShaderBlendFactor_t destinationBlend;
    bool separateAlphaBlend;
    ShaderBlendFactor_t sourceAlphaBlend;
    ShaderBlendFactor_t destinationAlphaBlend;
    ShaderBlendOp_t blendOperation;
    ShaderBlendOp_t alphaBlendOperation;
    bool alphaTest;
    ShaderAlphaFunc_t alphaFunction;
    float alphaReference;
    bool culling;
    bool sRgbWrite;
    uint32 textureEnableMask;
    uint32 sRgbReadMask;
    uint32 vertexTextureEnableMask;
    unsigned int vertexFormatFlags;
    int textureCoordinateCount;
    int vertexUserDataSize;
    int vertexShaderIndex;
    int pixelShaderIndex;
    uint32 dirty;
};

class CShaderShadowPs4 : public IShaderShadow
{
public:
    typedef CPs4SourceShadowState CPs4GnmDrawState;

    CShaderShadowPs4() : m_delegate( 0 )
    {
        SyncNativeState();
        m_nativeState.RetainDirtyMask(
            ::CPs4GnmDrawState::kDirtyDepthStencil |
            ::CPs4GnmDrawState::kDirtyBlend |
            ::CPs4GnmDrawState::kDirtyRenderTargetMask |
            ::CPs4GnmDrawState::kDirtyPrimitive );
    }
    void SetDelegate( IShaderShadow *delegate ) { m_delegate = delegate; }
    ::CPs4GnmDrawState &NativeDrawState() { return m_nativeState; }

    void SetDefaultState() { m_state.Reset(); SyncNativeState(); if ( m_delegate ) m_delegate->SetDefaultState(); }
    void DepthFunc( ShaderDepthFunc_t function )
    { m_state.depthFunction = function; m_state.dirty |= CPs4GnmDrawState::DIRTY_DEPTH; SyncDepthState(); if ( m_delegate ) m_delegate->DepthFunc( function ); }
    void EnableDepthWrites( bool enable )
    { m_state.depthWrites = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_DEPTH; SyncDepthState(); if ( m_delegate ) m_delegate->EnableDepthWrites( enable ); }
    void EnableDepthTest( bool enable )
    { m_state.depthTest = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_DEPTH; SyncDepthState(); if ( m_delegate ) m_delegate->EnableDepthTest( enable ); }
    void EnablePolyOffset( PolygonOffsetMode_t mode )
    { m_state.polygonOffset = mode; m_state.dirty |= CPs4GnmDrawState::DIRTY_RASTER; SyncRasterState(); if ( m_delegate ) m_delegate->EnablePolyOffset( mode ); }
    void EnableColorWrites( bool enable )
    { m_state.colorWrites = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_COLOR; SyncColorState(); if ( m_delegate ) m_delegate->EnableColorWrites( enable ); }
    void EnableAlphaWrites( bool enable )
    { m_state.alphaWrites = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_COLOR; SyncColorState(); if ( m_delegate ) m_delegate->EnableAlphaWrites( enable ); }
    void EnableBlending( bool enable )
    { m_state.blending = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->EnableBlending( enable ); }
    void EnableBlendingForceOpaque( bool enable )
    { m_state.forceOpaque = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->EnableBlendingForceOpaque( enable ); }
    void BlendFunc( ShaderBlendFactor_t source, ShaderBlendFactor_t destination )
    { m_state.sourceBlend = source; m_state.destinationBlend = destination; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->BlendFunc( source, destination ); }
    void EnableBlendingSeparateAlpha( bool enable )
    { m_state.separateAlphaBlend = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->EnableBlendingSeparateAlpha( enable ); }
    void BlendFuncSeparateAlpha( ShaderBlendFactor_t source, ShaderBlendFactor_t destination )
    { m_state.sourceAlphaBlend = source; m_state.destinationAlphaBlend = destination; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->BlendFuncSeparateAlpha( source, destination ); }
    void EnableAlphaTest( bool enable )
    { m_state.alphaTest = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; if ( m_delegate ) m_delegate->EnableAlphaTest( enable ); }
    void AlphaFunc( ShaderAlphaFunc_t function, float reference )
    { m_state.alphaFunction = function; m_state.alphaReference = reference; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; if ( m_delegate ) m_delegate->AlphaFunc( function, reference ); }
    void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t mode )
    { if ( m_delegate ) m_delegate->PolyMode( face, mode ); }
    void EnableCulling( bool enable )
    { m_state.culling = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_RASTER; SyncRasterState(); if ( m_delegate ) m_delegate->EnableCulling( enable ); }
    void VertexShaderVertexFormat( unsigned int flags, int texCoordCount, int *texCoordDimensions,
                                   int userDataSize )
    { m_state.vertexFormatFlags = flags; m_state.textureCoordinateCount = texCoordCount; m_state.vertexUserDataSize = userDataSize; m_state.dirty |= CPs4GnmDrawState::DIRTY_SHADER; if ( m_delegate ) m_delegate->VertexShaderVertexFormat( flags, texCoordCount, texCoordDimensions, userDataSize ); }
    void SetVertexShader( const char *fileName, int staticIndex )
    { m_state.vertexShaderIndex = staticIndex; m_state.dirty |= CPs4GnmDrawState::DIRTY_SHADER; if ( m_delegate ) m_delegate->SetVertexShader( fileName, staticIndex ); }
    void SetPixelShader( const char *fileName, int staticIndex = 0 )
    { m_state.pixelShaderIndex = staticIndex; m_state.dirty |= CPs4GnmDrawState::DIRTY_SHADER; if ( m_delegate ) m_delegate->SetPixelShader( fileName, staticIndex ); }
    void EnableSRGBWrite( bool enable )
    { m_state.sRgbWrite = enable; m_state.dirty |= CPs4GnmDrawState::DIRTY_COLOR; if ( m_delegate ) m_delegate->EnableSRGBWrite( enable ); }
    void EnableSRGBRead( Sampler_t sampler, bool enable )
    { SetMaskBit( m_state.sRgbReadMask, static_cast< int >( sampler ), enable ); m_state.dirty |= CPs4GnmDrawState::DIRTY_TEXTURE; if ( m_delegate ) m_delegate->EnableSRGBRead( sampler, enable ); }
    void EnableTexture( Sampler_t sampler, bool enable )
    { SetMaskBit( m_state.textureEnableMask, static_cast< int >( sampler ), enable ); m_state.dirty |= CPs4GnmDrawState::DIRTY_TEXTURE; if ( m_delegate ) m_delegate->EnableTexture( sampler, enable ); }
    void FogMode( ShaderFogMode_t mode, bool vertexFog )
    { if ( m_delegate ) m_delegate->FogMode( mode, vertexFog ); }
    void DisableFogGammaCorrection( bool disable )
    { if ( m_delegate ) m_delegate->DisableFogGammaCorrection( disable ); }
    void EnableAlphaToCoverage( bool enable )
    { if ( m_delegate ) m_delegate->EnableAlphaToCoverage( enable ); }
    void EnableVertexTexture( VertexTextureSampler_t sampler, bool enable )
    { SetMaskBit( m_state.vertexTextureEnableMask, static_cast< int >( sampler ), enable ); m_state.dirty |= CPs4GnmDrawState::DIRTY_TEXTURE; if ( m_delegate ) m_delegate->EnableVertexTexture( sampler, enable ); }
    void BlendOp( ShaderBlendOp_t operation )
    { m_state.blendOperation = operation; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->BlendOp( operation ); }
    void BlendOpSeparateAlpha( ShaderBlendOp_t operation )
    { m_state.alphaBlendOperation = operation; m_state.dirty |= CPs4GnmDrawState::DIRTY_BLEND; SyncBlendState(); if ( m_delegate ) m_delegate->BlendOpSeparateAlpha( operation ); }
    float GetLightMapScaleFactor() const
    { return m_delegate ? m_delegate->GetLightMapScaleFactor() : 1.0f; }

private:
    void SyncDepthState()
    {
        m_nativeState.SetDepthStencilControl( Ps4BuildDepthStencilControl(
            m_state.depthTest, m_state.depthWrites, m_state.depthFunction ) );
    }
    void SyncBlendState()
    {
        m_nativeState.SetBlendControl( 0, Ps4BuildBlendControl(
            m_state.blending,
            m_state.sourceBlend, m_state.destinationBlend, m_state.blendOperation,
            m_state.separateAlphaBlend, m_state.sourceAlphaBlend,
            m_state.destinationAlphaBlend, m_state.alphaBlendOperation ) );
    }
    void SyncColorState()
    {
        m_nativeState.SetRenderTargetMask( Ps4BuildRenderTargetMask(
            m_state.colorWrites, m_state.alphaWrites ) );
    }
    void SyncRasterState()
    {
        m_nativeState.SetPrimitiveSetup( Ps4BuildPrimitiveSetup(
            m_state.culling, m_state.polygonOffset ) );
    }
    void SyncNativeState()
    {
        SyncDepthState();
        SyncBlendState();
        SyncColorState();
        SyncRasterState();
    }

    static void SetMaskBit( uint32 &mask, int index, bool enable )
    {
        if ( index < 0 || index >= 32 )
            return;
        const uint32 bit = 1u << index;
        mask = enable ? mask | bit : mask & ~bit;
    }

    IShaderShadow *m_delegate;
    CPs4SourceShadowState m_state;
    ::CPs4GnmDrawState m_nativeState;
};

CShaderShadowPs4 g_ShaderShadowPs4;

class CDebugTextureInfoPs4 : public IDebugTextureInfo
{
public:
    CDebugTextureInfoPs4() : m_delegate( 0 ) {}
    void SetDelegate( IDebugTextureInfo *delegate ) { m_delegate = delegate; }

    void EnableDebugTextureList( bool enable )
    { if ( m_delegate ) m_delegate->EnableDebugTextureList( enable ); }
    void EnableGetAllTextures( bool enable )
    { if ( m_delegate ) m_delegate->EnableGetAllTextures( enable ); }
    KeyValues *LockDebugTextureList()
    { return m_delegate ? m_delegate->LockDebugTextureList() : 0; }
    void UnlockDebugTextureList()
    { if ( m_delegate ) m_delegate->UnlockDebugTextureList(); }
    int GetTextureMemoryUsed( TextureMemoryType memoryType )
    {
        if ( memoryType == MEMORY_TOTAL_LOADED )
        {
            const uint64_t bytes = CPs4GnmTexture::TotalBackingBytes();
            return bytes > static_cast< uint64_t >( INT_MAX )
                ? INT_MAX : static_cast< int >( bytes );
        }
        return m_delegate ? m_delegate->GetTextureMemoryUsed( memoryType ) : 0;
    }
    bool IsDebugTextureListFresh( int framesAllowed = 1 )
    { return m_delegate && m_delegate->IsDebugTextureListFresh( framesAllowed ); }
    bool SetDebugTextureRendering( bool enable )
    { return m_delegate && m_delegate->SetDebugTextureRendering( enable ); }

private:
    IDebugTextureInfo *m_delegate;
};

CDebugTextureInfoPs4 g_DebugTextureInfoPs4;

void *Ps4CreateInterface( const char *interfaceName, int *returnCode )
{
    if ( !g_EmptyFactory )
    {
        if ( returnCode )
            *returnCode = 1;
        return 0;
    }
    if ( interfaceName && strcmp( interfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) == 0 )
    {
        IShaderDeviceMgr *delegate = static_cast< IShaderDeviceMgr * >(
            g_EmptyFactory( interfaceName, returnCode ) );
        if ( !delegate )
            return 0;
        g_DeviceMgrPs4.SetDelegate( delegate );
        if ( returnCode )
            *returnCode = 0;
        return &g_DeviceMgrPs4;
    }
    if ( interfaceName && strcmp( interfaceName, SHADER_DEVICE_INTERFACE_VERSION ) == 0 )
    {
        IShaderDevice *delegate = static_cast< IShaderDevice * >(
            g_EmptyFactory( interfaceName, returnCode ) );
        if ( !delegate )
            return 0;
        g_DevicePs4.SetDelegate( delegate );
        if ( returnCode )
            *returnCode = 0;
        return &g_DevicePs4;
    }
    if ( interfaceName && strcmp( interfaceName, SHADERSHADOW_INTERFACE_VERSION ) == 0 )
    {
        IShaderShadow *delegate = static_cast< IShaderShadow * >(
            g_EmptyFactory( interfaceName, returnCode ) );
        if ( !delegate )
            return 0;
        g_ShaderShadowPs4.SetDelegate( delegate );
        if ( returnCode )
            *returnCode = 0;
        return &g_ShaderShadowPs4;
    }
    if ( interfaceName && strcmp( interfaceName, DEBUG_TEXTURE_INFO_VERSION ) == 0 )
    {
        IDebugTextureInfo *delegate = static_cast< IDebugTextureInfo * >(
            g_EmptyFactory( interfaceName, returnCode ) );
        if ( !delegate )
            return 0;
        g_DebugTextureInfoPs4.SetDelegate( delegate );
        if ( returnCode )
            *returnCode = 0;
        return &g_DebugTextureInfoPs4;
    }
    return g_EmptyFactory( interfaceName, returnCode );
}
}

CreateInterfaceFn KisakShaderApiPs4Factory()
{
    // Keep a distinct PS4 factory identity even while individual interface
    // versions still forward to shaderapiempty. Native PS4 device, ShaderAPI,
    // shadow, and hardware-config objects can now replace one lookup at a time
    // without changing the material-system module lifecycle.
    g_EmptyFactory = KisakShaderApiEmptyFactory();
    return Ps4CreateInterface;
}

extern "C" uint32_t KisakPs4ApplyShaderShadowState( GnmCommandBuffer *command )
{
    return g_ShaderShadowPs4.NativeDrawState().Apply( command );
}

extern "C" void KisakPs4SetShaderShadowCulling( bool enabled )
{
    g_ShaderShadowPs4.EnableCulling( enabled );
}

extern "C" void KisakPs4SetShaderShadowDepth( bool testEnabled,
    bool writesEnabled, int depthFunction )
{
    g_ShaderShadowPs4.EnableDepthTest( testEnabled );
    g_ShaderShadowPs4.EnableDepthWrites( writesEnabled );
    g_ShaderShadowPs4.DepthFunc( static_cast< ShaderDepthFunc_t >( depthFunction ) );
}

extern "C" void KisakPs4SetShaderShadowBlend( bool enabled,
    int colorSource, int colorDestination, int colorOperation,
    bool separateAlpha, int alphaSource, int alphaDestination, int alphaOperation )
{
    g_ShaderShadowPs4.EnableBlending( enabled );
    g_ShaderShadowPs4.BlendFunc( static_cast< ShaderBlendFactor_t >( colorSource ),
        static_cast< ShaderBlendFactor_t >( colorDestination ) );
    g_ShaderShadowPs4.BlendOp( static_cast< ShaderBlendOp_t >( colorOperation ) );
    g_ShaderShadowPs4.EnableBlendingSeparateAlpha( separateAlpha );
    g_ShaderShadowPs4.BlendFuncSeparateAlpha( static_cast< ShaderBlendFactor_t >( alphaSource ),
        static_cast< ShaderBlendFactor_t >( alphaDestination ) );
    g_ShaderShadowPs4.BlendOpSeparateAlpha( static_cast< ShaderBlendOp_t >( alphaOperation ) );
}

extern "C" int KisakPs4TextureMemoryUsed()
{
    return g_DebugTextureInfoPs4.GetTextureMemoryUsed(
        IDebugTextureInfo::MEMORY_TOTAL_LOADED );
}

extern "C" bool KisakPs4ShaderDeviceDynamicBufferProbe()
{
    IVertexBuffer *vertexBuffer = g_DevicePs4.GetDynamicVertexBuffer(
        0, VERTEX_POSITION, true );
    IIndexBuffer *indexBuffer = g_DevicePs4.GetDynamicIndexBuffer();
    if ( !vertexBuffer || !indexBuffer )
        return false;
    VertexDesc_t vertexDesc = {};
    IndexDesc_t indexDesc = {};
    if ( !vertexBuffer->Lock( 3, false, vertexDesc ) || !vertexDesc.m_pPosition )
        return false;
    memset( vertexDesc.m_pPosition, 0, 3 * vertexDesc.m_ActualVertexSize );
    vertexBuffer->Unlock( 3, vertexDesc );
    if ( !indexBuffer->Lock( 3, false, indexDesc ) || !indexDesc.m_pIndices )
        return false;
    indexDesc.m_pIndices[0] = 0;
    indexDesc.m_pIndices[1] = 1;
    indexDesc.m_pIndices[2] = 2;
    indexBuffer->Unlock( 3, indexDesc );
    return true;
}

extern "C" bool KisakPs4ShaderApiVertexFormatProbe()
{
    if ( !g_EmptyFactory )
        return false;
    IShaderAPI *shaderApi = static_cast< IShaderAPI * >(
        g_EmptyFactory( SHADERAPI_INTERFACE_VERSION, 0 ) );
    if ( !shaderApi || shaderApi->VertexFormatSize( VERTEX_POSITION ) != 12 )
        return false;
    unsigned char storage[36] = {};
    MeshDesc_t desc = {};
    shaderApi->ComputeVertexDescription( storage, VERTEX_POSITION, desc );
    return desc.m_pPosition == reinterpret_cast< float * >( storage ) &&
        desc.m_ActualVertexSize == 12;
}

extern "C" bool KisakPs4LockDynamicVertices( VertexFormat_t format, int count,
    bool append, VertexDesc_t *desc )
{
    if ( !desc || count < 0 || g_LockedDynamicVertexBuffer )
        return false;
    IVertexBuffer *buffer = g_DevicePs4.GetDynamicVertexBuffer( 0, format, true );
    if ( !buffer || !buffer->Lock( count, append, *desc ) )
        return false;
    g_LockedDynamicVertexBuffer = buffer;
    return true;
}

extern "C" void KisakPs4UnlockDynamicVertices( int count, VertexDesc_t *desc )
{
    if ( !g_LockedDynamicVertexBuffer || !desc )
        return;
    CPs4SourceVertexBuffer *buffer = static_cast< CPs4SourceVertexBuffer * >(
        g_LockedDynamicVertexBuffer );
    g_LockedDynamicVertexBuffer->Unlock( count, *desc );
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( device )
        device->SetStreamSource( 0, &buffer->NativeBuffer(), 0,
            static_cast< uint32_t >( buffer->Stride() ) );
    g_LockedDynamicVertexBuffer = 0;
}

extern "C" bool KisakPs4LockDynamicIndices( int count, bool append, IndexDesc_t *desc )
{
    if ( !desc || count < 0 || g_LockedDynamicIndexBuffer )
        return false;
    IIndexBuffer *buffer = g_DevicePs4.GetDynamicIndexBuffer();
    if ( !buffer || !buffer->Lock( count, append, *desc ) )
        return false;
    g_LockedDynamicIndexBuffer = buffer;
    return true;
}

extern "C" void KisakPs4UnlockDynamicIndices( int count, IndexDesc_t *desc )
{
    if ( !g_LockedDynamicIndexBuffer || !desc )
        return;
    CPs4SourceIndexBuffer *buffer = static_cast< CPs4SourceIndexBuffer * >(
        g_LockedDynamicIndexBuffer );
    g_LockedDynamicIndexBuffer->Unlock( count, *desc );
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( device )
        device->SetIndices( &buffer->NativeBuffer() );
    g_LockedDynamicIndexBuffer = 0;
}

extern "C" bool KisakPs4DynamicMeshBridgeProbe()
{
    VertexDesc_t vertices = {};
    IndexDesc_t indices = {};
    if ( !KisakPs4LockDynamicVertices( VERTEX_POSITION, 3, false, &vertices ) )
        return false;
    const bool verticesValid = vertices.m_pPosition &&
        vertices.m_ActualVertexSize == 12;
    KisakPs4UnlockDynamicVertices( 3, &vertices );
    if ( !verticesValid || !KisakPs4LockDynamicIndices( 3, false, &indices ) )
        return false;
    const bool indicesValid = indices.m_pIndices && indices.m_nIndexSize == 1;
    KisakPs4UnlockDynamicIndices( 3, &indices );
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( !indicesValid || !device || !device->Stream( 0 ).resource ||
        device->Stream( 0 ).stride != 12 || !device->Indices().buffer ||
        device->Indices().index32 || !device->BeginScene() )
        return false;
    device->SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
    CPs4GnmDevice::IndexedDrawPacket packet = {};
    const bool packetValid = device->BuildIndexedDrawPacket(
        GNM_FMT_R32G32B32_FLOAT, 0, 3, 0, 3, &packet ) &&
        packet.indexCount == 3 && packet.indexSize == GNM_INDEX_16 &&
        packet.primitiveType == GNM_PT_TRILIST;
    return device->EndScene() && packetValid;
}

extern "C" bool KisakPs4PopulateShaderApiDynamicTriangle()
{
    if ( !g_EmptyFactory )
        return false;
    IShaderAPI *shaderApi = static_cast< IShaderAPI * >(
        g_EmptyFactory( SHADERAPI_INTERFACE_VERSION, 0 ) );
    const VertexFormat_t format = VERTEX_POSITION | VERTEX_NORMAL |
        VERTEX_FORMAT_PAD_POS_NORM;
    IMesh *mesh = shaderApi ? shaderApi->GetDynamicMeshEx(
        0, format, 0, true, 0, 0 ) : 0;
    if ( !mesh || mesh->GetVertexFormat() != format )
        return false;
    MeshDesc_t desc = {};
    mesh->LockMesh( 3, 3, desc );
    if ( !desc.m_pPosition || !desc.m_pNormal || !desc.m_pIndices ||
        desc.m_ActualVertexSize != 32 || desc.m_nIndexSize != 1 )
        return false;
    const float positions[3][4] = {
        { -0.90f, -0.66f, 0.0f, 1.0f },
        { -0.52f, -0.66f, 0.0f, 1.0f },
        { -0.71f, -0.28f, 0.0f, 1.0f }
    };
    const float colors[3][4] = {
        { 1.0f, 0.9f, 0.1f, 0.45f },
        { 0.1f, 1.0f, 0.9f, 0.45f },
        { 0.9f, 0.1f, 1.0f, 0.45f }
    };
    for ( int vertex = 0; vertex < 3; ++vertex )
    {
        float *base = desc.m_pPosition + vertex * 8;
        memcpy( base, positions[vertex], sizeof( positions[vertex] ) );
        memcpy( base + 4, colors[vertex], sizeof( colors[vertex] ) );
    }
    desc.m_pIndices[0] = 0;
    desc.m_pIndices[1] = 1;
    desc.m_pIndices[2] = 2;
    mesh->UnlockMesh( 3, 3, desc );
    mesh->SetPrimitiveType( MATERIAL_TRIANGLES );
    mesh->Draw( 0, 3 );
    return g_DynamicMeshDrawQueued;
}

extern "C" void KisakPs4QueueDynamicMeshDraw( int primitiveType,
    int firstIndex, int indexCount )
{
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( !device || !device->IsFrameOpen() || firstIndex < 0 || indexCount <= 0 )
        return;
    switch ( static_cast< MaterialPrimitiveType_t >( primitiveType ) )
    {
    case MATERIAL_TRIANGLES:
        device->SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
        break;
    case MATERIAL_TRIANGLE_STRIP:
        device->SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangleStrip );
        break;
    case MATERIAL_LINES:
        device->SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveLines );
        break;
    case MATERIAL_POINTS:
        device->SetPrimitiveTopology( CPs4GnmDevice::kPrimitivePoints );
        break;
    default:
        return;
    }
    g_QueuedDynamicFirstIndex = firstIndex;
    g_QueuedDynamicIndexCount = indexCount;
    g_DynamicMeshDrawQueued = true;
}

extern "C" bool KisakPs4TakeDynamicMeshDraw( int *firstIndex, int *indexCount )
{
    if ( !g_DynamicMeshDrawQueued || !firstIndex || !indexCount )
        return false;
    *firstIndex = g_QueuedDynamicFirstIndex;
    *indexCount = g_QueuedDynamicIndexCount;
    g_DynamicMeshDrawQueued = false;
    return true;
}
