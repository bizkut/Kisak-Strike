#include "ps4_gnm_constants.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    alignas( 256 ) uint8_t storage[4096] = {};
    CPs4GnmMemory arena;
    assert( arena.Initialize( storage, sizeof( storage ) ) );
    CPs4GnmConstants constants;
    const float values[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    assert( constants.SetFloat( CPs4GnmConstants::kVertex, 3, values, 2 ) );
    assert( constants.UsedRegisters( CPs4GnmConstants::kVertex ) == 5 );
    assert( !constants.SetFloat( CPs4GnmConstants::kPixel, 223, values, 2 ) );
    GnmBuffer buffer = {};
    assert( constants.BuildBuffer( CPs4GnmConstants::kVertex, &arena, &buffer ) );
    assert( reinterpret_cast< uintptr_t >( sceGnmBufGetBaseAddress( &buffer ) ) % 256 == 0 );
    assert( buffer.numrecords == 5 );
    const float *snapshot = static_cast< const float * >( sceGnmBufGetBaseAddress( &buffer ) );
    assert( snapshot[12] == 1 && snapshot[19] == 8 );
    constants.Reset();
    assert( !constants.BuildBuffer( CPs4GnmConstants::kVertex, &arena, &buffer ) );
    return 0;
}
