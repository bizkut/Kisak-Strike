#include "ps4_gnm_device.h"

CPs4GnmDevice::CPs4GnmDevice()
    : m_frameIndex( 0 ), m_submittedLabel( 0 ), m_initialized( false ), m_frameOpen( false ),
      m_sceneOpen( false ), m_topology( kPrimitiveTriangles )
{
    for ( unsigned int i = 0; i < kFrameCount; ++i )
        m_frames[i].pendingLabel = 0;
    for ( unsigned int i = 0; i < kMaxVertexStreams; ++i )
        m_streams[i] = StreamBinding{ 0, 0, 0, 0, 0 };
    m_indices = IndexBinding{ 0, 0, false };
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
    m_sceneOpen = false;
    for ( unsigned int i = 0; i < kMaxVertexStreams; ++i )
        m_streams[i] = StreamBinding{ 0, 0, 0, 0, 0 };
    m_indices = IndexBinding{ 0, 0, false };
    m_topology = kPrimitiveTriangles;
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

bool CPs4GnmDevice::BeginScene()
{
    if ( !m_initialized || m_sceneOpen )
        return false;
    m_sceneOpen = true;
    return true;
}

bool CPs4GnmDevice::EndScene()
{
    if ( !m_sceneOpen )
        return false;
    m_sceneOpen = false;
    return true;
}

bool CPs4GnmDevice::SetStreamSource( uint32_t stream, const void *buffer,
    size_t bufferSize, size_t offset, uint32_t stride )
{
    if ( stream >= kMaxVertexStreams )
        return false;
    if ( !buffer )
    {
        m_streams[stream] = StreamBinding{ 0, 0, 0, 0, 0 };
        return bufferSize == 0 && offset == 0 && stride == 0;
    }
    if ( !bufferSize || !stride || offset >= bufferSize )
        return false;
    m_streams[stream] = StreamBinding{ buffer, bufferSize, offset, stride, 0 };
    return true;
}

bool CPs4GnmDevice::SetStreamSource( uint32_t stream, const CPs4GnmBuffer *buffer,
    size_t offset, uint32_t stride )
{
    if ( !buffer )
        return SetStreamSource( stream, 0, 0, 0, 0 );
    if ( !buffer->IsValid() || buffer->BufferType() != CPs4GnmBuffer::kVertexBuffer )
        return false;
    if ( !SetStreamSource( stream, buffer->Data(), buffer->Size(), offset, stride ) )
        return false;
    m_streams[stream].resource = buffer;
    return true;
}

bool CPs4GnmDevice::SetIndices( const void *buffer, size_t bufferSize, bool index32 )
{
    if ( !buffer )
    {
        m_indices = IndexBinding{ 0, 0, false };
        return bufferSize == 0;
    }
    const size_t indexSize = index32 ? 4 : 2;
    if ( !bufferSize || bufferSize % indexSize )
        return false;
    m_indices = IndexBinding{ buffer, bufferSize, index32 };
    return true;
}

bool CPs4GnmDevice::SetIndices( const CPs4GnmBuffer *buffer )
{
    if ( !buffer )
        return SetIndices( 0, 0, false );
    if ( !buffer->IsValid() || buffer->BufferType() != CPs4GnmBuffer::kIndexBuffer )
        return false;
    return SetIndices( buffer->Data(), buffer->Size(), buffer->IsIndex32() );
}

void CPs4GnmDevice::SetPrimitiveTopology( PrimitiveTopology topology )
{
    m_topology = topology;
}

bool CPs4GnmDevice::ValidateDrawIndexed( uint32_t firstIndex, uint32_t indexCount,
    int32_t baseVertex, uint32_t vertexCount ) const
{
    if ( !m_sceneOpen || !indexCount || !vertexCount || baseVertex < 0 ||
        !m_streams[0].buffer || !m_indices.buffer )
        return false;
    const size_t indexSize = m_indices.index32 ? 4 : 2;
    if ( firstIndex > m_indices.bufferSize / indexSize ||
        indexCount > m_indices.bufferSize / indexSize - firstIndex )
        return false;
    const StreamBinding &stream = m_streams[0];
    const size_t availableVertices =
        ( stream.bufferSize - stream.offset ) / stream.stride;
    return static_cast< size_t >( baseVertex ) <= availableVertices &&
        vertexCount <= availableVertices - static_cast< size_t >( baseVertex );
}

bool CPs4GnmDevice::BuildIndexedDrawPacket( GnmDataFormat vertexFormat,
    uint32_t firstIndex, uint32_t indexCount, int32_t baseVertex,
    uint32_t vertexCount, IndexedDrawPacket *packet ) const
{
    if ( !packet || !ValidateDrawIndexed( firstIndex, indexCount, baseVertex,
        vertexCount ) )
        return false;

    const StreamBinding &stream = m_streams[0];
    const size_t vertexOffset = stream.offset +
        static_cast< size_t >( baseVertex ) * stream.stride;
    packet->vertexBuffer = sceGnmCreateVertexBuffer(
        static_cast< uint8_t * >( const_cast< void * >( stream.buffer ) ) + vertexOffset,
        vertexFormat, stream.stride, vertexCount );

    const size_t indexSize = m_indices.index32 ? 4 : 2;
    packet->indexAddress = static_cast< const uint8_t * >( m_indices.buffer ) +
        static_cast< size_t >( firstIndex ) * indexSize;
    packet->indexCount = indexCount;
    packet->indexSize = m_indices.index32 ? GNM_INDEX_32 : GNM_INDEX_16;
    switch ( m_topology )
    {
    case kPrimitiveTriangleStrip: packet->primitiveType = GNM_PT_TRISTRIP; break;
    case kPrimitiveLines: packet->primitiveType = GNM_PT_LINELIST; break;
    case kPrimitivePoints: packet->primitiveType = GNM_PT_POINTLIST; break;
    case kPrimitiveTriangles: packet->primitiveType = GNM_PT_TRILIST; break;
    }
    return sceGnmBufGetBaseAddress( &packet->vertexBuffer ) ==
        static_cast< uint8_t * >( const_cast< void * >( stream.buffer ) ) + vertexOffset;
}

bool CPs4GnmDevice::BuildPrimitiveDrawPacket( uint32_t startVertex,
    uint32_t primitiveCount, PrimitiveDrawPacket *packet ) const
{
    if ( !m_sceneOpen || !packet || !primitiveCount )
        return false;

    uint32_t verticesPerPrimitive = 1;
    uint32_t extraVertices = 0;
    GnmPrimitiveType primitiveType = GNM_PT_POINTLIST;
    switch ( m_topology )
    {
    case kPrimitiveTriangles:
        verticesPerPrimitive = 3;
        primitiveType = GNM_PT_TRILIST;
        break;
    case kPrimitiveTriangleStrip:
        extraVertices = 2;
        primitiveType = GNM_PT_TRISTRIP;
        break;
    case kPrimitiveLines:
        verticesPerPrimitive = 2;
        primitiveType = GNM_PT_LINELIST;
        break;
    case kPrimitivePoints:
        primitiveType = GNM_PT_POINTLIST;
        break;
    }
    if ( primitiveCount > ( UINT32_MAX - extraVertices ) / verticesPerPrimitive )
        return false;
    const uint32_t vertexCount = primitiveCount * verticesPerPrimitive + extraVertices;
    if ( startVertex > UINT32_MAX - vertexCount )
        return false;
    packet->startVertex = startVertex;
    packet->vertexCount = vertexCount;
    packet->primitiveType = primitiveType;
    return true;
}

bool CPs4GnmDevice::BuildVertexDescriptorTable(
    const CPs4GnmVertexDeclaration &declaration, int32_t baseVertex,
    uint32_t vertexCount, GnmBuffer *descriptors,
    uint32_t descriptorCapacity ) const
{
    if ( !declaration.IsValid() || baseVertex < 0 || !vertexCount ||
        !descriptors || descriptorCapacity < declaration.ElementCount() )
        return false;
    for ( uint32_t i = 0; i < declaration.ElementCount(); ++i )
    {
        const CPs4GnmVertexDeclaration::Element &element = declaration.GetElement( i );
        const StreamBinding &stream = m_streams[element.stream];
        const size_t elementBytes = sceGnmDfGetBytesPerElement( element.format );
        if ( !stream.resource || !stream.stride || element.offset > stream.stride ||
            elementBytes > stream.stride - element.offset )
            return false;
        const size_t baseOffset = stream.offset +
            static_cast< size_t >( baseVertex ) * stream.stride;
        if ( baseOffset < stream.offset || element.offset > SIZE_MAX - baseOffset ||
            !stream.resource->BuildVertexDescriptor( element.format, stream.stride,
                vertexCount, baseOffset + element.offset, &descriptors[i] ) )
            return false;
    }
    return true;
}
