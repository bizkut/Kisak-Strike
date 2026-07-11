#ifndef STATICMODULEREGISTRY_H
#define STATICMODULEREGISTRY_H

typedef void *(*CreateInterfaceFn)( const char *pName, int *pReturnCode );

// PS4 initially links Source modules into one eboot. Registration is explicit
// so startup order remains deterministic and does not depend on constructors.
bool RegisterStaticModule( const char *pModuleName, CreateInterfaceFn factory );
CreateInterfaceFn FindStaticModuleFactory( const char *pModuleName );
void ClearStaticModuleRegistryForTesting();

#endif
