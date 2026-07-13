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

    puts( "PS4 Scaleform asset path tests passed" );
    return 0;
}
