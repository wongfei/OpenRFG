// Original: volition/rfg/code/vehicle/vehicle_info.h
//
// Vehicle definitions from vehicles.xtbl (vehicle_parse_table, vehicle_table_parser.cpp; groups provide defaults that
// entries override): class, mesh, the GSC_VEHICLE container (srid, slot_id), hit points, LODs, turret mounts, engine,
// axles and wheels, transmission, steering, aerodynamics, cameras, damage scales, foley and effect ids, rig and the
// flyer parameters. Up to 163 entries (Vehicle_info_pool, vehicle_system.cpp).
#pragma once

#include "volition/anim_lib/code/anim_lib_types.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/util/varray.h"

struct turret_info;
struct vehicle_cover_info;
class vehicle_interaction_info;
struct explosion_info;
struct foot_ground_effects;

// size 0x4
struct lod_info
{
    float dist; // 0x00
};

// size 0x44
struct vehicle_camera_settings
{
    vehicle_camera_settings(); // 0x00B68ED0

    vector lookat_offset; // 0x00
    vector lookat_vehicle_offset; // 0x0C
    vector lookat_vehicle_enter_offset; // 0x18
    float lookfrom_height; // 0x24
    float enter_distance; // 0x28
    float follow_dist; // 0x2C
    float follow_height; // 0x30
    float y_axis_rotation_speed; // 0x34
    float camera_steer_angle; // 0x38
    float camera_steer_speed; // 0x3C
    bool turret_camera; // 0x40
    bool force_turret_cam; // 0x41
};

// size 0x4BC
class vehicle_info
{
public:
    // size 0x14
    struct turret_mount_info
    {
        const char* name; // 0x00
        const char* prop_point; // 0x04
        vehicle_seat_index seat; // 0x08
        unsigned int flags; // 0x0C
        const turret_info* attached_turret_info; // 0x10
    };

    // size 0x30
    struct axle_wheel_info
    {
        bool does_steer; // 0x00
        bool does_handbrake; // 0x01
        float engine_torque_factor; // 0x04
        float mass; // 0x08
        float friction; // 0x0C
        float ai_friction; // 0x10
        float braking_torque; // 0x14
        float spring_compression; // 0x18
        float spring_extension; // 0x1C
        float spring_strength; // 0x20
        float spring_power; // 0x24
        float compression_damping; // 0x28
        float expansion_damping; // 0x2C
    };

    // size 0x5C
    struct transmission_info
    {
        unsigned int num_gears; // 0x00
        float gear_ratios[6]; // 0x04
        float downshift_rpms[5]; // 0x1C
        float upshift_rpms[5]; // 0x30
        float differential_gear_ratio; // 0x44
        float reverse_gear_ratio; // 0x48
        float clutch_delay; // 0x4C
        float clutch_delay_pre_shift; // 0x50
        unsigned int forward_to_reverse_delay; // 0x54
        unsigned int reverse_to_forward_delay; // 0x58
    };

    float get_spawn_height_offset() const; // 0x00B40F10 (.h:141)
    vehicle_info(); // 0x00B68EE0

