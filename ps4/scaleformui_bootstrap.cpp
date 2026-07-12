#include "appframework/Ps4StaticModules.h"
#include "ps4/scaleform_gfx_manager.h"
#include "scaleformui/ps4_scaleformui.h"

#include <string.h>

extern "C" void KisakPs4StartupBreadcrumb( const char *line );

namespace
{
class CPs4ScaleformUIBootstrap final : public IPs4ScaleformUI
{
public:
    bool Connect( CreateInterfaceFn ) override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform ui bootstrap connect" );
        return true;
    }

    void Disconnect() override
    {
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform ui bootstrap disconnect" );
    }

    void *QueryInterface( const char *name ) override
    {
        return name && strcmp( name, PS4_SCALEFORMUI_INTERFACE_VERSION ) == 0 ? this : NULL;
    }

    InitReturnVal_t Init() override
    {
        const bool ready = KisakPs4ScaleformUiInitialize();
        KisakPs4StartupBreadcrumb( ready
            ? "kisak-ps4: scaleform ui bootstrap init"
            : "kisak-ps4: scaleform ui bootstrap init without movies" );
        return INIT_OK;
    }

    void Shutdown() override
    {
        KisakPs4ScaleformUiShutdown();
        KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform ui bootstrap shutdown" );
    }

    void RunFrame( float time ) override
    {
        KisakPs4ScaleformUiInitialize();
        KisakPs4ScaleformUiAdvance( time );
        static bool logged = false;
        if ( !logged )
        {
            KisakPs4StartupBreadcrumb( "kisak-ps4: scaleform run frame begin" );
            logged = true;
        }
    }

    bool HandleInputEvent( const InputEvent_t &event ) override
    {
        return KisakPs4ScaleformUiHandleInput( event );
    }

    void RenderMenuFrame() override
    {
        const bool captured = KisakPs4ScaleformUiRenderMenu();
        static bool logged = false;
        if ( !logged )
        {
            KisakPs4StartupBreadcrumb( captured
                ? "kisak-ps4: scaleform menu phase active"
                : "kisak-ps4: scaleform menu phase waiting for movie" );
            logged = true;
        }
    }

    void RenderHUDFrame() override
    {
        const bool captured = KisakPs4ScaleformUiRenderHud();
        static bool logged = false;
        if ( !logged )
        {
            KisakPs4StartupBreadcrumb( captured
                ? "kisak-ps4: scaleform HUD phase active"
                : "kisak-ps4: scaleform HUD phase waiting for movie" );
            logged = true;
        }
    }

    bool IsMovieReady( int slot ) const override
    {
        return KisakPs4ScaleformUiMovieReady( slot );
    }
};

CPs4ScaleformUIBootstrap g_ps4ScaleformUI;

void *ScaleformUIBootstrapCreateInterface( const char *name, int *returnCode )
{
    void *result = g_ps4ScaleformUI.QueryInterface( name );
    if ( returnCode )
        *returnCode = result ? 0 : 1;
    return result;
}
}

IPs4ScaleformUI *Ps4ScaleformUI()
{
    return &g_ps4ScaleformUI;
}

CreateInterfaceFn KisakPs4ScaleformUIBootstrapFactory()
{
    return ScaleformUIBootstrapCreateInterface;
}
