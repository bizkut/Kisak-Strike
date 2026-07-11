#include "ps4_gnm_buffer.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main()
{
    alignas( 16 ) uint8_t storage[64] = {};
    CPs4GnmBuffer buffer;
    assert( !buffer.Initialize( storage + 1, 32, CPs4GnmBuffer::kVertexBuffer, false ) );
    assert( buffer.Initialize( storage, sizeof( storage ),
        CPs4GnmBuffer::kVertexBuffer, true ) );
    assert( buffer.Generation() == 1 );
    GnmBuffer descriptor = {};
    assert( buffer.BuildVertexDescriptor( GNM_FMT_R32G32B32A32_FLOAT,
        16, 4, 0, &descriptor ) );
    assert( sceGnmBufGetBaseAddress( &descriptor ) == storage );
    assert( descriptor.stride == 16 && descriptor.numrecords == 4 );
    assert( !buffer.BuildVertexDescriptor( GNM_FMT_R32G32B32A32_FLOAT,
        16, 5, 0, &descriptor ) );
    assert( buffer.BuildVertexDescriptor( GNM_FMT_R32G32B32A32_FLOAT,
        32, 2, 16, &descriptor ) );
    assert( sceGnmBufGetBaseAddress( &descriptor ) == storage + 16 );
    assert( descriptor.stride == 32 && descriptor.numrecords == 2 );
    assert( !buffer.BuildVertexDescriptor( GNM_FMT_R32G32B32A32_FLOAT,
        32, 3, 16, &descriptor ) );

    uint8_t source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    assert( buffer.Upload( 4, source, sizeof( source ) ) );
    assert( memcmp( storage + 4, source, sizeof( source ) ) == 0 );

    void *locked = 0;
    assert( buffer.Lock( 4, 8, false, &locked ) );
    assert( locked == storage + 4 );
    assert( !buffer.Lock( 0, 0, false, &locked ) );
    assert( !buffer.Upload( 0, source, sizeof( source ) ) );
    assert( buffer.Unlock() );
    assert( !buffer.Unlock() );

    assert( !buffer.Lock( 4, 8, true, &locked ) );
    assert( buffer.Lock( 0, 0, true, &locked ) );
    assert( buffer.Generation() == 2 );
    assert( buffer.Unlock() );

    CPs4GnmBuffer indices;
    assert( !indices.Initialize( storage, 6, CPs4GnmBuffer::kIndexBuffer,
        false, true ) );
    assert( indices.Initialize( storage, 8, CPs4GnmBuffer::kIndexBuffer,
        false, true ) );
    assert( indices.IsIndex32() );
    assert( !indices.Lock( 0, 0, true, &locked ) );
    indices.Reset();
    assert( !indices.IsValid() );
    return 0;
}