    const char* name; // 0x00
    const char* display_name; // 0x04
    const char* mesh_name; // 0x08
    vehicle_class_type vehicle_class; // 0x0C
    float spawn_size; // 0x10
    bbox spawn_box; // 0x14
    const vehicle_cover_info* cover_info; // 0x2C
    int slot_id; // 0x30
    unsigned int srid; // 0x34
    const char* variant_family; // 0x38
    base_vehicle_types vehicle_classification; // 0x3C
    human_teams default_team; // 0x40
    vehicle_interaction_info* vi_info; // 0x44
    unsigned int m_max_hitpoints; // 0x48
    int m_abandon_hitpoints; // 0x4C
    float chassis_mass; // 0x50
    float m_value; // 0x54
    unsigned int m_chop_shop_props; // 0x58
    unsigned int m_num_lods; // 0x5C
    lod_info m_lod_info[4]; // 0x60
    int num_turret_mounts; // 0x70
    turret_mount_info turret_mounts[2]; // 0x74
    vehicle_road_preference m_road_preference; // 0x9C
    float engine_torque; // 0xA0
    float ai_engine_torque; // 0xA4
    float min_rpm; // 0xA8
    float optimal_rpm; // 0xAC
    float max_rpm; // 0xB0
    float min_rpm_torque_factor; // 0xB4
    float max_rpm_torque_factor; // 0xB8
    float min_rpm_resistance; // 0xBC
    float opt_rpm_resistance; // 0xC0
    float max_rpm_resistance; // 0xC4
    float reverse_torque_multiplier; // 0xC8
    float clutch_slip_rpm; // 0xCC
    float enlarged_wheel_radius; // 0xD0
    float autobrake_speed; // 0xD4
    float enforced_max_speed; // 0xD8
    float ai_enforced_max_speed; // 0xDC
    float enforced_max_rspeed; // 0xE0
    float powerslide_friction; // 0xE4
    float powerslide_power; // 0xE8
    float force_assist; // 0xEC
    unsigned int m_num_axles; // 0xF0
    axle_wheel_info m_axle_wheel_infos[4]; // 0xF4
    int m_exhaust_tag_id[4]; // 0x1B4
    transmission_info trans_info; // 0x1C4
    float max_steering_angle; // 0x220
    float max_speed_steering_angle; // 0x224
    float ai_max_speed_steering_angle; // 0x228
    float m_steering_wheel_max_speed; // 0x22C
    float m_steering_wheel_max_return_speed; // 0x230
    float m_steering_wheel_damp_angle; // 0x234
    float m_steering_wheel_return_damp_angle; // 0x238
    float counter_steer_min_speed; // 0x23C
    float counter_steer_max_speed; // 0x240
    float air_steer_roll_max_vel; // 0x244
    float air_steer_roll_max_angle_rad; // 0x248
    float air_steer_pitch_max_vel; // 0x24C
    float air_steer_pitch_max_angle_rad; // 0x250
    float min_pedal_input_to_block; // 0x254
    float min_time_to_block; // 0x258
    float ai_min_time_to_block; // 0x25C
    float air_density; // 0x260
    float frontal_area; // 0x264
    float drag_coefficient; // 0x268
    float lift_coefficient; // 0x26C
    float extra_gravity; // 0x270
    float center_of_mass_y; // 0x274
    float center_of_mass_z; // 0x278
    vehicle_camera_settings camera_settings[3]; // 0x27C
    int camera_settings_count; // 0x348
    float m_energy_scale; // 0x34C
    float m_collision_mass_scalar; // 0x350
    float m_collision_damage_scale; // 0x354
    float m_terrain_damage_scale; // 0x358
    float m_bullet_damage_scale; // 0x35C
    float m_vehicle_damage_scale; // 0x360
    float m_friction_equalizer; // 0x364
    float m_ai_friction_equalizer; // 0x368
    float m_torque_roll_factor; // 0x36C
    float m_torque_pitch_factor; // 0x370
    float m_torque_yaw_factor; // 0x374
    float m_torque_yaw_scalar; // 0x378
    float m_extra_torque_factor; // 0x37C
    float m_chassis_unit_inertia_roll; // 0x380
    float m_chassis_unit_inertia_pitch; // 0x384
    float m_chassis_unit_inertia_yaw; // 0x388
    float m_ai_chassis_unit_inertia_yaw; // 0x38C
    float m_viscosity_friction; // 0x390
    float m_ai_max_braking_decel; // 0x394
    float m_ai_max_radial_accel; // 0x398
    float m_alert_multiplier; // 0x39C
    int foley_start; // 0x3A0
    int engine_id; // 0x3A4
    short engine_wavebank_id; // 0x3A8
    int foley_off; // 0x3AC
    int foley_engine_peel; // 0x3B0
    int foley_shift; // 0x3B4
    int foley_grind; // 0x3B8
    int foley_honk; // 0x3BC
    int m_foley_impact_id[2]; // 0x3C0
    int m_foley_scraping_id; // 0x3C8
    int m_foley_corpse_impact_id; // 0x3CC
    int m_foley_component_impact_id; // 0x3D0
    int m_foley_wheel_impact_id; // 0x3D4
    int m_foley_chassis_land_id[2]; // 0x3D8
    int m_foley_pass_by; // 0x3E0
    int m_foley_door_open_id; // 0x3E4
    int m_foley_door_close_id; // 0x3E8
    unsigned int m_effect_engine_fire; // 0x3EC
    unsigned int m_effect_engine_smoke; // 0x3F0
    unsigned int m_effect_corpse_smoke; // 0x3F4
    unsigned int m_effect_component_fire; // 0x3F8
    unsigned int m_effect_exhaust_normal; // 0x3FC
    unsigned int m_effect_exhaust_burst; // 0x400
    unsigned int m_effect_collision; // 0x404
    unsigned int m_effect_scrape; // 0x408
    unsigned int m_effect_component_detach; // 0x40C
    unsigned int m_effect_head_light_primary; // 0x410
    unsigned int m_effect_head_light_secondary; // 0x414
    unsigned int m_effect_fog_light; // 0x418
    unsigned int m_effect_tail_light; // 0x41C
    unsigned int m_effect_brake_light; // 0x420
    unsigned int m_effect_reverse_light; // 0x424
    unsigned int m_effect_emergency_light; // 0x428
    unsigned int m_effect_strobe_light; // 0x42C
    const explosion_info* m_explosion_info; // 0x430
    const explosion_info* m_bomb_explosion_info; // 0x434
    unsigned int m_num_variants; // 0x438
    float m_normal_spin_damping; // 0x43C
    float m_normal_spin_damping_ai; // 0x440
    float m_collision_spin_damping; // 0x444
    float m_collision_spin_threshold; // 0x448
    float m_camera_fov_multiplier; // 0x44C
    float m_camera_fov_min_speed; // 0x450
    float m_camera_shake_min_speed; // 0x454
    float m_radial_blur_max; // 0x458
    float m_radial_blur_min_speed; // 0x45C
    float m_trailer_chance; // 0x460
    const char* rig_name; // 0x464
    const char* anim_set_name; // 0x468
    foot_ground_effects* footstep_effects; // 0x46C
    animlib_bones_used_info bones_used_info; // 0x470
    float flyer_max_up_thrust; // 0x47C
    float flyer_max_turn_angvel; // 0x480
    float flyer_max_turn_angaccl; // 0x484
    float flyer_max_thrust_offset_x; // 0x488
    float flyer_max_thrust_offset_z; // 0x48C
    float flyer_max_tilt_angvel; // 0x490
    float flyer_max_tilt_angaccl; // 0x494
    float flyer_spin_bank_scalar; // 0x498
    float flyer_thrust_tilt_scalar; // 0x49C
    float flyer_bank_tilt_scalar; // 0x4A0
    float flyer_def_lookat_y_scalar; // 0x4A4
    unsigned int flyer_wingtip_effect; // 0x4A8
    unsigned int flyer_thruster_effect; // 0x4AC
    unsigned int flyer_mainengine_effect; // 0x4B0
    unsigned int flyer_jetwash_effect; // 0x4B4
    struct
    {
        bool preload : 1; // 0x00:0
        bool can_block_roads : 1; // 0x00:1
        bool force_break_links : 1; // 0x00:2
        bool seat_count_warning : 1; // 0x00:3
        bool is_walker_flamer : 1; // 0x00:4
    } flags; // 0x4B8
};

// The vehicle type table (vehicle_system.cpp; placement inferred).
extern farray<vehicle_info,163> Vehicle_info_pool; // 0x016BA5F8
