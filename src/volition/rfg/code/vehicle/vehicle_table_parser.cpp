// Original: volition/rfg/code/vehicle/vehicle_table_parser.cpp
//
// vehicle_defaults.xtbl, vehicle_groups.xtbl, vehicles.xtbl (and the DLC's dlcNN_vehicles.xtbl), vehicle_cover.xtbl and
// vehicle_wheel_surface_effects.xtbl. Compiled in the vehicle unity TU: the statics below are shared with its files.

#include "volition/rfg/code/vehicle/vehicle_table_parser.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/vehicle/vehicle_cover.h"
#include "volition/rfg/code/vehicle/vehicle_defaults.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser_shared.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <stdio.h>

// ---- file-static globals
const char* const Vehicle_seat_tag_names[11] = {"driver", "passenger_1", "passenger_2", "passenger_3", "passenger_4", "passenger_5", "passenger_6", "passenger_7", "passenger_8", "passenger_9", "passenger_10"}; // 0x01336CB0
static const char VEHICLE_FAMILY_POOL_NAME[20] = "Vehicle Family Name"; // 0x01336E44
static const char VEHICLE_STRING_POOL_NAME[21] = "Vehicle info strings"; // 0x01336E58
static int Vehicle_base_unique_id = 1000000; // 0x016BA564
static const char* Vehicle_class_names[3] = {"Automobile", "Flyer", "Walker"}; // 0x016BA568
static const char* Vehicle_spawn_class_name[7] = {"None", "Courier", "Ambulance", "Tank", "Artillery Tank", "Bomber", "Special"}; // 0x016BA574
unsigned int Walker_jetpack_effect_info = 0xFFFFFFFF; // 0x016BA590
static vector DEFAULT_VEHICLE_SPAWN_BOX(2.0f, 2.0f, 2.0f); // 0x016EA9BC
static vehicle_defaults Vehicle_defaults; // 0x0376F7B8
char* Vehicle_info_string_pool_data; // 0x03770068
static char* Vehicle_family_pool_data; // 0x03770070
static vehicle_camera_settings Vehicle_default_camera_settings; // 0x037700B0
static string_pool Vehicle_family_name_pool(VEHICLE_FAMILY_POOL_NAME); // 0x03770160

vehicle_info Vehicle_group_info[16]; // 0x0386C0B8
int Num_vehicle_groups; // 0x0377006C
string_pool Vehicle_info_string_pool(VEHICLE_STRING_POOL_NAME); // 0x0381A5E0
const char* INVALID_VEHICLE_FAMILY_ID; // 0x0377002C
unsigned int Bone_indicies[74]; // 0x0376F680

static const float VEHICLE_DEG_TO_RAD = 0.017453f; // degrees to radians, the table's rounded constant
static const float VEHICLE_MPH_TO_MPS = 0.44704f; // miles per hour to metres per second
static const int VEHICLE_NAME_MAX_LENGTH = 0x1F; // longest mesh and display name

// 0x00B68EF0 (:175)
static void init_family_string_pool()
{
    if (Vehicle_family_pool_data == NULL) {
        Vehicle_family_pool_data = (char*)Permanent_mempool.alloc(0x200, 0);
        if (Vehicle_family_pool_data == NULL) {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the vehicle faimly name string table");
        }
        Vehicle_family_name_pool.init(Vehicle_family_pool_data, 0x200, VEHICLE_FAMILY_POOL_NAME, 0, '\0');
        Vehicle_family_name_pool.clear();
    }
}

// 0x00B68F50 (:194)
static const char* get_vehicle_family_id(const char* vehicle_family_name)
{
    const char* id = Vehicle_family_name_pool.add_unique(vehicle_family_name);
    return id != NULL ? id : INVALID_VEHICLE_FAMILY_ID;
}

// 0x00B68F70 (:208)
const char* get_xml_filename(xml_element* xml_filename_root)
{
    if (xml_filename_root != NULL) {
        xml_element* filename = xml_find(xml_filename_root, "Filename");
        if (filename != NULL) {
            return filename->text;
        }
    }
    return NULL;
}

// 0x00B68FA0 (:228)
static int vehicle_group_get_id(const char* name)
{
    for (int i = 0; i < Num_vehicle_groups; i++) {
        if (v_stricmp(name, Vehicle_group_info[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00B68FF0 (:240)
static base_vehicle_types get_base_vehicle_type(const char* name)
{
    for (int i = 0; i < 7; i++) {
        if (v_stricmp(name, Vehicle_spawn_class_name[i]) == 0) {
            return (base_vehicle_types)i;
        }
    }
    return BVT_NONE;
}

// 0x00B69030 (:260)
static bool vehicle_parse_defaults(xml_element* root)
{
    char name[128];
    float roll_max_angle;
    float pitch_max_angle;
    xml_element* section;
    xml_element* list;
    lod_info* lod;
    xml_table_get_req_uint(Vehicle_defaults.m_max_hitpoints, root, "Hitpoints");
    xml_table_get_req_float(Vehicle_defaults.m_center_of_mass_y, root, "Center_Of_Mass_Y");
    xml_table_get_req_float(Vehicle_defaults.m_center_of_mass_z, root, "Center_Of_Mass_Z");
    section = xml_table_find_req(root, "Lookat_Offset");
    xml_table_get_vector(Vehicle_default_camera_settings.lookat_offset, section);
    section = xml_table_find_req(root, "Lookat_Vehicle_Offset");
    xml_table_get_vector(Vehicle_default_camera_settings.lookat_vehicle_offset, section);
    xml_table_get_req_float(Vehicle_default_camera_settings.follow_dist, root, "Follow_Distance");
    xml_table_get_req_float(Vehicle_default_camera_settings.follow_height, root, "Follow_Height");
    Vehicle_default_camera_settings.y_axis_rotation_speed = 1.0;
    xml_table_get_req_float(Vehicle_default_camera_settings.camera_steer_angle, root, "Camera_Steer_Angle");
    xml_table_get_req_float(Vehicle_default_camera_settings.camera_steer_speed, root, "Camera_Steer_Speed");
    xml_table_get_req_float(Vehicle_defaults.m_friction_equalizer, root, "Static_Load_Friction");
    xml_table_get_req_float(Vehicle_defaults.m_torque_roll_factor, root, "Roll_Torque_Factor");
    xml_table_get_req_float(Vehicle_defaults.m_torque_pitch_factor, root, "Pitch_Torque_Factor");
    xml_table_get_req_float(Vehicle_defaults.m_torque_yaw_factor, root, "Yaw_Torque_Factor");
    xml_table_get_req_float(Vehicle_defaults.m_torque_yaw_scalar, root, "Yaw_Torque_Scalar");
    xml_table_get_req_float(Vehicle_defaults.m_extra_torque_factor, root, "Extra_Steering_Torque");
    xml_table_get_req_float(Vehicle_defaults.m_autobrake_speed, root, "Autobrake_Speed");
    xml_table_get_req_float(Vehicle_defaults.m_enforced_max_speed, root, "EnforcedMaxSpeed");
    xml_table_get_req_float(Vehicle_defaults.m_enforced_max_rspeed, root, "EnforcedMaxRSpeed");
    xml_table_get_req_float(Vehicle_defaults.m_chassis_unit_inertia_roll, root, "Roll_Unit_Inertia");
    xml_table_get_req_float(Vehicle_defaults.m_chassis_unit_inertia_pitch, root, "Pitch_Unit_Inertia");
    xml_table_get_req_float(Vehicle_defaults.m_chassis_unit_inertia_yaw, root, "Yaw_Unit_Inertia");
    xml_table_get_req_float(Vehicle_defaults.m_ai_chassis_unit_inertia_yaw, root, "AI_Yaw_Unit_Inertia");
    xml_table_get_req_float(Vehicle_defaults.m_ai_max_braking_decel, root, "AI_Max_Braking_Decel");
    xml_table_get_req_float(Vehicle_defaults.m_ai_max_radial_accel, root, "AI_Max_Radial_Accel");
    xml_table_get_req_float(Vehicle_defaults.m_viscosity_friction, root, "Viscosity_Friction");
    xml_table_get_req_float(Vehicle_defaults.m_energy_scale, root, "Energy_Scale");
    xml_table_get_req_float(Vehicle_defaults.m_collision_mass_scalar, root, "Collision_Mass_Scalar");
    xml_table_get_req_float(Vehicle_defaults.m_collision_damage_scale, root, "Collision_Damage_Scale");
    xml_table_get_req_float(Vehicle_defaults.m_terrain_damage_scale, root, "Terrain_Damage_Scale");
    xml_table_get_req_float(Vehicle_defaults.m_bullet_damage_scale, root, "Bullet_Damage_Scale");
    xml_table_get_req_float(Vehicle_defaults.m_vehicle_damage_scale, root, "Vehicle_Damage_Scale");
    xml_table_get_req_float(Vehicle_defaults.m_alert_multiplier, root, "Alert_Level_Multiplier");
    xml_table_get_req_float(Vehicle_defaults.m_powerslide_friction, root, "Powerslide_Friction");
    xml_table_get_req_float(Vehicle_defaults.m_powerslide_power, root, "Powerslide_Power");
    Vehicle_defaults.m_ai_friction_equalizer = Vehicle_defaults.m_friction_equalizer;
    section = xml_table_find_req(root, "LODs");
    Vehicle_defaults.m_num_lods = xml_table_count(section, "LOD_Distance");
    list = xml_table_find_req(section, "LOD_Distance");
    if (list != NULL) {
        lod = Vehicle_defaults.m_lod_info;
        do {
            xml_table_get_req_float(lod->dist, list, NULL);
            lod = lod + 1;
            list = xml_table_find_next(section, list, "LOD_Distance");
        } while (list != NULL);
    }
    section = xml_table_find_req(root, "Engine");
    xml_table_get_req_float(Vehicle_defaults.m_min_rpm, section, "Min_RPM");
    xml_table_get_req_float(Vehicle_defaults.m_optimal_rpm, section, "Opt_RPM");
    xml_table_get_req_float(Vehicle_defaults.m_max_rpm, section, "Max_RPM");
    xml_table_get_req_float(Vehicle_defaults.m_min_rpm_torque_factor, section, "Min_RPM_Torque_Factor");
    xml_table_get_req_float(Vehicle_defaults.m_max_rpm_torque_factor, section, "Max_RPM_Torque_Factor");
    xml_table_get_req_float(Vehicle_defaults.m_min_rpm_resistance, section, "Min_RPM_Resistance");
    xml_table_get_req_float(Vehicle_defaults.m_opt_rpm_resistance, section, "Opt_RPM_Resistance");
    xml_table_get_req_float(Vehicle_defaults.m_max_rpm_resistance, section, "Max_RPM_Resistance");
    xml_table_get_req_float(Vehicle_defaults.m_reverse_torque_multiplier, section, "Reverse_torque_multiplier");
    xml_table_get_req_float(Vehicle_defaults.m_force_assist, section, "Force_assist");
    section = xml_table_find_req(root, "Axles");
    xml_table_get_req_float(Vehicle_defaults.m_wheel_mass, section, "Wheel_Mass");
    xml_table_get_req_float(Vehicle_defaults.m_wheel_friction, section, "Wheel_Friction");
    xml_table_get_req_float(Vehicle_defaults.m_wheel_ai_friction, section, "AI_Wheel_Friction");
    xml_table_get_req_float(Vehicle_defaults.m_wheel_braking_torque, section, "Wheel_Braking_Torque");
    xml_table_get_req_float(Vehicle_defaults.m_spring_extension, section, "Spring_Extension_Distance");
    xml_table_get_req_float(Vehicle_defaults.m_spring_compression, section, "Spring_Compression_Distance");
    xml_table_get_req_float(Vehicle_defaults.m_spring_strength, section, "Spring_Strength");
    xml_table_get_req_float(Vehicle_defaults.m_spring_compression_damping, section, "Spring_Damp_Comp");
    xml_table_get_req_float(Vehicle_defaults.m_spring_expansion_damping, section, "Spring_Damp_Exp");
    section = xml_table_find_req(root, "Transmission");
    list = xml_table_find_req(section, "Downshift_RPMs");
    Vehicle_defaults.m_num_downshift_rpms = xml_table_get_float_list(Vehicle_defaults.m_downshift_rpms, 5, list);
    list = xml_table_find_req(section, "Upshift_RPMs");
    Vehicle_defaults.m_num_upshift_rpms = xml_table_get_float_list(Vehicle_defaults.m_upshift_rpms, 5, list);
    xml_table_get_req_float(Vehicle_defaults.m_diff_gear_ratio, section, "Diff_Gear_Ratio");
    xml_table_get_req_float(Vehicle_defaults.m_reverse_gear_ratio, section, "Reverse_Gear_Ratio");
    xml_table_get_req_float(Vehicle_defaults.m_clutch_delay, section, "Clutch_Delay");
    xml_table_get_req_float(Vehicle_defaults.m_clutch_delay_pre_shift, section, "Clutch_Delay_Pre_Shift");
    xml_table_get_req_float(Vehicle_defaults.m_clutch_slip_rpm, section, "Clutch_Slip_RPM");
    xml_table_get_req_uint(Vehicle_defaults.m_forward_to_reverse_delay, section, "Forward_To_Reverse_Delay");
    xml_table_get_req_uint(Vehicle_defaults.m_reverse_to_forward_delay, section, "Reverse_To_Forward_Delay");
    section = xml_table_find_req(root, "Steering");
    xml_table_get_req_float(Vehicle_defaults.m_max_steering_angle, section, "Max_Steering_Angle");
    xml_table_get_req_float(Vehicle_defaults.m_max_speed_steering_angle, section, "Max_Speed_Steering");
    xml_table_get_req_float(Vehicle_defaults.m_ai_max_speed_steering_angle, section, "AI_Max_Speed_Steering");
    xml_table_get_req_float(Vehicle_defaults.m_steering_wheel_max_speed, section, "Steering_Wheel_Max_Speed");
    Vehicle_defaults.m_steering_wheel_max_speed = Vehicle_defaults.m_steering_wheel_max_speed * VEHICLE_DEG_TO_RAD;
    xml_table_get_req_float(Vehicle_defaults.m_steering_wheel_max_return_speed, section, "Steering_Wheel_Max_Return_Speed");
    Vehicle_defaults.m_steering_wheel_max_return_speed = Vehicle_defaults.m_steering_wheel_max_return_speed * VEHICLE_DEG_TO_RAD;
    xml_table_get_req_float(Vehicle_defaults.m_steering_wheel_damp_angle, section, "Steering_Wheel_Damp_Angle");
    Vehicle_defaults.m_steering_wheel_damp_angle = Vehicle_defaults.m_steering_wheel_damp_angle * VEHICLE_DEG_TO_RAD;
    xml_table_get_req_float(Vehicle_defaults.m_steering_wheel_return_damp_angle, section, "Steering_Wheel_Return_Damp_Angle");
    Vehicle_defaults.m_steering_wheel_return_damp_angle = Vehicle_defaults.m_steering_wheel_return_damp_angle * VEHICLE_DEG_TO_RAD;
    xml_table_get_req_float(Vehicle_defaults.m_counter_steer_min_speed, section, "CounterSteerMinSpeed");
    xml_table_get_req_float(Vehicle_defaults.m_counter_steer_max_speed, section, "CounterSteerMaxSpeed");
    xml_table_get_req_float(Vehicle_defaults.m_air_steer_roll_max_vel, section, "AirSteerRollMaxVel");
    roll_max_angle = 0.0;
    xml_table_get_req_float(roll_max_angle, section, "AirSteerRollMaxAngle");
    Vehicle_defaults.m_air_steer_roll_max_angle_rad = roll_max_angle * VEHICLE_DEG_TO_RAD;
    xml_table_get_req_float(Vehicle_defaults.m_air_steer_pitch_max_vel, section, "AirSteerPitchMaxVel");
    pitch_max_angle = 0.0;
    xml_table_get_req_float(pitch_max_angle, section, "AirSteerPitchMaxAngle");
    Vehicle_defaults.m_air_steer_pitch_max_angle_rad = pitch_max_angle * VEHICLE_DEG_TO_RAD;
    section = xml_table_find_req(root, "Braking");
    xml_table_get_req_float(Vehicle_defaults.m_min_pedal_input_to_block, section, "Min_Pedal_To_Block");
    xml_table_get_req_float(Vehicle_defaults.m_min_time_to_block, section, "Min_Time_To_Block");
    xml_table_get_req_float(Vehicle_defaults.m_ai_min_time_to_block, section, "AI_Min_Time_To_Block");
    section = xml_table_find_req(root, "Aerodynamics");
    xml_table_get_req_float(Vehicle_defaults.m_air_density, section, "Air_Density");
    xml_table_get_req_float(Vehicle_defaults.m_frontal_area, section, "Frontal_Area");
    xml_table_get_req_float(Vehicle_defaults.m_drag_coefficient, section, "Drag_Coefficient");
    xml_table_get_req_float(Vehicle_defaults.m_lift_coefficient, section, "Lift_Coefficient");
    xml_table_get_req_float(Vehicle_defaults.m_extra_gravity, section, "Extra_Gravity");
    section = xml_table_find_req(root, "Foleys");
    xml_table_get_req_string(name, 0x80, section, "Engine_Start");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_engine_start);
    xml_table_get_req_string(name, 0x80, section, "Engine");
    Vehicle_defaults.m_engine = audiolib_vehicle_engine_get_id(name);
    xml_table_get_req_string(name, 0x80, section, "Engine_Off");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_engine_off);
    Vehicle_defaults.m_foley_engine_peel = -1;
    if (xml_table_get_opt_string(name, 0x80, section, "Engine_Peel")) {
        audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_engine_peel);
    }
    Vehicle_defaults.m_foley_gear_shift = -1;
    if (xml_table_get_opt_string(name, 0x80, section, "Gear_Shift")) {
        audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_gear_shift);
    }
    xml_table_get_req_string(name, 0x80, section, "Gear_Grind");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_gear_grind);
    xml_table_get_req_string(name, 0x80, section, "Horn");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_horn);
    xml_table_get_req_string(name, 0x80, section, "Low_Impact");
    audiolib_cue_get_id(name, Vehicle_defaults.m_foley_impact_id);
    xml_table_get_req_string(name, 0x80, section, "High_Impact");
    audiolib_cue_get_id(name, Vehicle_defaults.m_foley_impact_id + 1);
    xml_table_get_req_string(name, 0x80, section, "Scraping");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_scraping_id);
    xml_table_get_req_string(name, 0x80, section, "Corpse_Impact");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_corpse_impact_id);
    xml_table_get_req_string(name, 0x80, section, "Component_Impact");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_component_impact_id);
    xml_table_get_req_string(name, 0x80, section, "Wheel_Impact");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_wheel_impact_id);
    Vehicle_defaults.m_foley_chassis_land_id[0] = -1;
    Vehicle_defaults.m_foley_chassis_land_id[1] = -1;
    if (xml_table_get_opt_string(name, 0x80, section, "Chassis_Land_Low")) {
        audiolib_cue_get_id(name, Vehicle_defaults.m_foley_chassis_land_id);
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Chassis_Land_High")) {
        audiolib_cue_get_id(name, Vehicle_defaults.m_foley_chassis_land_id + 1);
    }
    Vehicle_defaults.m_foley_pass_by = -1;
    if (xml_table_get_opt_string(name, 0x80, section, "Pass_By")) {
        audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_pass_by);
    }
    xml_table_get_req_string(name, 0x80, section, "Door_Open");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_door_open_id);
    xml_table_get_req_string(name, 0x80, section, "Door_Close");
    audiolib_cue_get_id(name, &Vehicle_defaults.m_foley_door_close_id);
    section = xml_table_find_req(root, "Effects");
    xml_table_get_req_string(name, 0x80, section, "Engine_Fire");
    Vehicle_defaults.m_effect_engine_fire = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Engine_Smoke");
    Vehicle_defaults.m_effect_engine_smoke = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Corpse_Smoke");
    Vehicle_defaults.m_effect_corpse_smoke = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Component_Fire");
    Vehicle_defaults.m_effect_component_fire = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Exhaust");
    Vehicle_defaults.m_effect_exhaust_normal = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "ExhaustBurst");
    Vehicle_defaults.m_effect_exhaust_burst = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Collision_Effect");
    Vehicle_defaults.m_effect_collision = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Scrape_Effect");
    Vehicle_defaults.m_effect_scrape = effects_get_handle(name);
    Vehicle_defaults.m_effect_component_detach = 0xffffffff;
    if (xml_table_get_opt_string(name, 0x80, section, "ComponentDetach")) {
        Vehicle_defaults.m_effect_component_detach = effects_get_handle(name);
    }
    xml_table_get_req_string(name, 0x80, section, "Head_Light_Primary");
    Vehicle_defaults.m_effect_head_light_primary = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Head_Light_Secondary");
    Vehicle_defaults.m_effect_head_light_secondary = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Fog_Light");
    Vehicle_defaults.m_effect_fog_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Tail_Light");
    Vehicle_defaults.m_effect_tail_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Brake_Light");
    Vehicle_defaults.m_effect_brake_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Reverse_Light");
    Vehicle_defaults.m_effect_reverse_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Emergency_Light");
    Vehicle_defaults.m_effect_emergency_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Strobe_Light");
    Vehicle_defaults.m_effect_strobe_light = effects_get_handle(name);
    xml_table_get_req_string(name, 0x80, section, "Explosion");
    Vehicle_defaults.m_explosion_info = explosion_lookup(name);
    xml_table_get_req_string(name, 0x80, section, "Bomb_Explosion");
    Vehicle_defaults.m_bomb_explosion_info = explosion_lookup(name);
    section = xml_table_find_req(root, "Angular_Damping");
    xml_table_get_req_float(Vehicle_defaults.m_normal_spin_damping, section, "Normal_Spin_Damping");
    xml_table_get_req_float(Vehicle_defaults.m_normal_spin_damping_ai, section, "Normal_Spin_Damping_AI");
    xml_table_get_req_float(Vehicle_defaults.m_collision_spin_damping, section, "Collision_Spin_Damping");
    xml_table_get_req_float(Vehicle_defaults.m_collision_spin_threshold, section, "Collision_Spin_Threshold");
    Vehicle_defaults.m_collision_spin_threshold = Vehicle_defaults.m_collision_spin_threshold * VEHICLE_DEG_TO_RAD;
    section = xml_table_find_req(root, "speed_effects");
    xml_table_get_req_float(Vehicle_defaults.m_camera_fov_multiplier, section, "camera_fov_multiplier");
    xml_table_get_req_float(Vehicle_defaults.m_camera_fov_min_speed, section, "camera_fov_min_speed");
    xml_table_get_req_float(Vehicle_defaults.m_camera_shake_min_speed, section, "camera_shake_min_speed");
    xml_table_get_req_float(Vehicle_defaults.m_radial_blur_max, section, "radial_blur_max");
    xml_table_get_req_float(Vehicle_defaults.m_radial_blur_min_speed, section, "radial_blur_min_speed");
    return true;
}

