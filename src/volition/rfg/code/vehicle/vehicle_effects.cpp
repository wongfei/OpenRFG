// Original: volition/rfg/code/vehicle/vehicle_effects.cpp
//
// Automobile tire track decals, wheel and collision effects, scraping, the vehicle camera shakes and the lights
// (headlights by the hour, brake, reverse, emergency, strobe). Compiled in the vehicle unity TU.

#include "volition/rfg/code/vehicle/vehicle_effects.h"

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/vlib/code/util/varray.h"

// size 0x8
struct track_decal_info
{
    int decal_info_index; // 0x00
    float length; // 0x04
};

// ---- file-static globals
static float Tire_tracks_fade_rate = 0.0005f; // 0x016BA4F0
static float Tire_track_decal_fade_multiplier = 0.5f; // 0x016BA4F4
static float Automobile_tire_track_max_dist_from_track = 0.05f; // 0x016BA4F8
static float Vehicle_headlights_max_delay = 1.0f; // 0x016BA504, game hours
static float Vehicle_camera_shake_mass_small; // 0x0376F650, from the tweak table
static float Vehicle_camera_shake_mass_large; // 0x0376F658, from the tweak table
static tweak_table_item Tweak_table_item_Vehicle_camera_shake_mass_large("Vehicle_camera_shake_mass_large", &Vehicle_camera_shake_mass_large);
static tweak_table_item Tweak_table_item_Vehicle_camera_shake_mass_small("Vehicle_camera_shake_mass_small", &Vehicle_camera_shake_mass_small);
static tweak_table_item Tweak_table_item_Tire_tracks_fade_rate("Tire_tracks_fade_rate", &Tire_tracks_fade_rate);
static tweak_table_item Tweak_table_item_Tire_track_decal_fade_multiplier("Tire_track_decal_fade_multiplier", &Tire_track_decal_fade_multiplier);
static tweak_table_item Tweak_table_item_Automobile_tire_track_max_dist_from_track("Automobile_tire_track_max_dist_from_track", &Automobile_tire_track_max_dist_from_track);
static tweak_table_item Tweak_table_item_Vehicle_headlights_time_on("Vehicle_headlights_time_on", &Vehicle_headlights_time_on);
static tweak_table_item Tweak_table_item_Vehicle_headlights_time_off("Vehicle_headlights_time_off", &Vehicle_headlights_time_off);
static tweak_table_item Tweak_table_item_Vehicle_headlights_max_delay("Vehicle_headlights_max_delay", &Vehicle_headlights_max_delay);
static tweak_table_item Tweak_table_item_Vehicle_emergency_lights_blink_time("Vehicle_emergency_lights_blink_time", &Vehicle_emergency_lights_blink_time);
static track_decal_info* Tire_track_decal_infos; // 0x0376F628
static int Num_tire_track_decal_infos; // 0x03770010
static decal_circ_array_queue<96> Tire_track_decal_list; // 0x0383B630
static circ_array<int, 30> Counted_tracks; // 0x0386C008, tracks started in each of the last 30 frames

// <96> 0x00B948D0 (object_decal.h:68), as <10> in object_decal.cpp
template<>
void decal_circ_array_queue<96>::add(unsigned int handle, float opacity)
{
    if (num == 96) {
        num = 95;
        rl_decal_release(carray[95].handle);
    }
    const decal_fade_data data(handle, opacity);
    carray.add(data);
    num++;
}

// 0x00B663F0 (:193)
void automobile_tire_track_infos_read_table()
{
    xml_element* element = xml_table_parse("tire_track_decal.xtbl");
    Num_tire_track_decal_infos = xml_count(element, "Tire_track_decal");
    if (Num_tire_track_decal_infos < 1) {
        return;
    }
    Tire_track_decal_infos = (track_decal_info*)Permanent_mempool.alloc(Num_tire_track_decal_infos * sizeof(track_decal_info), 0);
    xml_element* root = xml_find(element, "Tire_track_decal");
    for (int i = 0; i < Num_tire_track_decal_infos; i++) {
        if (root == NULL) {
            return; // Original bug kept: the table stays open
        }
        track_decal_info& info = Tire_track_decal_infos[i];
        info.decal_info_index = decal_info_find(xml_table_get_req_string_ref(root, "Name"));
        rl_decal_parameters params;
        decal_info_get(&params, info.decal_info_index);
        info.length = params.m_planar_params.m_length;
        root = xml_find_next(element, root, "Tire_track_decal");
    }
    xml_table_close(element);
}

