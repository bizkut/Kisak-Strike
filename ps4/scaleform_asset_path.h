#pragma once

#include <stddef.h>

// GFx external movies use authored, case-sensitive relative URLs such as
// "Background.swf". PS4 package staging stores the flash closure below a
// lowercase resource/flash tree, so canonicalize only the packaged lookup.
bool KisakPs4NormalizeScaleformAssetUrl( const char *url, char *normalized,
    size_t normalizedCapacity );
