// Original: volition/rfg/code/vehicle/vehicle_effects.h
// Implementation: volition/rfg/code/vehicle/vehicle_effects.cpp
//
// Vehicle effects: wheel effects and tire track decals (by surface, faded out), collision and scraping effects, camera
// shakes (engine start, shifts, speed, landing, braking, skids, fire), and the lights (headlights at night, brake
// lights, emergency blinkers).
#pragma once

#include "volition/rfg/code/vehicle/automobile.h"

class hkpRigidBody;
class matrix;
class object;
class vector;
class vehicle;

void automobile_process_wheel_effect(unsigned int& effect_handle, bool play_effect, bool stop_effect, const vector& contact_point, const matrix& orient, unsigned int info_handle, bool looping); // 0x00B7C810 (:355)
bool automobile_tire_track_decal_adjust(automobile* ap, automobile::tire_track_decal_data& data, const vector& new_end, unsigned int contact_obj, float width); // 0x00B7C8D0 (:386)
void automobile_tire_track_decal_create(automobile* ap, automobile::tire_track_decal_data& data, const vector& wheel_pos, const vector& move_dir, const vector& normal, const vector& approx_start_pos, unsigned int on_object, float width); // 0x00B66970 (:471)
void automobile_tire_track_decal_start_fade(unsigned int handle); // 0x00B97E20 (:486)
bool automobile_tire_track_data_is_valid(const automobile::tire_track_decal_data& data); // 0x00B66A40 (:509)
bool automobile_tire_track_data_is_displaying(const automobile::tire_track_decal_data& data); // 0x00B66A50 (:514)
void automobile_tire_track_infos_read_table(); // 0x00B663F0 (:193)
void automobile_effects_tire_tracks_do_frame(); // 0x00B8EF00 (:520)
void automobile_play_collision_effects(automobile* ap, unsigned int component_index, object* other_object, const vector* position, const vector* normal, const hkpRigidBody* other_rigid_body, float speed_along_normal, float speed_along_surface, bool play_scraping_sounds); // 0x00B7CD10 (:586)
void automobile_stop_scraping_effect(automobile* ap, const object* other_obj); // 0x00B7D400 (:774)
void automobile_stop_all_scraping_effects(automobile* ap); // 0x00B7D4A0 (:790)
void automobile_collision_effects_update_pos_orient(automobile* ap); // 0x00B7D520 (:802)
void vehicle_camera_shake_play_engine_start(vehicle* vp); // 0x00B66A60 (:898)
void vehicle_camera_shake_play_shift(vehicle* vp); // 0x00B66B10 (:910)
void vehicle_camera_shake_play_fast(const vehicle* vp, float intensity); // 0x00B66BC0 (:922)
void vehicle_camera_shake_play_land(vehicle* vp, float intensity); // 0x00B66C80 (:935)
void vehicle_camera_shake_play_land_soft(vehicle* vp, float intensity); // 0x00B66CC0 (:945)
void vehicle_camera_shake_play_brake(vehicle* vp, float intensity); // 0x00B66D00 (:955)
void vehicle_camera_shake_play_skid(vehicle* vp, float intensity); // 0x00B66D40 (:965)
void vehicle_camera_shake_play_onfire(vehicle* vp); // 0x00B66D80 (:975)
void vehicle_effects_process_lights(vehicle* vp); // 0x00B7D960 (:986)
void vehicle_effects_kill_lights(vehicle* vp); // 0x00B7E000 (:1196)
bool vehicle_effects_headlights_should_be_on(); // 0x00B66DC0 (:1216)
int vehicle_effects_get_emergency_blink_time_ms(); // 0x00B66DE0 (:1231)
void vehicle_effects_system_init(); // 0x00B66E00 (:1236)
void vehicle_effects_level_init_pre(); // 0x00B7E050 (:1243)
void vehicle_effects_level_shutdown(); // 0x00B66E10 (:1257)
