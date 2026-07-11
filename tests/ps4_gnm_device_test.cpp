#include "ps4_gnm_device.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    uint8_t storage[1024];
    CPs4GnmDevice device;
    assert( !device.Initialize( 0, sizeof( storage ) ) );
    assert( device.Initialize( storage, sizeof( storage ) ) );
    assert( device.BeginFrame( 0 ) );

    void *constantData = device.FrameArena().Allocate( 64, 64 );
    assert( constantData );
    assert( reinterpret_cast< uintptr_t >( constantData ) % 64 == 0 );
    assert( !device.BeginFrame( 0 ) );
    assert( device.EndFrame() == 1 );

    assert( device.BeginFrame( 0 ) );
    assert( device.EndFrame() == 2 );
    assert( !device.BeginFrame( 0 ) );
    assert( device.BeginFrame( 1 ) );
    assert( device.FrameArena().Used() == 0 );
    device.CancelFrame();
    assert( device.BeginFrame( 1 ) );
    assert( device.EndFrame() == 3 );
    return 0;
}
