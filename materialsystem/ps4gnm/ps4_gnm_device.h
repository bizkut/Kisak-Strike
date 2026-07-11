#ifndef KISAK_PS4_GNM_DEVICE_H
#define KISAK_PS4_GNM_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "ps4_gnm_memory.h"

class CPs4GnmDevice
{
public:
    enum { kFrameCount = 2 };

    CPs4GnmDevice();

    bool Initialize( void *frameMemory, size_t frameMemorySize );
    bool BeginFrame( uint64_t completedLabel );
    void CancelFrame();
    uint64_t EndFrame();

    bool IsInitialized() const { return m_initialized; }
    unsigned int CurrentFrame() const { return m_frameIndex; }
    uint64_t SubmittedLabel() const { return m_submittedLabel; }
    CPs4GnmMemory &FrameArena() { return m_frames[m_frameIndex].arena; }

private:
    struct FrameState
    {
        CPs4GnmMemory arena;
        uint64_t pendingLabel;
    };

    FrameState m_frames[kFrameCount];
    unsigned int m_frameIndex;
    uint64_t m_submittedLabel;
    bool m_initialized;
    bool m_frameOpen;
};

#endif
