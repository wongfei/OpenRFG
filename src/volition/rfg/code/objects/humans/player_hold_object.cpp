// Original: volition/rfg/code/objects/humans/player_hold_object.cpp
//
// Picking up, carrying and throwing a physics object: PlayerHoldAction drives the held body to the hold position every
// Havok step. Compiled in rfg_objects_unity.cpp. stub_misc.md group 3: the whole file.
//
// Ghidra lists only reads of Player_pickup_allow, Player_hold_single_spring and Player_hold_swap_weapon (false) and of
// Player_hold_interpolate (true), so in rfg.exe a pickup only starts in the slew camera, and the two-spring path below
// is the one that runs. The references Ghidra lists do not include a console command writing through a pointer.

#include "volition/rfg/code/objects/humans/player_hold_object.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/input/input.h"

#include <math.h>
#include <stdio.h>

// ---- tweakables (data in rfg.exe, only read)
static float Player_hold_max_throw_speed = 20.0f; // 0x016944C8
static float Player_hold_throw_strength_min = 10.0f; // 0x016944CC
static float Player_hold_throw_strength_max = 150.0f; // 0x016944D0
static float Player_hold_max_lift_mass = 500.0f; // 0x016944D4
static float Player_hold_throw_arc = 0.5f; // 0x016944D8
static float Player_hold_max_lift_distance = 4.0f; // 0x016944DC
static float Player_hold_offset = 1.5f; // 0x016944E4
static float Player_hold_height = 1.0f; // 0x016944E8
static float Player_hold_camera_offset_y = -0.5f; // 0x016944EC
static bool Player_hold_single_spring; // 0x0302F0D5
static bool Player_hold_swap_weapon; // 0x0302F0D6
static bool Player_pickup_allow; // 0x0302F0D8
static bool Player_hold_interpolate = true; // 0x016944BE
static bool Player_hold_debug; // 0x0302F0D7

// ---- the held object
static PlayerHoldAction* Player_hold_action_instance; // 0x030291D0
static vector Player_hold_object_pos; // 0x0328B6CC, the grabbed point in the object's space
static matrix Player_hold_object_orient; // 0x03063BA8, relative to the camera
static float Player_hold_object_linear_dampening; // 0x03023868
static float Player_hold_object_angular_dampening; // 0x030269C8
static matrix43 Player_hold_object_last_matrix; // 0x03560A60
timestamp_percent Player_hold_throw_timestamp; // 0x01695634

// 0x00A115C0 (:93)
PlayerHoldAction::PlayerHoldAction(hkpRigidBody* body)
    : hkpUnaryAction(body, 0)
{
}

// 0x00A115E0 (:95)
PlayerHoldAction::~PlayerHoldAction()
{
}

// 0x00A115F0 (:98)
hkpAction* PlayerHoldAction::clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& newEntities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const
{
    return nullptr;
}

// 0x00A11600 (:236). Static in rfg.exe, LTCG: the result in ESI.
// In front of the camera in first person and slew, else in front of the player's flattened view and above its feet.
static vector player_hold_get_hold_pos()
{
    vector hold_pos;
    if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE || Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        hold_pos.x = Rfg_camera.real_pos.x + Rfg_camera.real_orient.fvec.x * Player_hold_offset + Rfg_camera.real_orient.uvec.x * Player_hold_camera_offset_y;
        hold_pos.y = Rfg_camera.real_orient.fvec.y * Player_hold_offset + Rfg_camera.real_pos.y + Rfg_camera.real_orient.uvec.y * Player_hold_camera_offset_y;
        hold_pos.z = Rfg_camera.real_orient.fvec.z * Player_hold_offset + Rfg_camera.real_pos.z + Rfg_camera.real_orient.uvec.z * Player_hold_camera_offset_y;
    }
    else
    {
        vector forward;
        forward.x = Rfg_camera.real_orient.fvec.x;
        forward.y = 0.0f;
        forward.z = Rfg_camera.real_orient.fvec.z;
        forward.normalize_safe();
        hold_pos.x = Player_hold_offset * forward.x + Player->pos.x + UP_VECTOR.x * Player_hold_height;
        hold_pos.y = Player->pos.y + Player_hold_offset * forward.y + UP_VECTOR.y * Player_hold_height;
        hold_pos.z = Player->pos.z + forward.z * Player_hold_offset + UP_VECTOR.z * Player_hold_height;
    }
    return hold_pos;
}

