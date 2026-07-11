#include "interface.h"
#include "shaderapi/IShaderDevice.h"

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
