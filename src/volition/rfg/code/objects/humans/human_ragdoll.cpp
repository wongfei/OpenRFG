// Original: volition/rfg/code/objects/humans/human_ragdoll.cpp
//
// Human ragdolls (decompiled; multiplayer.md MP7): starting one (human_ragdoll_start binds a havok_ragdoll slot and
// puts the human in the knockdown movement mode; flinches and active-on-impact ragdolls only bind), the knockdown
// timer, stopping (unbind, or keep the pose: deactivated or pose saved) and getting up: the human is moved to a place
// to stand (human_find_place_to_stand scores up to 128 candidate positions around the ragdoll and checks the best
// ones with human_ragdoll_get_up_check::get_errors: terrain, a walking surface below, a line of sight from the
// ragdoll and room for the body) and plays a get up (or fetal) animation facing along the spine.
// Compiled in rfg_objects_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - human_find_place_to_stand_check scores the remaining candidates around the last checked one by its error; for an
//   error without a case (too low) it reuses the previous center, and for a vehicle it uses the vehicle's half
//   diagonal from the previous vehicle check (computed before the bounding box is fetched).
// - human_ragdoll_get_up_check::get_errors reads the terrain height even when there is no terrain there, and sets
//   the vehicle error of a blocking object found by the capsule check from the object of the last ground hit.
// - human_ragdoll_get_up: a place found within 2 m is only used as the position for update_pos_and_orient.

#include "volition/rfg/code/objects/humans/human_ragdoll.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

#include <stdlib.h>

// ---- file-static globals
// float STUCK_UNDER_CAR_DAMAGE_PCT (0x016AFE5C, .data; placement inferred): the share of the maximum hit points a human
// pinned under an object, or without a place to stand, loses when it tries to get up.
static float STUCK_UNDER_CAR_DAMAGE_PCT = 0.1f;
// const float SILENT_DEATH_TREAT_AS_NORMAL_DIST_SQ (0x01310FFC) is used here too; placed in player.cpp.
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
// player* Player (0x03023874) is used here too; placed in player.h.
// unsigned int $S10 (0x035B5A5C) guards the function-local static of human_ragdoll_start_impulse.

// The get up search's candidate count (the size of its search array).
static const int MAX_GET_UP_CHECKS = 128;

// ---- human_ragdoll_get_up_check
// 0x00A0B900 (:97)
human_ragdoll_get_up_check::human_ragdoll_get_up_check()
{
    error = GET_UP_ERROR_NOT_CHECKED;
    blocking_object = nullptr;
}

// 0x00A0B920 (:101)
const vector& human_ragdoll_get_up_check::get_position()
{
    return position;
}

// 0x00A0BAB0 (:231)
// Sphere casts ignore objects a character pushes away and the human itself (its handle in info).
bool human_ragdoll_get_up_spherecast_cb(object* hit_objp, void* info)
{
    if (hit_objp == nullptr || character_controller_should_push_away(hit_objp, false))
    {
        return false;
    }
    return (void*)hit_objp->get_handle() != info;
}

// 0x00A0BB10 (:256)
void human_ragdoll_get_up_check::set(vector _position, unsigned int _ignore_obj, float _eval_fitness)
{
    position = _position;
    ignore_handle = _ignore_obj;
    eval_fitness = _eval_fitness;
    error = GET_UP_ERROR_NOT_CHECKED;
}

// 0x00A0BAE0 (:252)
bool human_ragdoll_get_up_collect_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp == nullptr || character_controller_should_push_away(hit_objp, false))
    {
        return false;
    }
    return (const void*)hit_objp->get_handle() != user_data;
}

// 0x00A80DE0 (:288)
// Whether a human could stand at position: on terrain no more than 0.1 above it (lifted 0.01 onto it), visible from
// from_pos at chest height, on a walking surface within 1.33 below (a sphere cast; a steep hit or one at head height
// is an object in the way) and with a free capsule above. destination is where it would stand (or the problem).
void human_ragdoll_get_up_check::get_errors(vector from_pos)
{
    destination = position;
    error = GET_UP_ERROR_NONE;
    object* ground_obj = nullptr;
    raycast_input in;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    float terrain_y;
    havok_terrain_height(terrain_y, position.x, position.z, false, nullptr);
    // Magic numbers: 0.1 below the terrain is bad terrain, within 0.01 the position is lifted onto it
    if (position.y + 0.1f < terrain_y)
    {
        destination.y = terrain_y;
        error = GET_UP_ERROR_BAD_TERRAIN;
        return;
    }
    if (position.y - 0.01f < terrain_y)
    {
        position.y = terrain_y + 0.01f;
        destination.y = terrain_y + 0.01f;
    }
    // the line of sight from the ragdoll, 0.5 up
    in.start = vector(from_pos.x, from_pos.y + 0.5f, from_pos.z);
    in.end = vector(destination.x, destination.y + 0.5f, destination.z);
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_filter_info = COLLIDE_HUMAN_MINIMUM;
    if (in.start.dist_squared(in.end) > 0.001f && havok_raycast(in, out))
    {
        destination = out.m_hits[0].hit_pos;
        error = GET_UP_ERROR_BAD_TERRAIN;
        return;
    }
    out.reset(true);
    // the ground: a 0.35 sphere from 0.6 above down to 1.33 below
    in.start = vector(position.x, position.y + 0.6f, position.z);
    in.end = vector(position.x, position.y - 1.33f, position.z);
    in.radius = 0.35f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_filter_info = COLLIDE_RAY_WALKING_SURFACE_ONLY;
    in.m_flags = 0;
    if (!havok_spherecast(in, out))
    {
        destination.y = destination.y - 1.33f;
        error = GET_UP_ERROR_TOO_LOW;
        return;
    }
    error = GET_UP_ERROR_BAD_TERRAIN;
    const int num_hits = out.m_hits.getSize();
    for (int i = 0; i < num_hits; i++)
    {
        ground_obj = World.object_from_handle(out.m_hits[i].hit_object_handle);
        if (ground_obj == nullptr || ignore_handle == ground_obj->get_handle())
        {
            continue;
        }
        // Magic number: 0.5, a ground normal steeper than 60 degrees is an object in the way
        if (out.m_hits[i].hit_normal.y < 0.5f)
        {
            blocking_object = World.object_from_handle(out.m_hits[i].hit_object_handle);
            if (blocking_object == nullptr)
            {
                error = GET_UP_ERROR_BAD_TERRAIN;
                return;
            }
            error = blocking_object->get_obj_type() == OT_VEHICLE ? GET_UP_ERROR_X_VEHICLE : GET_UP_ERROR_X_OBJECT;
            return;
        }
        destination.y = out.m_hits[i].hit_center_pos.y - in.radius;
        if (in.start.y + in.radius <= out.m_hits[i].hit_pos.y)
        {
            blocking_object = ground_obj;
            if (ground_obj->get_obj_type() == OT_VEHICLE)
            {
                error = GET_UP_ERROR_X_VEHICLE;
            }
        }
        else
        {
            error = GET_UP_ERROR_NONE;
        }
        break;
    }
    out.reset(true);
    if (error != GET_UP_ERROR_NONE)
    {
        return;
    }
    // room for the body: a 0.35 capsule from 0.5 to 0.9 above the destination
    {
        raycast_input rin;
        rin.start = vector(destination.x, destination.y + 0.5f, destination.z);
        rin.end = vector(destination.x, destination.y + 0.9f, destination.z);
        rin.radius = 0.35f;
        rin.filter_params = (void*)ignore_handle;
        rin.filter_callback = human_ragdoll_get_up_spherecast_cb;
        rin.m_filter_info = COLLIDE_RAY_DEFAULT;
        rin.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        rin.m_flags = 0;
        unsigned int blocker = 0xFFFFFFFF;
        if (havok_collect_objects_in_capsule(rin, &blocker, 1, human_ragdoll_get_up_collect_cb) != 0)
        {
            blocking_object = World.object_from_handle(blocker);
            destination = blocking_object->pos;
            // Original: the vehicle test reads the last ground hit's object, not the blocker
            error = ground_obj->get_obj_type() == OT_VEHICLE ? GET_UP_ERROR_X_VEHICLE : GET_UP_ERROR_X_OBJECT;
            return;
        }
    }
    if (error != GET_UP_ERROR_NONE)
    {
        return;
    }
    // the line of sight to the final destination
    in.start = vector(from_pos.x, from_pos.y + 0.5f, from_pos.z);
    in.end = vector(destination.x, destination.y + 0.5f, destination.z);
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_flags = 0;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_filter_info = COLLIDE_HUMAN_MINIMUM;
    if (havok_raycast(in, out))
    {
        destination = out.m_hits[0].hit_pos;
        error = GET_UP_ERROR_BAD_TERRAIN;
    }
}

