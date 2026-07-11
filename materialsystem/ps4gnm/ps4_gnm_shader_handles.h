#ifndef KISAK_PS4_GNM_SHADER_HANDLES_H
#define KISAK_PS4_GNM_SHADER_HANDLES_H

#include <stdint.h>

class CPs4GnmShader;

enum Ps4GnmShaderHandleStage
{
    PS4_GNM_SHADER_HANDLE_VERTEX = 0,
    PS4_GNM_SHADER_HANDLE_PIXEL = 1
};

typedef uint32_t Ps4GnmShaderHandle;
const Ps4GnmShaderHandle PS4_GNM_SHADER_HANDLE_INVALID = 0;

class CPs4GnmShaderHandleTable
{
public:
    enum { kMaxHandles = 64 };

    CPs4GnmShaderHandleTable();
    Ps4GnmShaderHandle Register( CPs4GnmShader *shader, Ps4GnmShaderHandleStage stage );
    CPs4GnmShader *Resolve( Ps4GnmShaderHandle handle, Ps4GnmShaderHandleStage stage ) const;
    bool Destroy( Ps4GnmShaderHandle handle, Ps4GnmShaderHandleStage stage );
    uint32_t Count() const { return m_count; }

private:
    struct Entry
    {
        CPs4GnmShader *shader;
        uint16_t generation;
        uint8_t stage;
    };
    Entry m_entries[kMaxHandles];
    uint32_t m_count;
};

#endif
