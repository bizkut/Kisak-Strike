#ifndef PS4STATICMODULES_H
#define PS4STATICMODULES_H

#include "appframework/StaticModuleRegistry.h"

CreateInterfaceFn KisakTier0Factory();
CreateInterfaceFn KisakVstdlibFactory();
CreateInterfaceFn KisakLauncherFactory();
CreateInterfaceFn KisakFilesystemFactory();
CreateInterfaceFn KisakEngineBootstrapFactory();
CreateInterfaceFn KisakInputSystemFactory();

extern "C" int KisakRegisterStaticModules();

#endif