// 0x00A0B930 (:159)
// The object (over 1000 kg in all, above the upper body) pinning a ragdoll that touches something at or above it.
static const object* const human_crushed_under_object(human* hp)
{
    vector upper;
    havok_ragdoll_get_pos(hp->ragdoll_idx, upper, RAGDOLL_UPPER_BODY);
    contact_node* node = hp->contact_info.m_contact_list;
    for (;;)
    {
        if (node == nullptr)
        {
            return nullptr;
        }
        object* contacted = World.object_from_handle(node->m_contacted_object);
        if (upper.y <= contacted->pos.y)
        {
            break;
        }
        node = node->next;
        if (node == hp->contact_info.m_contact_list)
        {
            return nullptr;
        }
    }
    // Magic numbers: a 0.5 x 0.9 x 0.5 box from 0.25 above the upper body, and 1000 kg
    upper.y = upper.y + 0.25f;
    const vector bmin(-0.25f, 0.0f, -0.25f);
    const vector bmax(0.25f, 0.9f, 0.25f);
    unsigned int objects[20];
    const int num = havok_collect_objects(bmin, bmax, upper, IDENTITY_MATRIX, objects, 20, COLLIDE_OBJECT_COLLECTOR, nullptr, nullptr);
    float mass = 0.0f;
    for (int i = 0; i < num; i++)
    {
        if (objects[i] == hp->get_handle())
        {
            continue;
        }
        object* objp = World.object_from_handle(objects[i]);
        if (objp == nullptr)
        {
            continue;
        }
        mass = (float)((double)objp->get_mass() + (double)mass);
        if (mass > 1000.0f)
        {
            return objp;
        }
    }
    return nullptr;
}

