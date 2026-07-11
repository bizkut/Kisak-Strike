#ifndef PS4STATICMODULES_H
#define PS4STATICMODULES_H

#include "appframework/StaticModuleRegistry.h"

CreateInterfaceFn KisakTier0Factory();
CreateInterfaceFn KisakVstdlibFactory();
CreateInterfaceFn KisakLauncherFactory();

extern "C" int KisakRegisterStaticModules();

#endif
