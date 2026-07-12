#include "ps4_source_buffers.h"

#include "ps4_gnm_runtime.h"
#include "../shaderapidx9/meshbase.h"

#include <string.h>

CPs4SourceVertexBuffer::CPs4SourceVertexBuffer()
    : m_format( 0 ), m_count( 0 ), m_stride( 0 ), m_written( 0 ),
      m_lockStart( 0 ), m_lockCapacity( 0 ), m_frameIndex( 0 ), m_dynamic( false )
{
}

bool CPs4SourceVertexBuffer::Initialize( ShaderBufferType_t type,
    VertexFormat_t format, int count )
{
    if ( count <= 0 || !KisakPs4GnmRuntime().IsReady() )
        return false;
    VertexDesc_t sizeDesc = {};
    const int stride = ComputeVertexDesc< true >( 0, format, sizeDesc );
    if ( stride <= 0 || static_cast< size_t >( count ) > SIZE_MAX / stride )
        return false;
    m_dynamic = IsDynamicBufferType( type );
    m_format = format;
    m_count = count;
    m_stride = stride;
    m_written = 0;
    if ( m_dynamic )
        return true;
    void *memory = KisakPs4GnmRuntime().PersistentArena().Allocate(
        static_cast< size_t >( count ) * stride, 256 );
    if ( !memory || !m_buffer.Initialize( memory,
            static_cast< size_t >( count ) * stride,
            CPs4GnmBuffer::kVertexBuffer, false ) )
        return false;
    return true;
}

void CPs4SourceVertexBuffer::BeginCastBuffer( VertexFormat_t format )
{
    if ( m_dynamic && m_written == 0 )
        m_format = format;
}

void CPs4SourceVertexBuffer::EndCastBuffer()
{
}

bool CPs4SourceVertexBuffer::Lock( int count, bool append, VertexDesc_t &desc )
{
    if ( count <= 0 || count > m_count || ( append && count > GetRoomRemaining() ) )
        return false;
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( m_dynamic && append && ( !device || !device->IsFrameOpen() ||
        device->CurrentFrame() != m_frameIndex ) )
        return false;
    m_lockStart = append ? m_written : 0;
    m_lockCapacity = count;
    if ( m_dynamic && !append )
    {
        if ( !device || !device->IsFrameOpen() )
            return false;
        const size_t bytes = static_cast< size_t >( m_count ) * m_stride;
        void *memory = device->FrameArena().Allocate( bytes, 256 );
        if ( !memory || !m_buffer.Initialize( memory, bytes,
                CPs4GnmBuffer::kVertexBuffer, true ) )
            return false;
        m_written = 0;
        m_frameIndex = device->CurrentFrame();
    }
    void *data = 0;
    if ( !m_buffer.Lock( static_cast< size_t >( m_lockStart ) * m_stride,
            static_cast< size_t >( count ) * m_stride,
            m_dynamic && !append && count == m_count, &data ) )
        return false;
    memset( &desc, 0, sizeof( desc ) );
    ComputeVertexDesc< false >( static_cast< unsigned char * >( data ), m_format, desc );
    desc.m_nFirstVertex = m_lockStart;
    desc.m_nOffset = static_cast< unsigned int >( m_lockStart * m_stride );
    return true;
}

void CPs4SourceVertexBuffer::Unlock( int count, VertexDesc_t &desc )
{
    if ( count < 0 || count > m_lockCapacity )
        count = 0;
    m_buffer.Unlock();
    m_written = m_lockStart + count;
    m_lockCapacity = 0;
    memset( &desc, 0, sizeof( desc ) );
}

CPs4SourceIndexBuffer::CPs4SourceIndexBuffer()
    : m_format( MATERIAL_INDEX_FORMAT_UNKNOWN ), m_count( 0 ), m_indexSize( 0 ),
      m_written( 0 ), m_lockStart( 0 ), m_lockCapacity( 0 ), m_frameIndex( 0 ),
      m_dynamic( false )
{
}

bool CPs4SourceIndexBuffer::Initialize( ShaderBufferType_t type,
    MaterialIndexFormat_t format, int count )
{
    if ( count <= 0 || ( format != MATERIAL_INDEX_FORMAT_16BIT &&
        format != MATERIAL_INDEX_FORMAT_32BIT ) || !KisakPs4GnmRuntime().IsReady() )
        return false;
    m_indexSize = format == MATERIAL_INDEX_FORMAT_32BIT ? 4 : 2;
    if ( static_cast< size_t >( count ) > SIZE_MAX / m_indexSize )
        return false;
    m_dynamic = IsDynamicBufferType( type );
    m_format = format;
    m_count = count;
    m_written = 0;
    if ( m_dynamic )
        return true;
    void *memory = KisakPs4GnmRuntime().PersistentArena().Allocate(
        static_cast< size_t >( count ) * m_indexSize, 256 );
    if ( !memory || !m_buffer.Initialize( memory,
            static_cast< size_t >( count ) * m_indexSize,
            CPs4GnmBuffer::kIndexBuffer, false, m_indexSize == 4 ) )
        return false;
    return true;
}

void CPs4SourceIndexBuffer::BeginCastBuffer( MaterialIndexFormat_t format )
{
    if ( m_dynamic && m_written == 0 && format == m_format )
        m_format = format;
}

void CPs4SourceIndexBuffer::EndCastBuffer()
{
}

