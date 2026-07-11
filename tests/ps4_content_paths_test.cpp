#include "../launcher/ps4_content_paths.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
	char normalized[64];
	assert( KisakPs4NormalizeRelativeContentPath( "csgo", normalized, sizeof( normalized ) ) );
	assert( strcmp( normalized, "csgo" ) == 0 );
	assert( KisakPs4NormalizeRelativeContentPath( "./csgo//custom\\maps/", normalized, sizeof( normalized ) ) );
	assert( strcmp( normalized, "csgo/custom/maps" ) == 0 );
	assert( !KisakPs4NormalizeRelativeContentPath( "", normalized, sizeof( normalized ) ) );
	assert( !KisakPs4NormalizeRelativeContentPath( "/app0/csgo", normalized, sizeof( normalized ) ) );
	assert( !KisakPs4NormalizeRelativeContentPath( "../csgo", normalized, sizeof( normalized ) ) );
	assert( !KisakPs4NormalizeRelativeContentPath( "csgo/../platform", normalized, sizeof( normalized ) ) );
	assert( !KisakPs4NormalizeRelativeContentPath( "C:/csgo", normalized, sizeof( normalized ) ) );
	assert( !KisakPs4NormalizeRelativeContentPath( "csgo", normalized, 4 ) );

	KisakPs4ContentLayout layout;
	assert( KisakPs4BuildContentLayout( NULL, &layout ) );
	assert( strcmp( layout.packagedRoot, "/app0" ) == 0 );
	assert( strcmp( layout.externalRoot, "/data/kisak-strike" ) == 0 );
	assert( strcmp( layout.packagedGame, "/app0/csgo" ) == 0 );
	assert( strcmp( layout.externalGame, "/data/kisak-strike/csgo" ) == 0 );
	assert( strcmp( layout.packagedPlatform, "/app0/platform" ) == 0 );
	assert( strcmp( layout.externalPlatform, "/data/kisak-strike/platform" ) == 0 );
	assert( strcmp( layout.writeRoot, "/data/kisak-strike" ) == 0 );

	assert( KisakPs4BuildContentLayout( "csgo/custom", &layout ) );
	assert( strcmp( layout.packagedGame, "/app0/csgo/custom" ) == 0 );
	assert( strcmp( layout.externalGame, "/data/kisak-strike/csgo/custom" ) == 0 );
	assert( !KisakPs4BuildContentLayout( "../escape", &layout ) );
	assert( !KisakPs4BuildContentLayout( "csgo", NULL ) );

	puts( "PS4 content path tests passed" );
	return 0;
}
