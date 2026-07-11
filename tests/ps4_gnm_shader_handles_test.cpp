#include "ps4_gnm_shader_handles.h"

#include <assert.h>

int main()
{
    CPs4GnmShaderHandleTable table;
    CPs4GnmShader *vertex = reinterpret_cast< CPs4GnmShader * >( 0x1000 );
    CPs4GnmShader *pixel = reinterpret_cast< CPs4GnmShader * >( 0x2000 );
    const Ps4GnmShaderHandle vertexHandle = table.Register(
        vertex, PS4_GNM_SHADER_HANDLE_VERTEX );
    const Ps4GnmShaderHandle pixelHandle = table.Register(
        pixel, PS4_GNM_SHADER_HANDLE_PIXEL );
    assert( vertexHandle && pixelHandle && vertexHandle != pixelHandle );
    assert( table.Count() == 2 );
    assert( table.Register( vertex, PS4_GNM_SHADER_HANDLE_VERTEX ) == vertexHandle );
    assert( table.Resolve( vertexHandle, PS4_GNM_SHADER_HANDLE_VERTEX ) == vertex );
    assert( table.Resolve( vertexHandle, PS4_GNM_SHADER_HANDLE_PIXEL ) == 0 );
    assert( table.Destroy( vertexHandle, PS4_GNM_SHADER_HANDLE_VERTEX ) );
    assert( table.Resolve( vertexHandle, PS4_GNM_SHADER_HANDLE_VERTEX ) == 0 );
    const Ps4GnmShaderHandle replacement = table.Register(
        vertex, PS4_GNM_SHADER_HANDLE_VERTEX );
    assert( replacement && replacement != vertexHandle );
    assert( !table.Destroy( vertexHandle, PS4_GNM_SHADER_HANDLE_VERTEX ) );
    return 0;
}