// 0x00A34F80 (:101)
void PlayerHoldAction::applyAction(const hkStepInfo& stepInfo)
{
    object* objp = World.object_from_handle(Player->hold_object_handle);
    if (objp == nullptr)
    {
        return;
    }
    hkpRigidBody* rb = static_cast<hkpRigidBody*>(m_entity);
    const float dt = stepInfo.m_deltaTime;
    vector velocity;
    if (Player_hold_single_spring)
    {
        // One spring from the grabbed point to the hold position (never runs in rfg.exe, see the file comment).
        const vector hold_pos = player_hold_get_hold_pos();
        vector grabbed = Player_hold_object_pos;
        vec_change_to_global_coords(grabbed, objp->pos, objp->orient);
        havok_body_set_linear_dampening(rb, 10.0f);
        havok_body_set_angular_dampening(rb, 10.0f);
        if (Player->rdd.is_loaded())
        {
            Player->rdd.get_data()->controller->get_velocity(velocity);
        }
        else
        {
            velocity.zero();
        }
        const float strength = havok_body_get_mass(rb) * 70.0f; // Magic number: the spring's strength per kg.
        vector impulse;
        impulse.x = ((velocity.x * 0.2f + hold_pos.x) - grabbed.x) * strength * dt;
        impulse.y = ((hold_pos.y + velocity.y * 0.2f) - grabbed.y) * strength * dt;
        impulse.z = ((hold_pos.z + velocity.z * 0.2f) - grabbed.z) * strength * dt;
        havok_body_apply_point_impulse(rb, impulse, grabbed);
        return;
    }
    // Cancels gravity (Magic number: 9.8), then six springs, one per face of the box, towards the hold orientation.
    const float weight = havok_body_get_mass(rb) * 9.8f;
    vector lift;
    lift.x = dt * 0.0f;
    lift.y = weight * dt;
    lift.z = lift.x;
    havok_body_apply_linear_impulse(rb, lift);
    havok_body_set_linear_dampening(rb, 20.0f);
    havok_body_set_angular_dampening(rb, 20.0f);
    const vector hold_pos = player_hold_get_hold_pos();
    const matrix hold_orient = Player_hold_object_orient * Rfg_camera.real_orient;
    if (Player->rdd.is_loaded())
    {
        Player->rdd.get_data()->controller->get_velocity(velocity);
    }
    else
    {
        velocity.zero();
    }
    // Magic number: 0.07 s of the player's velocity ahead.
    const float target_x = velocity.x * 0.07f + hold_pos.x;
    const float target_y = hold_pos.y + velocity.y * 0.07f;
    const float target_z = hold_pos.z + velocity.z * 0.07f;
    vector local_min;
    vector local_max;
    objp->get_local_bbox(local_min, local_max);
    const float half_u = (local_max.y - local_min.y) * 0.5f;
    const float half_f = (local_max.z - local_min.z) * 0.5f;
    const float half_r = (local_max.x - local_min.x) * 0.5f;
    vector local_centre;
    local_centre.x = (local_min.x + local_max.x) * 0.5f;
    local_centre.y = (local_max.y + local_min.y) * 0.5f;
    local_centre.z = (local_max.z + local_min.z) * 0.5f;
    const vector rotated_centre = local_centre.copy_unrotate(objp->orient);
    vector centre;
    centre.x = objp->pos.x + rotated_centre.x;
    centre.y = rotated_centre.y + objp->pos.y;
    centre.z = rotated_centre.z + objp->pos.z;
    // Each spring runs from a point on the box to where that point should be. Magic number: 1.5, the targets lie outside
    // the box by half its size.
    struct spring
    {
        const vector& target_axis;
        const vector& object_axis;
        float half;
        float sign;
    };
    const spring springs[6] =
    {
        {hold_orient.uvec, objp->orient.uvec, half_u, 1.0f},
        {hold_orient.uvec, objp->orient.uvec, half_u, -1.0f},
        {hold_orient.rvec, objp->orient.rvec, half_r, 1.0f},
        {hold_orient.rvec, objp->orient.rvec, half_r, -1.0f},
        {hold_orient.fvec, objp->orient.fvec, half_f, 1.0f},
        {hold_orient.fvec, objp->orient.fvec, half_f, -1.0f},
    };
    const float strength = havok_body_get_mass(rb) * 70.0f; // Magic number: the spring's strength per kg.
    for (const spring& s : springs)
    {
        vector target;
        target.x = target_x + s.sign * (s.target_axis.x * 1.5f * s.half);
        target.y = target_y + s.sign * (s.target_axis.y * 1.5f * s.half);
        target.z = target_z + s.sign * (s.target_axis.z * 1.5f * s.half);
        vector point;
        point.x = centre.x + s.sign * (s.object_axis.x * s.half);
        point.y = centre.y + s.sign * (s.object_axis.y * s.half);
        point.z = centre.z + s.sign * (s.object_axis.z * s.half);
        vector impulse;
        impulse.x = (target.x - point.x) * strength * dt;
        impulse.y = (target.y - point.y) * strength * dt;
        impulse.z = (target.z - point.z) * strength * dt;
        havok_body_apply_point_impulse(rb, impulse, point);
    }
}

