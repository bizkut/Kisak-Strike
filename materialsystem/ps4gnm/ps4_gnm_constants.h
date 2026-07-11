#ifndef KISAK_PS4_GNM_CONSTANTS_H
#define KISAK_PS4_GNM_CONSTANTS_H

#include "ps4_gnm_memory.h"

#include <gnm.h>
#include <stdint.h>

class CPs4GnmConstants
{
public:
    enum Stage { kVertex, kPixel };
    enum { kVertexRegisters = 256, kPixelRegisters = 224 };

    CPs4GnmConstants();
    void Reset();
    bool SetFloat( Stage stage, uint32_t startRegister, const float *values,
        uint32_t registerCount );
    bool BuildBuffer( Stage stage, CPs4GnmMemory *arena, GnmBuffer *buffer );
    uint32_t UsedRegisters( Stage stage ) const;

private:
    float m_vertex[kVertexRegisters][4];
    float m_pixel[kPixelRegisters][4];
    uint32_t m_vertexUsed;
    uint32_t m_pixelUsed;
};

#endif
