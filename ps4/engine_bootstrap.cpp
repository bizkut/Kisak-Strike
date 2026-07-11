#include "icvar.h"
#include "tier1/convar.h"

#include <string.h>

namespace
{
class CPs4CvarQuery final : public ICvarQuery
{
public:
    bool Connect( CreateInterfaceFn factory ) override
    {
        ICvar *cvar = static_cast<ICvar *>( factory( CVAR_INTERFACE_VERSION, NULL ) );
        if ( !cvar )
            return false;
        cvar->InstallCVarQuery( this );
        return true;
    }

    void Disconnect() override {}

    void *QueryInterface( const char *name ) override
    {
        return name && strcmp( name, CVAR_QUERY_INTERFACE_VERSION ) == 0 ? this : NULL;
    }

    InitReturnVal_t Init() override { return INIT_OK; }
    void Shutdown() override {}

    bool AreConVarsLinkable( const ConVar *child, const ConVar *parent ) override
    {
        if ( !child || !parent )
            return false;
        const bool childReplicated = child->IsFlagSet( FCVAR_REPLICATED );
        const bool parentReplicated = parent->IsFlagSet( FCVAR_REPLICATED );
        if ( childReplicated != parentReplicated )
            return false;
        if ( childReplicated )
        {
            if ( child->IsFlagSet( FCVAR_PROTECTED ) || parent->IsFlagSet( FCVAR_PROTECTED ) )
                return false;
            if ( child->IsCommand() || parent->IsCommand() )
                return false;
            if ( child->IsFlagSet( FCVAR_GAMEDLL ) && !parent->IsFlagSet( FCVAR_CLIENTDLL ) )
                return false;
            if ( child->IsFlagSet( FCVAR_CLIENTDLL ) && !parent->IsFlagSet( FCVAR_GAMEDLL ) )
                return false;
            return true;
        }
        return !parent->IsFlagSet( FCVAR_CLIENTDLL | FCVAR_GAMEDLL );
    }
};

CPs4CvarQuery g_Ps4CvarQuery;
}

CreateInterfaceFn KisakEngineBootstrapFactory();

namespace
{
void *EngineBootstrapCreateInterface( const char *name, int *returnCode )
{
    void *result = g_Ps4CvarQuery.QueryInterface( name );
    if ( returnCode )
        *returnCode = result ? 0 : 1;
    return result;
}
}

CreateInterfaceFn KisakEngineBootstrapFactory()
{
    return EngineBootstrapCreateInterface;
}