// 0x00B69E20 (:675)
static bool vehicle_parse_lods(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "LODs");
    if (section == NULL) {
        // a group without LODs takes the defaults, a vehicle its group's
        const unsigned int num_lods = parsing_groups ? Vehicle_defaults.m_num_lods : v_group->m_num_lods;
        v_info->m_num_lods = num_lods;
        for (unsigned int i = 0; i < v_info->m_num_lods; i++) {
            v_info->m_lod_info[i].dist = parsing_groups ? Vehicle_defaults.m_lod_info[i].dist : v_group->m_lod_info[i].dist;
        }
    }
    else {
        v_info->m_num_lods = xml_table_count(section, "LOD_Distance");
        lod_info* lod = v_info->m_lod_info;
        for (xml_element* cur = xml_table_find_opt(section, "LOD_Distance"); cur != NULL; cur = xml_table_find_next(section, cur, "LOD_Distance")) {
            xml_table_get_req_float(lod->dist, cur, NULL);
            lod++;
        }
    }
    return true;
}

// 0x00B69F00 (:725)
static bool vehicle_parse_engine(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Engine");
    if (section == NULL) {
        if (parsing_groups) {
            return false;
        }
        v_info->engine_torque = v_group->engine_torque;
        v_info->ai_engine_torque = v_group->engine_torque;
        v_info->min_rpm = v_group->min_rpm;
        v_info->optimal_rpm = v_group->optimal_rpm;
        v_info->max_rpm = v_group->max_rpm;
        v_info->min_rpm_torque_factor = v_group->min_rpm_torque_factor;
        v_info->max_rpm_torque_factor = v_group->max_rpm_torque_factor;
        v_info->min_rpm_resistance = v_group->min_rpm_resistance;
        v_info->opt_rpm_resistance = v_group->opt_rpm_resistance;
        v_info->max_rpm_resistance = v_group->max_rpm_resistance;
        v_info->reverse_torque_multiplier = v_group->reverse_torque_multiplier;
        v_info->force_assist = v_group->force_assist;
        return true;
    }
    if (parsing_groups) {
        xml_table_get_req_float(v_info->engine_torque, section, "Torque");
        v_info->min_rpm = Vehicle_defaults.m_min_rpm;
    }
    else {
        v_info->engine_torque = v_group->engine_torque;
        xml_table_get_opt_float(v_info->engine_torque, section, "Torque");
        v_info->ai_engine_torque = v_info->engine_torque;
        xml_table_get_opt_float(v_info->ai_engine_torque, section, "AITorque");
        v_info->min_rpm = v_group->min_rpm;
    }
    xml_table_get_opt_float(v_info->min_rpm, section, "Min_RPM");
    v_info->optimal_rpm = parsing_groups ? Vehicle_defaults.m_optimal_rpm : v_group->optimal_rpm;
    xml_table_get_opt_float(v_info->optimal_rpm, section, "Opt_RPM");
    v_info->max_rpm = parsing_groups ? Vehicle_defaults.m_max_rpm : v_group->max_rpm;
    xml_table_get_opt_float(v_info->max_rpm, section, "Max_RPM");
    v_info->min_rpm_torque_factor = parsing_groups ? Vehicle_defaults.m_min_rpm_torque_factor : v_group->min_rpm_torque_factor;
    xml_table_get_opt_float(v_info->min_rpm_torque_factor, section, "Min_RPM_Torque_Factor");
    v_info->max_rpm_torque_factor = parsing_groups ? Vehicle_defaults.m_max_rpm_torque_factor : v_group->max_rpm_torque_factor;
    xml_table_get_opt_float(v_info->max_rpm_torque_factor, section, "Max_RPM_Torque_Factor");
    v_info->min_rpm_resistance = parsing_groups ? Vehicle_defaults.m_min_rpm_resistance : v_group->min_rpm_resistance;
    xml_table_get_opt_float(v_info->min_rpm_resistance, section, "Min_RPM_Resistance");
    v_info->opt_rpm_resistance = parsing_groups ? Vehicle_defaults.m_opt_rpm_resistance : v_group->opt_rpm_resistance;
    xml_table_get_opt_float(v_info->opt_rpm_resistance, section, "Opt_RPM_Resistance");
    v_info->max_rpm_resistance = parsing_groups ? Vehicle_defaults.m_max_rpm_resistance : v_group->max_rpm_resistance;
    xml_table_get_opt_float(v_info->max_rpm_resistance, section, "Max_RPM_Resistance");
    v_info->reverse_torque_multiplier = parsing_groups ? Vehicle_defaults.m_reverse_torque_multiplier : v_group->reverse_torque_multiplier;
    xml_table_get_opt_float(v_info->reverse_torque_multiplier, section, "Reverse_torque_multiplier");
    v_info->force_assist = parsing_groups ? Vehicle_defaults.m_force_assist : v_group->force_assist;
    xml_table_get_opt_float(v_info->force_assist, section, "Force_assist");
    return true;
}

