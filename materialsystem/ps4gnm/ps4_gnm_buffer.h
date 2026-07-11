#ifndef KISAK_PS4_GNM_BUFFER_H
#define KISAK_PS4_GNM_BUFFER_H

#include <stddef.h>
#include <stdint.h>

class CPs4GnmBuffer
{
public:
    enum Type
    {
        kVertexBuffer,
        kIndexBuffer
    };

    CPs4GnmBuffer();

    bool Initialize( void *memory, size_t size, Type type, bool dynamic,
        bool index32 = false );
    void Reset();
    bool Lock( size_t offset, size_t size, bool discard, void **data );
    bool Unlock();
    bool Upload( size_t offset, const void *source, size_t size );

    bool IsValid() const { return m_memory != 0; }
    bool IsLocked() const { return m_locked; }
    bool IsDynamic() const { return m_dynamic; }
    bool IsIndex32() const { return m_index32; }
    Type BufferType() const { return m_type; }
    void *Data() const { return m_memory; }
    size_t Size() const { return m_size; }
    uint32_t Generation() const { return m_generation; }

private:
    uint8_t *m_memory;
    size_t m_size;
    Type m_type;
    bool m_dynamic;
    bool m_index32;
    bool m_locked;
    uint32_t m_generation;
};

#endif
