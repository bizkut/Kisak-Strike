#ifndef KISAK_PS4_GNM_SHADER_H
#define KISAK_PS4_GNM_SHADER_H

#include <stddef.h>
#include <stdint.h>

#include <gnm_shaderbinary.h>

class CPs4GnmShader
{
public:
    enum { kMaxStageBytes = 1024 };

    CPs4GnmShader();
    void Reset();
    bool Initialize( const void *binary, size_t binarySize, GnmShaderType expectedType,
        void *codeMemory, size_t codeCapacity );

    bool IsValid() const { return m_valid; }
    GnmShaderType Type() const { return m_type; }
    uint32_t CodeSize() const { return m_codeSize; }
    const GnmShaderCommonData *Common() const;
    GnmVsShader *VertexShader();
    GnmPsShader *PixelShader();

private:
    uint8_t m_stage[kMaxStageBytes];
    GnmShaderType m_type;
    uint32_t m_codeSize;
    bool m_valid;
};

#endif
