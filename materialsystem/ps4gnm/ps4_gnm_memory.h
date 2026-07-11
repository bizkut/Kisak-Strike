#ifndef KISAK_PS4_GNM_MEMORY_H
#define KISAK_PS4_GNM_MEMORY_H

#include <stddef.h>
#include <stdint.h>

class CPs4GnmMemory
{
public:
    CPs4GnmMemory();

    bool Initialize( void *base, size_t size );
    void Reset();
    void *Allocate( size_t size, size_t alignment );

    size_t Capacity() const { return m_size; }
    size_t Used() const { return m_offset; }
    size_t Available() const { return m_size - m_offset; }

private:
    uint8_t *m_base;
    size_t m_size;
    size_t m_offset;
};

#endif