bool CPs4SourceIndexBuffer::Lock( int count, bool append, IndexDesc_t &desc )
{
    if ( count <= 0 || count > m_count || ( append && count > GetRoomRemaining() ) )
        return false;
    CPs4GnmDevice *device = KisakPs4GnmRuntime().Device();
    if ( m_dynamic && append && ( !device || !device->IsFrameOpen() ||
        device->CurrentFrame() != m_frameIndex ) )
        return false;
    m_lockStart = append ? m_written : 0;
    m_lockCapacity = count;
    if ( m_dynamic && !append )
    {
        if ( !device || !device->IsFrameOpen() )
            return false;
        const size_t bytes = static_cast< size_t >( m_count ) * m_indexSize;
        void *memory = device->FrameArena().Allocate( bytes, 256 );
        if ( !memory || !m_buffer.Initialize( memory, bytes,
                CPs4GnmBuffer::kIndexBuffer, true, m_indexSize == 4 ) )
            return false;
        m_written = 0;
        m_frameIndex = device->CurrentFrame();
    }
    void *data = 0;
    if ( !m_buffer.Lock( static_cast< size_t >( m_lockStart ) * m_indexSize,
            static_cast< size_t >( count ) * m_indexSize,
            m_dynamic && !append && count == m_count, &data ) )
        return false;
    desc.m_pIndices = static_cast< unsigned short * >( data );
    desc.m_nOffset = static_cast< unsigned int >( m_lockStart * m_indexSize );
    desc.m_nFirstIndex = m_lockStart;
    // Source expresses this field in 16-bit index units, not bytes.
    desc.m_nIndexSize = m_indexSize >> 1;
    return true;
}

void CPs4SourceIndexBuffer::Unlock( int count, IndexDesc_t &desc )
{
    if ( count < 0 || count > m_lockCapacity )
        count = 0;
    m_buffer.Unlock();
    m_written = m_lockStart + count;
    m_lockCapacity = 0;
    memset( &desc, 0, sizeof( desc ) );
}

void CPs4SourceIndexBuffer::ModifyBegin( bool, int firstIndex, int count,
    IndexDesc_t &desc )
{
    if ( firstIndex < 0 || count <= 0 || firstIndex > m_count - count )
    {
        memset( &desc, 0, sizeof( desc ) );
        return;
    }
    m_lockStart = firstIndex;
    m_lockCapacity = count;
    void *data = 0;
    if ( !m_buffer.Lock( static_cast< size_t >( firstIndex ) * m_indexSize,
            static_cast< size_t >( count ) * m_indexSize, false, &data ) )
    {
        memset( &desc, 0, sizeof( desc ) );
        return;
    }
    desc.m_pIndices = static_cast< unsigned short * >( data );
    desc.m_nOffset = static_cast< unsigned int >( firstIndex * m_indexSize );
    desc.m_nFirstIndex = firstIndex;
    desc.m_nIndexSize = m_indexSize >> 1;
}

void CPs4SourceIndexBuffer::ModifyEnd( IndexDesc_t &desc )
{
    m_buffer.Unlock();
    m_lockCapacity = 0;
    memset( &desc, 0, sizeof( desc ) );
}

extern "C" bool KisakPs4SourceBufferProbe()
{
    CPs4SourceVertexBuffer vertexBuffer;
    CPs4SourceIndexBuffer indexBuffer;
    VertexDesc_t vertexDesc = {};
    IndexDesc_t indexDesc = {};
    if ( !vertexBuffer.Initialize( SHADER_BUFFER_TYPE_STATIC,
            VERTEX_POSITION, 3 ) || !vertexBuffer.Lock( 3, false, vertexDesc ) ||
        !vertexDesc.m_pPosition )
        return false;
    memset( vertexDesc.m_pPosition, 0, 3 * vertexDesc.m_ActualVertexSize );
    vertexBuffer.Unlock( 3, vertexDesc );
    if ( !indexBuffer.Initialize( SHADER_BUFFER_TYPE_STATIC,
            MATERIAL_INDEX_FORMAT_16BIT, 3 ) ||
        !indexBuffer.Lock( 3, false, indexDesc ) || !indexDesc.m_pIndices )
        return false;
    indexDesc.m_pIndices[0] = 0;
    indexDesc.m_pIndices[1] = 1;
    indexDesc.m_pIndices[2] = 2;
    indexBuffer.Unlock( 3, indexDesc );
    return true;
}

extern "C" bool KisakPs4DynamicSourceBufferProbe()
{
    CPs4SourceVertexBuffer vertexBuffer;
    CPs4SourceIndexBuffer indexBuffer;
    VertexDesc_t vertexDesc = {};
    IndexDesc_t indexDesc = {};
    if ( !vertexBuffer.Initialize( SHADER_BUFFER_TYPE_DYNAMIC,
            VERTEX_POSITION, 3 ) || !vertexBuffer.Lock( 3, false, vertexDesc ) ||
        !vertexDesc.m_pPosition )
        return false;
    memset( vertexDesc.m_pPosition, 0, 3 * vertexDesc.m_ActualVertexSize );
    vertexBuffer.Unlock( 3, vertexDesc );
    if ( !indexBuffer.Initialize( SHADER_BUFFER_TYPE_DYNAMIC,
            MATERIAL_INDEX_FORMAT_16BIT, 3 ) ||
        !indexBuffer.Lock( 3, false, indexDesc ) || !indexDesc.m_pIndices )
        return false;
    indexDesc.m_pIndices[0] = 0;
    indexDesc.m_pIndices[1] = 1;
    indexDesc.m_pIndices[2] = 2;
    indexBuffer.Unlock( 3, indexDesc );
    return true;
}

extern "C" int KisakPs4SourceVertexFormatSize( VertexFormat_t format )
{
    VertexDesc_t desc = {};
    return ComputeVertexDesc< true >( 0, format, desc );
}

extern "C" void KisakPs4ComputeSourceVertexDescription( unsigned char *buffer,
    VertexFormat_t format, VertexDesc_t *desc )
{
    if ( desc )
        ComputeVertexDesc< false >( buffer, format, *desc );
}
