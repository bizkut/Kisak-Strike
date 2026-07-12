#include "ps4_gnm_runtime.h"

CPs4GnmRuntime::CPs4GnmRuntime() : m_device( 0 )
{
}

bool CPs4GnmRuntime::Register( CPs4GnmDevice *device, void *persistentMemory,
    size_t persistentBytes )
{
    if ( !device || !device->IsInitialized() || !persistentMemory ||
        !persistentBytes || !m_persistent.Initialize( persistentMemory,
            persistentBytes ) )
        return false;
    m_device = device;
    return true;
}

void CPs4GnmRuntime::Reset()
{
    m_device = 0;
    m_persistent = CPs4GnmMemory();
}

CPs4GnmRuntime &KisakPs4GnmRuntime()
{
    static CPs4GnmRuntime runtime;
    return runtime;
}