// 0x00A11860 (:253)
void player_hold_object_interpolate_havok()
{
    if (!Player_hold_interpolate)
    {
        return;
    }
    object* objp = World.object_from_handle(Player->hold_object_handle);
    if (objp == nullptr)
    {
        return;
    }
    const matrix43 current(objp->pos, objp->orient);
    Player_hold_object_last_matrix = havok_interpolate_transform(Player_hold_object_last_matrix, current);
    objp->update_pos_and_orient(Player_hold_object_last_matrix.m_translation, Player_hold_object_last_matrix.m_rotation, false);
}

// 0x00ABE950 (:276)
void player_hold_object_pickup()
{
    player* pp = Player;
    if (Rfg_camera.mode != CAMERA_SLEW_MODE && !Player_pickup_allow)
    {
        return;
    }
    unsigned int handle = 0xFFFFFFFF;
    if (Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        raycast_input input;
        input.start = Rfg_camera.real_pos;
        input.end.x = Rfg_camera.real_pos.x + Rfg_camera.real_orient.fvec.x * Player_hold_max_lift_distance;
        input.end.y = Rfg_camera.real_orient.fvec.y * Player_hold_max_lift_distance + Rfg_camera.real_pos.y;
        input.end.z = Rfg_camera.real_orient.fvec.z * Player_hold_max_lift_distance + Rfg_camera.real_pos.z;
        input.radius = 0.0f;
        input.filter_params = nullptr;
        input.filter_callback = nullptr;
        input.m_filter_info = COLLIDE_RAY_DEFAULT;
        input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        input.m_flags = 0;
        raycast_output output;
        if (havok_raycast(input, output))
        {
            handle = output.m_hits[0].hit_object_handle;
        }
    }
    else
    {
        handle = pp->aim_target;
    }
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() == OT_HUMAN || objp->get_obj_type() == OT_ZONE || objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        return;
    }
    if ((Multiplayer && !objp->remote_obj_flags.remote_object) || objp->havok_handle == 0xFFFFFFFF || !havok_body_is_movable(objp->havok_handle))
    {
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(objp->havok_handle);
    const vector hold_pos = player_hold_get_hold_pos();
    vector offset;
    offset.x = hold_pos.x - objp->pos.x;
    offset.y = hold_pos.y - objp->pos.y;
    offset.z = hold_pos.z - objp->pos.z;
    if (Rfg_camera.mode != CAMERA_SLEW_MODE && !(havok_body_get_mass(rb) < Player_hold_max_lift_mass))
    {
        return;
    }
    if (!(offset.mag() < Player_hold_max_lift_distance))
    {
        return;
    }
    pp->hold_object_available = true;
    if (!controls_is_button_just_pressed(CBA_GAC_HOLD_OBJECT, Player_controller_index))
    {
        // Magic number: 6, the slew camera's pickup button.
        if (Rfg_camera.mode != CAMERA_SLEW_MODE || !input_just_pressed(multi_core_platform_get_active_controller(), 6))
        {
            return;
        }
    }
    pp->hold_object_handle = handle;
    Player_hold_action_instance = new PlayerHoldAction(rb);
    Havok_world->addAction(Player_hold_action_instance);
    Player_hold_action_instance->removeReference();
    // The grabbed point: where the player aims, in the object's space.
    Player_hold_object_pos = pp->aim_pos;
    vec_change_to_local_coords(Player_hold_object_pos, objp->pos, objp->orient);
    Player_hold_object_orient = objp->orient * Rfg_camera.real_orient.copy_transpose();
    Player_hold_object_linear_dampening = havok_body_get_linear_dampening(rb);
    Player_hold_object_angular_dampening = havok_body_get_angular_dampening(rb);
    Player_hold_object_last_matrix = matrix43(objp->pos, objp->orient);
    havok_set_ignore_property(rb, pp->get_handle());
    if (Player_hold_swap_weapon)
    {
        human_unequip_inventory_item(pp);
        human_do_equip_unequip(pp, true);
    }
}

