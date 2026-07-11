#include "ps4_gnm_device.h"

CPs4GnmDevice::CPs4GnmDevice()
    : m_frameIndex( 0 ), m_submittedLabel( 0 ), m_initialized( false ), m_frameOpen( false )
{
    for ( unsigned int i = 0; i < kFrameCount; ++i )
        m_frames[i].pendingLabel = 0;
}

bool CPs4GnmDevice::Initialize( void *frameMemory, size_t frameMemorySize )
{
    if ( !frameMemory || frameMemorySize < kFrameCount || frameMemorySize % kFrameCount )
        return false;

    const size_t arenaSize = frameMemorySize / kFrameCount;
    uint8_t *base = static_cast< uint8_t * >( frameMemory );
    for ( unsigned int i = 0; i < kFrameCount; ++i )
    {
        if ( !m_frames[i].arena.Initialize( base + i * arenaSize, arenaSize ) )
            return false;
        m_frames[i].pendingLabel = 0;
    }

    m_frameIndex = 0;
    m_submittedLabel = 0;
    m_frameOpen = false;
    m_initialized = true;
    return true;
}

bool CPs4GnmDevice::BeginFrame( uint64_t completedLabel )
{
    if ( !m_initialized || m_frameOpen )
        return false;

    FrameState &frame = m_frames[m_frameIndex];
    if ( frame.pendingLabel && completedLabel < frame.pendingLabel )
        return false;

    frame.arena.Reset();
    m_frameOpen = true;
    return true;
}

uint64_t CPs4GnmDevice::EndFrame()
{
    if ( !m_initialized || !m_frameOpen )
        return 0;

    const uint64_t label = ++m_submittedLabel;
    m_frames[m_frameIndex].pendingLabel = label;
    m_frameIndex = ( m_frameIndex + 1 ) % kFrameCount;
    m_frameOpen = false;
    return label;
}

void CPs4GnmDevice::CancelFrame()
{
    m_frameOpen = false;
}

bool CPs4GnmDevice::BeginSubmission( uint64_t completedLabel, size_t commandBytes,
    size_t commandAlignment, SubmissionFrame *submission )
{
    if ( !submission || !commandBytes || !commandAlignment ||
        !BeginFrame( completedLabel ) )
        return false;

    submission->commandMemory = FrameArena().Allocate( commandBytes, commandAlignment );
    submission->completionLabel = static_cast< volatile uint64_t * >(
        FrameArena().Allocate( sizeof( uint64_t ), 8 ) );
    submission->submittedLabel = SubmittedLabel() + 1;
    if ( !submission->commandMemory || !submission->completionLabel )
    {
        CancelFrame();
        submission->commandMemory = 0;
        submission->completionLabel = 0;
        submission->submittedLabel = 0;
        return false;
    }

    *submission->completionLabel = 0;
    return true;
}

bool CPs4GnmDevice::CommitSubmission( const SubmissionFrame &submission )
{
    if ( !m_frameOpen || !submission.commandMemory || !submission.completionLabel ||
        submission.submittedLabel != m_submittedLabel + 1 )
        return false;
    return EndFrame() == submission.submittedLabel;
}
