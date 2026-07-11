#include "interface.h"
#include "shaderapi/IShaderDevice.h"
#include "shaderapi/ishadershadow.h"
#include "ps4_gnm_draw_state.h"
#include "ps4_shadow_state_translate.h"
#include "shaderapips4.h"

#include <string.h>

extern CreateInterfaceFn KisakShaderApiEmptyFactory();

namespace
{
CreateInterfaceFn g_EmptyFactory = 0;
void *Ps4CreateInterface( const char *interfaceName, int *returnCode );

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

    int GetAdapterCount() const { return m_delegate ? m_delegate->GetAdapterCount() : 0; }
    void GetAdapterInfo( int adapter, MaterialAdapterInfo_t &info ) const
    { if ( m_delegate ) m_delegate->GetAdapterInfo( adapter, info ); }
    bool GetRecommendedConfigurationInfo( int adapter, int dxLevel, KeyValues *configuration )
    { return m_delegate && m_delegate->GetRecommendedConfigurationInfo( adapter, dxLevel, configuration ); }
    int GetModeCount( int adapter ) const { return m_delegate ? m_delegate->GetModeCount( adapter ) : 0; }
    void GetModeInfo( ShaderDisplayMode_t *info, int adapter, int mode ) const
    { if ( m_delegate ) m_delegate->GetModeInfo( info, adapter, mode ); }
    void GetCurrentModeInfo( ShaderDisplayMode_t *info, int adapter ) const
    { if ( m_delegate ) m_delegate->GetCurrentModeInfo( info, adapter ); }
    bool SetAdapter( int adapter, int flags )
    { return m_delegate && m_delegate->SetAdapter( adapter, flags ); }
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
    { return m_delegate && m_delegate->GetRecommendedVideoConfig( adapter, configuration ); }
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
    CShaderDevicePs4() : m_delegate( 0 ) {}
    void SetDelegate( IShaderDevice *delegate ) { m_delegate = delegate; }

    void ReleaseResources( bool releaseManagedResources = true )
    { if ( m_delegate ) m_delegate->ReleaseResources( releaseManagedResources ); }
    void ReacquireResources() { if ( m_delegate ) m_delegate->ReacquireResources(); }
    ImageFormat GetBackBufferFormat() const
    { return m_delegate ? m_delegate->GetBackBufferFormat() : IMAGE_FORMAT_UNKNOWN; }
    void GetBackBufferDimensions( int &width, int &height ) const
    { if ( m_delegate ) m_delegate->GetBackBufferDimensions( width, height ); }
    const AspectRatioInfo_t &GetAspectRatioInfo() const
    {
        static AspectRatioInfo_t empty;
        return m_delegate ? m_delegate->GetAspectRatioInfo() : empty;
    }
    int GetCurrentAdapter() const { return m_delegate ? m_delegate->GetCurrentAdapter() : 0; }
    bool IsUsingGraphics() const { return m_delegate && m_delegate->IsUsingGraphics(); }
    void SpewDriverInfo() const { if ( m_delegate ) m_delegate->SpewDriverInfo(); }
    int StencilBufferBits() const { return m_delegate ? m_delegate->StencilBufferBits() : 0; }
    bool IsAAEnabled() const { return m_delegate && m_delegate->IsAAEnabled(); }
    void Present() { if ( m_delegate ) m_delegate->Present(); }
    void GetWindowSize( int &width, int &height ) const
    { if ( m_delegate ) m_delegate->GetWindowSize( width, height ); }
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
    { return m_delegate ? m_delegate->CreateVertexBuffer( type, format, count, budgetGroup ) : 0; }
    void DestroyVertexBuffer( IVertexBuffer *buffer )
    { if ( m_delegate ) m_delegate->DestroyVertexBuffer( buffer ); }
    IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t type, MaterialIndexFormat_t format, int count,
                                    const char *budgetGroup )
    { return m_delegate ? m_delegate->CreateIndexBuffer( type, format, count, budgetGroup ) : 0; }
    void DestroyIndexBuffer( IIndexBuffer *buffer )
    { if ( m_delegate ) m_delegate->DestroyIndexBuffer( buffer ); }
    IVertexBuffer *GetDynamicVertexBuffer( int stream, VertexFormat_t format, bool buffered = true )
    { return m_delegate ? m_delegate->GetDynamicVertexBuffer( stream, format, buffered ) : 0; }
    IIndexBuffer *GetDynamicIndexBuffer()
    { return m_delegate ? m_delegate->GetDynamicIndexBuffer() : 0; }
    void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *info = 0 )
    { if ( m_delegate ) m_delegate->EnableNonInteractiveMode( mode, info ); }
    void RefreshFrontBufferNonInteractive()
    { if ( m_delegate ) m_delegate->RefreshFrontBufferNonInteractive(); }
    void HandleThreadEvent( uint32 event )
    { if ( m_delegate ) m_delegate->HandleThreadEvent( event ); }

private:
    IShaderDevice *m_delegate;
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
