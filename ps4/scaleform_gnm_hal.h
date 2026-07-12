#ifndef KISAK_PS4_SCALEFORM_GNM_HAL_H
#define KISAK_PS4_SCALEFORM_GNM_HAL_H

#include <stdint.h>

#include <gnm.h>

// OpenGNM state translation boundary for Scaleform batches. The movie manager
// owns GFx lifetime and capture; this class owns PS4 blend/scissor semantics
// and will become the Render::HAL adapter once tree batches are wired through.
class CPs4ScaleformHal
{
public:
    enum BlendMode
    {
        kBlendNormal,
        kBlendAdd,
        kBlendMultiply,
        kBlendScreen
    };

    CPs4ScaleformHal();

    void BeginFrame( uint64_t frame );
    void EndFrame();
    bool QueueCapturedTree( bool captured, const char *phase );

    bool TranslateBlend( BlendMode mode, GnmBlendControl *control ) const;
    bool TranslateScissor( int left, int top, int right, int bottom,
        uint32_t width, uint32_t height, uint32_t scissor[4] ) const;

    uint64_t CapturedTrees() const { return m_capturedTrees; }
    uint64_t PendingBatches() const { return m_pendingBatches; }

private:
    bool m_frameOpen;
    uint64_t m_frame;
    uint64_t m_capturedTrees;
    uint64_t m_pendingBatches;
};

CPs4ScaleformHal &KisakPs4ScaleformHal();

#endif