// The original copies the table's wheels without spring_power: it is always 1 unless an entry names it.
static void vehicle_copy_axle_from_group(vehicle_info::axle_wheel_info& dst, const vehicle_info::axle_wheel_info& src)
{
    dst = src;
    dst.spring_power = 1.0f;
}

// 0x00B6A210 (:801)
static bool vehicle_parse_axles(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    if (parsing_groups) {
        xml_element* section = xml_table_find_req(root, "Axles");
        v_info->m_num_axles = xml_table_count(section, "Axle");
        vehicle_info::axle_wheel_info* wheel = v_info->m_axle_wheel_infos;
        for (xml_element* axle = xml_table_find_opt(section, "Axle"); axle != NULL; axle = xml_table_find_next(section, axle, "Axle")) {
            xml_table_get_req_bool(wheel->does_steer, axle, "Wheel_Does_Steer");
            xml_table_get_req_bool(wheel->does_handbrake, axle, "Wheel_Does_Handbrake");
            xml_table_get_req_float(wheel->engine_torque_factor, axle, "Wheel_Engine_Torque");
            wheel->mass = Vehicle_defaults.m_wheel_mass;
            xml_table_get_opt_float(wheel->mass, axle, "Wheel_Mass");
            wheel->friction = Vehicle_defaults.m_wheel_friction;
            xml_table_get_opt_float(wheel->friction, axle, "Wheel_Friction");
            wheel->ai_friction = wheel->friction;
            xml_table_get_opt_float(wheel->ai_friction, axle, "AI_Wheel_Friction");
            wheel->braking_torque = Vehicle_defaults.m_wheel_braking_torque;
            xml_table_get_opt_float(wheel->braking_torque, axle, "Wheel_Braking_Torque");
            wheel->spring_compression = Vehicle_defaults.m_spring_compression;
            xml_table_get_opt_float(wheel->spring_compression, axle, "Spring_Compression_Distance");
            wheel->spring_extension = Vehicle_defaults.m_spring_extension;
            xml_table_get_opt_float(wheel->spring_extension, axle, "Spring_Extension_Distance");
            wheel->spring_strength = Vehicle_defaults.m_spring_strength;
            xml_table_get_opt_float(wheel->spring_strength, axle, "Wheel_Spring_Strength");
            wheel->spring_power = 1.0f;
            // original quirk: groups read "Wheel_Spring_Pow", vehicles "Wheel_Spring_Power"
            xml_table_get_opt_float(wheel->spring_power, axle, "Wheel_Spring_Pow");
            wheel->compression_damping = Vehicle_defaults.m_spring_compression_damping;
            xml_table_get_opt_float(wheel->compression_damping, axle, "Wheel_Damp_Comp");
            wheel->expansion_damping = Vehicle_defaults.m_spring_expansion_damping;
            xml_table_get_opt_float(wheel->expansion_damping, axle, "Wheel_Damp_Exp");
            wheel++;
        }
        return true;
    }

    xml_element* section = xml_table_find_opt(root, "Axles");
    if (section == NULL) {
        float torque_sum = 0.0f;
        v_info->m_num_axles = v_group->m_num_axles;
        for (unsigned int i = 0; i < v_info->m_num_axles; i++) {
            vehicle_copy_axle_from_group(v_info->m_axle_wheel_infos[i], v_group->m_axle_wheel_infos[i]);
            torque_sum += v_info->m_axle_wheel_infos[i].engine_torque_factor;
        }
        for (unsigned int i = 0; i < v_info->m_num_axles; i++) {
            v_info->m_axle_wheel_infos[i].engine_torque_factor *= 1.0f / torque_sum;
        }
        return true;
    }

    const unsigned int group_axles = v_group->m_num_axles;
    const unsigned int entry_axles = xml_table_count(section, "Axle");
    v_info->m_num_axles = entry_axles > group_axles ? entry_axles : group_axles;
    for (unsigned int i = 0; i < group_axles; i++) {
        vehicle_copy_axle_from_group(v_info->m_axle_wheel_infos[i], v_group->m_axle_wheel_infos[i]);
    }
    for (unsigned int i = group_axles; i < entry_axles; i++) {
        vehicle_info::axle_wheel_info& wheel = v_info->m_axle_wheel_infos[i];
        wheel.does_steer = false;
        wheel.does_handbrake = false;
        wheel.engine_torque_factor = 0.0f;
        wheel.mass = Vehicle_defaults.m_wheel_mass;
        wheel.friction = Vehicle_defaults.m_wheel_friction;
        wheel.ai_friction = Vehicle_defaults.m_wheel_friction;
        wheel.braking_torque = Vehicle_defaults.m_wheel_braking_torque;
        wheel.spring_compression = Vehicle_defaults.m_spring_compression;
        wheel.spring_extension = Vehicle_defaults.m_spring_extension;
        wheel.spring_strength = Vehicle_defaults.m_spring_strength;
        wheel.spring_power = 1.0f;
        wheel.compression_damping = Vehicle_defaults.m_spring_compression_damping;
        wheel.expansion_damping = Vehicle_defaults.m_spring_expansion_damping;
    }
    vehicle_info::axle_wheel_info* wheel = v_info->m_axle_wheel_infos;
    for (xml_element* axle = xml_table_find_opt(section, "Axle"); axle != NULL; axle = xml_table_find_next(section, axle, "Axle")) {
        xml_table_get_opt_bool(wheel->does_steer, axle, "Wheel_Does_Steer");
        xml_table_get_opt_bool(wheel->does_handbrake, axle, "Wheel_Does_Handbrake");
        xml_table_get_opt_float(wheel->engine_torque_factor, axle, "Wheel_Engine_Torque");
        xml_table_get_opt_float(wheel->mass, axle, "Wheel_Mass");
        xml_table_get_opt_float(wheel->friction, axle, "Wheel_Friction");
        if (!xml_table_get_opt_float(wheel->ai_friction, axle, "AI_Wheel_Friction")) {
            wheel->ai_friction = wheel->friction;
        }
        xml_table_get_opt_float(wheel->braking_torque, axle, "Wheel_Braking_Torque");
        xml_table_get_opt_float(wheel->spring_compression, axle, "Spring_Compression_Distance");
        xml_table_get_opt_float(wheel->spring_extension, axle, "Spring_Extension_Distance");
        xml_table_get_opt_float(wheel->spring_strength, axle, "Wheel_Spring_Strength");
        xml_table_get_opt_float(wheel->spring_power, axle, "Wheel_Spring_Power");
        xml_table_get_opt_float(wheel->compression_damping, axle, "Wheel_Damp_Comp");
        xml_table_get_opt_float(wheel->expansion_damping, axle, "Wheel_Damp_Exp");
        wheel++;
    }
    float torque_sum = 0.0f;
    for (unsigned int i = 0; i < v_info->m_num_axles; i++) {
        torque_sum += v_info->m_axle_wheel_infos[i].engine_torque_factor;
    }
    for (unsigned int i = 0; i < v_info->m_num_axles; i++) {
        v_info->m_axle_wheel_infos[i].engine_torque_factor *= 1.0f / torque_sum;
    }
    return true;
}

// 0x00B6A980 (:1014)
static bool vehicle_parse_transmission(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    vehicle_info::transmission_info& trans = v_info->trans_info;
    xml_element* section = xml_table_find_opt(root, "Transmission");
    if (section == NULL) {
        if (parsing_groups) {
            return false;
        }
        const vehicle_info::transmission_info& group = v_group->trans_info;
        trans.num_gears = group.num_gears;
        for (unsigned int i = 0; i < trans.num_gears; i++) {
            trans.gear_ratios[i] = group.gear_ratios[i];
        }
        for (unsigned int i = 0; i < trans.num_gears - 1; i++) {
            trans.downshift_rpms[i] = group.downshift_rpms[i];
            trans.upshift_rpms[i] = group.upshift_rpms[i];
        }
        trans.differential_gear_ratio = group.differential_gear_ratio;
        trans.reverse_gear_ratio = group.reverse_gear_ratio;
        trans.clutch_delay = group.clutch_delay;
        v_info->clutch_slip_rpm = v_group->clutch_slip_rpm;
        trans.forward_to_reverse_delay = group.forward_to_reverse_delay;
        trans.reverse_to_forward_delay = group.reverse_to_forward_delay;
        return true;
    }

    xml_element* list = xml_table_find_opt(section, "Gear_Ratios");
    if (list == NULL) {
        if (parsing_groups) {
            return false;
        }
        trans.num_gears = v_group->trans_info.num_gears;
        for (unsigned int i = 0; i < trans.num_gears; i++) {
            trans.gear_ratios[i] = v_group->trans_info.gear_ratios[i];
        }
    }
    else {
        trans.num_gears = xml_table_get_float_list(trans.gear_ratios, 6, list);
    }

    list = xml_table_find_opt(section, "Downshift_RPMs");
    if (list == NULL) {
        for (unsigned int i = 0; i < trans.num_gears - 1; i++) {
            trans.downshift_rpms[i] = parsing_groups ? Vehicle_defaults.m_downshift_rpms[i] : v_group->trans_info.downshift_rpms[i];
        }
    }
    else {
        xml_table_get_float_list(trans.downshift_rpms, 5, list);
    }

    list = xml_table_find_opt(section, "Upshift_RPMs");
    if (list == NULL) {
        for (unsigned int i = 0; i < trans.num_gears - 1; i++) {
            trans.upshift_rpms[i] = parsing_groups ? Vehicle_defaults.m_upshift_rpms[i] : v_group->trans_info.upshift_rpms[i];
        }
    }
    else {
        xml_table_get_float_list(trans.upshift_rpms, 5, list);
    }

    trans.differential_gear_ratio = parsing_groups ? Vehicle_defaults.m_diff_gear_ratio : v_group->trans_info.differential_gear_ratio;
    xml_table_get_opt_float(trans.differential_gear_ratio, section, "Diff_Gear_Ratio");
    trans.reverse_gear_ratio = parsing_groups ? Vehicle_defaults.m_reverse_gear_ratio : v_group->trans_info.reverse_gear_ratio;
    xml_table_get_opt_float(trans.reverse_gear_ratio, section, "Reverse_Gear_Ratio");
    trans.clutch_delay = parsing_groups ? Vehicle_defaults.m_clutch_delay : v_group->trans_info.clutch_delay;
    xml_table_get_opt_float(trans.clutch_delay, section, "Clutch_Delay");
    trans.clutch_delay_pre_shift = parsing_groups ? Vehicle_defaults.m_clutch_delay_pre_shift : v_group->trans_info.clutch_delay_pre_shift;
    xml_table_get_opt_float(trans.clutch_delay_pre_shift, section, "Clutch_Delay_Pre_Shift");
    v_info->clutch_slip_rpm = parsing_groups ? Vehicle_defaults.m_clutch_slip_rpm : v_group->clutch_slip_rpm;
    xml_table_get_opt_float(v_info->clutch_slip_rpm, section, "Clutch_Slip_RPM");
    trans.forward_to_reverse_delay = parsing_groups ? Vehicle_defaults.m_forward_to_reverse_delay : v_group->trans_info.forward_to_reverse_delay;
    xml_table_get_opt_uint(trans.forward_to_reverse_delay, section, "Forward_To_Reverse_Delay");
    trans.reverse_to_forward_delay = parsing_groups ? Vehicle_defaults.m_reverse_to_forward_delay : v_group->trans_info.reverse_to_forward_delay;
    xml_table_get_opt_uint(trans.reverse_to_forward_delay, section, "Reverse_To_Forward_Delay");
    return true;
}

