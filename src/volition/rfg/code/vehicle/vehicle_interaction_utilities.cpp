// Original: volition/rfg/code/vehicle/vehicle_interaction_utilities.cpp
//
// Who may enter, seat lookups and the camera switches around entering and leaving a vehicle.

#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"

#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/control/control_spectator.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"

#include <math.h>

// 0x00B689A0 (:62)
bool vi_utilities::can_human_enter_vehicles(human* hp)
{
    if (human_is_ragdolled(hp)) {
        return false;
    }
    if (human_is_throwing_weapon(hp)) {
        return false;
    }
    if (!human_is_in_movement_mode(hp, HMM_JUMP, HMSM_NONE) && !human_is_in_movement_mode(hp, HMM_FALL, HMSM_NONE)) {
        return true;
    }
    // in the air: a client trusts the server
    return Multiplayer && !multi_is_server();
}

// 0x00B83880 (:87)
bool vi_utilities::vehicle_seat_is_valid(const vehicle* vp, vehicle_seat_index seat_index)
{
    if (seat_index == VSI_INVALID) {
        return false;
    }
    if (vp->info->vi_info == NULL) {
        return seat_index == VSI_DRIVER;
    }
    return waypoint_index_from_seat_index(vp, seat_index) != -1;
}

// 0x00B68A00 (:105)
int vi_utilities::waypoint_index_from_seat_index(const vehicle* vp, vehicle_seat_index seat_index)
{
    if (seat_index != VSI_INVALID) {
        const vehicle_interaction_info* info = vp->info->vi_info;
        if (info != NULL) {
            for (int i = 0; i < info->num_waypoints; i++) {
                if (info->waypoints[i].seat_index == seat_index) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// 0x00B838C0 (:146)
void vi_utilities::switch_to_vehicle_camera(human* hp, vehicle* target_vp)
{
    if (!object_is_local_player(hp) && (!Multiplayer || !control_spectator_is_spectating_object(hp))) {
        return;
    }
    if (Rfg_camera.mode == CAMERA_LOOKAROUND_MODE) {
        camera_stop_lookaround_mode(false);
    }
    else if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE) {
        camera_stop_first_person(false);
    }
    else if (Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE) {
        camera_stop_third_person(false);
    }

    const base_vehicle_types classification = target_vp->info->vehicle_classification;
    bool turret_camera = false;
    if (hp->vehicle_seat_idx != VSI_DRIVER || classification == BVT_TANK || classification == BVT_ART_TANK) {
        turret* manned = human_combat_get_manned_turret(hp);
        vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(target_vp, hp->vehicle_seat_idx);
        turret* seat_turret = (mount != NULL && mount->attached_turret != 0xFFFFFFFF) ? World.object_from_handle<turret>(mount->attached_turret) : NULL;
        turret_camera = manned != NULL && seat_turret != NULL && manned == seat_turret;
    }
    if (turret_camera) {
        camera_start_turret_mode(true);
        camera_free_set_target(hp);
        fb_accum_blur_set_cap(Vehicle_turret_cam_blur);
    }
    else {
        camera_free_set_target(target_vp);
        camera_free_set_submode(FREE_CAMERA_SUBMODE_VEHICLE_DRIVER, 1.0f, 1.5f);
        if (wrecking_crew_is_active()) {
            camera_trans_abort();
        }
        camera_free_reset_pitch();
        // the distance the swing starts from: the camera to the vehicle's lookat point
        const free_submode_params* params = Rfg_camera.free_params.submode_params;
        vector lookat(params->lookat_offset_x, 0.0f, params->lookat_offset_z);
        lookat.unrotate(target_vp->orient);
        lookat.x += target_vp->pos.x + 0.0f;
        lookat.y += target_vp->pos.y + params->lookat_offset_y;
        lookat.z += target_vp->pos.z + 0.0f;
        const vector& cam = Rfg_camera.real_pos;
        Rfg_camera.free_params.cur_dist_from_lookat = sqrtf((cam.y - lookat.y) * (cam.y - lookat.y) + (cam.x - lookat.x) * (cam.x - lookat.x) + (cam.z - lookat.z) * (cam.z - lookat.z));
        Rfg_camera.free_params.goal_dist_from_lookat = Rfg_camera.free_params.cur_dist_from_lookat;
        camera_free_swing_to_dir(target_vp->orient.fvec, 1.5f);
        if (target_vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
            Rfg_camera.free_params.control_type = (classification == BVT_TANK || classification == BVT_ART_TANK) ? camera_free_get_tank_control_type() : camera_free_get_vehicle_control_type();
        }
        else {
            Rfg_camera.free_params.control_type = CAMERA_FREE_CONTROL_TYPE_NORMAL;
        }
    }
    if (!Multiplayer) {
        gamelog_mark_vehicle_start_time();
    }
}

// 0x00B68A50 (:264)
void vi_utilities::switch_to_on_foot_camera(human* new_target)
{
    if (!object_is_local_player(new_target->get_handle()) && (!Multiplayer || !control_spectator_is_spectating_object(new_target))) {
        return;
    }
    if (Rfg_camera.mode == CAMERA_DEATH_MODE) {
        return;
    }
    if (camera_get_mode() == CAMERA_TURRET_MODE) {
        camera_stop_turret_mode(true);
        fb_accum_blur_set_cap(0.0f);
    }
    if (RFG_on_foot_camera_mode == CAMERA_FIRST_PERSON_MODE || RFG_on_foot_camera_mode == CAMERA_THIRD_PERSON_MODE) {
        camera_free_set_target(new_target);
        if (RFG_on_foot_camera_mode != CAMERA_FIRST_PERSON_MODE) {
            camera_start_third_person(true);
        }
        else {
            camera_start_first_person(true);
        }
        return;
    }
    camera_free_set_submode(FREE_CAMERA_SUBMODE_EXTERIOR_STAND, 1.0f, 0.3f);
    camera_free_set_target(new_target);
}
