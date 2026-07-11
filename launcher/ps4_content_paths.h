#ifndef KISAK_PS4_CONTENT_PATHS_H
#define KISAK_PS4_CONTENT_PATHS_H

#include <stddef.h>

enum { KISAK_PS4_CONTENT_PATH_MAX = 512 };

struct KisakPs4ContentLayout
{
	char packagedRoot[KISAK_PS4_CONTENT_PATH_MAX];
	char externalRoot[KISAK_PS4_CONTENT_PATH_MAX];
	char packagedGame[KISAK_PS4_CONTENT_PATH_MAX];
	char externalGame[KISAK_PS4_CONTENT_PATH_MAX];
	char packagedPlatform[KISAK_PS4_CONTENT_PATH_MAX];
	char externalPlatform[KISAK_PS4_CONTENT_PATH_MAX];
	char writeRoot[KISAK_PS4_CONTENT_PATH_MAX];
};

bool KisakPs4NormalizeRelativeContentPath( const char *input, char *output, size_t outputSize );
bool KisakPs4BuildContentLayout( const char *gameDirectory, KisakPs4ContentLayout *layout );

#endif
