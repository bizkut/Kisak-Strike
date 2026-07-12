#include "ps4_gnm_draw_packet.h"

#include "ps4_gnm_draw_state.h"

bool Ps4EmitIndexedDraw( GnmCommandBuffer *command, CPs4GnmDrawState *drawState,
    const CPs4GnmDevice::IndexedDrawPacket &packet, uint32_t vertexUserDataSlot,
    uint32_t *emittedDirtyMask )
{
    if ( !command || !drawState || !packet.indexAddress || !packet.indexCount )
        return false;

    if ( vertexUserDataSlot != UINT32_MAX )
        drawState->SetVertexBuffer( GNM_STAGE_VS, vertexUserDataSlot,
            packet.vertexBuffer );
    drawState->SetIndexSize( packet.indexSize, GNM_POLICY_LRU );
    drawState->SetPrimitiveType( packet.primitiveType );
    const uint32_t emitted = drawState->Apply( command );
    if ( emitted & CPs4GnmDrawState::kDirtyBlend )
        drawState->ReassertBlendControl( command );
    sceGnmDrawCmdDrawIndex( command, packet.indexCount, packet.indexAddress );
    if ( emittedDirtyMask )
        *emittedDirtyMask = emitted;
    return true;
}

bool Ps4EmitPrimitiveDraw( GnmCommandBuffer *command,
    CPs4GnmDrawState *drawState,
    const CPs4GnmDevice::PrimitiveDrawPacket &packet,
    uint32_t *emittedDirtyMask )
{
    if ( !command || !drawState || !packet.vertexCount )
        return false;
    drawState->SetPrimitiveType( packet.primitiveType );
    const uint32_t emitted = drawState->Apply( command );
    if ( emitted & CPs4GnmDrawState::kDirtyBlend )
        drawState->ReassertBlendControl( command );
    sceGnmDrawCmdDrawIndexAuto( command, packet.vertexCount );
    if ( emittedDirtyMask )
        *emittedDirtyMask = emitted;
    return true;
}