// 0x00B6AD70 (:1144)
static bool vehicle_parse_steering(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Steering");
    if (section == NULL) {
        if (!parsing_groups) {
            v_info->max_steering_angle = v_group->max_steering_angle;
            v_info->max_speed_steering_angle = v_group->max_speed_steering_angle;
            v_info->ai_max_speed_steering_angle = v_group->ai_max_speed_steering_angle;
            v_info->m_steering_wheel_max_speed = v_group->m_steering_wheel_max_speed;
            v_info->m_steering_wheel_max_return_speed = v_group->m_steering_wheel_max_return_speed;
            v_info->m_steering_wheel_damp_angle = v_group->m_steering_wheel_damp_angle;
            v_info->m_steering_wheel_return_damp_angle = v_group->m_steering_wheel_return_damp_angle;
            v_info->counter_steer_min_speed = v_group->counter_steer_min_speed;
            v_info->counter_steer_max_speed = v_group->counter_steer_max_speed;
            v_info->air_steer_roll_max_vel = v_group->air_steer_roll_max_vel;
            v_info->air_steer_roll_max_angle_rad = v_group->air_steer_roll_max_angle_rad;
            v_info->air_steer_pitch_max_vel = v_group->air_steer_pitch_max_vel;
            v_info->air_steer_pitch_max_angle_rad = v_group->air_steer_pitch_max_angle_rad;
        }
        else {
        v_info->max_steering_angle = Vehicle_defaults.m_max_steering_angle;
        v_info->max_speed_steering_angle = Vehicle_defaults.m_max_speed_steering_angle;
        v_info->ai_max_speed_steering_angle = Vehicle_defaults.m_ai_max_speed_steering_angle;
        v_info->m_steering_wheel_max_speed = Vehicle_defaults.m_steering_wheel_max_speed;
        v_info->m_steering_wheel_max_return_speed = Vehicle_defaults.m_steering_wheel_max_return_speed;
        v_info->m_steering_wheel_damp_angle = Vehicle_defaults.m_steering_wheel_damp_angle;
        v_info->m_steering_wheel_return_damp_angle = Vehicle_defaults.m_steering_wheel_return_damp_angle;
        v_info->counter_steer_min_speed = Vehicle_defaults.m_counter_steer_min_speed;
        v_info->counter_steer_max_speed = Vehicle_defaults.m_counter_steer_max_speed;
        v_info->air_steer_roll_max_vel = Vehicle_defaults.m_air_steer_roll_max_vel;
        v_info->air_steer_roll_max_angle_rad = Vehicle_defaults.m_air_steer_roll_max_angle_rad;
        v_info->air_steer_pitch_max_vel = Vehicle_defaults.m_air_steer_pitch_max_vel;
        Vehicle_defaults.m_air_steer_pitch_max_angle_rad = Vehicle_defaults.m_air_steer_pitch_max_angle_rad;
            v_info->air_steer_pitch_max_angle_rad = Vehicle_defaults.m_air_steer_pitch_max_angle_rad;
        }
    }
    else {
        float roll_max_angle;
        float pitch_max_angle;
        v_info->max_steering_angle = parsing_groups ? Vehicle_defaults.m_max_steering_angle : v_group->max_steering_angle;
        xml_table_get_opt_float(v_info->max_steering_angle, section, "Max_Steering_Angle");
        v_info->max_speed_steering_angle = parsing_groups ? Vehicle_defaults.m_max_speed_steering_angle : v_group->max_speed_steering_angle;
        xml_table_get_opt_float(v_info->max_speed_steering_angle, section, "Max_Speed_Steering");
        v_info->ai_max_speed_steering_angle = parsing_groups ? Vehicle_defaults.m_ai_max_speed_steering_angle : v_group->ai_max_speed_steering_angle;
        xml_table_get_opt_float(v_info->ai_max_speed_steering_angle, section, "AI_Max_Speed_Steering");
        v_info->m_steering_wheel_max_speed = parsing_groups ? Vehicle_defaults.m_steering_wheel_max_speed : v_group->m_steering_wheel_max_speed;
        if (xml_table_get_opt_float(v_info->m_steering_wheel_max_speed, section, "Steering_Wheel_Max_Speed")) {
            v_info->m_steering_wheel_max_speed *= VEHICLE_DEG_TO_RAD;
        }
        v_info->m_steering_wheel_max_return_speed = parsing_groups ? Vehicle_defaults.m_steering_wheel_max_return_speed : v_group->m_steering_wheel_max_return_speed;
        if (xml_table_get_opt_float(v_info->m_steering_wheel_max_return_speed, section, "Steering_Wheel_Max_Return_Speed")) {
            v_info->m_steering_wheel_max_return_speed *= VEHICLE_DEG_TO_RAD;
        }
        v_info->m_steering_wheel_damp_angle = parsing_groups ? Vehicle_defaults.m_steering_wheel_damp_angle : v_group->m_steering_wheel_damp_angle;
        if (xml_table_get_opt_float(v_info->m_steering_wheel_damp_angle, section, "Steering_Wheel_Damp_Angle")) {
            v_info->m_steering_wheel_damp_angle *= VEHICLE_DEG_TO_RAD;
        }
        v_info->m_steering_wheel_return_damp_angle = parsing_groups ? Vehicle_defaults.m_steering_wheel_return_damp_angle : v_group->m_steering_wheel_return_damp_angle;
        if (xml_table_get_opt_float(v_info->m_steering_wheel_return_damp_angle, section, "Steering_Wheel_Return_Damp_Angle")) {
            v_info->m_steering_wheel_return_damp_angle *= VEHICLE_DEG_TO_RAD;
        }
        v_info->counter_steer_min_speed = parsing_groups ? Vehicle_defaults.m_counter_steer_min_speed : v_group->counter_steer_min_speed;
        xml_table_get_opt_float(v_info->counter_steer_min_speed, section, "CounterSteerMinSpeed");
        v_info->counter_steer_max_speed = parsing_groups ? Vehicle_defaults.m_counter_steer_max_speed : v_group->counter_steer_max_speed;
        xml_table_get_opt_float(v_info->counter_steer_max_speed, section, "CounterSteerMaxSpeed");
        v_info->air_steer_roll_max_vel = parsing_groups ? Vehicle_defaults.m_air_steer_roll_max_vel : v_group->air_steer_roll_max_vel;
        xml_table_get_opt_float(v_info->air_steer_roll_max_vel, section, "AirSteerRollMaxVel");
        v_info->air_steer_roll_max_angle_rad = parsing_groups ? Vehicle_defaults.m_air_steer_roll_max_angle_rad : v_group->air_steer_roll_max_angle_rad;
        if (xml_table_get_opt_float(roll_max_angle, section, "AirSteerRollMaxAngle")) {
            v_info->air_steer_roll_max_angle_rad = roll_max_angle * VEHICLE_DEG_TO_RAD;
        }
        v_info->air_steer_pitch_max_vel = parsing_groups ? Vehicle_defaults.m_air_steer_pitch_max_vel : v_group->air_steer_pitch_max_vel;
        xml_table_get_opt_float(v_info->air_steer_pitch_max_vel, section, "AirSteerPitchMaxVel");
        if (xml_table_get_opt_float(pitch_max_angle, section, "AirSteerPitchMaxAngle")) {
            v_info->air_steer_pitch_max_angle_rad = pitch_max_angle * VEHICLE_DEG_TO_RAD;
        }
    }
    if (parsing_groups) {
        return true;
    }
    // the table's units; a group keeps degrees and miles per hour, its vehicles convert what they copied
    v_info->max_steering_angle = v_info->max_steering_angle * VEHICLE_DEG_TO_RAD;
    v_info->max_speed_steering_angle = v_info->max_speed_steering_angle * VEHICLE_MPH_TO_MPS;
    v_info->counter_steer_min_speed = v_info->counter_steer_min_speed * VEHICLE_MPH_TO_MPS;
    v_info->counter_steer_max_speed = v_info->counter_steer_max_speed * VEHICLE_MPH_TO_MPS;
    return true;
}

// 0x00B6B300 (:1252)
static bool vehicle_parse_braking(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Braking");
    if (section != NULL) {
        v_info->min_pedal_input_to_block = parsing_groups ? Vehicle_defaults.m_min_pedal_input_to_block : v_group->min_pedal_input_to_block;
        xml_table_get_opt_float(v_info->min_pedal_input_to_block, section, "Min_Pedal_To_Block");
        v_info->min_time_to_block = parsing_groups ? Vehicle_defaults.m_min_time_to_block : v_group->min_time_to_block;
        xml_table_get_opt_float(v_info->min_time_to_block, section, "Min_Time_To_Block");
        v_info->ai_min_time_to_block = parsing_groups ? Vehicle_defaults.m_ai_min_time_to_block : v_group->ai_min_time_to_block;
        xml_table_get_opt_float(v_info->ai_min_time_to_block, section, "AI_Min_Time_To_Block");
        return true;
    }
    v_info->min_pedal_input_to_block = parsing_groups ? Vehicle_defaults.m_min_pedal_input_to_block : v_group->min_pedal_input_to_block;
    v_info->min_time_to_block = parsing_groups ? Vehicle_defaults.m_min_time_to_block : v_group->min_time_to_block;
    v_info->ai_min_time_to_block = parsing_groups ? Vehicle_defaults.m_ai_min_time_to_block : v_group->ai_min_time_to_block;
    return true;
}

// 0x00B6B420 (:1283)
static bool vehicle_parse_aerodynamics(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Aerodynamics");
    if (section != NULL) {
        v_info->air_density = parsing_groups ? Vehicle_defaults.m_air_density : v_group->air_density;
        xml_table_get_opt_float(v_info->air_density, section, "Air_Density");
        v_info->frontal_area = parsing_groups ? Vehicle_defaults.m_frontal_area : v_group->frontal_area;
        xml_table_get_opt_float(v_info->frontal_area, section, "Frontal_Area");
        v_info->drag_coefficient = parsing_groups ? Vehicle_defaults.m_drag_coefficient : v_group->drag_coefficient;
        xml_table_get_opt_float(v_info->drag_coefficient, section, "Drag_Coefficient");
        v_info->lift_coefficient = parsing_groups ? Vehicle_defaults.m_lift_coefficient : v_group->lift_coefficient;
        xml_table_get_opt_float(v_info->lift_coefficient, section, "Lift_Coefficient");
        v_info->extra_gravity = parsing_groups ? Vehicle_defaults.m_extra_gravity : v_group->extra_gravity;
        xml_table_get_opt_float(v_info->extra_gravity, section, "Extra_Gravity");
        return true;
    }
    v_info->air_density = parsing_groups ? Vehicle_defaults.m_air_density : v_group->air_density;
    v_info->frontal_area = parsing_groups ? Vehicle_defaults.m_frontal_area : v_group->frontal_area;
    v_info->drag_coefficient = parsing_groups ? Vehicle_defaults.m_drag_coefficient : v_group->drag_coefficient;
    v_info->lift_coefficient = parsing_groups ? Vehicle_defaults.m_lift_coefficient : v_group->lift_coefficient;
    v_info->extra_gravity = parsing_groups ? Vehicle_defaults.m_extra_gravity : v_group->extra_gravity;
    return true;
}

