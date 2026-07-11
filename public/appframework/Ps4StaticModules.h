#ifndef PS4STATICMODULES_H
#define PS4STATICMODULES_H

#include "appframework/StaticModuleRegistry.h"

CreateInterfaceFn KisakTier0Factory();
CreateInterfaceFn KisakVstdlibFactory();
CreateInterfaceFn KisakLauncherFactory();
CreateInterfaceFn KisakFilesystemFactory();
CreateInterfaceFn KisakEngineBootstrapFactory();
CreateInterfaceFn KisakRocketUIBootstrapFactory();
CreateInterfaceFn KisakInputSystemFactory();
CreateInterfaceFn KisakVPhysicsFactory();
CreateInterfaceFn KisakShaderApiEmptyFactory();
CreateInterfaceFn KisakMaterialSystemFactory();
CreateInterfaceFn KisakDataCacheFactory();
CreateInterfaceFn KisakStudioRenderFactory();
CreateInterfaceFn KisakSoundEmitterSystemFactory();
CreateInterfaceFn KisakVScriptFactory();
CreateInterfaceFn KisakVGuiFactory();
CreateInterfaceFn KisakLocalizeFactory();
CreateInterfaceFn KisakVGuiMatSurfaceFactory();

extern "C" int KisakRegisterStaticModules();

#endif
