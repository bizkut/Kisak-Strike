#include "interface.h"

// The PS4 module is selected independently now so its implementation can be
// brought up behind the stable empty backend one D3D operation at a time.
// Remove this delegation only after the PS4 device passes clear and triangle
// validation on hardware.
extern CreateInterfaceFn KisakShaderApiEmptyFactory();

CreateInterfaceFn KisakShaderApiPs4Factory()
{
    return KisakShaderApiEmptyFactory();
}
