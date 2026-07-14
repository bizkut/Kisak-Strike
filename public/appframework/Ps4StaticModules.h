#ifndef PS4STATICMODULES_H
#define PS4STATICMODULES_H

#include "appframework/StaticModuleRegistry.h"

CreateInterfaceFn KisakTier0Factory();
CreateInterfaceFn KisakVstdlibFactory();
CreateInterfaceFn KisakLauncherFactory();
CreateInterfaceFn KisakFilesystemFactory();
CreateInterfaceFn KisakEngineBootstrapFactory();
CreateInterfaceFn KisakSourceEngineFactory();
CreateInterfaceFn KisakGameClientFactory();
CreateInterfaceFn KisakGameServerFactory();
CreateInterfaceFn KisakMatchmakingFactory();
CreateInterfaceFn KisakSceneFileCacheFactory();
CreateInterfaceFn KisakPs4ScaleformUIBootstrapFactory();
CreateInterfaceFn KisakInputSystemFactory();
CreateInterfaceFn KisakVPhysicsFactory();
CreateInterfaceFn KisakShaderApiEmptyFactory();
CreateInterfaceFn KisakShaderApiPs4Factory();
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
