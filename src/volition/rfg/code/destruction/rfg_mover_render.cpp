// Original: volition/rfg/code/destruction/rfg_mover_render.cpp
//
// The render update of rfg_movers (decompiled; header comment in rfg_mover.h). Compiled in rfg_destruction_unity.cpp.
// update_render jitters the pieces shaken by the mover's pending explosion and damaged pieces, applies the pending
// repairs, then does the object_mover render update.
//
// Partial: process_jitter only consumes the pending explosion and damaged subpiece events. The jitter itself is not
// recovered: in rfg.exe each hit piece of a jitter-enabled material (physical flag 0x400, not large, not base flag
// 0x20) within the explosion radius, or within Collision_jitter_max_depth links of a damaged piece, gets a random
// offset and rotation scaled by its distance (Jitter_info per source: explosion, nano explosion, collision), is marked
// jittered (0x20000, repaired later by process_repair_events) and optionally blends there through a mover fader. It
// only changes the render sub instances.

#include "volition/rfg/code/destruction/rfg_mover.h"

// 0x0073E4A0 (:111)
void rfg_mover::process_jitter()
{
    release_explosion_event();
    release_damaged_subpiece_events();
}

// 0x0073F3A0 (:497)
void rfg_mover::update_render(rl_renderer* pRenderer)
{
    process_jitter();
    process_repair_events();
    object_mover::update_render(pRenderer);
}
