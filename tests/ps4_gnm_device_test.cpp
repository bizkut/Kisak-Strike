#include "ps4_gnm_device.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    uint8_t storage[1024];
    CPs4GnmDevice device;
    assert( !device.Initialize( 0, sizeof( storage ) ) );
    assert( device.Initialize( storage, sizeof( storage ) ) );
    assert( device.BeginFrame( 0 ) );

    void *constantData = device.FrameArena().Allocate( 64, 64 );
    assert( constantData );
    assert( reinterpret_cast< uintptr_t >( constantData ) % 64 == 0 );
    assert( !device.BeginFrame( 0 ) );
    assert( device.EndFrame() == 1 );

    assert( device.BeginFrame( 0 ) );
    assert( device.EndFrame() == 2 );
    assert( !device.BeginFrame( 0 ) );
    assert( device.BeginFrame( 1 ) );
    assert( device.FrameArena().Used() == 0 );
    device.CancelFrame();
    assert( device.BeginFrame( 1 ) );
    assert( device.EndFrame() == 3 );

    CPs4GnmDevice::SubmissionFrame submission = {};
    assert( device.BeginSubmission( 2, 128, 64, &submission ) );
    assert( submission.commandMemory );
    assert( submission.completionLabel );
    assert( submission.submittedLabel == 4 );
    assert( device.CommitSubmission( submission ) );
    assert( device.SubmittedLabel() == 4 );

    CPs4GnmDevice::SubmissionFrame oversized = {};
    assert( !device.BeginSubmission( 3, sizeof( storage ), 64, &oversized ) );
    assert( !device.IsFrameOpen() );
    assert( oversized.commandMemory == 0 );
    assert( oversized.completionLabel == 0 );
    assert( oversized.submittedLabel == 0 );

    uint8_t vertices[3 * 16] = {};
    uint16_t indices[3] = { 0, 1, 2 };
    assert( device.BeginScene() );
    assert( !device.BeginScene() );
    assert( !device.SetStreamSource( CPs4GnmDevice::kMaxVertexStreams,
        vertices, sizeof( vertices ), 0, 16 ) );
    CPs4GnmBuffer vertexBuffer;
    CPs4GnmBuffer indexBuffer;
    assert( vertexBuffer.Initialize( vertices, sizeof( vertices ),
        CPs4GnmBuffer::kVertexBuffer, true ) );
    assert( indexBuffer.Initialize( indices, sizeof( indices ),
        CPs4GnmBuffer::kIndexBuffer, false ) );
    assert( !device.SetStreamSource( 0, &indexBuffer, 0, 16 ) );
    assert( !device.SetIndices( &vertexBuffer ) );
    assert( device.SetStreamSource( 0, &vertexBuffer, 0, 16 ) );
    assert( device.SetIndices( &indexBuffer ) );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
    assert( device.ValidateDrawIndexed( 0, 3, 0, 3 ) );
    assert( !device.ValidateDrawIndexed( 1, 3, 0, 3 ) );
    assert( !device.ValidateDrawIndexed( 0, 3, 1, 3 ) );
    assert( device.EndScene() );
    assert( !device.EndScene() );
    assert( !device.ValidateDrawIndexed( 0, 3, 0, 3 ) );
    return 0;
}
