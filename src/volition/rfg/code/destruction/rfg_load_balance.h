// Original: volition/rfg/code/destruction/rfg_load_balance.h
// Implementation: volition/rfg/code/destruction/rfg_load_balance.cpp
//
// Destruction load balancing: keeps the number of live movers and debris objects under budget by deleting the least
// important ones (rfgl_maybe_delete_objects), lowers the destruction quality under load (rfgl_desired_values), and in
// multiplayer throttles which movers are synced.
#pragma once


class object_mover;

// size 0x8
struct rfgl_balance_values
{
    int quality_count; // 0x00
    int delete_count; // 0x04
};

void rfgl_system_init(); // 0x00715630 (:641)
void rfgl_system_shutdown(); // 0x00715650 (:650)
void rfgl_level_init_pre(); // 0x00715670 (:659)
void rfgl_level_shutdown(); // 0x007156A0 (:668)
void rfgl_do_frame(); // 0x007250D0 (:673)
void rfgl_emergency_cleanup(); // 0x00725290 (:706)
void rfgl_object_created(object_mover* objp); // 0x007156B0 (:723)
void rfgl_object_destroyed(object_mover* objp); // 0x00715750 (:757)
void rfgl_desired_values(rfgl_balance_values& bvl, int incoming_objects, int flags); // 0x0071D080 (:773)
void rfgl_render_debug(); // 0x00715760 (:821)