// 0x00B6B5D0 (:1322)
static bool vehicle_parse_foleys(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Foley");
    if (section == NULL) {
        v_info->foley_start = parsing_groups ? Vehicle_defaults.m_foley_engine_start : v_group->foley_start;
        // original quirk: without a Foley section a vehicle takes the default engine, not its group's
        v_info->engine_id = Vehicle_defaults.m_engine;
        v_info->engine_wavebank_id = audiolib_vehicle_engine_get_wavebank_id(Vehicle_defaults.m_engine);
        v_info->foley_off = parsing_groups ? Vehicle_defaults.m_foley_engine_off : v_group->foley_off;
        v_info->foley_engine_peel = parsing_groups ? Vehicle_defaults.m_foley_engine_peel : v_group->foley_engine_peel;
        v_info->foley_shift = parsing_groups ? Vehicle_defaults.m_foley_gear_shift : v_group->foley_shift;
        v_info->foley_grind = parsing_groups ? Vehicle_defaults.m_foley_gear_grind : v_group->foley_grind;
        v_info->foley_honk = parsing_groups ? Vehicle_defaults.m_foley_horn : v_group->foley_honk;
        v_info->m_foley_impact_id[0] = parsing_groups ? Vehicle_defaults.m_foley_impact_id[0] : v_group->m_foley_impact_id[0];
        v_info->m_foley_impact_id[1] = parsing_groups ? Vehicle_defaults.m_foley_impact_id[1] : v_group->m_foley_impact_id[1];
        v_info->m_foley_scraping_id = parsing_groups ? Vehicle_defaults.m_foley_scraping_id : v_group->m_foley_scraping_id;
        v_info->m_foley_corpse_impact_id = parsing_groups ? Vehicle_defaults.m_foley_corpse_impact_id : v_group->m_foley_corpse_impact_id;
        v_info->m_foley_component_impact_id = parsing_groups ? Vehicle_defaults.m_foley_component_impact_id : v_group->m_foley_component_impact_id;
        v_info->m_foley_wheel_impact_id = parsing_groups ? Vehicle_defaults.m_foley_wheel_impact_id : v_group->m_foley_wheel_impact_id;
        v_info->m_foley_chassis_land_id[0] = parsing_groups ? Vehicle_defaults.m_foley_chassis_land_id[0] : v_group->m_foley_chassis_land_id[0];
        v_info->m_foley_chassis_land_id[1] = parsing_groups ? Vehicle_defaults.m_foley_chassis_land_id[1] : v_group->m_foley_chassis_land_id[1];
        v_info->m_foley_pass_by = parsing_groups ? Vehicle_defaults.m_foley_pass_by : v_group->m_foley_pass_by;
        v_info->m_foley_door_open_id = parsing_groups ? Vehicle_defaults.m_foley_door_open_id : v_group->m_foley_door_open_id;
        v_info->m_foley_door_close_id = parsing_groups ? Vehicle_defaults.m_foley_door_close_id : v_group->m_foley_door_close_id;
        return true;
    }

    char name[32];
    // Original bug kept: the wave bank is looked up before "Engine" below is read, from whatever engine_id holds.
    if (xml_table_get_opt_string(name, 0x20, section, "Wavebank")) {
        v_info->engine_wavebank_id = audiolib_sound_bank_get_id(name, 1);
    }
    else {
        v_info->engine_wavebank_id = audiolib_vehicle_engine_get_wavebank_id(v_info->engine_id);
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Engine_Start")) {
        audiolib_cue_get_id(name, &v_info->foley_start);
        if (v_info->foley_start == -1) {
            v_info->foley_start = parsing_groups ? Vehicle_defaults.m_foley_engine_start : v_group->foley_start;
        }
    }
    else {
        v_info->foley_start = -1;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Engine")) {
        v_info->engine_id = audiolib_vehicle_engine_get_id(name);
    }
    else {
        v_info->engine_id = parsing_groups ? Vehicle_defaults.m_engine : v_group->engine_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Engine_Off")) {
        audiolib_cue_get_id(name, &v_info->foley_off);
        if (v_info->foley_off == -1) {
            v_info->foley_off = parsing_groups ? Vehicle_defaults.m_foley_engine_off : v_group->foley_off;
        }
    }
    else {
        v_info->foley_off = -1;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Engine_Peel")) {
        audiolib_cue_get_id(name, &v_info->foley_engine_peel);
        if (v_info->foley_engine_peel == -1) {
            v_info->foley_engine_peel = parsing_groups ? Vehicle_defaults.m_foley_engine_peel : v_group->foley_engine_peel;
        }
    }
    else {
        v_info->foley_engine_peel = parsing_groups ? Vehicle_defaults.m_foley_engine_peel : v_group->foley_engine_peel;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Gear_Shift")) {
        audiolib_cue_get_id(name, &v_info->foley_shift);
        if (v_info->foley_shift == -1) {
            v_info->foley_shift = parsing_groups ? Vehicle_defaults.m_foley_gear_shift : v_group->foley_shift;
        }
    }
    else {
        v_info->foley_shift = parsing_groups ? Vehicle_defaults.m_foley_gear_shift : v_group->foley_shift;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Gear_Grind")) {
        audiolib_cue_get_id(name, &v_info->foley_grind);
    }
    else {
        v_info->foley_grind = parsing_groups ? Vehicle_defaults.m_foley_gear_grind : v_group->foley_grind;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Horn")) {
        audiolib_cue_get_id(name, &v_info->foley_honk);
    }
    else {
        v_info->foley_honk = parsing_groups ? Vehicle_defaults.m_foley_horn : v_group->foley_honk;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Low_Impact")) {
        audiolib_cue_get_id(name, &v_info->m_foley_impact_id[0]);
    }
    else {
        v_info->m_foley_impact_id[0] = parsing_groups ? Vehicle_defaults.m_foley_impact_id[0] : v_group->m_foley_impact_id[0];
    }
    if (xml_table_get_opt_string(name, 0x20, section, "High_Impact")) {
        audiolib_cue_get_id(name, &v_info->m_foley_impact_id[1]);
    }
    else {
        v_info->m_foley_impact_id[1] = parsing_groups ? Vehicle_defaults.m_foley_impact_id[1] : v_group->m_foley_impact_id[1];
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Scraping")) {
        audiolib_cue_get_id(name, &v_info->m_foley_scraping_id);
    }
    else {
        v_info->m_foley_scraping_id = parsing_groups ? Vehicle_defaults.m_foley_scraping_id : v_group->m_foley_scraping_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Corpse_Impact")) {
        audiolib_cue_get_id(name, &v_info->m_foley_corpse_impact_id);
    }
    else {
        v_info->m_foley_corpse_impact_id = parsing_groups ? Vehicle_defaults.m_foley_corpse_impact_id : v_group->m_foley_corpse_impact_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Component_Impact")) {
        audiolib_cue_get_id(name, &v_info->m_foley_component_impact_id);
    }
    else {
        v_info->m_foley_component_impact_id = parsing_groups ? Vehicle_defaults.m_foley_component_impact_id : v_group->m_foley_component_impact_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Wheel_Impact")) {
        audiolib_cue_get_id(name, &v_info->m_foley_wheel_impact_id);
    }
    else {
        v_info->m_foley_wheel_impact_id = parsing_groups ? Vehicle_defaults.m_foley_wheel_impact_id : v_group->m_foley_wheel_impact_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Chassis_Land_Low")) {
        audiolib_cue_get_id(name, &v_info->m_foley_chassis_land_id[0]);
    }
    else {
        v_info->m_foley_chassis_land_id[0] = parsing_groups ? Vehicle_defaults.m_foley_chassis_land_id[0] : v_group->m_foley_chassis_land_id[0];
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Chassis_Land_High")) {
        audiolib_cue_get_id(name, &v_info->m_foley_chassis_land_id[1]);
    }
    else {
        v_info->m_foley_chassis_land_id[1] = parsing_groups ? Vehicle_defaults.m_foley_chassis_land_id[1] : v_group->m_foley_chassis_land_id[1];
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Pass_By")) {
        audiolib_cue_get_id(name, &v_info->m_foley_pass_by);
    }
    else {
        v_info->m_foley_pass_by = parsing_groups ? Vehicle_defaults.m_foley_pass_by : v_group->m_foley_pass_by;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Door_Open")) {
        audiolib_cue_get_id(name, &v_info->m_foley_door_open_id);
    }
    else {
        v_info->m_foley_door_open_id = parsing_groups ? Vehicle_defaults.m_foley_door_open_id : v_group->m_foley_door_open_id;
    }
    if (xml_table_get_opt_string(name, 0x20, section, "Door_Close")) {
        audiolib_cue_get_id(name, &v_info->m_foley_door_close_id);
    }
    else {
        v_info->m_foley_door_close_id = parsing_groups ? Vehicle_defaults.m_foley_door_close_id : v_group->m_foley_door_close_id;
    }
    return true;
}

// 0x00B6BE50 (:1532)
static bool vehicle_parse_effects(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Effects");
    if (section == NULL) {
        v_info->m_effect_engine_fire = parsing_groups ? Vehicle_defaults.m_effect_engine_fire : v_group->m_effect_engine_fire;
        v_info->m_effect_engine_smoke = parsing_groups ? Vehicle_defaults.m_effect_engine_smoke : v_group->m_effect_engine_smoke;
        v_info->m_effect_corpse_smoke = parsing_groups ? Vehicle_defaults.m_effect_corpse_smoke : v_group->m_effect_corpse_smoke;
        v_info->m_effect_component_fire = parsing_groups ? Vehicle_defaults.m_effect_component_fire : v_group->m_effect_component_fire;
        v_info->m_effect_exhaust_normal = parsing_groups ? Vehicle_defaults.m_effect_exhaust_normal : v_group->m_effect_exhaust_normal;
        v_info->m_effect_exhaust_burst = parsing_groups ? Vehicle_defaults.m_effect_exhaust_burst : v_group->m_effect_exhaust_burst;
        v_info->m_effect_collision = parsing_groups ? Vehicle_defaults.m_effect_collision : v_group->m_effect_collision;
        v_info->m_effect_scrape = parsing_groups ? Vehicle_defaults.m_effect_scrape : v_group->m_effect_scrape;
        v_info->m_effect_component_detach = parsing_groups ? Vehicle_defaults.m_effect_component_detach : v_group->m_effect_component_detach;
        v_info->m_effect_head_light_primary = parsing_groups ? Vehicle_defaults.m_effect_head_light_primary : v_group->m_effect_head_light_primary;
        v_info->m_effect_head_light_secondary = parsing_groups ? Vehicle_defaults.m_effect_head_light_secondary : v_group->m_effect_head_light_secondary;
        v_info->m_effect_fog_light = parsing_groups ? Vehicle_defaults.m_effect_fog_light : v_group->m_effect_fog_light;
        v_info->m_effect_tail_light = parsing_groups ? Vehicle_defaults.m_effect_tail_light : v_group->m_effect_tail_light;
        v_info->m_effect_brake_light = parsing_groups ? Vehicle_defaults.m_effect_brake_light : v_group->m_effect_brake_light;
        v_info->m_effect_reverse_light = parsing_groups ? Vehicle_defaults.m_effect_reverse_light : v_group->m_effect_reverse_light;
        v_info->m_effect_emergency_light = parsing_groups ? Vehicle_defaults.m_effect_emergency_light : v_group->m_effect_emergency_light;
        v_info->m_effect_strobe_light = parsing_groups ? Vehicle_defaults.m_effect_strobe_light : v_group->m_effect_strobe_light;
        v_info->m_explosion_info = parsing_groups ? Vehicle_defaults.m_explosion_info : v_group->m_explosion_info;
        v_info->m_bomb_explosion_info = parsing_groups ? Vehicle_defaults.m_bomb_explosion_info : v_group->m_bomb_explosion_info;
        return true;
    }
    char name[128];
    if (xml_table_get_opt_string(name, 0x80, section, "Engine_Fire")) {
        v_info->m_effect_engine_fire = effects_get_handle(name);
    }
    else {
        v_info->m_effect_engine_fire = parsing_groups ? Vehicle_defaults.m_effect_engine_fire : v_group->m_effect_engine_fire;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Engine_Smoke")) {
        v_info->m_effect_engine_smoke = effects_get_handle(name);
    }
    else {
        v_info->m_effect_engine_smoke = parsing_groups ? Vehicle_defaults.m_effect_engine_smoke : v_group->m_effect_engine_smoke;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Corpse_Smoke")) {
        v_info->m_effect_corpse_smoke = effects_get_handle(name);
    }
    else {
        v_info->m_effect_corpse_smoke = parsing_groups ? Vehicle_defaults.m_effect_corpse_smoke : v_group->m_effect_corpse_smoke;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Component_Fire")) {
        v_info->m_effect_component_fire = effects_get_handle(name);
    }
    else {
        v_info->m_effect_component_fire = parsing_groups ? Vehicle_defaults.m_effect_component_fire : v_group->m_effect_component_fire;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Exhaust")) {
        v_info->m_effect_exhaust_normal = effects_get_handle(name);
    }
    else {
        v_info->m_effect_exhaust_normal = parsing_groups ? Vehicle_defaults.m_effect_exhaust_normal : v_group->m_effect_exhaust_normal;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "ExhaustBurst")) {
        v_info->m_effect_exhaust_burst = effects_get_handle(name);
    }
    else {
        v_info->m_effect_exhaust_burst = parsing_groups ? Vehicle_defaults.m_effect_exhaust_burst : v_group->m_effect_exhaust_burst;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Collision_Effect")) {
        v_info->m_effect_collision = effects_get_handle(name);
    }
    else {
        v_info->m_effect_collision = parsing_groups ? Vehicle_defaults.m_effect_collision : v_group->m_effect_collision;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Scrape_Effect")) {
        v_info->m_effect_scrape = effects_get_handle(name);
    }
    else {
        v_info->m_effect_scrape = parsing_groups ? Vehicle_defaults.m_effect_scrape : v_group->m_effect_scrape;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "ComponentDetach")) {
        v_info->m_effect_component_detach = effects_get_handle(name);
    }
    else {
        v_info->m_effect_component_detach = parsing_groups ? Vehicle_defaults.m_effect_component_detach : v_group->m_effect_component_detach;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Head_Light_Primary")) {
        v_info->m_effect_head_light_primary = effects_get_handle(name);
    }
    else {
        v_info->m_effect_head_light_primary = parsing_groups ? Vehicle_defaults.m_effect_head_light_primary : v_group->m_effect_head_light_primary;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Head_Light_Secondary")) {
        v_info->m_effect_head_light_secondary = effects_get_handle(name);
    }
    else {
        v_info->m_effect_head_light_secondary = parsing_groups ? Vehicle_defaults.m_effect_head_light_secondary : v_group->m_effect_head_light_secondary;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Fog_Light")) {
        v_info->m_effect_fog_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_fog_light = parsing_groups ? Vehicle_defaults.m_effect_fog_light : v_group->m_effect_fog_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Tail_Light")) {
        v_info->m_effect_tail_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_tail_light = parsing_groups ? Vehicle_defaults.m_effect_tail_light : v_group->m_effect_tail_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Brake_Light")) {
        v_info->m_effect_brake_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_brake_light = parsing_groups ? Vehicle_defaults.m_effect_brake_light : v_group->m_effect_brake_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Reverse_Light")) {
        v_info->m_effect_reverse_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_reverse_light = parsing_groups ? Vehicle_defaults.m_effect_reverse_light : v_group->m_effect_reverse_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Emergency_Light")) {
        v_info->m_effect_emergency_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_emergency_light = parsing_groups ? Vehicle_defaults.m_effect_emergency_light : v_group->m_effect_emergency_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Strobe_Light")) {
        v_info->m_effect_strobe_light = effects_get_handle(name);
    }
    else {
        v_info->m_effect_strobe_light = parsing_groups ? Vehicle_defaults.m_effect_strobe_light : v_group->m_effect_strobe_light;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Explosion")) {
        v_info->m_explosion_info = explosion_lookup(name);
    }
    else {
        v_info->m_explosion_info = parsing_groups ? Vehicle_defaults.m_explosion_info : v_group->m_explosion_info;
    }
    if (xml_table_get_opt_string(name, 0x80, section, "Bomb_Explosion")) {
        v_info->m_bomb_explosion_info = explosion_lookup(name);
    }
    else {
        v_info->m_bomb_explosion_info = parsing_groups ? Vehicle_defaults.m_bomb_explosion_info : v_group->m_bomb_explosion_info;
    }
    return true;
}

// 0x00B6C630 (:1717)
static bool vehicle_parse_angular_damping(vehicle_info* v_info, vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    v_info->m_normal_spin_damping = parsing_groups ? Vehicle_defaults.m_normal_spin_damping : v_group->m_normal_spin_damping;
    v_info->m_normal_spin_damping_ai = parsing_groups ? Vehicle_defaults.m_normal_spin_damping_ai : v_group->m_normal_spin_damping_ai;
    v_info->m_collision_spin_damping = parsing_groups ? Vehicle_defaults.m_collision_spin_damping : v_group->m_collision_spin_damping;
    v_info->m_collision_spin_threshold = parsing_groups ? Vehicle_defaults.m_collision_spin_threshold : v_group->m_collision_spin_threshold;
    xml_element* section = xml_table_find_opt(root, "Angular_Damping");
    if (section != NULL) {
        xml_table_get_opt_float(v_info->m_normal_spin_damping, section, "Normal_Spin_Damping");
        xml_table_get_opt_float(v_info->m_normal_spin_damping_ai, section, "Normal_Spin_Damping_AI");
        xml_table_get_opt_float(v_info->m_collision_spin_damping, section, "Collision_Spin_Damping");
        if (xml_table_get_opt_float(v_info->m_collision_spin_threshold, section, "Collision_Spin_Threshold")) {
            v_info->m_collision_spin_threshold *= VEHICLE_DEG_TO_RAD;
        }
    }
    return true;
}

