#include "interface.h"

// Defined beside the CMatchFramework singleton in main.cpp. Referencing it
// forces that archive member and its MATCHFRAMEWORK_001 registrar into the
// PS4 monolith.
extern void LinkMatchmakingLib();

CreateInterfaceFn KisakMatchmakingFactory()
{
    LinkMatchmakingLib();
    return Sys_GetFactoryThis();
}
