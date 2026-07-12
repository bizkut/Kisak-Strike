#ifndef KISAK_PS4_GNM_RUNTIME_H
#define KISAK_PS4_GNM_RUNTIME_H

#include "ps4_gnm_device.h"
#include "ps4_gnm_memory.h"

class CPs4GnmRuntime
{
public:
    CPs4GnmRuntime();
    bool Register( CPs4GnmDevice *device, void *persistentMemory,
        size_t persistentBytes );
    void Reset();

    bool IsReady() const { return m_device && m_persistent.Capacity(); }
    CPs4GnmDevice *Device() const { return m_device; }
    CPs4GnmMemory &PersistentArena() { return m_persistent; }
    const CPs4GnmMemory &PersistentArena() const { return m_persistent; }

private:
    CPs4GnmDevice *m_device;
    CPs4GnmMemory m_persistent;
};

CPs4GnmRuntime &KisakPs4GnmRuntime();

#endif
