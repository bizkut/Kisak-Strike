#include "ps4_gnm_draw_packet.h"

#include "ps4_gnm_draw_state.h"

bool Ps4EmitIndexedDraw( GnmCommandBuffer *command, CPs4GnmDrawState *drawState,
    const CPs4GnmDevice::IndexedDrawPacket &packet, uint32_t vertexUserDataSlot,
    uint32_t *emittedDirtyMask )
{
    if ( !command || !drawState || !packet.indexAddress || !packet.indexCount )
        return false;

    drawState->SetVertexBuffer( GNM_STAGE_VS, vertexUserDataSlot,
        packet.vertexBuffer );
    drawState->SetIndexSize( packet.indexSize, GNM_POLICY_LRU );
    drawState->SetPrimitiveType( packet.primitiveType );
    const uint32_t emitted = drawState->Apply( command );
    sceGnmDrawCmdDrawIndex( command, packet.indexCount, packet.indexAddress );
    if ( emittedDirtyMask )
        *emittedDirtyMask = emitted;
    return true;
}
