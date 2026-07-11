#ifndef KISAK_PS4_GNM_TEXTURE_H
#define KISAK_PS4_GNM_TEXTURE_H

#include <stddef.h>
#include <stdint.h>

#include <gnm.h>

class CPs4GnmTexture
{
public:
    CPs4GnmTexture();

    bool Initialize2D( void *memory, size_t memorySize, GnmDataFormat format,
        uint32_t width, uint32_t height, uint32_t mipCount, GnmTileMode tileMode,
        GnmGpuMode gpuMode );
    void Reset();

    bool IsValid() const { return m_valid; }
    const GnmTexture &Descriptor() const { return m_texture; }
    void *Data() const { return m_data; }
    uint64_t Size() const { return m_size; }
    uint32_t Alignment() const { return m_alignment; }

private:
    GnmTexture m_texture;
    void *m_data;
    uint64_t m_size;
    uint32_t m_alignment;
    bool m_valid;
};

#endif
