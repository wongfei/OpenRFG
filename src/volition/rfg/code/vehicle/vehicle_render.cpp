// Original: volition/rfg/code/vehicle/vehicle_render.cpp
//
// The pool of per-frame track pad transforms (four vehicles' worth at a time). rfg.exe's dynamic lights of vehicles are
// compiled out: only the walk over the vehicles remains. Compiled in the vehicle unity TU.

#include "volition/rfg/code/vehicle/vehicle_render.h"

#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<vehicle_spline_render_update, 4, 1> Spline_render_update_pool; // 0x03770BB0

// 0x00B68BB0 (:125)
static void vehicle_add_lights(vehicle* vp)
{
    // empty in rfg.exe
}

// 0x00B918D0 (:338)
void vehicle_render_add_dynlights()
{
    // Original quirk kept: asks for the time of day for each occupied vehicle in view, and does nothing with it.
    for (int i = 0; i < World.get_num_objects<vehicle>(); i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->m_flags.m_occupied != 0 && vp->obj_flags._dont_use_me != 0) {
            game_clock_get_time_of_day();
        }
    }
}

// 0x00B91950 (:378)
vehicle_spline_render_update* vehicle_render_allocate_spline_render_update()
{
    vehicle_spline_render_update* update = Spline_render_update_pool.alloc(0);
    if (update != NULL) {
        update->num_objects = 0;
    }
    return update;
}

// 0x00B990C0 (:387)
void vehicle_render_free_spline_render_update(vehicle_spline_render_update* vsru_p)
{
    Spline_render_update_pool.free(vsru_p);
}