// 0x00A81410 (:527)
// LTCG: _human_pos in EAX. Checks the best scored (lowest eval_fitness) unchecked candidate, at most 32 times; after a
// failure the others are rescored around the failure (closer is worse) or, for a vehicle, inside its box.
static bool human_find_place_to_stand_check(vector& destination, const vector& _human_pos, human_ragdoll_get_up_check* search_locations, int num_get_up_checks)
{
    vector from = _human_pos;
    float terrain_y;
    // Magic number: 0.05, the start lifted above the terrain
    if (havok_terrain_height(terrain_y, from.x, from.z, false, nullptr) && (terrain_y + 0.05f) - from.y > 0.0f)
    {
        from.y = terrain_y + 0.05f;
    }
    vector center(0.0f, 0.0f, 0.0f); // (uninitialized in the binary until a scored error)
    vector vehicle_min(0.0f, 0.0f, 0.0f);
    vector vehicle_max(0.0f, 0.0f, 0.0f);
    for (int tries = 32; ; ) // Magic number: 32 candidates checked at most
    {
        float best = 3.4028235e+38f; // FLT_MAX
        human_ragdoll_get_up_check* check = nullptr;
        for (int i = 0; i < num_get_up_checks; i++)
        {
            human_ragdoll_get_up_check& c = search_locations[i];
            if ((c.error == GET_UP_ERROR_NOT_CHECKED || c.error == GET_UP_ERROR_UNDER_BBOX) && c.eval_fitness < best)
            {
                check = &c;
                best = c.eval_fitness;
            }
        }
        if (check == nullptr)
        {
            return false;
        }
        check->get_errors(from);
        switch (check->error)
        {
        case GET_UP_ERROR_NONE:
            destination = check->destination;
            return true;
        case GET_UP_ERROR_BAD_TERRAIN:
            center = check->get_position();
            break;
        case GET_UP_ERROR_X_OBJECT:
            center = check->destination;
            break;
        case GET_UP_ERROR_X_VEHICLE:
        {
            // Original: the half diagonal comes from the previous vehicle's box (see the file comment)
            const float hx = (vehicle_max.x - vehicle_min.x) * 0.5f;
            const float hy = (vehicle_max.y - vehicle_min.y) * 0.5f;
            const float hz = (vehicle_max.z - vehicle_min.z) * 0.5f;
            const float half_diagonal2 = hx * hx + hy * hy + hz * hz;
            check->blocking_object->get_local_bbox(vehicle_min, vehicle_max);
            for (int i = 0; i < num_get_up_checks; i++)
            {
                human_ragdoll_get_up_check& c = search_locations[i];
                if (c.error != GET_UP_ERROR_NOT_CHECKED && c.error != GET_UP_ERROR_UNDER_BBOX)
                {
                    continue;
                }
                const vector p = c.get_position();
                if (ix_point_in_box_oriented(p, check->blocking_object->pos, check->blocking_object->orient, vehicle_min, vehicle_max))
                {
                    c.eval_fitness = (half_diagonal2 - vec_dist_squared(p, check->blocking_object->pos)) + c.eval_fitness;
                }
            }
            if (--tries < 1)
            {
                return false;
            }
            continue;
        }
        default:
            break;
        }
        // Magic numbers: within 0.5 m +5, out to 3 m up to +5 (quadratic), beyond nothing
        for (int i = 0; i < num_get_up_checks; i++)
        {
            human_ragdoll_get_up_check& c = search_locations[i];
            if (c.error != GET_UP_ERROR_NOT_CHECKED && c.error != GET_UP_ERROR_UNDER_BBOX)
            {
                continue;
            }
            const float d2 = vec_dist_squared(center, c.get_position());
            if (d2 < 0.25f)
            {
                c.eval_fitness = c.eval_fitness + 5.0f;
            }
            else if (d2 <= 9.0f)
            {
                const float f = (9.0f - d2) / 8.75f;
                c.eval_fitness = f * f * 5.0f + c.eval_fitness;
            }
            else
            {
                c.eval_fitness = c.eval_fitness + 0.0f;
            }
        }
        if (--tries < 1)
        {
            return false;
        }
    }
}

// 0x00A818A0 (:641)
// The listed points as candidates (at most 128), best first by their distance from human_pos.
bool human_find_place_to_stand_from_list(vector& destination, const vector& human_pos, const vector* const vec_list, int num_points, unsigned int ignore_obj)
{
    if (num_points == 0)
    {
        return false;
    }
    if (num_points > MAX_GET_UP_CHECKS)
    {
        num_points = MAX_GET_UP_CHECKS;
    }
    human_ragdoll_get_up_check checks[MAX_GET_UP_CHECKS];
    for (int i = 0; i < num_points; i++)
    {
        const float d2 = vec_dist_squared(human_pos, vec_list[i]);
        checks[i].set(vec_list[i], ignore_obj, d2);
    }
    return human_find_place_to_stand_check(destination, human_pos, checks, num_points);
}

// One candidate of the search (inlined human_ragdoll_get_up_check::set).
static void add_get_up_check(human_ragdoll_get_up_check* checks, int& count, const vector& pos, unsigned int ignore, float fitness)
{
    checks[count].set(pos, ignore, fitness);
    count++;
}

