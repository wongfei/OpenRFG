// Original: volition/rfg/code/vehicle_ai/vehicle_offscreen_pathfollow.cpp
//
// A hidden vehicle does not drive: it slides towards its steering goal at a speed taken from the route.

#include "volition/rfg/code/vehicle_ai/vehicle_offscreen_pathfollow.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/os/frametime.h"

static const float HIDDEN_SPEED_SCALE = 0.75f;
static const float HIDDEN_MAX_SPEED = 26.8224f; // 60 mph

// 0x00BCA4A0 (:94)
float vehicle_hidden_calc_speed(vehicle_ai* vai)
{
    vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    if (vehicle_passenger_in_seat(vp, VSI_DRIVER) == NULL)
    {
        return 0.0f;
    }
    const route_segment* segment = vehicle_ai_get_center_route_segment(vai->route_info);
    if (segment == NULL)
    {
        return 0.0f;
    }
    float speed = (vai->vai_flags.m_override_speed ? vai->override_speed : segment->speed_limit) * HIDDEN_SPEED_SCALE;
    if (speed < 0.0f)
    {
        speed = 0.0f;
    }
    if (speed >= HIDDEN_MAX_SPEED)
    {
        speed = HIDDEN_MAX_SPEED;
    }
    return speed;
}

// 0x00BCA580 (:133)
void vehicle_hidden_do_movement(vehicle_ai* vai)
{
    if (vai == NULL)
    {
        return;
    }
    vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    if (vp == NULL)
    {
        return;
    }
    const float step = vehicle_hidden_calc_speed(vai) * Frametime;
    vector new_pos = vai->steer_info.goal_pos;
    vector to_goal = new_pos - vp->pos;
    vector flat(to_goal.x, 0.0f, to_goal.z);
    const float dist = to_goal.mag_and_normalize();
    matrix orient;
    if (flat.mag_and_normalize() >= 0.1f)
    {
        orient.make(flat);
    }
    else
    {
        orient = vp->orient;
    }
    if (step <= dist)
    {
        new_pos = vp->pos + to_goal * step;
    }
    vp->update_pos_and_orient(new_pos, orient, true);
    hkpRigidBody* body = vehicle_get_rigid_body(vp->get_handle());
    if (body != NULL)
    {
        // Stops the body: it moved by teleport.
        const hkVector4 zero(0.0f, 0.0f, 0.0f, 0.0f);
        body->setLinearVelocity(zero);
        body->setAngularVelocity(zero);
    }
}