// 0x00B664D0 (LTCG: length in XMM0) The decal whose length is closest to this; -1 for under half the shortest.
static int tire_track_array_idx_from_length(float length)
{
    if (length < Tire_track_decal_infos[0].length * 0.5f) {
        return -1;
    }
    const int last = Num_tire_track_decal_infos - 1;
    for (int i = 0; i < last; i++) {
        if (length <= Tire_track_decal_infos[i + 1].length) {
            return length < (Tire_track_decal_infos[i + 1].length + Tire_track_decal_infos[i].length) * 0.5f ? i : i + 1;
        }
    }
    return last;
}

// 0x00B66560 (LTCG: data in ESI, ap in EDI, width and decal_info_idx on the stack)
static void automobile_decal_from_track_data(automobile* ap, automobile::tire_track_decal_data& data, float width, int decal_info_idx)
{
    object* on_object = World.object_from_handle(data.m_obj_handle);
    if (on_object == NULL) {
        return;
    }
    if (data.m_decal_handle != 0xFFFFFFFF) {
        rl_decal_release(data.m_decal_handle);
    }
    rl_decal_parameters params;
    if (decal_info_idx == -1) {
        const int index = tire_track_array_idx_from_length(data.m_length);
        if (index == -1) {
            data.m_decal_handle = 0xFFFFFFFF;
            return;
        }
        decal_info_idx = Tire_track_decal_infos[index].decal_info_index;
    }
    decal_info_get(&params, decal_info_idx);
    const float half_length = params.m_planar_params.m_length * 0.5f;
    params.m_pos.set(data.m_start_pos.x + data.m_move_dir.x * half_length, data.m_start_pos.y + data.m_move_dir.y * half_length, data.m_start_pos.z + data.m_move_dir.z * half_length);
    params.m_planar_params.m_width = width;
    params.m_transform.make_uvec(data.m_normal, data.m_move_dir);
    if (!ap->m_decal_collection_occurred) {
        // the surfaces near the first track of the frame, shared by all four wheels
        rl_decal_parameters collect_params;
        collect_params.m_pos = params.m_pos;
        object_zone_decal_collect(on_object, ap->m_decal_collection_data, ap->m_num_decal_collections, collect_params, ap->m_decal_width);
        ap->m_decal_collection_occurred = true;
    }
    data.m_decal_handle = object_zone_decal_apply_collection(on_object, ap->m_decal_collection_data, ap->m_num_decal_collections, params, 0.0f);
}

// 0x00B66760 (LTCG: handle_a in ECX, pos in EDI, relative_velocity in ESI)
static float get_scraping_relative_speed(vector* relative_velocity, unsigned int handle_a, unsigned int handle_b, const vector* pos, const vector* normal)
{
    vector velocity_a;
    vector velocity_b;
    havok_body_get_point_velocity(handle_a, *pos, velocity_a);
    havok_body_get_point_velocity(handle_b, *pos, velocity_b);
    const vector difference(velocity_b.x - velocity_a.x, velocity_b.y - velocity_a.y, velocity_b.z - velocity_a.z);
    vec_projection_onto_plane(*relative_velocity, difference, *normal);
    return relative_velocity->mag();
}