// 0x00A819D0 (:669)
// A place to stand near the ragdoll's upper body (or hint_pos): the start itself, else the best of the last valid
// position before the ragdoll, 4 points at 0.75 m and 6 at 1.25 m around it (away from where the start failed), 112
// random points out to rand_check_radius and, when a vehicle is in the way, points beside the vehicle.
bool human_find_place_to_stand(vector& destination, human* p_human, float rand_check_radius, const vector* const hint_pos)
{
    human_ragdoll_get_up_check checks[MAX_GET_UP_CHECKS];
    vector from;
    if (hint_pos != nullptr)
    {
        from = *hint_pos;
    }
    else if (p_human->ragdoll_idx < 0)
    {
        from = p_human->pos;
    }
    else
    {
        havok_ragdoll_get_pos(p_human->ragdoll_idx, from, RAGDOLL_UPPER_BODY);
    }
    float terrain_y;
    if (havok_terrain_height(terrain_y, from.x, from.z, false, nullptr) && (terrain_y + 0.05f) - from.y > 0.0f)
    {
        from.y = terrain_y + 0.05f;
    }
    const unsigned int handle = p_human->get_handle();
    checks[0].set(from, handle, 0.0f);
    checks[0].get_errors(from);
    if (checks[0].error == GET_UP_ERROR_NONE)
    {
        destination = checks[0].destination;
        return true;
    }
    int count = 1;
    const vector& last_valid = p_human->last_valid_pos_before_ragdoll[p_human->last_valid_pos_before_ragdoll_index];
    add_get_up_check(checks, count, last_valid, handle, vec_dist_squared(from, last_valid));
    // Magic numbers: 4 points 0.75 m away, turned by 0, -90, 180, -270 degrees in turn (pi/2 = 1.5707964)
    vector offset(from.x - checks[0].destination.x, from.y - checks[0].destination.y, from.z - checks[0].destination.z);
    offset.flatten_normalize_safe();
    offset = vector(offset.x * 0.75f, offset.y * 0.75f, offset.z * 0.75f);
    for (int i = 0; i < 4; i++)
    {
        offset.rotate_around_y_axis((float)i * 1.5707964f * ((i & 1) == 0 ? 1.0f : -1.0f));
        add_get_up_check(checks, count, vector(offset.x + from.x, offset.y + from.y, offset.z + from.z), handle, offset.y * offset.y + offset.x * offset.x + offset.z * offset.z);
    }
    // Magic numbers: 6 points 1.25 m away, turned by multiples of 60 degrees (1.0471976)
    offset.flatten_normalize_safe();
    offset = vector(offset.x * 1.25f, offset.y * 1.25f, offset.z * 1.25f);
    for (int i = 0; i < 6; i++)
    {
        offset.rotate_around_y_axis((float)i * 1.0471976f * ((i & 1) == 0 ? 1.0f : -1.0f));
        add_get_up_check(checks, count, vector(offset.x + from.x, offset.y + from.y, offset.z + from.z), handle, offset.y * offset.y + offset.x * offset.x + offset.z * offset.z);
    }
    if (rand_check_radius > 0.0f)
    {
        // Magic numbers: 112 random points from 0.5 m out, scored by height change and distance (+1)
        for (int i = 0; i < 112; i++)
        {
            vector r;
            r.rand_sphere();
            const float s = fl_rand_range(0.5f, rand_check_radius);
            r = vector(r.x * s, r.y * s, r.z * s);
            add_get_up_check(checks, count, vector(r.x + from.x, r.y + from.y, r.z + from.z), handle, fabsf(r.y) + s * s + 1.0f);
        }
    }
    if (checks[0].error == GET_UP_ERROR_X_VEHICLE)
    {
        // points inside the vehicle score worse; then up to 4 points beside it (not on the side it moves to)
        object* vp = (object*)checks[0].blocking_object;
        vector lmin;
        vector lmax;
        vp->get_local_bbox(lmin, lmax);
        matrix flat;
        flat.make_uvec(vp->orient.fvec, vector(0.0f, 1.0f, 0.0f));
        const matrix rel = vp->orient.copy_transpose() * flat;
        vector bmin;
        vector bmax;
        bbox_instance(bmin, bmax, rel, ZERO_VECTOR, lmin, lmax);
        const vector base(vp->pos.x, from.y, vp->pos.z);
        vector box_min;
        vector box_max;
        vp->get_local_bbox(box_min, box_max);
        const float hx = (box_max.x - box_min.x) * 0.5f;
        const float hy = (box_max.y - box_min.y) * 0.5f;
        const float hz = (box_max.z - box_min.z) * 0.5f;
        const float half_diagonal2 = hx * hx + hy * hy + hz * hz;
        for (int i = 0; i < count; i++)
        {
            if (checks[i].error != GET_UP_ERROR_NOT_CHECKED)
            {
                continue;
            }
            const vector p = checks[i].get_position();
            if (ix_point_in_box_oriented(p, vp->pos, vp->orient, box_min, box_max))
            {
                checks[i].eval_fitness = (half_diagonal2 - vec_dist_squared(p, vp->pos)) + checks[i].eval_fitness;
            }
        }
        // Magic numbers: 0.55 m of room beside the box, 0.22352 m/s (0.5 mph) of side / forward speed
        bmin = vector(bmin.x - 0.55f, bmin.y - 0.55f, bmin.z - 0.55f);
        bmax = vector(bmax.x + 0.55f, bmax.y + 0.55f, bmax.z + 0.55f);
        vector vel;
        vehicle_get_velocity((const vehicle*)vp, vel);
        const float side = vp->orient.rvec.y * vel.y + vel.x * vp->orient.rvec.x + vp->orient.rvec.z * vel.z;
        if (side <= 0.22352f)
        {
            const vector p(base.x + flat.rvec.x * bmax.x, flat.rvec.y * bmax.x + base.y, base.z + flat.rvec.z * bmax.x);
            add_get_up_check(checks, count, p, vp->get_handle(), vec_dist_squared(from, p) * 0.9f);
        }
        if (side >= -0.22352f)
        {
            const vector p(flat.rvec.x * bmin.x + base.x, flat.rvec.y * bmin.x + base.y, flat.rvec.z * bmin.x + base.z);
            add_get_up_check(checks, count, p, vp->get_handle(), vec_dist_squared(from, p) * 0.9f);
        }
        const float forward = vp->orient.fvec.y * vel.y + vel.x * vp->orient.fvec.x + vp->orient.fvec.z * vel.z;
        if (forward <= 0.22352f)
        {
            const vector p(flat.fvec.x * bmax.z + base.x, flat.fvec.y * bmax.z + base.y, flat.fvec.z * bmax.z + base.z);
            add_get_up_check(checks, count, p, vp->get_handle(), vec_dist_squared(from, p) * 0.9f);
        }
        if (forward >= -0.22352f)
        {
            const vector p(flat.fvec.x * bmin.z + base.x, flat.fvec.y * bmin.z + base.y, flat.fvec.z * bmin.z + base.z);
            add_get_up_check(checks, count, p, vp->get_handle(), vec_dist_squared(from, p) * 0.9f);
        }
    }
    return human_find_place_to_stand_check(destination, from, checks, count);
}

// 0x00A0BBA0 (:938)
// LTCG: hp in ESI. The upper and lower spine positions; face down when the lower spine's forward points down.
static bool human_ragdoll_is_face_down(const human* hp, vector* uspine_pos, vector* lspine_pos)
{
    vector upper;
    vector lower;
    matrix lower_orient;
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->upper_spine_bone_index, hp->pos, hp->orient, upper, nullptr);
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->lower_spine_bone_index, hp->pos, hp->orient, lower, &lower_orient);
    if (uspine_pos != nullptr)
    {
        *uspine_pos = upper;
    }
    if (lspine_pos != nullptr)
    {
        *lspine_pos = lower;
    }
    return lower_orient.fvec.y < 0.0f;
}

// ---- starting
// 0x00AD4F30 (:116)
void unused_dcf_ragdoll_player()
{
    if (Player != nullptr)
    {
        human_ragdoll_start(Player, false, 0xFFFFFFFF);
    }
}

// 0x00A31420 (:966)
// A flinch: a ragdoll that follows the animation (not for the local single player player).
bool human_ragdoll_start_flinch(human* hp, bool immediate)
{
    if ((hp == nullptr || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER || Multiplayer) && hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        const int idx = havok_ragdoll_bind_to_human(hp, immediate, RAGDOLL_PRIORITY_BULLET_DAMAGE, -1, true);
        if (idx >= 0)
        {
            hp->ragdoll_idx = idx;
            hp->ragdoll_state = HUMAN_RAGDOLL_STATE_FLINCH;
        }
        return true;
    }
    return false;
}

