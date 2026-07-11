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
    bool UploadLinear( const void *source, size_t sourceRowBytes,
        uint32_t rowCount );
    bool CreateColorTargetView( GnmDataFormat format, uint32_t width,
        uint32_t height, GnmTileMode tileMode, GnmGpuMode gpuMode );

    bool IsValid() const { return m_valid; }
    const GnmTexture &Descriptor() const { return m_texture; }
    const GnmRenderTarget &ColorTarget() const { return m_colorTarget; }
    bool HasColorTarget() const { return m_colorTargetValid; }
    void *Data() const { return m_data; }
    uint64_t Size() const { return m_size; }
    uint32_t Alignment() const { return m_alignment; }

private:
    GnmTexture m_texture;
    GnmRenderTarget m_colorTarget;
    void *m_data;
    uint64_t m_size;
    uint64_t m_capacity;
    uint32_t m_alignment;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_bytesPerElement;
    bool m_valid;
    bool m_colorTargetValid;
};

#endif