// 0x00B7C720 (LTCG: f_collision_info in ESI)
static void stop_scraping_effect_from_struct(automobile::collision_effect_info* f_collision_info)
{
    if (audiolib_is_playing(f_collision_info->m_foley_info, true)) {
        audiolib_play_fade_out(f_collision_info->m_foley_info, 200);
    }
    object_effect* effect = vehicle_effect_from_handle(f_collision_info->m_effect_obj);
    if (effect != NULL) {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
    f_collision_info->m_position.zero();
    f_collision_info->m_normal.zero();
    f_collision_info->m_other_handle = 0xFFFFFFFF;
    f_collision_info->m_foley_info = -1;
    f_collision_info->m_effect_obj = 0xFFFFFFFF;
}

// 0x00B66830 (:878) How large the vehicle is, between the small and large shakes.
static float camera_shake_blend_value(const vehicle* vp)
{
    return fl_parameter(vp->get_mass(), Vehicle_camera_shake_mass_small, Vehicle_camera_shake_mass_large, true);
}

// 0x00B66870 (LTCG: pos in ESI) A quarter meter towards the camera, so the corona clears the mesh.
static void vehicle_light_adjust_position_for_camera(vector* pos)
{
    vector to_camera(Rfg_camera.real_pos.x - pos->x, Rfg_camera.real_pos.y - pos->y, Rfg_camera.real_pos.z - pos->z);
    to_camera.normalize_safe();
    pos->x += to_camera.x * vehicle_light_camera_fudge_distance;
    pos->y += to_camera.y * vehicle_light_camera_fudge_distance;
    pos->z += to_camera.z * vehicle_light_camera_fudge_distance;
}

// 0x00B7C790 (LTCG: vp in ESI)
static bool vehicle_light_brake_lights_on(vehicle* vp)
{
    if (vp->get_sub_type() != OT_SUB_VEHICLE_AUTO || vp->most_recent_driver_exit_time.valid() || vehicle_human_from_handle(vp->most_recent_driver) == NULL) {
        return false;
    }
    automobile* ap = (automobile*)vp;
    return 0.2f <= automobile_get_brake_value(vp->get_handle()) || ap->brake_effect_release_timer.valid() || automobile_get_handbrake_on(ap);
}

// 0x00B7C810 (:292)
void automobile_process_wheel_effect(unsigned int& effect_handle, bool play_effect, bool stop_effect, const vector& contact_point, const matrix& orient, unsigned int info_handle, bool looping)
{
    if (play_effect) {
        if (effect_handle == 0xFFFFFFFF) {
            effect_handle = effects_play(info_handle, contact_point, orient, looping, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            return;
        }
        object_effect* effect = World.object_from_handle<object_effect>(effect_handle);
        if (effect != NULL) {
            effect->update_pos_and_orient(contact_point, orient, true);
        }
        else if (looping) {
            effect_handle = 0xFFFFFFFF;
        }
    }
    else if (effect_handle != 0xFFFFFFFF && stop_effect) {
        object_effect* effect = World.object_from_handle<object_effect>(effect_handle);
        if (effect != NULL) {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
        effect_handle = 0xFFFFFFFF;
    }
}

// 0x00B7C8D0 (:362)
bool automobile_tire_track_decal_adjust(automobile* ap, automobile::tire_track_decal_data& data, const vector& new_end, unsigned int contact_obj, float width)
{
    if (data.m_obj_handle == 0xFFFFFFFF) {
        return false;
    }
    if (data.m_obj_handle != contact_obj) {
        // left the object: the track ends here. Original quirk kept: rfg.exe searches for the decal that fits the length,
        // then uses the longest anyway.
        automobile_decal_from_track_data(ap, data, width, Tire_track_decal_infos[Num_tire_track_decal_infos - 1].decal_info_index);
        return false;
    }
    float new_length;
    if (data.m_decal_handle == 0xFFFFFFFF) {
        data.m_move_dir.set(new_end.x - data.m_start_pos.x, new_end.y - data.m_start_pos.y, new_end.z - data.m_start_pos.z);
        new_length = data.m_move_dir.mag_and_normalize_safe();
    }
    else {
        line ray;
        ray.dir = data.m_move_dir;
        ray.point = data.m_start_pos;
        vector closest;
        new_length = ix_closest_point_on_ray(new_end, ray, closest);
        const vector off_track(new_end.x - closest.x, new_end.y - closest.y, new_end.z - closest.z);
        if (Automobile_tire_track_max_dist_from_track * Automobile_tire_track_max_dist_from_track < off_track.x * off_track.x + off_track.y * off_track.y + off_track.z * off_track.z) {
            return false;
        }
    }
    const int index = tire_track_array_idx_from_length(new_length);
    if (index == -1) {
        return data.m_decal_handle == 0xFFFFFFFF;
    }
    if (index != tire_track_array_idx_from_length(data.m_length)) {
        // a longer decal: straighten the track to where the wheel is now
        const vector to_end(new_end.x - data.m_start_pos.x, new_end.y - data.m_start_pos.y, new_end.z - data.m_start_pos.z);
        data.m_move_dir = to_end.copy_normalize_safe();
        const float along = data.m_normal.x * data.m_move_dir.x + data.m_normal.y * data.m_move_dir.y + data.m_normal.z * data.m_move_dir.z;
        const vector normal(data.m_normal.x - data.m_move_dir.x * along, data.m_normal.y - data.m_move_dir.y * along, data.m_normal.z - data.m_move_dir.z * along);
        data.m_normal = normal.copy_normalize_safe();
        automobile_decal_from_track_data(ap, data, width, Tire_track_decal_infos[index].decal_info_index);
    }
    data.m_length = new_length < Tire_track_decal_infos[index].length ? new_length : Tire_track_decal_infos[index].length;
    return new_length <= Tire_track_decal_infos[Num_tire_track_decal_infos - 1].length;
}

// 0x00B66970 (:457)
void automobile_tire_track_decal_create(automobile* ap, automobile::tire_track_decal_data& data, const vector& wheel_pos, const vector& move_dir, const vector& normal, const vector& approx_start_pos, unsigned int on_object, float width)
{
    data.m_start_pos = approx_start_pos;
    data.m_move_dir.set(wheel_pos.x - approx_start_pos.x, wheel_pos.y - approx_start_pos.y, wheel_pos.z - approx_start_pos.z);
    data.m_length = data.m_move_dir.mag_and_normalize_safe();
    data.m_normal = normal;
    data.m_obj_handle = on_object;
    automobile_decal_from_track_data(ap, data, width, -1);
}

// 0x00B97E20 (:486)
void automobile_tire_track_decal_start_fade(unsigned int handle)
{
    if (handle == 0xFFFFFFFF) {
        return;
    }
    Tire_track_decal_list.add(handle, 1.0f);
    Counted_tracks[0]++;
}

// 0x00B66A40 (:509)
bool automobile_tire_track_data_is_valid(const automobile::tire_track_decal_data& data)
{
    return data.m_obj_handle != 0xFFFFFFFF;
}

// 0x00B66A50 (:514)
bool automobile_tire_track_data_is_displaying(const automobile::tire_track_decal_data& data)
{
    return data.m_decal_handle != 0xFFFFFFFF;
}

// 0x00B8EF00 (:520)
void automobile_effects_tire_tracks_do_frame()
{
    int recent_tracks = 0;
    for (int i = 0; i < Counted_tracks.size(); i++) {
        recent_tracks += Counted_tracks[i];
    }
    // the more tracks are being made, the faster they fade (96 decals over 30 frames)
    float fade = (float)recent_tracks / 2880.0f;
    if (fade < Tire_tracks_fade_rate) {
        fade = Tire_tracks_fade_rate;
    }
    int age_weight = Tire_track_decal_list.size() - 97 + (Tire_track_decal_list.size() - 1);
    for (int i = Tire_track_decal_list.size() - 1; -1 < i; i--) {
        decal_fade_data& decal = Tire_track_decal_list[i];
        decal.opacity -= fade;
        decal.opacity -= ((float)age_weight / 96.0f) * fade * Tire_track_decal_fade_multiplier;
        if (decal.opacity <= 0.0f) {
            Tire_track_decal_list.remove(); // the oldest
        }
        else {
            rl_color_float color;
            color.red = 1.0f;
            color.green = 1.0f;
            color.blue = 1.0f;
            color.alpha = decal.opacity;
            rl_decal_update_color(decal.handle, color, 0xFFFFFFFF);
        }
        age_weight -= 2;
    }
    Counted_tracks.add(0); // this frame's slot
}

// 0x00B7CD10 (:611)
void automobile_play_collision_effects(automobile* ap, unsigned int component_index, object* other_object, const vector* position, const vector* normal, const hkpRigidBody* other_rigid_body, float speed_along_normal, float speed_along_surface, bool play_scraping_sounds)
{
    if (ap->m_sound_delay_after_explosion.valid() && !ap->m_sound_delay_after_explosion.elapsed()) {
        return;
    }
    const unsigned int other_handle = havok_rigid_body_get_handle(other_rigid_body);
    bool played_material_effect = false;
    // an impact, at 2.5 m/s into the surface, in a free slot of 4
    if (2.5f <= speed_along_normal && (!ap->m_impact_sound_delay.valid() || ap->m_impact_sound_delay.elapsed())) {
        for (int i = 0; i < 4; i++) {
            automobile::collision_effect_info& impact = ap->m_collision_impact_sounds[i];
            if (audiolib_is_playing(impact.m_foley_info, true) || impact.m_effect_obj != 0xFFFFFFFF) {
                continue;
            }
            material_event_size size = MATERIAL_EVENT_SIZE_MEDIUM;
            int cue;
            if (component_index != 0xFFFFFFFF) {
                cue = ap->info->m_foley_component_impact_id;
            }
            else if (ap->m_flags.m_corpse != 0) {
                cue = ap->info->m_foley_corpse_impact_id;
            }
            else if (10.0f <= speed_along_normal) {
                cue = ap->info->m_foley_impact_id[1];
                size = MATERIAL_EVENT_SIZE_LARGE;
            }
            else {
                cue = ap->info->m_foley_impact_id[0];
            }
            impact.m_foley_info = audiolib_cue_play_3d(cue, position, NULL, NULL, NULL);
            impact.m_position.set(position->x - ap->pos.x, position->y - ap->pos.y, position->z - ap->pos.z);
            impact.m_position.rotate(ap->orient);
            impact.m_normal = normal->copy_rotate(ap->orient);
            impact.m_other_handle = other_object->get_handle();
            ap->m_impact_sound_delay.set(250);
            const vector& effect_dir = normal != NULL ? *normal : ap->orient.fvec;
            if (other_object->get_obj_type() != OT_MOVER) {
                const unsigned char own_material = ap->get_effect_material(0xFFFFFFFF, position, NULL);
                const unsigned char other_material = other_object->get_effect_material(0xFFFFFFFF, position, NULL);
                player* local = player_get_local_player();
                const bool local_inside = local != NULL && human_in_vehicle(local, ap->get_handle());
                rfg_effect_candidate_create(own_material, *position, MATERIAL_EVENT_IMPACT, size, MATERIAL_EVENT_SHAPE_SOLID, local_inside, 0, NULL, NULL, 0xFFFFFFFF, other_material, 0xFF);
                played_material_effect = true;
            }
            if (ap->info->m_effect_collision != 0xFFFFFFFF) {
                matrix orient;
                orient.make(effect_dir);
                impact.m_effect_obj = effects_play(ap->info->m_effect_collision, *position, orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            break;
        }
    }
    // a scrape, at 2 m/s along the surface
    if (!play_scraping_sounds || speed_along_surface < 2.0f || (ap->m_scraping_sound_delay.valid() && !ap->m_scraping_sound_delay.elapsed())) {
        return;
    }
    if (component_index != 0xFFFFFFFF && (ap->rdd.get_data()->m_components[component_index].get_flags() & VCF_DETACHED) != 0) {
        return;
    }
    if (ap->is_killed()) {
        return;
    }
    automobile::collision_effect_info* scrape = NULL;
    for (int i = 0; i < 4 && scrape == NULL; i++) {
        if (ap->m_collision_scraping_sounds[i].m_foley_info == -1 && ap->m_collision_scraping_sounds[i].m_effect_obj == 0xFFFFFFFF) {
            scrape = &ap->m_collision_scraping_sounds[i];
        }
    }
    if (scrape == NULL) {
        return;
    }
    vector relative;
    const float relative_speed = get_scraping_relative_speed(&relative, ap->havok_handle, other_handle, position, normal); // inlined in rfg.exe
    if (relative_speed == 0.0f) {
        return;
    }
    scrape->m_foley_info = audiolib_cue_play_3d(ap->info->m_foley_scraping_id, position, NULL, NULL, NULL);
    matrix orient;
    if (vec_test_parallel(relative, *normal, 0.0001f)) {
        orient.make(relative);
    }
    else {
        orient.make_rvec(relative, *normal);
    }
    // Original bug kept: the effect is played with the vehicle's orientation; the matrix just made goes unused.
    scrape->m_effect_obj = effects_play(ap->info->m_effect_scrape, *position, ap->orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    object_effect* effect = vehicle_effect_from_handle(scrape->m_effect_obj);
    if (effect != NULL) {
        effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, relative_speed);
    }
    scrape->m_position.set(position->x - ap->pos.x, position->y - ap->pos.y, position->z - ap->pos.z);
    scrape->m_position.rotate(ap->orient);
    scrape->m_normal = normal->copy_rotate(ap->orient);
    scrape->m_other_handle = other_object->get_handle();
    ap->m_scraping_sound_delay.set(150);
    if (other_object->get_obj_type() != OT_MOVER && !played_material_effect) {
        const unsigned char own_material = ap->get_effect_material(0xFFFFFFFF, position, NULL);
        const unsigned char other_material = other_object->get_effect_material(0xFFFFFFFF, position, NULL);
        rfg_effect_candidate_create(own_material, *position, MATERIAL_EVENT_SLIDE, MATERIAL_EVENT_SIZE_LARGE, MATERIAL_EVENT_SHAPE_SOLID, vehicle_player_in_vehicle(ap), 0, NULL, NULL, 0xFFFFFFFF, other_material, 0xFF);
    }
}

// 0x00B7D400 (:774)
void automobile_stop_scraping_effect(automobile* ap, const object* other_obj)
{
    for (int i = 0; i < 4; i++) {
        automobile::collision_effect_info& scrape = ap->m_collision_scraping_sounds[i];
        if (World.object_from_handle(scrape.m_other_handle) == other_obj) {
            stop_scraping_effect_from_struct(&scrape); // inlined in rfg.exe
        }
    }
}

// 0x00B7D4A0 (:790)
void automobile_stop_all_scraping_effects(automobile* ap)
{
    for (int i = 0; i < 4; i++) {
        stop_scraping_effect_from_struct(&ap->m_collision_scraping_sounds[i]); // inlined in rfg.exe
    }
}

// 0x00B7D520 (:802)
void automobile_collision_effects_update_pos_orient(automobile* ap)
{
    for (int i = 0; i < 4; i++) {
        automobile::collision_effect_info& impact = ap->m_collision_impact_sounds[i];
        if (audiolib_is_playing(impact.m_foley_info, true)) {
            const vector offset = impact.m_position.copy_unrotate(ap->orient);
            const vector world_pos(offset.x + ap->pos.x, ap->pos.y + offset.y, ap->pos.z + offset.z);
            audiolib_play_set_3d_position(impact.m_foley_info, &world_pos, NULL, NULL);
        }
        if (vehicle_effect_from_handle(impact.m_effect_obj) == NULL) {
            impact.m_effect_obj = 0xFFFFFFFF;
        }
    }
    for (int i = 0; i < 4; i++) {
        automobile::collision_effect_info& scrape = ap->m_collision_scraping_sounds[i];
        if (scrape.m_foley_info == -1 && scrape.m_effect_obj == 0xFFFFFFFF) {
            continue;
        }
        if (!audiolib_is_playing(scrape.m_foley_info, true)) {
            scrape.m_foley_info = -1;
        }
        object_effect* effect = vehicle_effect_from_handle(scrape.m_effect_obj);
        if (effect == NULL) {
            scrape.m_effect_obj = 0xFFFFFFFF;
        }
        const vector offset = scrape.m_position.copy_unrotate(ap->orient);
        const vector world_pos(offset.x + ap->pos.x, ap->pos.y + offset.y, ap->pos.z + offset.z);
        object* other = World.object_from_handle(scrape.m_other_handle);
        if (other == NULL || havok_body_get_pointer(other->havok_handle) == NULL) {
            stop_scraping_effect_from_struct(&scrape); // inlined in rfg.exe
            continue;
        }
        const vector world_normal = scrape.m_normal.copy_unrotate(ap->orient);
        vector relative = ZERO_VECTOR;
        const float speed = get_scraping_relative_speed(&relative, ap->havok_handle, other->havok_handle, &world_pos, &world_normal); // inlined in rfg.exe
        float volume = 0.0f;
        if (speed < 2.0f) {
            if (speed < 1.0f) {
                stop_scraping_effect_from_struct(&scrape); // inlined in rfg.exe
                continue;
            }
        }
        else {
            volume = fl_parameter(speed, 2.0f, 30.0f, true);
        }
        audiolib_play_set_3d_position(scrape.m_foley_info, &world_pos, NULL, NULL);
        audiolib_play_set_volume(scrape.m_foley_info, volume, 100);
        if (effect != NULL) {
            relative.normalize_safe();
            matrix orient;
            orient.make_rvec(relative, world_normal);
            effect->update_pos_and_orient(world_pos, orient, true);
            effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
        }
    }
}

// Whether the local player is in the vehicle. Inlined in the shakes below.
static bool vehicle_shake_applies(const vehicle* vp)
{
    player* local = player_get_local_player();
    return vp != NULL && local != NULL && human_in_vehicle(local, vp->get_handle());
}

// 0x00B66A60 (:898)
void vehicle_camera_shake_play_engine_start(vehicle* vp)
{
    player* local = player_get_local_player(); // Original bug kept: not checked for null
    if (human_in_vehicle_interaction(local, vp) || local->ground_obj_handle == vp->get_handle()) {
        const float blend = camera_shake_blend_value(vp);
        camera_shake_play("vehicle_start_small", 1.0f - blend, false);
        camera_shake_play("vehicle_start_large", blend, false);
    }
}

// 0x00B66B10 (:910)
void vehicle_camera_shake_play_shift(vehicle* vp)
{
    if (vehicle_shake_applies(vp)) {
        const float blend = camera_shake_blend_value(vp);
        camera_shake_play("vehicle_shift_small", 1.0f - blend, false);
        camera_shake_play("vehicle_shift_large", blend, false);
    }
}

// 0x00B66BC0 (:922)
void vehicle_camera_shake_play_fast(const vehicle* vp, float intensity)
{
    if (vehicle_shake_applies(vp)) {
        const float blend = camera_shake_blend_value(vp);
        camera_shake_play("vehicle_fast_small", (1.0f - blend) * intensity, false);
        camera_shake_play("vehicle_fast_large", blend * intensity, false);
    }
}

// 0x00B66C80 (:935)
void vehicle_camera_shake_play_land(vehicle* vp, float intensity)
{
    if (vehicle_shake_applies(vp)) {
        camera_shake_play("vehicle_jump_land", intensity, false);
    }
}

// 0x00B66CC0 (:945)
void vehicle_camera_shake_play_land_soft(vehicle* vp, float intensity)
{
    if (vehicle_shake_applies(vp)) {
        camera_shake_play("vehicle_jump_land_soft", intensity, false);
    }
}

// 0x00B66D00 (:955)
void vehicle_camera_shake_play_brake(vehicle* vp, float intensity)
{
    if (vehicle_shake_applies(vp)) {
        camera_shake_play("vehicle_braking", intensity, false);
    }
}

// 0x00B66D40 (:965)
void vehicle_camera_shake_play_skid(vehicle* vp, float intensity)
{
    if (vehicle_shake_applies(vp)) {
        camera_shake_play("vehicle_skidding", intensity, false);
    }
}

// 0x00B66D80 (:975)
void vehicle_camera_shake_play_onfire(vehicle* vp)
{
    if (vehicle_shake_applies(vp)) {
        camera_shake_play("vehicle_onfire", 1.0f, false);
    }
}

// 0x00B66DC0 (:1216)
bool vehicle_effects_headlights_should_be_on()
{
    const int hour = game_clock_get()->m_hour;
    return !(Vehicle_headlights_time_off < hour && hour < Vehicle_headlights_time_on);
}

// 0x00B7D960 (:986)
void vehicle_effects_process_lights(vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    // each vehicle's headlights come on at a random moment within the delay
    if (vehicle_effects_headlights_should_be_on() != (vp->m_flags.m_headlights_on != 0)) { // inlined in rfg.exe
        const float game_seconds = game_clock_get()->m_time_scale * Frametime;
        if (fl_rand() <= game_seconds / (Vehicle_headlights_max_delay * 60.0f * 60.0f)) {
            vp->m_flags.m_headlights_on = vp->m_flags.m_headlights_on == 0;
        }
    }
    if (vp->emergency_light_timer.elapsed()) {
        vp->m_flags.m_emergency_lights_on = vp->m_flags.m_emergency_lights_on == 0;
        vp->emergency_light_timer.set(vehicle_effects_get_emergency_blink_time_ms()); // inlined in rfg.exe
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        vehicle_light& light = data->m_vehicle_lights[i];
        if (light.type == VLT_INVALID || light.flags.is_broken) {
            continue;
        }
        if (light.flags.broken) {
            // just hit: once its sparks, and it stays off
            light.stop_all_effects();
            const int tag = (int)(((i & 0xFF) | 0x300) << 16); // the light itself, see vehicle::get_tag_world_coords
            vector pos;
            matrix orient;
            vp->get_tag_world_coords(tag, pos, &orient, NULL, NULL);
            effects_play(effects_get_handle("veh_broken_light_fx_a"), pos, orient, false, vp->get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            light.flags.is_on = false;
            light.flags.dirty = true;
            light.flags.is_broken = true;
            vp->set_render_flags(8);
            continue;
        }
        unsigned int effect_info = 0xFFFFFFFF;
        unsigned int second_effect_info = 0xFFFFFFFF;
        bool wanted = false;
        const vehicle_info* info = vp->info;
        const bool lit = vp->m_flags.m_engine_running != 0 && vp->m_flags.m_headlights_on != 0;
        if (vp->obj_flags._dont_use_me != 0) {
            switch (light.type) {
            case VLT_HEAD_LIGHT:
                wanted = lit;
                second_effect_info = info->m_effect_head_light_secondary;
                effect_info = info->m_effect_head_light_primary;
                break;
            case VLT_FOG_LIGHT:
                wanted = lit;
                effect_info = info->m_effect_fog_light;
                break;
            case VLT_TAIL_LIGHT:
                if (vehicle_light_brake_lights_on(vp)) {
                    effect_info = info->m_effect_brake_light;
                    wanted = true;
                }
                else {
                    effect_info = info->m_effect_tail_light;
                    wanted = lit;
                }
                break;
            case VLT_BRAKE_LIGHT:
                wanted = vehicle_light_brake_lights_on(vp);
                effect_info = info->m_effect_brake_light;
                break;
            case VLT_REVERSE_LIGHT:
                wanted = vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && !vp->most_recent_driver_exit_time.valid() && vehicle_human_from_handle(vp->most_recent_driver) != NULL && automobile_moving_in_reverse(vp->get_handle()) && vp->forward_velocity < -1.0f;
                effect_info = info->m_effect_reverse_light;
                break;
            case VLT_EMERGENCY_LIGHT:
                wanted = vp->m_flags.m_engine_running != 0 && vp->m_flags.m_emergency_lights_on != 0;
                effect_info = info->m_effect_emergency_light;
                break;
            case VLT_STROBE_LIGHT:
                effect_info = info->m_effect_strobe_light;
                wanted = vp->m_flags.m_engine_running != 0;
                break;
            default:
                break;
            }
        }
        vector pos;
        pos.zero();
        matrix orient;
        bool turn_off = false;
        if (light.tag_index != -1) {
            const int component = vp->get_component_index_from_tag_index(light.tag_index);
            if (component != -1 && (data->m_components[component].get_flags() & VCF_DETACHED) != 0) {
                turn_off = true;
            }
            else if (wanted) {
                vp->get_tag_world_coords(light.tag_index, pos, &orient, NULL, NULL);
                // not lit while it faces away from the camera
                if (0.0f <= orient.fvec.y * Rfg_camera.ideal_orient.fvec.y + orient.fvec.x * Rfg_camera.ideal_orient.fvec.x + orient.fvec.z * Rfg_camera.ideal_orient.fvec.z) {
                    turn_off = true;
                }
                else {
                    vehicle_light_adjust_position_for_camera(&pos); // inlined in rfg.exe
                }
            }
        }
        if (turn_off || !wanted) {
            if (!light.flags.is_on) {
                continue;
            }
            light.stop_all_effects();
            light.flags.is_on = false;
            light.flags.dirty = true;
            vp->set_render_flags(8);
            continue;
        }
        if (light.flags.is_on && light.tag_index != -1 && light.effect_handle == 0xFFFFFFFF) {
            light.flags.is_on = false;
        }
        if (!light.flags.is_on) {
            if (light.effect_handle == 0xFFFFFFFF && light.tag_index != -1) {
                light.effect_handle = effects_play(effect_info, pos, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            light.start_coronas(effect_info, second_effect_info, vp);
            light.flags.is_on = true;
            light.flags.dirty = true;
            vp->set_render_flags(8);
        }
        else if (light.corona_effect_info_handle != effect_info) {
            // a tail light turning into a brake light
            light.stop_all_effects();
            if (light.effect_handle == 0xFFFFFFFF && light.tag_index != -1) {
                light.effect_handle = effects_play(effect_info, pos, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            light.start_coronas(effect_info, second_effect_info, vp);
        }
        if (light.flags.is_on) {
            object_effect* effect = World.object_from_handle<object_effect>(light.effect_handle);
            if (effect == NULL) {
                light.effect_handle = 0xFFFFFFFF;
            }
            else {
                effect->update_pos_and_orient(pos, orient, true);
            }
        }
    }
}

// 0x00B7E000 (:1196)
void vehicle_effects_kill_lights(vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        data->m_vehicle_lights[i].stop_all_effects();
        data->m_vehicle_lights[i].flags.broken = true;
        data->m_vehicle_lights[i].flags.dirty = true;
    }
    vp->set_render_flags(8);
}

// 0x00B66DE0 (:1231)
int vehicle_effects_get_emergency_blink_time_ms()
{
    return (int)(Vehicle_emergency_lights_blink_time * 1000.0f);
}

// 0x00B66E00 (:1236)
void vehicle_effects_system_init()
{
    automobile_tire_track_infos_read_table(); // inlined in rfg.exe
}

// 0x00B7E050 (:1243)
void vehicle_effects_level_init_pre()
{
    Counted_tracks.add(0); // inlined in rfg.exe
}

// 0x00B66E10 (:1257)
void vehicle_effects_level_shutdown()
{
}
