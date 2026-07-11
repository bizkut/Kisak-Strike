#include "ps4_gnm_device.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    alignas( 256 ) uint8_t displayStorage[256] = {};
    assert( CPs4GnmDevice::ValidateDisplayRenderTarget(
        displayStorage, 1920, 1080, 1920 ) );
    assert( !CPs4GnmDevice::ValidateDisplayRenderTarget(
        displayStorage + 1, 1920, 1080, 1920 ) );
    assert( !CPs4GnmDevice::ValidateDisplayRenderTarget(
        displayStorage, 1920, 1080, 1912 ) );
    assert( !CPs4GnmDevice::ValidateDisplayRenderTarget(
        displayStorage, 1920, 1080, 1921 ) );
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
    const CPs4GnmVertexDeclaration::Element declarationElements[] = {
        { 0, 0, GNM_FMT_R32G32B32A32_FLOAT }
    };
    CPs4GnmVertexDeclaration declaration;
    assert( declaration.Initialize( declarationElements, 1 ) );
    GnmBuffer descriptorTable[1] = {};
    assert( device.BuildVertexDescriptorTable( declaration, 0, 3,
        descriptorTable, 1 ) );
    assert( sceGnmBufGetBaseAddress( &descriptorTable[0] ) == vertices );
    assert( !device.BuildVertexDescriptorTable( declaration, 1, 3,
        descriptorTable, 1 ) );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
    assert( device.ValidateDrawIndexed( 0, 3, 0, 3 ) );
    CPs4GnmDevice::IndexedDrawPacket packet = {};
    assert( device.BuildIndexedDrawPacket( GNM_FMT_R32G32B32A32_FLOAT,
        0, 3, 0, 3, &packet ) );
    assert( sceGnmBufGetBaseAddress( &packet.vertexBuffer ) == vertices );
    assert( packet.vertexBuffer.stride == 16 &&
        packet.vertexBuffer.numrecords == 3 );
    assert( packet.indexAddress == indices && packet.indexCount == 3 );
    assert( packet.indexSize == GNM_INDEX_16 );
    assert( packet.primitiveType == GNM_PT_TRILIST );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangleStrip );
    assert( device.BuildIndexedDrawPacket( GNM_FMT_R32G32B32A32_FLOAT,
        0, 2, 1, 2, &packet ) );
    assert( sceGnmBufGetBaseAddress( &packet.vertexBuffer ) == vertices + 16 );
    assert( packet.primitiveType == GNM_PT_TRISTRIP );
    CPs4GnmDevice::PrimitiveDrawPacket primitivePacket = {};
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
    assert( device.BuildPrimitiveDrawPacket( 0, 12, &primitivePacket ) );
    assert( primitivePacket.startVertex == 0 && primitivePacket.vertexCount == 36 );
    assert( primitivePacket.primitiveType == GNM_PT_TRILIST );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangleStrip );
    assert( device.BuildPrimitiveDrawPacket( 4, 10, &primitivePacket ) );
    assert( primitivePacket.startVertex == 4 && primitivePacket.vertexCount == 12 );
    assert( primitivePacket.primitiveType == GNM_PT_TRISTRIP );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveLines );
    assert( device.BuildPrimitiveDrawPacket( 0, 3, &primitivePacket ) );
    assert( primitivePacket.vertexCount == 6 &&
        primitivePacket.primitiveType == GNM_PT_LINELIST );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitivePoints );
    assert( device.BuildPrimitiveDrawPacket( 0, 7, &primitivePacket ) );
    assert( primitivePacket.vertexCount == 7 &&
        primitivePacket.primitiveType == GNM_PT_POINTLIST );
    assert( !device.BuildPrimitiveDrawPacket( 0, 0, &primitivePacket ) );
    device.SetPrimitiveTopology( CPs4GnmDevice::kPrimitiveTriangles );
    assert( !device.BuildPrimitiveDrawPacket( 0, UINT32_MAX, &primitivePacket ) );
    assert( !device.ValidateDrawIndexed( 1, 3, 0, 3 ) );
    assert( !device.ValidateDrawIndexed( 0, 3, 1, 3 ) );
    assert( device.EndScene() );
    assert( !device.EndScene() );
    assert( !device.ValidateDrawIndexed( 0, 3, 0, 3 ) );
    return 0;
}
