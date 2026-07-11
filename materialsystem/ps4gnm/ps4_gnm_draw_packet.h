#ifndef KISAK_PS4_GNM_DRAW_PACKET_H
#define KISAK_PS4_GNM_DRAW_PACKET_H

#include "ps4_gnm_device.h"

class CPs4GnmDrawState;

bool Ps4EmitIndexedDraw( GnmCommandBuffer *command, CPs4GnmDrawState *drawState,
    const CPs4GnmDevice::IndexedDrawPacket &packet, uint32_t vertexUserDataSlot,
    uint32_t *emittedDirtyMask = 0 );

#endif