// 0x00B6C740 (:1755)
static bool vehicle_parse_camera_settings(vehicle_info* v_info, const vehicle_info* v_group, xml_element* root, bool parsing_groups)
{
    xml_element* section = xml_table_find_opt(root, "Cameras");
    v_info->camera_settings_count = xml_table_count(section, "Camera");
    xml_element* camera = xml_table_find_opt(section, "Camera");
    bool use_defaults = parsing_groups;
    for (int i = 0; i < v_info->camera_settings_count; i++) {
        vehicle_camera_settings& cs = v_info->camera_settings[i];
        // once past the group's cameras, the defaults (sticky: no later camera returns to the group's)
        if (!use_defaults && (v_group == NULL || v_group->camera_settings_count <= i)) {
            use_defaults = true;
        }
        const vehicle_camera_settings& base = use_defaults ? Vehicle_default_camera_settings : v_group->camera_settings[i];

        xml_element* offset = xml_table_find_opt(camera, "Lookat_Offset");
        if (offset == NULL) {
            cs.lookat_offset = base.lookat_offset;
        }
        else {
            xml_table_get_vector(cs.lookat_offset, offset);
        }
        offset = xml_table_find_opt(camera, "Lookat_Vehicle_Offset");
        if (offset == NULL) {
            cs.lookat_vehicle_offset = base.lookat_vehicle_offset;
        }
        else {
            xml_table_get_vector(cs.lookat_vehicle_offset, offset);
        }
        offset = xml_table_find_opt(camera, "Lookat_Enter_Offset");
        if (offset == NULL) {
            // 9.080706e+08: the "not set" vector
            cs.lookat_vehicle_enter_offset.x = 9.080706e+08f;
            cs.lookat_vehicle_enter_offset.y = 9.080706e+08f;
            cs.lookat_vehicle_enter_offset.z = 9.080706e+08f;
        }
        else {
            xml_table_get_vector(cs.lookat_vehicle_enter_offset, offset);
        }
        cs.lookfrom_height = 1.5f; // DEFAULT_LOOKFROM_HEIGHT
        xml_table_get_opt_float(cs.lookfrom_height, camera, "Lookfrom_Height");
        cs.enter_distance = 0.0f;
        xml_table_get_opt_float(cs.enter_distance, camera, "Enter_Distance");
        cs.y_axis_rotation_speed = 1.0f;
        xml_table_get_opt_float(cs.y_axis_rotation_speed, camera, "Y_Axis_Rotation");
        cs.follow_dist = base.follow_dist;
        xml_table_get_opt_float(cs.follow_dist, camera, "Follow_Distance");
        cs.follow_height = base.follow_height;
        xml_table_get_opt_float(cs.follow_height, camera, "Follow_Height");
        cs.camera_steer_angle = base.camera_steer_angle;
        xml_table_get_opt_float(cs.camera_steer_angle, camera, "Camera_Steer_Angle");
        cs.camera_steer_speed = base.camera_steer_speed;
        xml_table_get_opt_float(cs.camera_steer_speed, camera, "Camera_Steer_Speed");
        cs.turret_camera = false;
        xml_table_get_opt_bool(cs.turret_camera, camera, "Turret_Camera");
        cs.force_turret_cam = false;
        xml_table_get_opt_bool(cs.force_turret_cam, camera, "Force_T_Cam");
        camera = xml_table_find_next(section, camera, "Camera");
    }

    if (parsing_groups) {
        return true;
    }
    // a vehicle's camera marked as a turret camera over a group's plain one: the group's plain one is kept as an extra
    const int parsed_cameras = v_info->camera_settings_count;
    for (int i = 0; i < parsed_cameras; i++) {
        if (v_group->camera_settings_count <= i || 2 < v_info->camera_settings_count) {
            break;
        }
        if (v_info->camera_settings[i].turret_camera && !v_group->camera_settings[i].turret_camera) {
            v_info->camera_settings[v_info->camera_settings_count] = v_group->camera_settings[i];
            v_info->camera_settings_count++;
        }
    }
    // the group's remaining cameras; a vehicle with none anywhere gets the default
    int count = v_info->camera_settings_count;
    if (count < v_group->camera_settings_count) {
        for (; count < v_group->camera_settings_count; count++) {
            v_info->camera_settings[count] = v_group->camera_settings[count];
        }
        v_info->camera_settings_count = v_group->camera_settings_count;
    }
    else if (count == 0) {
        v_info->camera_settings_count = 1;
        v_info->camera_settings[0] = Vehicle_default_camera_settings;
    }
    // every camera is a turret camera and there is room: add the default as the non-turret one
    count = v_info->camera_settings_count;
    int turret_cameras = 0;
    while (turret_cameras < count && v_info->camera_settings[turret_cameras].turret_camera) {
        turret_cameras++;
    }
    if (turret_cameras <= 2 && turret_cameras == count) {
        v_info->camera_settings[count] = Vehicle_default_camera_settings;
        v_info->camera_settings_count++;
    }
    return true;
}

// a copy of a table string in the vehicle's pool
static char* vehicle_pool_string(mempool* pool, const char* text)
{
    const unsigned int size = (unsigned int)strlen(text) + 1;
    char* copy = (char*)pool->alloc(size, 1);
    if (copy != NULL) {
        strncpy_safe(copy, text, size);
    }
    return copy;
}

// 0x00B6CB50 (:1879)
static bool vehicle_parse_turret_mounts(vehicle_info* v_info, const vehicle_info* v_group, xml_element* root, bool parsing_groups, mempool* pool)
{
    xml_element* section = xml_table_find_opt(root, "Turrets");
    if (!parsing_groups && section == NULL) {
        v_info->num_turret_mounts = v_group->num_turret_mounts;
        for (int i = 0; i < v_info->num_turret_mounts; i++) {
            v_info->turret_mounts[i] = v_group->turret_mounts[i];
        }
        return true;
    }
    v_info->num_turret_mounts = xml_table_count(section, "Mount");
    xml_element* mount = xml_table_find_opt(section, "Mount");
    for (int i = 0; i < v_info->num_turret_mounts; i++) {
        vehicle_info::turret_mount_info& info = v_info->turret_mounts[i];
        info.name = vehicle_pool_string(pool, xml_table_get_req_string_ref(mount, "Name"));
        if (info.name == NULL) {
            return false;
        }
        info.prop_point = vehicle_pool_string(pool, xml_table_get_req_string_ref(mount, "PropPoint"));
        const char* seat = xml_table_get_req_string_ref(mount, "Seat");
        info.seat = VSI_INVALID;
        for (int s = 0; s < 11; s++) {
            if (v_stricmp(seat, Vehicle_seat_tag_names[s]) == 0) {
                info.seat = (vehicle_seat_index)s;
                break;
            }
        }
        xml_element* flags = xml_table_find_opt(mount, "Flags");
        if (flags != NULL) {
            const char* flag_names[1] = {"Driver Turret"};
            info.flags = xml_table_get_flags(flag_names, 1, flags, false);
        }
        info.attached_turret_info = turret_find_info(xml_table_get_req_string_ref(mount, "AttachedTurret"));
        mount = xml_table_find_next(section, mount, "Mount");
    }
    return true;
}

// 0x00B6CD50 (:1966)
static bool vehicle_parse_rig(vehicle_info* v_info, xml_element* root, mempool* pool)
{
    xml_element* section = xml_table_find_opt(root, "Rig");
    if (section == NULL) {
        v_info->rig_name = NULL;
        v_info->anim_set_name = NULL;
        v_info->footstep_effects = NULL;
        return true;
    }
    char rig_name[64];
    cf_add_ext(rig_name, 0x40, xml_table_get_req_filename_ref(section, "Rig"), ".rig_pc", false);
    v_info->rig_name = vehicle_pool_string(pool, rig_name);
    v_info->anim_set_name = vehicle_pool_string(pool, xml_table_get_req_string_ref(section, "Anim_set"));
    anim_rig* r = animlib_get_loaded_rig(v_info->rig_name);
    v_info->bones_used_info.unique_id = Vehicle_base_unique_id;
    Vehicle_base_unique_id++;
    v_info->bones_used_info.num_bones = r->num_bones;
    for (unsigned int i = 0; i < 74; i++) {
        Bone_indicies[i] = i;
    }
    v_info->bones_used_info.bone_indices = Bone_indicies;
    animlib_register_entity_base_bone_set(r, v_info->bones_used_info);

    foot_ground_effects* footsteps = NULL;
    const char* name = xml_table_get_opt_string_ref(section, "Footsteps");
    if (name != NULL) {
        // Original quirk kept: an unknown list returns before footstep_effects is written.
        int i = 0;
        for (;;) {
            if (i >= Num_footstep_effect_lists) {
                return true;
            }
            if (v_stricmp(name, Footstep_effects_list[i].name) == 0) {
                break;
            }
            i++;
        }
        footsteps = &Footstep_effects_list[i];
    }
    v_info->footstep_effects = footsteps;
    return true;
}

// 0x00B6CF20 (:2028)
static bool vehicle_parse_flyer(vehicle_info* v_info, xml_element* root)
{
    xml_element* section = xml_table_find_opt(root, "FlyerParams");
    if (section == NULL) {
        return true;
    }
    xml_table_get_req_float(v_info->flyer_max_up_thrust, section, "FlyerMaxUpThrust");
    xml_table_get_req_float(v_info->flyer_max_turn_angvel, section, "FlyerMaxTurnAngvel");
    xml_table_get_req_float(v_info->flyer_max_turn_angaccl, section, "FlyerMaxTurnAngaccl");
    xml_table_get_req_float(v_info->flyer_max_thrust_offset_x, section, "FlyerMaxThrustOffsetX");
    xml_table_get_req_float(v_info->flyer_max_thrust_offset_z, section, "FlyerMaxThrustOffsetZ");
    xml_table_get_req_float(v_info->flyer_max_tilt_angvel, section, "FlyerMaxTiltAngvel");
    xml_table_get_req_float(v_info->flyer_max_tilt_angaccl, section, "FlyerMaxTiltAngaccl");
    xml_table_get_req_float(v_info->flyer_spin_bank_scalar, section, "FlyerSpinBankScalar");
    xml_table_get_req_float(v_info->flyer_thrust_tilt_scalar, section, "FlyerThrustTiltScalar");
    xml_table_get_req_float(v_info->flyer_bank_tilt_scalar, section, "FlyerBankTiltScalar");
    xml_table_get_req_float(v_info->flyer_def_lookat_y_scalar, section, "FlyerDefaultLookatYScalar");
    v_info->flyer_max_thrust_offset_x *= VEHICLE_DEG_TO_RAD;
    v_info->flyer_max_thrust_offset_z *= VEHICLE_DEG_TO_RAD;
    v_info->flyer_wingtip_effect = 0xFFFFFFFF;
    v_info->flyer_thruster_effect = 0xFFFFFFFF;
    v_info->flyer_mainengine_effect = 0xFFFFFFFF;
    const char* name = xml_table_get_opt_string_ref(section, "FlyerWingtipEffect");
    if (name != NULL) {
        v_info->flyer_wingtip_effect = effects_get_handle(name);
    }
    name = xml_table_get_opt_string_ref(section, "FlyerThrusterEffect");
    if (name != NULL) {
        v_info->flyer_thruster_effect = effects_get_handle(name);
    }
    name = xml_table_get_opt_string_ref(section, "FlyerMainengineEffect");
    if (name != NULL) {
        v_info->flyer_mainengine_effect = effects_get_handle(name);
    }
    name = xml_table_get_opt_string_ref(section, "FlyerJetwashEffect");
    if (name != NULL) {
        v_info->flyer_jetwash_effect = effects_get_handle(name);
    }
    return true;
}

// 0x00B6D0E0 (:2081)
static bool vehicle_parse_speed_effects(vehicle_info* v_info, xml_element* root)
{
    xml_element* section = xml_table_find_opt(root, "speed_effects");
    if (section != NULL) {
        xml_table_get_req_float(v_info->m_camera_fov_multiplier, section, "camera_fov_multiplier");
        xml_table_get_req_float(v_info->m_camera_fov_min_speed, section, "camera_fov_min_speed");
        xml_table_get_req_float(v_info->m_camera_shake_min_speed, section, "camera_shake_min_speed");
        xml_table_get_req_float(v_info->m_radial_blur_max, section, "radial_blur_max");
        xml_table_get_req_float(v_info->m_radial_blur_min_speed, section, "radial_blur_min_speed");
        return true;
    }
    v_info->m_camera_fov_multiplier = Vehicle_defaults.m_camera_fov_multiplier;
    v_info->m_camera_fov_min_speed = Vehicle_defaults.m_camera_fov_min_speed;
    v_info->m_camera_shake_min_speed = Vehicle_defaults.m_camera_shake_min_speed;
    v_info->m_radial_blur_max = Vehicle_defaults.m_radial_blur_max;
    v_info->m_radial_blur_min_speed = Vehicle_defaults.m_radial_blur_min_speed;
    return true;
}

