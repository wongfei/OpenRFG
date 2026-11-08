// Original: volition/rfg/code/objects/object_render_distance.cpp
//
// The apparent (zoom-scaled) distance of an object from the camera, cached once per frame (player_minimal.md P4).
// The external interface is declared in object_render_distance.h.

#include "volition/rfg/code/objects/object_render_distance.h"

#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/vlib/code/os/frametime.h"

// 0x00A05670 (.h:20)
object_render_distance::object_render_distance()
{
    apparent_distance = 0.0f;
}

// 0x00A2C5C0 (.h:22)
float object_render_distance::get_apparent_distance(const vector& pos) const
{
    object_render_distance* self = const_cast<object_render_distance*>(this);
    if (Framecount != last_frame_processed)
    {
        self->last_frame_processed = Framecount;
        self->apparent_distance = game_render_get_apparent_distance_from_camera(pos);
    }
    return apparent_distance;
}

// 0x00A1B0D0 (:16)
void object_render_distance::update(const vector& pos)
{
    apparent_distance = game_render_get_apparent_distance_from_camera(pos);
}
