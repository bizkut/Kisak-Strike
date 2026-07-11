#ifndef KISAK_PS4_SHADER_MANIFEST_H
#define KISAK_PS4_SHADER_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

enum Ps4ShaderStage
{
    PS4_SHADER_STAGE_VERTEX = 0,
    PS4_SHADER_STAGE_PIXEL = 1
};

struct Ps4ShaderManifestKey
{
    const char *name;
    Ps4ShaderStage stage;
    uint32_t staticCombo;
    uint32_t dynamicCombo;
    uint64_t vertexFormat;
};

struct Ps4ShaderManifestEntry
{
    char name[64];
    char path[128];
    Ps4ShaderStage stage;
    uint32_t staticCombo;
    uint32_t dynamicCombo;
    uint64_t vertexFormat;
};

class CPs4ShaderManifest
{
public:
    enum { kMaxEntries = 128 };

    CPs4ShaderManifest();
    void Clear();
    bool Register( const Ps4ShaderManifestKey &key, const char *path );
    const Ps4ShaderManifestEntry *Find( const Ps4ShaderManifestKey &key ) const;
    size_t Count() const { return m_count; }

private:
    Ps4ShaderManifestEntry m_entries[kMaxEntries];
    size_t m_count;
};

#endif
