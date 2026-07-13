#include "../ps4/scaleform_asset_path.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char normalized[128];
    assert( KisakPs4NormalizeScaleformAssetUrl(
        "Background.swf", normalized, sizeof( normalized ) ) );
    assert( strcmp( normalized, "resource/flash/background.swf" ) == 0 );
    assert( KisakPs4NormalizeScaleformAssetUrl(
        "resource/flash/MainMenu.swf", normalized, sizeof( normalized ) ) );
    assert( strcmp( normalized, "resource/flash/mainmenu.swf" ) == 0 );
    assert( KisakPs4NormalizeScaleformAssetUrl(
        "/app0/resource/flash/FontLib.GFX?cache=1", normalized,
        sizeof( normalized ) ) );
    assert( strcmp( normalized, "resource/flash/fontlib.gfx" ) == 0 );
    assert( !KisakPs4NormalizeScaleformAssetUrl(
        "../Background.swf", normalized, sizeof( normalized ) ) );
    assert( !KisakPs4NormalizeScaleformAssetUrl(
        "https://example.invalid/movie.swf", normalized, sizeof( normalized ) ) );
    assert( !KisakPs4NormalizeScaleformAssetUrl(
        "Background.swf", normalized, 8 ) );

    unsigned char movieHeader[8] = { 'F', 'W', 'S', 8, 0x34, 0x12, 0, 0 };
    bool isDds = true;
    assert( KisakPs4ScaleformPackagedPayloadSize(
        movieHeader, sizeof( movieHeader ), &isDds ) == 0x1234 );
    assert( !isDds );

    unsigned char ddsHeader[128] = {};
    memcpy( ddsHeader, "DDS ", 4 );
    ddsHeader[4] = 124;
    ddsHeader[20] = 0x00;
    ddsHeader[21] = 0x00;
    ddsHeader[22] = 0x04;
    assert( KisakPs4ScaleformPackagedPayloadSize(
        ddsHeader, sizeof( ddsHeader ), &isDds ) == 262272 );
    assert( isDds );
    ddsHeader[4] = 0;
    assert( KisakPs4ScaleformPackagedPayloadSize(
        ddsHeader, sizeof( ddsHeader ), &isDds ) == 0 );
    assert( !isDds );

    puts( "PS4 Scaleform asset path tests passed" );
    return 0;
}
