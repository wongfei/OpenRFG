// Original: volition/rfg/code/vehicle/vehicle_render.h
// Implementation: volition/rfg/code/vehicle/vehicle_render.cpp
//
// Vehicle render hooks: render updates of vehicles (lights, damage, spline-driven parts via
// vehicle_spline_render_update).
#pragma once

#include "volition/vlib/code/math/matrix.h"

// size 0x3004
struct vehicle_spline_render_update
{
    void add_transform(const matrix43& _transform); // 0x00B630E0 (.h:37)
    void clear(); // 0x00B63140 (.h:43)
    vehicle_spline_render_update(); // 0x00B70390

    matrix43 transform[256]; // 0x00
    int num_objects; // 0x3000
};

void vehicle_render_add_dynlights(); // 0x00B918D0 (:338)
vehicle_spline_render_update* vehicle_render_allocate_spline_render_update(); // 0x00B91950 (:378)
void vehicle_render_free_spline_render_update(vehicle_spline_render_update* vsru_p); // 0x00B990C0 (:387)

// 0x00B630E0 (.h:37)
inline void vehicle_spline_render_update::add_transform(const matrix43& _transform)
{
    if (num_objects < 256) {
        transform[num_objects] = _transform;
        num_objects++;
    }
}

// 0x00B63140 (.h:43)
inline void vehicle_spline_render_update::clear()
{
    num_objects = 0;
}

// 0x00B70390
inline vehicle_spline_render_update::vehicle_spline_render_update()
{
}
