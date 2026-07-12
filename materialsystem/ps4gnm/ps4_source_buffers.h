#ifndef KISAK_PS4_SOURCE_BUFFERS_H
#define KISAK_PS4_SOURCE_BUFFERS_H

#include "materialsystem/imesh.h"
#include "shaderapi/IShaderDevice.h"
#include "ps4_gnm_buffer.h"

class CPs4SourceVertexBuffer : public IVertexBuffer
{
public:
    CPs4SourceVertexBuffer();
    bool Initialize( ShaderBufferType_t type, VertexFormat_t format, int count );
    int VertexCount() const { return m_count; }
    VertexFormat_t GetVertexFormat() const { return m_format; }
    bool IsDynamic() const { return m_dynamic; }
    void BeginCastBuffer( VertexFormat_t format );
    void EndCastBuffer();
    int GetRoomRemaining() const { return m_count - m_written; }
    bool Lock( int count, bool append, VertexDesc_t &desc );
    void Unlock( int count, VertexDesc_t &desc );
    void Spew( int, const VertexDesc_t & ) {}
    void ValidateData( int, const VertexDesc_t & ) {}
    const CPs4GnmBuffer &NativeBuffer() const { return m_buffer; }

private:
    CPs4GnmBuffer m_buffer;
    VertexFormat_t m_format;
    int m_count;
    int m_stride;
    int m_written;
    int m_lockStart;
    int m_lockCapacity;
    unsigned int m_frameIndex;
    bool m_dynamic;
};

class CPs4SourceIndexBuffer : public IIndexBuffer
{
public:
    CPs4SourceIndexBuffer();
    bool Initialize( ShaderBufferType_t type, MaterialIndexFormat_t format,
        int count );
    int IndexCount() const { return m_count; }
    MaterialIndexFormat_t IndexFormat() const { return m_format; }
    bool IsDynamic() const { return m_dynamic; }
    void BeginCastBuffer( MaterialIndexFormat_t format );
    void EndCastBuffer();
    int GetRoomRemaining() const { return m_count - m_written; }
    bool Lock( int count, bool append, IndexDesc_t &desc );
    void Unlock( int count, IndexDesc_t &desc );
    void ModifyBegin( bool readOnly, int firstIndex, int count, IndexDesc_t &desc );
    void ModifyEnd( IndexDesc_t &desc );
    void Spew( int, const IndexDesc_t & ) {}
    void ValidateData( int, const IndexDesc_t & ) {}
    IMesh *GetMesh() { return 0; }
    const CPs4GnmBuffer &NativeBuffer() const { return m_buffer; }

private:
    CPs4GnmBuffer m_buffer;
    MaterialIndexFormat_t m_format;
    int m_count;
    int m_indexSize;
    int m_written;
    int m_lockStart;
    int m_lockCapacity;
    unsigned int m_frameIndex;
    bool m_dynamic;
};

#endif
