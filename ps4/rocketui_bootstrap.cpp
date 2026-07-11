class IRocketUI;
#include "rocketui/rocketui.h"

#include <string.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

namespace
{
class CPs4RocketUIBootstrap final : public IRocketUI
{
public:
    bool Connect( CreateInterfaceFn ) override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: rocketui bootstrap connect" );
        return true;
    }

    void Disconnect() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: rocketui bootstrap disconnect" );
    }

    void *QueryInterface( const char *name ) override
    {
        return name && strcmp( name, ROCKETUI_INTERFACE_VERSION ) == 0 ? this : NULL;
    }

    InitReturnVal_t Init() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: rocketui bootstrap init" );
        return INIT_OK;
    }

    void Shutdown() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: rocketui bootstrap shutdown" );
    }

    void RunFrame( float ) override {}
    bool ReloadDocuments() override { return false; }
    bool HandleInputEvent( const InputEvent_t & ) override { return false; }
    void DenyInputToGame( bool value, const char * ) override { m_bConsumesInput = value; }
    bool IsConsumingInput() override { return m_bConsumesInput; }
    void EnableCursor( bool ) override {}

    Rml::ElementDocument *LoadDocumentFile( RocketDesinationContext_t, const char *, LoadDocumentFn, UnloadDocumentFn ) override
    {
        return NULL;
    }

    void RenderHUDFrame() override {}
    void RenderMenuFrame() override {}
    Rml::Context *AccessHudContext() override { return NULL; }
    Rml::Context *AccessMenuContext() override { return NULL; }
    void RegisterPauseMenu( TogglePauseMenuFn ) override {}
    void AddDeviceDependentObject( IShaderDeviceDependentObject * ) override {}
    void RemoveDeviceDependentObject( IShaderDeviceDependentObject * ) override {}

private:
    bool m_bConsumesInput = false;
};

CPs4RocketUIBootstrap g_Ps4RocketUIBootstrap;

void *RocketUIBootstrapCreateInterface( const char *name, int *returnCode )
{
    void *result = g_Ps4RocketUIBootstrap.QueryInterface( name );
    if ( returnCode )
        *returnCode = result ? 0 : 1;
    return result;
}
}

extern IRocketUI *g_pRocketUI;

CreateInterfaceFn KisakRocketUIBootstrapFactory()
{
    g_pRocketUI = &g_Ps4RocketUIBootstrap;
    return RocketUIBootstrapCreateInterface;
}