// 0x00B928F0 (:2137)
static bool vehicle_parse_vehicle(vehicle_info* v_info, xml_element* root, bool parsing_groups, bool dlc)
{
    mempool* pool = dlc ? (mempool*)&Level_mempool : (mempool*)&Permanent_mempool;
    vehicle_info* v_group = NULL;
    *(unsigned char*)&v_info->flags = 0;
    v_info->slot_id = -1;
    v_info->srid = 0;
    xml_element* element = xml_find(root, "Name");
    if (element == NULL) {
        return false;
    }
    v_info->name = Vehicle_info_string_pool.add_unique(element->text);
    v_info->flags.is_walker_flamer = false;
    if (v_stricmp(v_info->name, "walker_flamer") == 0) {
        v_info->flags.is_walker_flamer = true;
    }

    const char* text = xml_table_get_opt_string_ref(root, "Vehicle_Interaction");
    v_info->vi_info = NULL;
    if (text != NULL) {
        for (int i = 0; i < Num_vehicle_interaction_infos; i++) {
            if (v_stricmp(text, Vehicle_interaction_infos[i].name) == 0) {
                v_info->vi_info = &Vehicle_interaction_infos[i];
                break;
            }
        }
    }

    element = xml_table_find_opt(root, "VehicleClass");
    if (element != NULL) {
        v_info->vehicle_class = (vehicle_class_type)xml_table_get_type_from_list(Vehicle_class_names, 3, element);
    }
    v_info->cover_info = NULL;
    element = xml_find(root, "vehicle_family");
    v_info->variant_family = element != NULL ? get_vehicle_family_id(element->text) : INVALID_VEHICLE_FAMILY_ID;

    v_info->spawn_box.max = DEFAULT_VEHICLE_SPAWN_BOX;
    v_info->spawn_box.min.set(-DEFAULT_VEHICLE_SPAWN_BOX.x, -DEFAULT_VEHICLE_SPAWN_BOX.y, -DEFAULT_VEHICLE_SPAWN_BOX.z);
    xml_element* spawn_offset = xml_find(root, "spawn_offset_info");
    if (spawn_offset != NULL) {
        element = xml_find(spawn_offset, "spawn_height");
        const float spawn_height = element != NULL ? xml_get_float(element, 0.0f) : 0.0f;
        xml_table_get_req_vector(v_info->spawn_box.max, spawn_offset, "spawn_box_max");
        v_info->spawn_box.min.set(-v_info->spawn_box.max.x, -v_info->spawn_box.max.y, -v_info->spawn_box.max.z);
        if (vec_dist_squared(v_info->spawn_box.max, v_info->spawn_box.min) <= 0.0f) {
            v_info->spawn_box.max = DEFAULT_VEHICLE_SPAWN_BOX;
            v_info->spawn_box.min.set(-DEFAULT_VEHICLE_SPAWN_BOX.x, -DEFAULT_VEHICLE_SPAWN_BOX.y, -DEFAULT_VEHICLE_SPAWN_BOX.z);
        }
        if (0.0f < spawn_height) {
            v_info->spawn_box.min.y = -spawn_height;
        }
    }
    const vector& box_max = v_info->spawn_box.max;
    const vector& box_min = v_info->spawn_box.min;
    const float max_length = sqrtf(box_max.x * box_max.x + box_max.y * box_max.y + box_max.z * box_max.z);
    const float min_length = sqrtf(box_min.x * box_min.x + box_min.y * box_min.y + box_min.z * box_min.z);
    v_info->spawn_size = min_length - max_length < 0.0f ? max_length : min_length;

    if (parsing_groups) {
        v_info->m_max_hitpoints = Vehicle_defaults.m_max_hitpoints;
    }
    else {
        element = xml_find(root, "Mesh_Name");
        if (element == NULL) {
            return false;
        }
        text = get_xml_filename(element);
        if (text == NULL || VEHICLE_NAME_MAX_LENGTH < (int)strlen(text)) {
            return false;
        }
        char base_name[256];
        char mesh_name[256];
        cf_drop_ext(base_name, 0x100, text);
        cf_add_ext(mesh_name, 0x100, base_name, ".ccar_pc", false);
        v_info->mesh_name = Vehicle_info_string_pool.add_unique(mesh_name);
        v_info->cover_info = vehicle_cover_get_info_for_mesh(v_info->mesh_name);
        element = xml_find(root, "Display_Name");
        if (element == NULL) {
            return false;
        }
        text = element->text;
        if (VEHICLE_NAME_MAX_LENGTH < (int)strlen(text)) {
            return false;
        }
        v_info->display_name = Vehicle_info_string_pool.add_unique(text);
        char group_name[128];
        xml_table_get_req_string(group_name, 0x80, root, "Group");
        // Original bug kept: an unknown group gives index -1, read in front of the array.
        v_group = Vehicle_group_info + vehicle_group_get_id(group_name);
        v_info->default_team = human_team_from_name(xml_table_get_req_string_ref(root, "Default_Team"));
        v_info->m_max_hitpoints = v_group->m_max_hitpoints;
    }
    xml_table_get_opt_uint(v_info->m_max_hitpoints, root, "Max_Hitpoints");
    if (parsing_groups) {
        v_info->m_abandon_hitpoints = -1;
        xml_table_get_req_float(v_info->chassis_mass, root, "Mass");
        v_info->center_of_mass_y = Vehicle_defaults.m_center_of_mass_y;
    }
    else {
        unsigned int abandon_percent = 90; // 0x5A
        xml_table_get_opt_uint(abandon_percent, root, "Abandon_Health_Percent");
        float fraction = (float)abandon_percent / 100.0f;
        if (fraction - 0.0f < 0.0f) {
            fraction = 0.0f;
        }
        // Original bug kept: the fraction is clamped to 100, not 1 (after the division by 100 it cannot reach it).
        if (0.0f <= fraction - 100.0f) {
            fraction = 100.0f;
        }
        v_info->m_abandon_hitpoints = (int)((float)v_info->m_max_hitpoints * fraction);
        v_info->chassis_mass = v_group->chassis_mass;
        xml_table_get_opt_float(v_info->chassis_mass, root, "Mass");
        xml_table_get_req_float(v_info->m_value, root, "Value");
        xml_table_get_req_uint(v_info->m_chop_shop_props, root, "Props");
        v_info->center_of_mass_y = v_group->center_of_mass_y;
    }
    xml_table_get_opt_float(v_info->center_of_mass_y, root, "Center_Of_Mass_Y");
    v_info->center_of_mass_z = parsing_groups ? Vehicle_defaults.m_center_of_mass_z : v_group->center_of_mass_z;
    xml_table_get_opt_float(v_info->center_of_mass_z, root, "Center_Of_Mass_Z");
    vehicle_parse_camera_settings(v_info, v_group, root, parsing_groups);
    element = xml_find(root, "Special_Vehicle_Type");
    v_info->vehicle_classification = element != NULL ? get_base_vehicle_type(element->text) : BVT_NONE;
    v_info->m_energy_scale = parsing_groups ? Vehicle_defaults.m_energy_scale : v_group->m_energy_scale;
    xml_table_get_opt_float(v_info->m_energy_scale, root, "Energy_Scale");
    v_info->m_collision_mass_scalar = parsing_groups ? Vehicle_defaults.m_collision_mass_scalar : v_group->m_collision_mass_scalar;
    xml_table_get_opt_float(v_info->m_collision_mass_scalar, root, "Collision_Mass_Scalar");
    v_info->m_collision_damage_scale = parsing_groups ? Vehicle_defaults.m_collision_damage_scale : v_group->m_collision_damage_scale;
    xml_table_get_opt_float(v_info->m_collision_damage_scale, root, "Collision_Damage_Scale");
    v_info->m_terrain_damage_scale = parsing_groups ? Vehicle_defaults.m_terrain_damage_scale : v_group->m_terrain_damage_scale;
    xml_table_get_opt_float(v_info->m_terrain_damage_scale, root, "Terrain_Damage_Scale");
    v_info->m_vehicle_damage_scale = parsing_groups ? Vehicle_defaults.m_vehicle_damage_scale : v_group->m_vehicle_damage_scale;
    xml_table_get_opt_float(v_info->m_vehicle_damage_scale, root, "Vehicle_Damage_Scale");
    v_info->m_alert_multiplier = parsing_groups ? Vehicle_defaults.m_alert_multiplier : v_group->m_alert_multiplier;
    xml_table_get_opt_float(v_info->m_alert_multiplier, root, "Alert_Level_Multiplier");
    bool force_break_links = parsing_groups ? false : v_group->flags.force_break_links;
    xml_table_get_opt_bool(force_break_links, root, "Force_Break_Links");
    v_info->flags.force_break_links = force_break_links;
    v_info->m_bullet_damage_scale = parsing_groups ? Vehicle_defaults.m_bullet_damage_scale : v_group->m_bullet_damage_scale;
    xml_table_get_opt_float(v_info->m_bullet_damage_scale, root, "Bullet_Damage_Scale");
    v_info->m_friction_equalizer = parsing_groups ? Vehicle_defaults.m_friction_equalizer : v_group->m_friction_equalizer;
    xml_table_get_opt_float(v_info->m_friction_equalizer, root, "Static_Load_Friction");
    v_info->m_ai_friction_equalizer = parsing_groups ? Vehicle_defaults.m_ai_friction_equalizer : v_group->m_ai_friction_equalizer;
    xml_table_get_opt_float(v_info->m_ai_friction_equalizer, root, "AI_Static_Load_Friction");
    v_info->m_torque_roll_factor = parsing_groups ? Vehicle_defaults.m_torque_roll_factor : v_group->m_torque_roll_factor;
    xml_table_get_opt_float(v_info->m_torque_roll_factor, root, "Roll_Torque_Factor");
    v_info->m_torque_pitch_factor = parsing_groups ? Vehicle_defaults.m_torque_pitch_factor : v_group->m_torque_pitch_factor;
    xml_table_get_opt_float(v_info->m_torque_pitch_factor, root, "Pitch_Torque_Factor");
    v_info->m_torque_yaw_factor = parsing_groups ? Vehicle_defaults.m_torque_yaw_factor : v_group->m_torque_yaw_factor;
    xml_table_get_opt_float(v_info->m_torque_yaw_factor, root, "Yaw_Torque_Factor");
    v_info->m_torque_yaw_scalar = parsing_groups ? Vehicle_defaults.m_torque_yaw_scalar : v_group->m_torque_yaw_scalar;
    xml_table_get_opt_float(v_info->m_torque_yaw_scalar, root, "Yaw_Torque_Scalar");
    v_info->m_extra_torque_factor = parsing_groups ? Vehicle_defaults.m_extra_torque_factor : v_group->m_extra_torque_factor;
    xml_table_get_opt_float(v_info->m_extra_torque_factor, root, "Extra_Steering_Torque");
    v_info->enlarged_wheel_radius = 0.0;
    xml_table_get_opt_float(v_info->enlarged_wheel_radius, root, "Enlarged_Wheel_Radius");
    v_info->autobrake_speed = parsing_groups ? Vehicle_defaults.m_autobrake_speed : v_group->autobrake_speed;
    xml_table_get_opt_float(v_info->autobrake_speed, root, "Autobrake_Speed");
    v_info->enforced_max_speed = parsing_groups ? Vehicle_defaults.m_enforced_max_speed : v_group->enforced_max_speed;
    xml_table_get_opt_float(v_info->enforced_max_speed, root, "EnforcedMaxSpeed");
    v_info->ai_enforced_max_speed = v_info->enforced_max_speed;
    xml_table_get_opt_float(v_info->ai_enforced_max_speed, root, "AIEnforcedMaxSpeed");
    v_info->enforced_max_rspeed = parsing_groups ? Vehicle_defaults.m_enforced_max_rspeed : v_group->enforced_max_rspeed;
    xml_table_get_opt_float(v_info->enforced_max_rspeed, root, "EnforcedMaxRSpeed");
    v_info->powerslide_friction = parsing_groups ? Vehicle_defaults.m_powerslide_friction : v_group->powerslide_friction;
    xml_table_get_opt_float(v_info->powerslide_friction, root, "Powerslide_Friction");
    v_info->powerslide_power = parsing_groups ? Vehicle_defaults.m_powerslide_power : v_group->powerslide_power;
    xml_table_get_opt_float(v_info->powerslide_power, root, "Powerslide_Power");
    v_info->m_chassis_unit_inertia_roll = parsing_groups ? Vehicle_defaults.m_chassis_unit_inertia_roll : v_group->m_chassis_unit_inertia_roll;
    xml_table_get_opt_float(v_info->m_chassis_unit_inertia_roll, root, "Roll_Unit_Inertia");
    v_info->m_chassis_unit_inertia_pitch = parsing_groups ? Vehicle_defaults.m_chassis_unit_inertia_pitch : v_group->m_chassis_unit_inertia_pitch;
    xml_table_get_opt_float(v_info->m_chassis_unit_inertia_pitch, root, "Pitch_Unit_Inertia");
    v_info->m_chassis_unit_inertia_yaw = parsing_groups ? Vehicle_defaults.m_chassis_unit_inertia_yaw : v_group->m_chassis_unit_inertia_yaw;
    xml_table_get_opt_float(v_info->m_chassis_unit_inertia_yaw, root, "Yaw_Unit_Inertia");
    v_info->m_ai_chassis_unit_inertia_yaw = parsing_groups ? Vehicle_defaults.m_ai_chassis_unit_inertia_yaw : v_group->m_ai_chassis_unit_inertia_yaw;
    xml_table_get_opt_float(v_info->m_ai_chassis_unit_inertia_yaw, root, "AI_Yaw_Unit_Inertia");
    v_info->m_viscosity_friction = parsing_groups ? Vehicle_defaults.m_viscosity_friction : v_group->m_viscosity_friction;
    xml_table_get_opt_float(v_info->m_viscosity_friction, root, "Viscosity_Friction");
    v_info->m_ai_max_braking_decel = parsing_groups ? Vehicle_defaults.m_ai_max_braking_decel : v_group->m_ai_max_braking_decel;
    xml_table_get_opt_float(v_info->m_ai_max_braking_decel, root, "AI_Max_Braking_Decel");
    v_info->m_ai_max_radial_accel = parsing_groups ? Vehicle_defaults.m_ai_max_radial_accel : v_group->m_ai_max_radial_accel;
    xml_table_get_opt_float(v_info->m_ai_max_radial_accel, root, "AI_Max_Radial_Accel");
    if (!vehicle_parse_lods(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the LOD information!");
        return false;
    }
    if (!vehicle_parse_engine(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the engine properties!");
        return false;
    }
    if (!vehicle_parse_axles(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the wheel properties!");
        return false;
    }
    if (!vehicle_parse_transmission(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the transmission properties!");
        return false;
    }
    if (!vehicle_parse_steering(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the steering properties!");
        return false;
    }
    if (!vehicle_parse_braking(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the braking properties!");
        return false;
    }
    if (!vehicle_parse_aerodynamics(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the aerodynamic properties!");
        return false;
    }
    if (!vehicle_parse_foleys(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the foley names!");
        return false;
    }
    if (!vehicle_parse_effects(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to read in the effects!");
        return false;
    }
    if (!vehicle_parse_angular_damping(v_info, v_group, root, parsing_groups)) {
        keen::debug::nativePrintArgs("Failed to parse the angular damping properties!");
        return false;
    }
    if (!vehicle_parse_turret_mounts(v_info, v_group, root, parsing_groups, pool)) {
        keen::debug::nativePrintArgs("Failed to parse the turret mounts!");
        return false;
    }
    if (!vehicle_parse_rig(v_info, root, pool)) {
        keen::debug::nativePrintArgs("Failed to parse the rigging info!");
        return false;
    }
    if (!vehicle_parse_flyer(v_info, root)) {
        keen::debug::nativePrintArgs("Failed to parse the flyer info!");
        return false;
    }
    if (!vehicle_parse_speed_effects(v_info, root)) {
        keen::debug::nativePrintArgs("Failed to parse the speed effects info!");
        return false;
    }
    if (!parsing_groups) {
        v_info->srid = stream2_container_find(v_info->mesh_name);
        bool preload = false;
        xml_table_get_req_bool(preload, root, "Preload");
        if (preload && stream2_preload(v_info->srid)) {
            v_info->flags.preload = true;
        }
        bool can_block_roads = false;
        xml_table_get_req_bool(can_block_roads, root, "CanBlockRoads");
        v_info->flags.can_block_roads = can_block_roads;
    }
    return true;
}

// 0x00B93740 (:2582)
static bool parse_vehicle_cover_info(vehicle_cover_info* info, xml_element* root)
{
    const char* mesh = xml_table_get_opt_string_ref(root, "Mesh_Name");
    if (mesh == NULL || VEHICLE_NAME_MAX_LENGTH < (int)strlen(mesh)) {
        return false;
    }
    char base_name[256];
    char mesh_name[256];
    cf_drop_ext(base_name, 0x100, mesh);
    cf_add_ext(mesh_name, 0x100, base_name, ".ccar_pc", false);
    info->mesh_name = Vehicle_info_string_pool.add_unique(mesh_name);
    xml_element* nodes = xml_find(root, "Cover_Nodes");
    if (nodes == NULL) {
        return false;
    }
    for (xml_element* cur = xml_find(nodes, "Cover_Node"); cur != NULL; cur = xml_find_next(nodes, cur, "Cover_Node")) {
        vehicle_cover_node_definition def;
        bool ok = false;
        xml_element* offset = xml_find(cur, "Offset");
        if (offset != NULL) {
            xml_table_get_vector(def.offset, offset);
            const char* orientation = xml_table_get_opt_string_ref(cur, "Orientation");
            bool oriented = true;
            if (orientation == NULL) {
                oriented = false;
            }
            else if (v_stricmp(orientation, "forwards") == 0) {
                def.relative_fvec = FORWARD_VECTOR;
            }
            else if (v_stricmp(orientation, "backwards") == 0) {
                def.relative_fvec.set(-FORWARD_VECTOR.x, -FORWARD_VECTOR.y, -FORWARD_VECTOR.z);
            }
            else if (v_stricmp(orientation, "to the left") == 0) {
                def.relative_fvec.set(-RIGHT_VECTOR.x, -RIGHT_VECTOR.y, -RIGHT_VECTOR.z);
            }
            else if (v_stricmp(orientation, "to the right") == 0) {
                def.relative_fvec = RIGHT_VECTOR;
            }
            else {
                oriented = false;
            }
            xml_element* node_info = oriented ? xml_find(cur, "Node_Info") : NULL;
            if (node_info != NULL) {
                *(unsigned int*)&def.cover_flags = 0x40; // the "vehicle cover" flag
                bool value;
                ok = xml_table_get_opt_bool(value, node_info, "Crouching");
                if (ok) {
                    def.cover_flags.crouch = value;
                    ok = xml_table_get_opt_bool(value, node_info, "Fire_Left");
                }
                if (ok) {
                    def.cover_flags.fire_left = value;
                    ok = xml_table_get_opt_bool(value, node_info, "Fire_Right");
                }
                if (ok) {
                    def.cover_flags.fire_right = value;
                    ok = xml_table_get_opt_bool(value, node_info, "Fire_Popup");
                }
                if (ok) {
                    def.cover_flags.fire_popup = value;
                    ok = xml_table_get_opt_float(def.def_left_arc, node_info, "Def_Left")
                        && xml_table_get_opt_float(def.def_right_arc, node_info, "Def_Right")
                        && xml_table_get_opt_float(def.off_left_arc, node_info, "Off_Left")
                        && xml_table_get_opt_float(def.off_right_arc, node_info, "Off_Right");
                }
            }
        }
        if (!ok) {
            // Int3 in a debug build; a node with a missing value is skipped
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        info->node_info.add(def);
        if (info->node_info.size() >= 10) {
            break;
        }
    }
    return true;
}

// 0x00B6D1C0 (:2730)
static bool parse_vehicle_defaults()
{
    xml_element* element = xml_table_parse("vehicle_defaults.xtbl");
    if (element == NULL) {
        return false;
    }
    if (vehicle_parse_defaults(xml_find(element, "Defaults"))) {
        xml_table_close(element);
        return true;
    }
    char message[256];
    sprintf(message, "Failed to load the defaults from \"%s\"!", "vehicle_defaults.xtbl");
    keen::debug::nativePrintArgs(message);
    return false;
}

// 0x00B93B80 (:2752)
static bool parse_vehicle_groups()
{
    xml_element* element = xml_table_parse("vehicle_groups.xtbl");
    if (element == NULL) {
        return false;
    }
    Num_vehicle_groups = 0;
    for (xml_element* cur = xml_find(element, "Vehicle_Group"); cur != NULL; cur = xml_find_next(element, cur, "Vehicle_Group")) {
        if (0xF < Num_vehicle_groups) {
            return false; // Original quirk kept: more than 16 groups fails without closing the table.
        }
        if (vehicle_parse_vehicle(&Vehicle_group_info[Num_vehicle_groups], cur, true, false)) {
            Num_vehicle_groups++;
        }
    }
    xml_table_close(element);
    return true;
}

// 0x00B99370 (:2785)
static bool parse_single_vehicle_info(farray<vehicle_info,163>& vehicle_info_pool, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* element;
    if (dlc) {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vehicles.xtbl");
        element = xml_table_parse(filename);
    }
    else {
        element = xml_table_parse("vehicles.xtbl");
    }
    if (element == NULL) {
        return false;
    }
    init_family_string_pool();
    for (xml_element* cur = xml_find(element, "Vehicle"); cur != NULL; cur = xml_find_next(element, cur, "Vehicle")) {
        vehicle_info info;
        if (!vehicle_parse_vehicle(&info, cur, false, dlc)) {
            continue;
        }
        vehicle_info* existing = NULL;
        for (int i = 0; i < vehicle_info_pool.size(); i++) {
            if (v_stricmp(vehicle_info_pool[i].name, info.name) == 0) {
                existing = &vehicle_info_pool[i];
                break;
            }
        }
        if (existing != NULL) {
            // a reparse (or a DLC entry of the same name) keeps the loaded container
            const int slot_id = existing->slot_id;
            const unsigned int srid = existing->srid;
            *existing = info;
            existing->slot_id = slot_id;
            existing->srid = srid;
        }
        else {
            vehicle_info* added = vehicle_info_pool.add_in_place();
            if (added != NULL) {
                *added = info;
            }
        }
    }
    xml_table_close(element);
    return true;
}

// 0x00B93C20 (:2845)
static void find_variant_cover_info(farray<vehicle_info,163>& vehicle_info_pool)
{
    farray<vehicle_info const*,163> family;
    for (int i = 0; i < vehicle_info_pool.size(); i++) {
        vehicle_info& info = vehicle_info_pool[i];
        if (info.cover_info != NULL || info.variant_family == NULL) {
            continue;
        }
        family.clear();
        vehicle_collect_family(family, info.variant_family, false);
        for (int j = 0; j < family.size(); j++) {
            if (family[j]->cover_info != NULL) {
                info.cover_info = family[j]->cover_info;
                break;
            }
        }
    }
}

// 0x00B6D230 (:2881)
static bool parse_wheel_effect_table()
{
    xml_element* element = xml_table_parse("vehicle_wheel_surface_effects.xtbl");
    if (element == NULL) {
        return false;
    }
    xml_element* root = xml_find(element, "Vehicle_wheel_surface_effect");
    Automobile_motion_front_effect_info_handle = effects_get_handle(xml_table_get_opt_string_ref(root, "Tire_motion_effect_front"));
    Automobile_motion_back_effect_info_handle = effects_get_handle(xml_table_get_opt_string_ref(root, "Tire_motion_effect_back"));
    Automobile_skid_effect_info_handle = effects_get_handle(xml_table_get_opt_string_ref(root, "Skid_effect"));
    Automobile_peel_effect_info_handle = effects_get_handle(xml_table_get_opt_string_ref(root, "Peel_effect"));
    Automobile_brake_effect_info_handle = effects_get_handle(xml_table_get_opt_string_ref(root, "Brake_effect"));
    audiolib_cue_get_id(xml_table_get_opt_string_ref(root, "Tire_motion_foley"), &Automobile_wheel_motion_cue_id);
    audiolib_cue_get_id(xml_table_get_opt_string_ref(root, "Skid_foley"), &Automobile_wheel_skid_cue_id);
    audiolib_cue_get_id(xml_table_get_opt_string_ref(root, "Peel_foley"), &Automobile_wheel_peel_cue_id);
    audiolib_cue_get_id(xml_table_get_opt_string_ref(root, "Brake_foley"), &Automobile_wheel_brake_cue_id);
    audiolib_cue_get_id(xml_table_get_opt_string_ref(root, "Brake_light_foley"), &Automobile_wheel_brake_light_cue_id);
    audiolib_cue_get_id("VEH_TREADS_DRIVE_HIGH", &Tank_tread_cue_id);
    xml_table_close(element);
    return true;
}

// 0x00B6D370 (:2920)
bool vehicle_table_parsed()
{
    return Vehicle_info_string_pool_data != NULL;
}

// 0x00BAD240 (:2928)
bool vehicle_parse_table(farray<vehicle_info,163>& vehicle_info_pool, mempool* pool, int dlc_id)
{
    if (Vehicle_info_string_pool_data == NULL) {
        Vehicle_info_string_pool_data = (char*)pool->alloc(0x2000, 0);
        if (Vehicle_info_string_pool_data == NULL) {
            keen::debug::nativePrintArgs("Unable to allocate memory for the vehicle string pool");
        }
        Vehicle_info_string_pool.init(Vehicle_info_string_pool_data, 0x2000, VEHICLE_STRING_POOL_NAME, 0, '\0');
    }
    build_vehicle_containers_shared(&Common_mempool, pool, PLATFORM_PC, false, true, dlc_id);
    vehicle_cover_info_init(dlc_id);
    if (dlc_id == -1) {
        if (!parse_vehicle_defaults() || !parse_vehicle_groups()) {
            return false;
        }
    }
    if (!parse_single_vehicle_info(vehicle_info_pool, dlc_id)) {
        return false;
    }
    find_variant_cover_info(vehicle_info_pool);
    return parse_wheel_effect_table();
}

// 0x00BA2F90 (:2973)
bool vehicle_parse_cover_table(object_pool<vehicle_cover_info,163,1>& vehicle_cover_pool, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* element;
    if (dlc) {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vehicle_cover.xtbl");
        element = xml_table_parse(filename);
    }
    else {
        element = xml_table_parse("vehicle_cover.xtbl");
    }
    if (element == NULL) {
        return false;
    }
    if (!dlc) {
        vehicle_cover_pool.clear();
    }
    for (xml_element* cur = xml_find(element, "Chassis"); cur != NULL; cur = xml_find_next(element, cur, "Chassis")) {
        vehicle_cover_info* info = vehicle_cover_pool.alloc(0);
        info->mesh_name = NULL;
        info->node_info.clear();
        if (parse_vehicle_cover_info(info, cur)) {
            if (dlc) {
                Dlc_vehicle_cover_info_list.add(info);
            }
        }
        else {
            vehicle_cover_pool.free(info);
        }
    }
    xml_table_close(element);
    return true;
}

// 0x00B6D380 (:3019): a debug command's empty remains: it sums the vehicle containers' sizes and prints nothing.
void unused_dcf_vehicle_slots()
{
}

// 0x00B68ED0
vehicle_camera_settings::vehicle_camera_settings()
{
}

// 0x00B68EE0
vehicle_info::vehicle_info()
{
}

// 0x00B6D1B0
vehicle_cover_node_definition::vehicle_cover_node_definition()
{
}

// 0x00B953F0
vehicle_cover_info::vehicle_cover_info()
{
}