// 0x00AD4F50 (:996)
// Starts a ragdoll and pushes it: impulse / mass as a (capped) velocity and the angular impulse / mass as a spin (at
// most 4 pi rad/s) on the body parts 0x3B (head, pelvis, upper body, thighs).
bool human_ragdoll_start_impulse(human* hp, const vector& impulse, const vector& angular_impulse)
{
    human_ragdoll_start(hp, false, 0xFFFFFFFF);
    if (!human_is_ragdolled(hp))
    {
        return false;
    }
    const float inv_mass = (float)(1.0 / (double)hp->get_mass());
    havok_ragdoll_apply_velocity(hp->ragdoll_idx, vector(inv_mass * impulse.x, impulse.y * inv_mass, inv_mass * impulse.z), true);
    static float max_angular_speed = 12.566371f; // 4 pi
    vector axis = angular_impulse;
    const float magnitude = axis.mag_and_normalize_safe();
    float speed = max_angular_speed;
    if (max_angular_speed - magnitude * inv_mass >= 0.0f)
    {
        speed = magnitude * inv_mass;
    }
    havok_ragdoll_apply_angular_velocity(hp->ragdoll_idx, vector(axis.x * speed, axis.y * speed, axis.z * speed), 0x3B);
    return true;
}

// 0x00A31480 (:1027)
// When the human may get up: 0.5 s while playing the 0x3F4 action, 1.2 s (multiplayer) / 1.5 s for players, a random
// time growing with the speed in a free fall, else 2 s (2.5 s active on impact), 70% of it unless getting up.
void human_ragdoll_set_knockdown_timer(human* hp)
{
    const int time = (hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT ? 500 : 0) + 2000;
    if (hp->char_inst->is_playing_action(0x3F4)) // Magic number: action alias 1012
    {
        hp->knockdown_timestamp.set(500);
        return;
    }
    object* objp = World.object_from_handle(hp->get_handle());
    if (objp != nullptr && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        hp->knockdown_timestamp.set(Multiplayer ? 1200 : 1500);
        return;
    }
    if (hp->movement_submode == HMSM_FALL_FREE)
    {
        hp->knockdown_timestamp.set_rand(900, 900 - (int)((double)hp->actual_velocity_mag * -40.0));
        return;
    }
    if (hp->movement_submode != HMSM_KNOCKDOWN_GETUP)
    {
        hp->knockdown_timestamp.set((int)((double)(float)time * 0.7));
        return;
    }
    hp->knockdown_timestamp.set(time);
}

// The ragdoll starts with the human's knockdown (inlined human_set_movement_mode and human::stop_voice; human_ragdoll_start).
static void human_ragdoll_enter_knockdown(human* hp)
{
    human_set_movement_mode(hp, HMM_KNOCKDOWN, HMSM_KNOCKDOWN_RAGDOLL);
}

