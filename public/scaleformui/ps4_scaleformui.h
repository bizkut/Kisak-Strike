#ifndef KISAK_PS4_SCALEFORMUI_H
#define KISAK_PS4_SCALEFORMUI_H

#include "appframework/iappsystem.h"
#include "inputsystem/InputEnums.h"

#define PS4_SCALEFORMUI_INTERFACE_VERSION "Ps4ScaleformUI001"

// Focused PS4 boundary used while the full Source IScaleformUI implementation
// is being moved onto the OpenGNM HAL.  The source frame lifecycle deliberately
// mirrors the old RocketUI contract so engine timing and input ownership remain
// stable during the renderer migration.
class IPs4ScaleformUI : public IAppSystem
{
public:
    virtual void RunFrame( float time ) = 0;
    virtual bool HandleInputEvent( const InputEvent_t &event ) = 0;
    virtual void RenderMenuFrame() = 0;
    virtual void RenderHUDFrame() = 0;
    virtual bool IsMovieReady( int slot ) const = 0;
};

IPs4ScaleformUI *Ps4ScaleformUI();
CreateInterfaceFn KisakPs4ScaleformUIBootstrapFactory();

#endif
