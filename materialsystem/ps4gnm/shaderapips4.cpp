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
