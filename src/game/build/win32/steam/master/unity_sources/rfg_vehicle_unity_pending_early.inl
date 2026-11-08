// Statics that files early in rfg_vehicle_unity.cpp use before the file that owns them, and stand-ins for file statics of
// files not ported yet (stub_impl.md S8).
#pragma once

#include "volition/rfg/code/vehicle/vehicle_component_data.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

struct explosion_info;

class vehicle;

// vehicle_interaction.cpp (later in the unity TU than its caller in vehicle_class.cpp)
class vi_instance;
static void vi_update_human_pos(vi_instance* vii, unsigned int human_index); // 0x00BB2770 (:7865)

// File statics of the unported vehicle_ai.cpp, which flyer_do_frame_presim calls: no AI until it joins.
static void vehicle_ai_process_u_turn(vehicle*) // 0x00BD0BD0 (vehicle_ai.cpp:3174)
{
}
static void check_for_dead_driver(vehicle*) // 0x00BC9290 (vehicle_ai.cpp:2968)
{
}
static void vehicle_ai_check_extra_mass(vehicle*) // 0x00BC9340 (vehicle_ai.cpp:3033)
{
}

// vehicle_component.cpp (later in the unity TU than its callers in vehicle_class.cpp)
class vehicle;
class vehicle_component;
class matrix;
class vector;
static void vehicle_component_calc_ws_pos_orient_recursive(vehicle_component*, vehicle*, const matrix*, const vector*);

// a vehicle tweak (its owning file is not identified yet)
static float Vehicle_turret_cam_blur = 0.075f; // 0x016BA55C

// walker.cpp, set by vehicle_system.cpp too
static int Walker_footprint_decal_id; // 0x0376F66C
static const explosion_info* Large_walker_explosion_info; // 0x0376F674
static const explosion_info* Small_walker_explosion_info; // 0x0376F678

// automobile.cpp: the wheel surface table, written by vehicle_table_parser.cpp, which comes first in this unity file
static unsigned int Automobile_motion_front_effect_info_handle = 0xFFFFFFFF; // 0x016BA438
static unsigned int Automobile_motion_back_effect_info_handle = 0xFFFFFFFF; // 0x016BA43C
static int Automobile_wheel_motion_cue_id = -1; // 0x016BA440
static unsigned int Automobile_skid_effect_info_handle = 0xFFFFFFFF; // 0x016BA444
static int Automobile_wheel_skid_cue_id = -1; // 0x016BA448
static unsigned int Automobile_peel_effect_info_handle = 0xFFFFFFFF; // 0x016BA44C
static int Automobile_wheel_peel_cue_id = -1; // 0x016BA450
static unsigned int Automobile_brake_effect_info_handle = 0xFFFFFFFF; // 0x016BA454
static int Automobile_wheel_brake_light_cue_id = -1; // 0x016BA458
static int Automobile_wheel_brake_cue_id = -1; // 0x016BA45C
static int Tank_tread_cue_id;
