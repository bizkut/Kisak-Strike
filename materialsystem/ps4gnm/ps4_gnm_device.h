#ifndef KISAK_PS4_GNM_DEVICE_H
#define KISAK_PS4_GNM_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "ps4_gnm_memory.h"
#include "ps4_gnm_buffer.h"
#include "ps4_gnm_vertex_declaration.h"

class CPs4GnmDevice
{
public:
    enum { kFrameCount = 2 };
    enum { kMaxVertexStreams = 8 };

    enum PrimitiveTopology
    {
        kPrimitiveTriangles,
        kPrimitiveTriangleStrip,
        kPrimitiveLines,
        kPrimitivePoints
    };

    struct StreamBinding
    {
        const void *buffer;
        size_t bufferSize;
        size_t offset;
        uint32_t stride;
        const CPs4GnmBuffer *resource;
    };

    struct IndexBinding
    {
        const void *buffer;
        size_t bufferSize;
        bool index32;
    };

    struct IndexedDrawPacket
    {
        GnmBuffer vertexBuffer;
        const void *indexAddress;
        uint32_t indexCount;
        GnmIndexSize indexSize;
        GnmPrimitiveType primitiveType;
    };

    struct SubmissionFrame
    {
        void *commandMemory;
        volatile uint64_t *completionLabel;
        uint64_t submittedLabel;
    };

    CPs4GnmDevice();

    bool Initialize( void *frameMemory, size_t frameMemorySize );
    bool BeginFrame( uint64_t completedLabel );
    void CancelFrame();
    uint64_t EndFrame();
    bool BeginSubmission( uint64_t completedLabel, size_t commandBytes,
        size_t commandAlignment, SubmissionFrame *submission );
    bool CommitSubmission( const SubmissionFrame &submission );
    bool BeginScene();
    bool EndScene();
    bool SetStreamSource( uint32_t stream, const void *buffer, size_t bufferSize,
        size_t offset, uint32_t stride );
    bool SetStreamSource( uint32_t stream, const CPs4GnmBuffer *buffer,
        size_t offset, uint32_t stride );
    bool SetIndices( const void *buffer, size_t bufferSize, bool index32 );
    bool SetIndices( const CPs4GnmBuffer *buffer );
    void SetPrimitiveTopology( PrimitiveTopology topology );
    bool ValidateDrawIndexed( uint32_t firstIndex, uint32_t indexCount,
        int32_t baseVertex, uint32_t vertexCount ) const;
    bool BuildIndexedDrawPacket( GnmDataFormat vertexFormat,
        uint32_t firstIndex, uint32_t indexCount, int32_t baseVertex,
        uint32_t vertexCount, IndexedDrawPacket *packet ) const;
    bool BuildVertexDescriptorTable( const CPs4GnmVertexDeclaration &declaration,
        int32_t baseVertex, uint32_t vertexCount, GnmBuffer *descriptors,
        uint32_t descriptorCapacity ) const;

    bool IsInitialized() const { return m_initialized; }
    bool IsFrameOpen() const { return m_frameOpen; }
    bool IsSceneOpen() const { return m_sceneOpen; }
    unsigned int CurrentFrame() const { return m_frameIndex; }
    uint64_t SubmittedLabel() const { return m_submittedLabel; }
    CPs4GnmMemory &FrameArena() { return m_frames[m_frameIndex].arena; }
    const StreamBinding &Stream( uint32_t stream ) const { return m_streams[stream]; }
    const IndexBinding &Indices() const { return m_indices; }
    PrimitiveTopology Topology() const { return m_topology; }

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
    bool m_sceneOpen;
    StreamBinding m_streams[kMaxVertexStreams];
    IndexBinding m_indices;
    PrimitiveTopology m_topology;
};

#endif
