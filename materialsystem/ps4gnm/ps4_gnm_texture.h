#ifndef KISAK_PS4_GNM_TEXTURE_H
#define KISAK_PS4_GNM_TEXTURE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <gnm.h>

class CPs4GnmTexture
{
public:
    CPs4GnmTexture();
    ~CPs4GnmTexture();

    bool Initialize2D( void *memory, size_t memorySize, GnmDataFormat format,
        uint32_t width, uint32_t height, uint32_t mipCount, GnmTileMode tileMode,
        GnmGpuMode gpuMode );
    void Reset();
    bool UploadLinear( const void *source, size_t sourceRowBytes,
        uint32_t rowCount );
    bool CreateColorTargetView( GnmDataFormat format, uint32_t width,
        uint32_t height, GnmTileMode tileMode, GnmGpuMode gpuMode );
    bool BuildSamplerTable( const GnmSampler &sampler, void *storage,
        size_t storageSize, void **table ) const;
    static size_t SamplerTableSize()
    {
        return sizeof( GnmTexture ) + sizeof( GnmSampler );
    }
    static bool WriteSamplerTable( const GnmTexture &texture,
        const GnmSampler &sampler, void *storage, size_t storageSize,
        void **table )
    {
        if ( !storage || !table || storageSize < SamplerTableSize() )
            return false;
        memcpy( storage, &texture, sizeof( texture ) );
        memcpy( static_cast< uint8_t * >( storage ) + sizeof( texture ),
            &sampler, sizeof( sampler ) );
        *table = storage;
        return true;
    }

    bool IsValid() const { return m_valid; }
    const GnmTexture &Descriptor() const { return m_texture; }
    const GnmRenderTarget &ColorTarget() const { return m_colorTarget; }
    bool HasColorTarget() const { return m_colorTargetValid; }
    void *Data() const { return m_data; }
    uint64_t Size() const { return m_size; }
    uint32_t Alignment() const { return m_alignment; }
    static uint64_t TotalBackingBytes();

private:
    CPs4GnmTexture( const CPs4GnmTexture & ) = delete;
    CPs4GnmTexture &operator=( const CPs4GnmTexture & ) = delete;
    static void AddBackingBytes( uint64_t bytes );
    static void RemoveBackingBytes( uint64_t bytes );

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
    static uint64_t s_totalBackingBytes;
};

#endif
