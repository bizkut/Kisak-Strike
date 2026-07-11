#include "ps4_shader_manifest.h"

#include <assert.h>
#include <string.h>

int main()
{
    CPs4ShaderManifest manifest;
    const Ps4ShaderManifestKey vertex = {
        "vertexlitgeneric", PS4_SHADER_STAGE_VERTEX, 3, 7, 0x10203u
    };
    const Ps4ShaderManifestKey pixel = {
        "vertexlitgeneric", PS4_SHADER_STAGE_PIXEL, 3, 7, 0
    };
    assert( manifest.Register( vertex, "/app0/shaders/vertexlitgeneric_vs_3_7.sb" ) );
    assert( manifest.Register( pixel, "/app0/shaders/vertexlitgeneric_ps_3_7.sb" ) );
    assert( !manifest.Register( vertex, "/app0/shaders/duplicate.sb" ) );
    assert( manifest.Count() == 2 );

    const Ps4ShaderManifestEntry *found = manifest.Find( vertex );
    assert( found );
    assert( strcmp( found->path, "/app0/shaders/vertexlitgeneric_vs_3_7.sb" ) == 0 );

    Ps4ShaderManifestKey missing = vertex;
    missing.dynamicCombo = 8;
    assert( manifest.Find( missing ) == 0 );
    missing = vertex;
    missing.vertexFormat = 0x10204u;
    assert( manifest.Find( missing ) == 0 );
    missing = vertex;
    missing.stage = PS4_SHADER_STAGE_PIXEL;
    assert( manifest.Find( missing ) == 0 );

    manifest.Clear();
    assert( manifest.Count() == 0 && manifest.Find( vertex ) == 0 );
    return 0;
}
