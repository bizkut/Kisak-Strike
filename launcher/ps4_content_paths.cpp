#include "ps4_content_paths.h"

#include <stdio.h>
#include <string.h>

namespace
{
const char kPackagedRoot[] = "/app0";
const char kExternalRoot[] = "/data/kisak-strike";
const char kDefaultGame[] = "csgo";

bool CopyPath( char *output, size_t outputSize, const char *value )
{
	if ( !output || outputSize == 0 || !value )
		return false;

	const size_t length = strlen( value );
	if ( length >= outputSize )
		return false;

	memcpy( output, value, length + 1 );
	return true;
}

bool JoinPath( const char *root, const char *relative, char *output, size_t outputSize )
{
	if ( !root || !relative || !output || outputSize == 0 )
		return false;

	const int written = snprintf( output, outputSize, "%s/%s", root, relative );
	return written >= 0 && static_cast<size_t>( written ) < outputSize;
}
}

bool KisakPs4NormalizeRelativeContentPath( const char *input, char *output, size_t outputSize )
{
	if ( !input || !output || outputSize == 0 || input[0] == '\0' ||
		input[0] == '/' || input[0] == '\\' )
	{
		return false;
	}

	size_t outputLength = 0;
	const char *cursor = input;
	while ( *cursor )
	{
		while ( *cursor == '/' || *cursor == '\\' )
			++cursor;
		if ( !*cursor )
			break;

		const char *segment = cursor;
		while ( *cursor && *cursor != '/' && *cursor != '\\' )
			++cursor;
		const size_t segmentLength = static_cast<size_t>( cursor - segment );

		if ( segmentLength == 1 && segment[0] == '.' )
			continue;
		if ( segmentLength == 2 && segment[0] == '.' && segment[1] == '.' )
			return false;

		for ( size_t i = 0; i < segmentLength; ++i )
		{
			if ( segment[i] == ':' )
				return false;
		}

		const size_t separatorLength = outputLength == 0 ? 0 : 1;
		if ( outputLength + separatorLength + segmentLength >= outputSize )
			return false;
		if ( separatorLength )
			output[outputLength++] = '/';
		memcpy( output + outputLength, segment, segmentLength );
		outputLength += segmentLength;
	}

	if ( outputLength == 0 )
		return false;
	output[outputLength] = '\0';
	return true;
}

bool KisakPs4BuildContentLayout( const char *gameDirectory, KisakPs4ContentLayout *layout )
{
	if ( !layout )
		return false;

	char normalizedGame[KISAK_PS4_CONTENT_PATH_MAX];
	const char *requestedGame = gameDirectory && gameDirectory[0] ? gameDirectory : kDefaultGame;
	if ( !KisakPs4NormalizeRelativeContentPath( requestedGame, normalizedGame, sizeof( normalizedGame ) ) )
		return false;

	memset( layout, 0, sizeof( *layout ) );
	return CopyPath( layout->packagedRoot, sizeof( layout->packagedRoot ), kPackagedRoot ) &&
		CopyPath( layout->externalRoot, sizeof( layout->externalRoot ), kExternalRoot ) &&
		JoinPath( kPackagedRoot, normalizedGame, layout->packagedGame, sizeof( layout->packagedGame ) ) &&
		JoinPath( kExternalRoot, normalizedGame, layout->externalGame, sizeof( layout->externalGame ) ) &&
		JoinPath( kPackagedRoot, "platform", layout->packagedPlatform, sizeof( layout->packagedPlatform ) ) &&
		JoinPath( kExternalRoot, "platform", layout->externalPlatform, sizeof( layout->externalPlatform ) ) &&
		CopyPath( layout->writeRoot, sizeof( layout->writeRoot ), kExternalRoot );
}