// 0x00AC9B00 (:1072)
// Ragdolls the human (multiplayer: the local player only when the server forces it, other players only from the
// server): a ragdoll already active only extends the knockdown, a deactivated one is reactivated, a flinch becomes a
// full ragdoll; the player loses its zoom, turret, ladder, cover and non-inventory weapon. False when it is immune,
// localization forbids ragdolls of the dead, or no ragdoll slot is free (single player then removes the human).
bool human_ragdoll_start(human* hp, bool forced_by_server, unsigned int perp)
{
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if ((!Multiplayer || multi_is_server()) && pp != nullptr && pp->backpack != nullptr)
    {
        pp->backpack->stop_ramp_up();
    }
    if (!Multiplayer || (hp != nullptr && (player*)hp == Player && !forced_by_server))
    {
        if (human_is_immune_to_ragdoll(hp))
        {
            return false;
        }
    }
    else if (!forced_by_server)
    {
        return false;
    }
    if ((hp == nullptr || hp->is_killed()) && !localization_allow_ragdolls() && hp->ragdoll_state == HUMAN_RAGDOLL_STATE_POSE_SAVED)
    {
        return false;
    }
    if (Multiplayer)
    {
        hp->ragdoll_damaged.set(250);
    }
    hp->ragdoll_source_human_handle = perp;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        if (hp->knockdown_timestamp.time_until() < 500)
        {
            hp->knockdown_timestamp.set(500);
        }
        return true;
    }
    if (vehicle_interaction_ragdoll(hp, perp))
    {
        return true;
    }
    hp->knockdown_timeout_ts.set(6000);
    human_ragdoll_set_knockdown_timer(hp);
    if (hp->char_inst != nullptr)
    {
        hp->char_inst->vp->flags.interpolate_orient = 0;
    }
    if ((player*)hp == Player)
    {
        camera_trans_start_linear_transition(0.75f, 7, true); // Magic numbers: 0.75 s, transition type 7
        if (hp->cur_state == HS_COVER)
        {
            player_exit_cover((player*)hp, false);
        }
        if (!hp->is_killed() && !Multiplayer)
        {
            human_weapons_cancel_reload(hp);
            human_weapons_immediately_holster_weapon(hp);
            hp->desired_equipped_inv_item = hp->last_equipped_weapon;
            hp->last_equipped_weapon = hp->second_last_equipped_weapon;
            hp->second_last_equipped_weapon = nullptr;
        }
        if (hp->get_desired_action_node() != nullptr)
        {
            ((player*)hp)->action_node_reset(nullptr);
        }
    }
    if (hp->hflags.is_on_ladder)
    {
        hp->char_inst->set_state(index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        human_dismount_ladder(hp);
    }
    human_combat_cover_abandon_node(hp);
    human_combat_abandon_turret(hp);
    if (player_has_non_inventory(pp))
    {
        if (hp->get_equipped_weapon() != nullptr)
        {
            human_weapons_drop_equipped_weapon(hp);
        }
        hp->desired_equipped_inv_item = hp->last_equipped_weapon;
        hp->last_equipped_weapon = hp->second_last_equipped_weapon;
        hp->second_last_equipped_weapon = nullptr;
    }
    hp->combat.cur_melee_combat_move = nullptr;
    hp->combat.shoot_enemy_timestamp.invalidate();
    hp->combat.hold_trigger_timestamp.invalidate();
    hp->combat.melee_attack_interval_timer.invalidate();
    hp->combat.move_to_melee_timestamp.invalidate();
    if (pp != nullptr)
    {
        if (pp->zoom_state != PZS_OFF)
        {
            turret* tp = human_combat_get_manned_turret(pp);
            if (tp == nullptr || !tp->t_info->flags.force_fine_aim)
            {
                const player_zoom_state previous = pp->zoom_state;
                pp->previous_zoom_state = previous;
                pp->zoom_state = PZS_OFF;
                bool set_view = true;
                if (previous != PZS_OFF)
                {
                    if (pp != Player)
                    {
                        set_view = false;
                    }
                    else
                    {
                        pp->get_equipped_weapon();
                        if (pp->previous_zoom_state == PZS_SCOPE)
                        {
                            pp->set_opacity_plus_child_items(1.0f);
                            pp->set_casts_transparent_shadows_plus_child_items(false);
                            hud_ui_rail_driver_set_visible(false);
                            hud_ui_sniper_set_visible(false);
                        }
                    }
                }
                if (set_view && pp == Player)
                {
                    camera_set_view_player(PZS_OFF, false);
                }
            }
        }
        human_weapons_cancel_reload(pp);
        pp->steering_vector.zero();
        pp->direct_control_dir.zero();
        pp->direct_control_vel.zero();
    }
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_FLINCH)
    {
        havok_ragdoll_unbind_from_human(hp->ragdoll_idx);
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
        hp->ragdoll_source_human_handle = 0xFFFFFFFF;
        hp->ragdoll_idx = -1;
    }
    const human_ragdoll_state state = hp->ragdoll_state;
    if (state == HUMAN_RAGDOLL_STATE_DEACTIVATED)
    {
        if (!animlib_allocate_bones(hp->char_inst->aim_handle))
        {
            return false;
        }
        havok_ragdoll_recall_matrices(hp->ragdoll_idx, hp->char_inst);
        havok_ragdoll_init_relative_orients(hp->ragdoll_idx, hp->char_inst);
        human_ragdoll_enter_knockdown(hp);
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_ACTIVE;
        return true;
    }
    if (state != HUMAN_RAGDOLL_STATE_NONE && state != HUMAN_RAGDOLL_STATE_POSE_SAVED)
    {
        // blending out or active on impact: the bound ragdoll takes over
        hp->hflags.dont_deform_bones = 0;
        if (state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            vector vel;
            human_controller_get_vel(hp, vel);
            havok_sync_ragdoll_to_anim(hp->ragdoll_idx, hp->pos, hp->orient, vel, 0.0f, 1.0f, false, false, false);
        }
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_ACTIVE;
        human_update_collision_filter(hp, true);
        human_ragdoll_enter_knockdown(hp);
        if (!human_is_dead(hp))
        {
            hp->cur_state = HS_STANDARD;
        }
        animlib_clear_looping_animations(hp->char_inst->aim_handle, false);
        animlib_clear_non_looping_animations(hp->char_inst->aim_handle);
        hp->char_inst->vp->desired_state = nullptr;
        hp->char_inst->vp->secondary_state = nullptr;
        vanim_process_completed_actions(hp->char_inst->vp);
        vanim_change_state(hp->char_inst->vp, index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
        hp->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        hp->hflags.is_falling = 0;
        return true;
    }
    const int idx = havok_ragdoll_bind_to_human(hp, true, RAGDOLL_PRIORITY_BULLET_DAMAGE, -1, true);
    if (idx < 0)
    {
        if (!Multiplayer)
        {
            World.destroy_object(hp, false);
        }
        return false;
    }
    if (Multiplayer && pp != nullptr && pp->mp_ragdoll_force_timeout.valid() && !pp->mp_ragdoll_force_timeout.elapsed())
    {
        // the push the server sent with the ragdoll (multi_game_player_process_player_ragdoll)
        havok_ragdoll_apply_point_velocity(idx, pp->mp_ragdoll_force, pp->mp_ragdoll_pos);
    }
    hp->ragdoll_idx = idx;
    hp->ragdoll_state = HUMAN_RAGDOLL_STATE_ACTIVE;
    human_update_collision_filter(hp, true);
    if (hp->cloth_sim != nullptr)
    {
        hp->cloth_sim->initialize_nodes = 1;
    }
    if (!havok_ragdoll_is_synced(hp->ragdoll_idx))
    {
        hp->hflags.dont_deform_bones = 0;
    }
    object_is_player(hp->get_handle()); // (result unused)
    if (!human_is_dead(hp))
    {
        hp->cur_state = HS_STANDARD;
    }
    human_ragdoll_enter_knockdown(hp);
    hp->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    hp->hflags.is_falling = 0;
    animlib_clear_looping_animations(hp->char_inst->aim_handle, false);
    animlib_clear_non_looping_animations(hp->char_inst->aim_handle);
    vanim_change_state(hp->char_inst->vp, index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
    return true;
}

// ---- stopping and getting up
// 0x00AC1850 (:1329)
// hold_pos keeps the pose: the dead without localized ragdolls and corpse npcs drop the ragdoll (pose saved), the
// others deactivate it (matrices stored). Else the ragdoll is unbound and a living human stands again (the player
// re-equips).
bool human_ragdoll_stop(human* hp, bool hold_pos)
{
    if (hold_pos)
    {
        npc* np = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? (npc*)hp : nullptr;
        if (((hp == nullptr || hp->is_killed()) && !localization_allow_ragdolls()) || (np != nullptr && np->npc_flags.spawned_as_corpse))
        {
            if (havok_ragdoll_unbind_from_human(hp->ragdoll_idx))
            {
                hp->ragdoll_state = HUMAN_RAGDOLL_STATE_POSE_SAVED;
                human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
                hp->ragdoll_source_human_handle = 0xFFFFFFFF;
                hp->ragdoll_idx = -1;
            }
            return true;
        }
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_DEACTIVATED;
        havok_ragdoll_store_matrices(hp->ragdoll_idx, hp->char_inst);
        human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
        return true;
    }
    if (!havok_ragdoll_unbind_from_human(hp->ragdoll_idx) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_POSE_SAVED)
    {
        return true;
    }
    hp->hflags.dont_deform_bones = 0;
    hp->ragdoll_idx = -1;
    hp->ragdoll_source_human_handle = 0xFFFFFFFF;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
    {
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
        return true;
    }
    hp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
    if (!hp->is_killed())
    {
        hp->hflags.prevent_ragdoll_sfx = 0;
        hp->move_speed = -1.0f;
        hp->cur_state = HS_STANDARD;
        hp->cur_movement_state = HMS_IDLE;
    }
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    if ((player*)hp == Player)
    {
        control_gameplay_reset_movement_controls();
    }
    if (hp->char_inst->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->flags.sliding = 0;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        inventory_item* iip = hp->desired_equipped_inv_item;
        if (iip == nullptr)
        {
            iip = hp->last_equipped_weapon;
        }
        human_equip_inventory_item(hp, iip);
    }
    return true;
}

// 0x00A0BC50 (:1396)
// The next impact (faster than min_velocity) ragdolls the human: a ragdoll bound now, activated by
// havok_collide_ragdoll. The longest time and the smallest positive speed of several calls win.
void human_ragdoll_on_impact(human* hp, float time, float min_velocity, bool all_collisions)
{
    if (hp->ragdoll_on_impact_time - time < 0.0f)
    {
        hp->ragdoll_on_impact_time = time;
    }
    float min_vel2 = min_velocity * min_velocity;
    if (hp->ragdoll_on_impact_min_vel > 0.0f && hp->ragdoll_on_impact_min_vel - min_vel2 < 0.0f)
    {
        min_vel2 = hp->ragdoll_on_impact_min_vel;
    }
    hp->ragdoll_on_impact_min_vel = min_vel2;
    hp->hflags.ragdoll_on_impact_all_collisions = all_collisions ? 1 : 0;
    hp->hflags.ragdoll_on_impact_use_ragdoll_pos = 0;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
    {
        return;
    }
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        hp->ragdoll_idx = havok_ragdoll_bind_to_human(hp, true, RAGDOLL_PRIORITY_BULLET_DAMAGE, -1, false);
    }
    if (hp->ragdoll_idx >= 0 && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT;
    }
}