// 0x00AC35E0 (:394)
// Every single player frame (gamestate_gameplay.cpp): the held object is dropped in a vehicle, on the use button, on the
// attack button (which throws it) or when too far from the hold position.
void player_hold_object_process()
{
    player* pp = Player;
    pp->hold_object_available = false;
    object* objp = World.object_from_handle(pp->hold_object_handle);
    if (objp == nullptr)
    {
        pp->hold_object_handle = 0xFFFFFFFF;
    }
    const vector hold_pos = player_hold_get_hold_pos();
    if (objp == nullptr)
    {
        player_hold_object_pickup();
        return;
    }
    havok_body_get_pointer(objp->havok_handle)->activate();
    vector grabbed = Player_hold_object_pos;
    vec_change_to_global_coords(grabbed, objp->pos, objp->orient);
    vector to_hold;
    to_hold.x = hold_pos.x - grabbed.x;
    to_hold.y = hold_pos.y - grabbed.y;
    to_hold.z = hold_pos.z - grabbed.z;
    vector centre_to_hold;
    centre_to_hold.x = hold_pos.x - objp->pos.x;
    centre_to_hold.y = hold_pos.y - objp->pos.y;
    centre_to_hold.z = hold_pos.z - objp->pos.z;
    const vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
    bool drop = (human_in_vehicle(pp, vp) && vp != nullptr) || controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index) ||
                controls_is_button_just_pressed(CBA_GAC_HOLD_OBJECT, Player_controller_index);
    if (!drop)
    {
        // With two springs both distances count: the second is only read when the first is over the limit.
        if (Player_hold_single_spring)
        {
            drop = Player_hold_max_lift_distance < to_hold.mag();
        }
        else
        {
            drop = Player_hold_max_lift_distance < to_hold.mag() && Player_hold_max_lift_distance < centre_to_hold.mag();
        }
    }
    if (!drop && Rfg_camera.mode == CAMERA_SLEW_MODE && input_just_pressed(multi_core_platform_get_active_controller(), 6)) // Magic number: as in the pickup.
    {
        drop = true;
    }
    if (!drop)
    {
        human_set_combat_ready(pp, true, 0);
        pp->hold_object_available = true;
        player_hold_object_interpolate_havok();
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(objp->havok_handle);
    if (rb != nullptr)
    {
        havok_body_set_linear_dampening(rb, Player_hold_object_linear_dampening);
        havok_body_set_angular_dampening(rb, Player_hold_object_angular_dampening);
        havok_body_set_collision_type(rb, COLLIDE_MOVER, false, false);
        havok_remove_ignore_property(rb);
    }
    Player_hold_throw_timestamp.set(500); // Magic number: melee attacks stay blocked for 0.5 s.
    if (World.object_from_handle(pp->hold_object_handle) != nullptr)
    {
        Havok_world->removeAction(Player_hold_action_instance);
    }
    pp->hold_object_handle = 0xFFFFFFFF;
    if (Player_hold_swap_weapon)
    {
        human_equip_inventory_item(pp, pp->last_equipped_weapon);
    }
    if (!controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index))
    {
        return;
    }
    // The throw: the trigger picks the strength and lowers the arc. Original: rb is used here without the null test above.
    const float analog = controls_get_button_analog(CBA_OFC_ATTACK_PRIMARY, Player_controller_index);
    const float mass = havok_body_get_mass(rb);
    const float mass_scale = (float)pow((double)mass, (double)0.7f); // Magic number: 0.7, lighter objects fly faster.
    vector dir;
    dir.x = Rfg_camera.real_orient.fvec.x;
    dir.y = (float)((1.0 - (double)analog) * (double)Player_hold_throw_arc + (double)Rfg_camera.real_orient.fvec.y);
    dir.z = Rfg_camera.real_orient.fvec.z;
    dir.normalize_safe();
    const float strength = (float)(((double)Player_hold_throw_strength_max - (double)Player_hold_throw_strength_min) * (double)analog + (double)Player_hold_throw_strength_min);
    vector velocity;
    velocity.x = dir.x * strength * mass_scale / mass;
    velocity.y = dir.y * strength * mass_scale / mass;
    velocity.z = dir.z * strength * mass_scale / mass;
    if (Player_hold_max_throw_speed < velocity.mag())
    {
        const vector capped = velocity.copy_normalize();
        velocity.x = capped.x * Player_hold_max_throw_speed;
        velocity.y = capped.y * Player_hold_max_throw_speed;
        velocity.z = capped.z * Player_hold_max_throw_speed;
    }
    vector impulse;
    impulse.x = velocity.x * mass;
    impulse.y = velocity.y * mass;
    impulse.z = velocity.z * mass;
    havok_body_apply_linear_impulse(rb, impulse);
}

// 0x00A11920 (:481): empty in the release build.
void player_hold_object_level_init()
{
}

// 0x00A11930 (:486): prints the held object's mass with the debug flag; retail strips debug drawing (render_debug.md).
void player_hold_render_debug()
{
    if (!Player_hold_debug)
    {
        return;
    }
    object* objp = World.object_from_handle(Player->hold_object_handle);
    if (objp == nullptr)
    {
        return;
    }
    // Deviation: the text is not drawn (gr_string has no target here); the flag is false in rfg.exe anyway.
}
