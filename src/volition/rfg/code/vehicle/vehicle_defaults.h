// Original: volition/rfg/code/vehicle/vehicle_defaults.h
//
// Default vehicle parameters, the values a vehicle table entry starts from.
#pragma once

#include "volition/rfg/code/vehicle/vehicle_info.h"

struct explosion_info;

// size 0x22C
struct vehicle_defaults
{
    unsigned int m_max_hitpoints; // 0x00
    float m_center_of_mass_y; // 0x04
    float m_center_of_mass_z; // 0x08
    float m_autobrake_speed; // 0x0C
    float m_enforced_max_speed; // 0x10
    float m_enforced_max_rspeed; // 0x14
    float m_min_rpm; // 0x18
    float m_optimal_rpm; // 0x1C
    float m_max_rpm; // 0x20
    float m_min_rpm_torque_factor; // 0x24
    float m_max_rpm_torque_factor; // 0x28
    float m_min_rpm_resistance; // 0x2C
    float m_opt_rpm_resistance; // 0x30
    float m_max_rpm_resistance; // 0x34
    float m_reverse_torque_multiplier; // 0x38
    float m_force_assist; // 0x3C
    float m_wheel_mass; // 0x40
    float m_wheel_friction; // 0x44
    float m_wheel_ai_friction; // 0x48
    float m_wheel_braking_torque; // 0x4C
    unsigned int m_num_downshift_rpms; // 0x50
    float m_downshift_rpms[5]; // 0x54
    unsigned int m_num_upshift_rpms; // 0x68
    float m_upshift_rpms[5]; // 0x6C
    float m_diff_gear_ratio; // 0x80
    float m_reverse_gear_ratio; // 0x84
    float m_clutch_delay; // 0x88
    float m_clutch_delay_pre_shift; // 0x8C
    float m_clutch_slip_rpm; // 0x90
    unsigned int m_forward_to_reverse_delay; // 0x94
    unsigned int m_reverse_to_forward_delay; // 0x98
    float m_max_steering_angle; // 0x9C
    float m_max_speed_steering_angle; // 0xA0
    float m_steering_wheel_max_speed; // 0xA4
    float m_steering_wheel_max_return_speed; // 0xA8
    float m_steering_wheel_damp_angle; // 0xAC
    float m_steering_wheel_return_damp_angle; // 0xB0
    float m_counter_steer_min_speed; // 0xB4
    float m_counter_steer_max_speed; // 0xB8
    float m_air_steer_roll_max_vel; // 0xBC
    float m_air_steer_roll_max_angle_rad; // 0xC0
    float m_air_steer_pitch_max_vel; // 0xC4
    float m_air_steer_pitch_max_angle_rad; // 0xC8
    float m_min_pedal_input_to_block; // 0xCC
    float m_min_time_to_block; // 0xD0
    float m_spring_compression; // 0xD4
    float m_spring_extension; // 0xD8
    float m_spring_strength; // 0xDC
    float m_spring_compression_damping; // 0xE0
    float m_spring_expansion_damping; // 0xE4
    float m_air_density; // 0xE8
    float m_frontal_area; // 0xEC
    float m_drag_coefficient; // 0xF0
    float m_lift_coefficient; // 0xF4
    float m_extra_gravity; // 0xF8
    float m_energy_scale; // 0xFC
    float m_collision_mass_scalar; // 0x100
    float m_collision_damage_scale; // 0x104
    float m_terrain_damage_scale; // 0x108
    float m_bullet_damage_scale; // 0x10C
    float m_vehicle_damage_scale; // 0x110
    float m_friction_equalizer; // 0x114
    float m_torque_roll_factor; // 0x118
    float m_torque_pitch_factor; // 0x11C
    float m_torque_yaw_factor; // 0x120
    float m_torque_yaw_scalar; // 0x124
    float m_extra_torque_factor; // 0x128
    float m_chassis_unit_inertia_yaw; // 0x12C
    float m_chassis_unit_inertia_roll; // 0x130
    float m_chassis_unit_inertia_pitch; // 0x134
    float m_viscosity_friction; // 0x138
    unsigned int m_num_lods; // 0x13C
    lod_info m_lod_info[4]; // 0x140
    unsigned int m_effect_engine_fire; // 0x150
    unsigned int m_effect_engine_smoke; // 0x154
    unsigned int m_effect_corpse_smoke; // 0x158
    unsigned int m_effect_component_fire; // 0x15C
    unsigned int m_effect_exhaust_normal; // 0x160
    unsigned int m_effect_exhaust_burst; // 0x164
    unsigned int m_effect_collision; // 0x168
    unsigned int m_effect_scrape; // 0x16C
    unsigned int m_effect_component_detach; // 0x170
    unsigned int m_effect_head_light_primary; // 0x174
    unsigned int m_effect_head_light_secondary; // 0x178
    unsigned int m_effect_fog_light; // 0x17C
    unsigned int m_effect_tail_light; // 0x180
    unsigned int m_effect_brake_light; // 0x184
    unsigned int m_effect_reverse_light; // 0x188
    unsigned int m_effect_emergency_light; // 0x18C
    unsigned int m_effect_strobe_light; // 0x190
    const explosion_info* m_explosion_info; // 0x194
    const explosion_info* m_bomb_explosion_info; // 0x198
    int m_foley_engine_start; // 0x19C
    int m_engine; // 0x1A0
    int m_foley_engine_off; // 0x1A4
    int m_foley_engine_peel; // 0x1A8
    int m_foley_gear_shift; // 0x1AC
    int m_foley_gear_grind; // 0x1B0
    int m_foley_horn; // 0x1B4
    int m_foley_impact_id[2]; // 0x1B8
    int m_foley_scraping_id; // 0x1C0
    int m_foley_corpse_impact_id; // 0x1C4
    int m_foley_component_impact_id; // 0x1C8
    int m_foley_wheel_impact_id; // 0x1CC
    int m_foley_chassis_land_id[2]; // 0x1D0
    int m_foley_pass_by; // 0x1D8
    int m_foley_door_open_id; // 0x1DC
    int m_foley_door_close_id; // 0x1E0
    float m_ai_chassis_unit_inertia_yaw; // 0x1E4
    float m_ai_max_speed_steering_angle; // 0x1E8
    float m_ai_min_time_to_block; // 0x1EC
    float m_ai_friction_equalizer; // 0x1F0
    float m_ai_max_braking_decel; // 0x1F4
    float m_ai_max_radial_accel; // 0x1F8
    float m_alert_multiplier; // 0x1FC
    float m_powerslide_friction; // 0x200
    float m_powerslide_power; // 0x204
    float m_normal_spin_damping; // 0x208
    float m_normal_spin_damping_ai; // 0x20C
    float m_collision_spin_damping; // 0x210
    float m_collision_spin_threshold; // 0x214
    float m_camera_fov_multiplier; // 0x218
    float m_camera_fov_min_speed; // 0x21C
    float m_camera_shake_min_speed; // 0x220
    float m_radial_blur_max; // 0x224
    float m_radial_blur_min_speed; // 0x228
};