// 0x00A31580 (:1425)
// Players and vips cannot be crushed.
bool human_ragdoll_can_be_crushed(const human* const hp)
{
    if (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        return false;
    }
    return !hp->h_info->flags.vip;
}

// 0x00AD50D0 (:1438)
// Ends a full ragdoll: pinned humans take damage (the dead and the crushable stay down), the human is moved to a
// place to stand (search_for_valid_ground: within 2 m, else after damage within 6 or 12 m, else a 1 in 50 chance of
// an explosion to free it), the ragdoll blends out and the get up (or, for scared npcs, fetal) animation plays
// facing along the spine.
bool human_ragdoll_get_up(human* hp, bool search_for_valid_ground)
{
    if (!human_is_ragdolled(hp) || hp->movement_submode != HMSM_KNOCKDOWN_RAGDOLL)
    {
        return true;
    }
    if (vehicle_interaction_ragdoll_fall_in_seat(hp, nullptr))
    {
        return true;
    }
    object* crusher = (object*)human_crushed_under_object(hp);
    if (crusher != nullptr)
    {
        const bool by_vehicle = crusher->get_obj_type() == OT_VEHICLE;
        human* driver = by_vehicle ? vehicle_passenger_in_seat((const vehicle*)crusher, VSI_DRIVER) : nullptr;
        human_damage(hp, driver, (float)((double)(float)hp->max_hit_points * (double)STUCK_UNDER_CAR_DAMAGE_PCT), (damaged_by_types)by_vehicle, nullptr, crusher, true, false);
        if (hp->is_killed() || human_ragdoll_can_be_crushed(hp))
        {
            return false;
        }
    }
    const matrix orient_copy = hp->orient; // (unused)
    vector stand_pos = hp->pos;
    if (search_for_valid_ground)
    {
        if (human_find_place_to_stand(stand_pos, hp, 2.0f, nullptr))
        {
            vec_dist(stand_pos, hp->pos); // (result unused)
        }
        else
        {
            const float damage = (float)((double)(float)hp->max_hit_points * (double)STUCK_UNDER_CAR_DAMAGE_PCT);
            if (Multiplayer && !multi_is_server())
            {
                if (object_is_local_player(hp))
                {
                    multi_game_player_request_damage(damage, RDR_RAGDOLL);
                }
                return false;
            }
            if (human_damage(hp, nullptr, damage, DBT_NONE, nullptr, nullptr, true, false) > 0.0f)
            {
                return false;
            }
            stand_pos = hp->pos;
            // Magic numbers: 6 m, then 12 m (BULLET_SOUND_MAX_DIST, 0x01310FF0)
            if (human_find_place_to_stand(stand_pos, hp, 6.0f, nullptr) || human_find_place_to_stand(stand_pos, hp, 12.0f, nullptr))
            {
                const vector dir((float)((double)stand_pos.x - (double)hp->pos.x), (float)((double)stand_pos.y - (double)hp->pos.y), (float)((double)stand_pos.z - (double)hp->pos.z));
                hp->orient.make(dir);
                human_teleport_unsafe(hp, stand_pos, hp->orient);
            }
            else if (rand() % 50 == 0) // Magic number: a 1 in 50 chance
            {
                explosion_create(explosion_lookup("raid_demolish_building"), hp, hp, hp->pos, IDENTITY_MATRIX, &UP_VECTOR, nullptr, false);
            }
        }
    }
    (void)orient_copy;
    hp->ragdoll_blend_pct = 0.0f;
    hp->ragdoll_state = HUMAN_RAGDOLL_STATE_BLEND_OUT;
    havok_ragdoll_get_vel(hp->ragdoll_idx, hp->velocity);
    hp->actual_velocity = hp->velocity;
    hp->actual_velocity_mag = hp->actual_velocity.mag();
    if (hp->char_inst != nullptr && hp->char_inst->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->flags.sliding = 0;
    }
    animlib_clear_non_looping_animations(hp->char_inst->aim_handle);
    animlib_clear_looping_animations(hp->char_inst->aim_handle, false);
    hp->move_speed = -1.0f;
    hp->cur_state = HS_STANDARD;
    hp->cur_movement_state = HMS_IDLE;
    human_set_and_process_animation_states(hp);
    hp->hflags.just_got_up_from_ragdoll = 1;
    bool fetal = false;
    if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        fetal = ((npc*)hp)->get_ai()->should_go_to_fetal_position();
    }
    vector upper;
    vector lower;
    vector dir;
    animation_action anim_index;
    if (human_ragdoll_is_face_down(hp, &upper, &lower))
    {
        dir = vector((float)((double)upper.x - (double)lower.x), (float)((double)upper.y - (double)lower.y), (float)((double)upper.z - (double)lower.z));
        anim_index = fetal ? ANIMATION_ACTION_FACEDOWN_TO_FETAL : ANIMATION_ACTION_FW_GETUP;
    }
    else
    {
        dir = vector((float)((double)lower.x - (double)upper.x), (float)((double)lower.y - (double)upper.y), (float)((double)lower.z - (double)upper.z));
        anim_index = fetal ? ANIMATION_ACTION_FACEUP_TO_FETAL : ANIMATION_ACTION_BK_GETUP;
    }
    hp->char_inst->vp->flags.interpolate_orient = 0;
    hp->orient.set(0.0f, 0.0f, fl_atan2_safe(dir.x, dir.z));
    if (fetal)
    {
        hp->turnto_target = hp->pos;
        hp->steering_vector = ZERO_VECTOR;
        hp->custom_fire_animation = ANIMATION_ACTION_NONE;
        hp->hflags.override_default_fire_anim = 0;
        hp->hflags.override_default_anim_state = 1;
        // Magic number: 0.3 s (MIN_PATH_DIST, 0x01310F28), state alias 15
        hp->char_inst->set_state(index_alias(15, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        hp->hflags.in_fetal_position = 1;
        hp->hflags.anchor_orient = 1;
        hp->cur_stance = HST_PRONE;
    }
    else
    {
        // Magic number: a turn-to target 10 m along the spine
        const float tx = (float)((double)dir.x * 10.0);
        const float ty = (float)((double)dir.y * 10.0);
        const float tz = (float)((double)dir.z * 10.0);
        hp->turnto_target = vector((float)((double)hp->pos.x + (double)tx), (float)((double)ty + (double)hp->pos.y), (float)((double)hp->pos.z + (double)tz));
        hp->cur_stance = HST_STAND;
        human_in_vehicle_interaction(hp, nullptr); // (result unused)
        hp->combat.combat_ready_timestamp.invalidate();
    }
    // Magic numbers: multiplayer gets up 1.75x as fast; play flags 0x4040
    hp->char_inst->play_action(anim_index, ANIM_ALIAS_GET_UP, Multiplayer ? 1.75f : 1.0f, 0x4040);
    hp->update_pos_and_orient(stand_pos, hp->orient, true);
    if ((player*)hp == Player)
    {
        camera_trans_start_linear_transition(0.75f, 7, true); // Magic numbers: 0.75 s (HUMAN_TO_TURRET_DISTANCE), type 7
    }
    human_set_movement_mode(hp, HMM_KNOCKDOWN, HMSM_KNOCKDOWN_GETUP);
    human_update_collision_filter(hp, true);
    return true;
}

// 0x00A0BD30 (:1678)
// Records where a standing human last was (a ring of 3 positions, a new slot every 5 m).
void human_update_valid_position_before_ragdoll(human* hp)
{
    if (hp->cur_stance != HST_STAND || !(hp->move_speed >= 0.5f) || hp->movement_mode == HMM_KNOCKDOWN || hp->movement_mode == HMM_FALL)
    {
        return;
    }
    unsigned int last = hp->last_valid_pos_before_ragdoll_index - 1;
    if (last > 2)
    {
        last = 2;
    }
    // Magic number: 25 = 5 m squared
    if (vec_dist_squared(hp->last_valid_pos_before_ragdoll[last], hp->pos) > 25.0f)
    {
        hp->last_valid_pos_before_ragdoll_index++;
        if (hp->last_valid_pos_before_ragdoll_index > 2)
        {
            hp->last_valid_pos_before_ragdoll_index = 0;
        }
    }
    hp->last_valid_pos_before_ragdoll[hp->last_valid_pos_before_ragdoll_index] = hp->pos;
}

// 0x00A82980 (:1720)
// A falling human (not on ground, no ragdoll) about to hit its head: a 0.3 sphere from between the calves along 90%
// of the frame's velocity, else a 0.45 sphere from the neck along 110% of it.
bool human_move_fall_collide_head(human* hp)
{
    if (hp->ground_obj_handle != 0xFFFFFFFF || hp->ragdoll_idx != -1)
    {
        return false;
    }
    vector calf_l;
    vector calf_r;
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->calf_l_bone_index, hp->pos, hp->orient, calf_l, nullptr);
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->calf_r_bone_index, hp->pos, hp->orient, calf_r, nullptr);
    raycast_input in;
    in.start = vector((calf_l.x + calf_r.x) * 0.5f, (calf_l.y + calf_r.y) * 0.5f, (calf_r.z + calf_l.z) * 0.5f);
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    raycast_output out;
    in.filter_params = (void*)hp->vehicle_handle;
    // Magic numbers: 0.9 and 1.1 of the frame's movement
    in.end = vector(hp->actual_velocity.x * Frametime * 0.9f + in.start.x, Frametime * hp->actual_velocity.y * 0.9f + in.start.y, Frametime * hp->actual_velocity.z * 0.9f + in.start.z);
    in.radius = 0.3f;
    in.filter_callback = human_ragdoll_get_up_spherecast_cb;
    unsigned int filter;
    if (human_get_collision_filter(filter, hp))
    {
        in.m_filter_info = filter;
    }
    in.m_flags = 0;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    if (havok_spherecast(in, out))
    {
        return true;
    }
    vector neck;
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->neck_bone_index, hp->pos, hp->orient, neck, nullptr);
    in.start = neck;
    in.end = vector(hp->actual_velocity.x * Frametime * 1.1f + neck.x, Frametime * hp->actual_velocity.y * 1.1f + neck.y, Frametime * hp->actual_velocity.z * 1.1f + neck.z);
    in.radius = 0.45f;
    return havok_spherecast(in, out);
}

// 0x00A0BDE0 (:1803)
void human_ragdoll_set_pos(human* hp, const vector& pos)
{
    if (human_is_ragdolled(hp))
    {
        havok_ragdoll_set_pos(hp->ragdoll_idx, pos);
    }
}

// 0x00A0BE20 (:1810)
void human_ragdoll_set_velocity(human* hp, const vector& velocity)
{
    if (human_is_ragdolled(hp))
    {
        havok_ragdoll_set_velocity(hp->ragdoll_idx, velocity);
    }
}
