// Original: volition/rfg/code/objects/turret.h
// Implementation: volition/rfg/code/objects/turret.cpp
//
// turret, a mounted weapon object (object type OT_TURRET, derived from object, not item): its turret info, the mounted
// weapon, the human using it, aim limits and sweeps (catmull_rom_spline_2d). turret_do_frame_pre / _post run around
// the simulation.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
class item;
class matrix;
struct multi_turret_state;
class rl_renderer;
struct turret_info;
struct turret_physics;
class vector2;
class vehicle;
class weapon;
enum weapon_callback_idx;
struct weapon_fire_info;
struct weapon_info;

// size 0x164
class turret : public object
{
    // human::get_equipped_weapon reads primary_weapon_handle (friends are not in rfg.pdb).
    friend class human;
    // item::hold_item reads primary_weapon_handle too.
    friend class item;
    friend class activity_mission_vrail_data; // init locks the vehicle's turret (inlined)
    // The secondary trigger of human_weapons.cpp reads secondary_weapon_handle.
    friend bool human_weapons_firearm_pull_secondary_weapon_trigger(human* hp);
    friend void human_weapons_firearm_release_secondary_weapon_trigger(human* hp);
    // projectile.cpp's detonate_charges reads primary_weapon_handle too.
    friend bool detonate_charges(unsigned int owner_handle);
    // player_autoaim_calculate_assisted_aiming reads primary_weapon_handle too.
    friend void player_autoaim_calculate_assisted_aiming();
public:
    // size 0x2
    struct turret_flags_struct
    {
        bool vehicle_turret : 1; // 0x00:0
        bool locked_turret : 1; // 0x00:1
        bool fixed_turret : 1; // 0x00:2
        bool mp_automated_has_fired : 1; // 0x00:3
        bool walking_right : 1; // 0x00:4
        bool walking_left : 1; // 0x00:5
        bool full360 : 1; // 0x00:6
        bool fast_mode : 1; // 0x00:7
        bool has_autoaim_target : 1; // 0x01:0
        bool primary_weapon_physics : 1; // 0x01:1
        bool secondary_weapon_physics : 1; // 0x01:2
        bool roadblock_turret : 1; // 0x01:3
        bool primary_trigger_down : 1; // 0x01:4
        bool secondary_trigger_down : 1; // 0x01:5
    };

    // size 0x4
    struct resource_dependent_data
    {
        resource_dependent_data(); // 0x00A06C20 (.h:347)

        turret_physics* physics; // 0x00
    };

    turret(); // 0x00A41D30 (:683)
    static const object_type get_type_static(); // 0x005DD650 (.h:62)
    static const object_sub_type get_subtype_static(); // 0x005DD660 (.h:62)
    static const char* get_classname_static(); // 0x00A06B00 (.h:62)
    static int get_bpo_index_static(); // 0x00A06B10 (.h:62)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A41D90 (.h:62)
    static unsigned int get_classname_hash(); // 0x00A1F740 (:682)
    static int pool_remaining(); // 0x00A41D20 (:682)
    virtual void pool_free(); // vtbl+0x80 0x00A969A0 (:682)
    bool can_hit_target(const vector& target) const; // 0x00A42F30 (:2165)
    vector get_operation_position() const; // 0x00A438D0 (:2388)
    matrix get_operation_orient() const; // 0x00A43AF0 (:2408)
    void get_target_pos(vector& tpos) const; // 0x005DD670 (.h:79)
    void mp_set_target_pos(const vector& tpos); // 0x00951330 (.h:82)
    void get_target_direction(vector2& target_dir) const; // 0x00A43B70 (:2423)
    void set_target_direction(const vector2& target_dir); // 0x00A43BC0 (:2435)
    bool aim_turret(const vector& new_target_pos); // 0x00A72B30 (:2229)
    void set_turret_velocity(float h_vel, float v_vel, control_axis_action h_action, control_axis_action v_action); // 0x00A43150 (:2260)
    bool inside_firing_arc(const vector& target_posp) const; // 0x00A437F0 (:2372)
    void attach_new_operator(human* hp); // 0x00A96A20 (:2447)
    void detach_operator(human* hp); // 0x00A96C20 (:2512)
    void turret_process_gyro_values(float& h_vel, float& v_vel); // 0x00A1FA00 (:2246)
    bool is_functional(bool ignore_out_of_ammo) const; // 0x00A42D00 (:2045)
    unsigned int get_hit_points() const; // 0x00951350 (.h:113)
    void set_hit_points_mp(unsigned int new_hp); // 0x00A1F7E0 (:2069)
    virtual bool is_killed() const; // vtbl+0x10 0x00A41DA0 (.h:119)
    bool is_vehicle_turret() const; // 0x005DD690 (.h:122)
    bool object_is_physical_part(unsigned int h) const; // 0x00A1F810 (:2075)
    bool turret_on_vehicle(unsigned int vehicle_handle) const; // 0x006C3ED0 (.h:128)
    bool autoaim_has_fired() const; // 0x00951360 (.h:131)
    void autoaim_clear_fired_flag(); // 0x00951370 (.h:132)
    bool is_locked() const; // 0x005DD6A0 (.h:135)
    bool is_fixed() const; // 0x005DD6B0 (.h:137)
    void calculate_fov_blending(float fdot, float rdot, float& max, float& min) const; // 0x00A1F840 (:2084)
    void update_fov_blending(); // 0x00A42D70 (:2125)
    float get_h_fov_neg() const; // 0x00A06B20 (.h:144)
    float get_h_fov_pos() const; // 0x00A06B30 (.h:145)
    float get_v_fov_neg() const; // 0x00A06B40 (.h:146)
    float get_v_fov_pos() const; // 0x00A06B50 (.h:147)
    void get_kill_impulse(vector& linear, vector& angular); // 0x00A42EC0 (:2148)
    bool should_autoaim() const; // 0x00A72610 (:436)
    unsigned int get_detour_request_handle() const; // 0x00A41A10 (:458)
    bool is_stuck(int stuck_time) const; // 0x005DD6C0 (.h:158)
    void automatic_firing_pattern_start(int delay_ms); // 0x00A726D0 (:464)
    void automatic_firing_pattern_end(); // 0x00A1F4D0 (:480)
    bool automatic_firing_pattern_is_active();
    void automatic_firing_pattern_setup_target(); // 0x00A1F550 (:499)
    void disable_fov_limits(); // 0x00A72730 (:512)
    unsigned int get_linked_turret(); // 0x00A1F5C0 (:585)
    void set_linked_turret(unsigned int h); // 0x00A41B60 (:590)
    void set_h_fov_constraints(float h_pos, float h_neg); // 0x00A41A40 (:527)
    void set_v_fov_constraints(float v_pos, float v_neg); // 0x00A41B00 (:555)
    void look_for_nearby_explosives(); // 0x00A727B0 (:571)
    bool is_automated_turret() const; // 0x00A06B60 (.h:187)
    bool is_stationary_operator_turret() const; // 0x00A06B70 (.h:190)
    bool is_manned_vehicle_turret() const; // 0x00A06B80 (.h:193)
    bool no_operator_animation() const; // 0x007F2520 (.h:196)
    bool limited_ammo_primary() const; // 0x00BD9140 (.h:199)
    bool limited_ammo_secondary() const; // 0x00BD9160 (.h:202)
    bool force_fine_aim() const; // 0x006C3EF0 (.h:205)
    bool second_weapon_use_alt_fire() const;
    bool center_on_player_entry() const; // 0x00A06BA0 (.h:211)
    bool invulnerable() const; // 0x00A06BC0 (.h:214)
    bool gun_of_tharsis() const; // 0x00A06BE0 (.h:217)
    void calculate_aim_parameters(); // 0x00A42040 (:1456)
    void frame_process_pre(); // 0x00AC59F0 (:1738)
    void frame_process_post(); // 0x00AC6150 (:1950)
    static void vehicle_turrets_process_post(); // 0x00A72BC0 (:2557)
    bool has_left_hand_grip_tag() const; // 0x00A41F30 (:1420)
    bool has_right_hand_grip_tag() const; // 0x00A41F70 (:1425)
    vehicle* get_vehicle() const; // 0x00A41FB0 (:1432)
    weapon* get_weapon_inst() const; // 0x00A41FE0 (:1443)
    weapon* get_secondary_weapon_inst() const; // 0x00A42010 (:1448)
    void take_damage(unsigned int damage, const vector* linear_impulse, const vector* angular_impulse); // 0x00ACE0C0 (:1279)
    void kill(const vector* linear_impulse, const vector* angular_impulse); // 0x00AC5840 (:1362)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00AD8D50 (:1326)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00ACE140 (:1310)
    void process_explosion_hit(unsigned int damage, const vector& linear_impulse, const vector& angular_impulse); // 0x00ACE130 (:1305)
    void process_collision(const object* other_obj, float damage, const vector& hit_vel); // 0x00ACE1E0 (:1316)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1F7B0 (:1256)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A72AB0 (:1263)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAFC20 (:696)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AB4940 (:891)
    bool stream_out_common(); // 0x00AB02A0 (:844)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00AB0350 (:1003)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A72870 (:1034)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A96A10 (:1042)
    void snap_pos_and_orient(const vector& update_pos, const matrix& update_orient); // 0x00A41DF0 (:1048)
    virtual unsigned int get_alt_hk_body_handle(unsigned int index) const; // vtbl+0x78 0x00A41E60 (:1066)
    virtual unsigned int get_alt_hk_body_handle_index(unsigned int alt_handle) const; // vtbl+0x7C 0x00A41EC0 (:1093)
    virtual int get_vehicle_attach_tag_index() const; // vtbl+0x84 0x00A41DB0 (.h:282)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00A728A0 (:1132)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00A1F760 (:1155)
    void cycle_tag(int tag_index); // 0x00A72960 (:1168)
    void reset_cycle(int tag_index); // 0x00A729B0 (:1189)
    void play_alt_effect(int tag_index); // 0x00A72A00 (:1210)
    void set_roadblock_flag(); // 0x00A06C00 (.h:291)
    bool is_roadblock_turret() const; // 0x00A06C10 (.h:292)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A969B0 (:980)
    bool process_door_weapon_ready(int tag_index); // 0x00A72A50 (:1232)
private:
    void autoaim(); // 0x00A95FF0 (:205)
    bool get_pivot_point(vector& use_pos) const; // 0x00A41BB0 (:610)
    void handle_turning_sounds(float delta_heading); // 0x00A1F5D0 (:633)
    void set_up_fov_blend_splines(); // 0x00A1F6E0 (:658)
    void vehicle_raycast_check() const;
    turret_physics* get_physics() const; // 0x00A2D3F0 (.h:413)
protected:
    static bool weapon_cb_trigger_press(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle); // 0x00A43C40 (:3319)
    static bool weapon_cb_trigger_release(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle); // 0x00A72E50 (:3340)
    static bool weapon_cb_post_fire(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle); // 0x00A72F70 (:3390)
    static bool weapon_cb_ready_to_fire(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle); // 0x00A73070 (:3441)
public:
    virtual ~turret(); // vtbl+0x00 0x00A41DC0

    const turret_info* t_info; // 0x9C
    unsigned int current_operator; // 0xA0
    unsigned int spawn_node_handle; // 0xA4
    unsigned int first_parent; // 0xA8
    unsigned int linked_to; // 0xAC
    timestamp disable_for_npcs_time; // 0xB0
    timestamp divergence_time; // 0xB4
    vector target_average_velocity; // 0xB8
    multi_sim_state<multi_turret_state>* mp_sim_state; // 0xC4
    float vehicle_heading_offset; // 0xC8
    float walk_blend_left; // 0xCC
    float walk_blend_right; // 0xD0
    turret_flags_struct turret_flags; // 0xD4
    object_stream_resource<resource_dependent_data> rdd; // 0xD8
private:
    unsigned int hit_points; // 0xE0
    int vehicle_attach_tag_index; // 0xE4
    friend class npc; // play_random_cower_animation reads primary_weapon_handle (inferred)
    unsigned int primary_weapon_handle; // 0xE8
    unsigned int secondary_weapon_handle; // 0xEC
    float h_fov_pos; // 0xF0
    float h_fov_neg; // 0xF4
    float v_cur_fov_pos; // 0xF8
    float v_cur_fov_neg; // 0xFC
    bool has_rear_fov_limits; // 0x100
    bool has_side_fov_limits; // 0x101
    float h_current_mul; // 0x104
    float v_current_mul; // 0x108
    vector target_pos; // 0x10C
    vector cam_offset_default; // 0x118
    vector cam_offset_fine_aim; // 0x124
    int turning_alr_id; // 0x130
    catmull_rom_spline_2d v_fov_blend_spline; // 0x134
    bool afp_is_active; // 0x13C
    bool afp_played_prefix_sound; // 0x13D
    float afp_target_heading; // 0x140
    float afp_target_pitch; // 0x144
    int afp_delay_ms; // 0x148
    timestamp afp_delay_timer; // 0x14C
    timestamp auto_return; // 0x150
    timestamp primary_door_timer; // 0x154
    timestamp secondary_door_timer; // 0x158
    timestamp autoaim_keep_target_timer; // 0x15C
    unsigned int autoaim_keep_target_handle; // 0x160
};

// size 0xC
class obj_turret_props : public obj_prop_block
{
public:
    obj_turret_props(const turret_info* t_info, const vector& pos, const matrix& orient, int attach_tag, unsigned int owner_handle); // 0x00A41C70 (:674)
    ~obj_turret_props(); // 0x00A1CF80

};

object* alloc_turret(); // 0x00A72860 (:682)
int bpo_index_turret(); // 0x00A1F720 (:682)
const char* classname_turret(); // 0x00A1F730 (:682)
void types_turret(signed char& type, signed char& subtype); // 0x00A1F710 (:682)
turret* turret_from_item(const item* ip); // 0x00A1FB20 (:3230)
void turret_system_init(); // 0x00A43C00 (:3261)
void turret_level_shutdown(); // 0x00A72DF0 (:3273)
void turret_do_frame_pre(); // 0x00AC6450 (:3284)
void turret_do_frame_post(); // 0x00AC64B0 (:3293)
void validate_turret_destruction(const object* obj); // 0x00A43C30 (:3304)

// 0x005DD650 (.h:62)
inline const object_type turret::get_type_static()
{
    return OT_TURRET;
}

// 0x005DD660 (.h:62)
inline const object_sub_type turret::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06B00 (.h:62)
inline const char* turret::get_classname_static()
{
    return "turret";
}

// 0x00A06B10 (.h:62)
inline int turret::get_bpo_index_static()
{
    return 0;
}

// 0x00A41D90 (.h:62)
inline const char* turret::get_classname() const
{
    return "turret";
}

// 0x005DD670 (.h:79)
inline void turret::get_target_pos(vector& tpos) const
{
    tpos = target_pos;
}

// 0x00951330 (.h:82)
inline void turret::mp_set_target_pos(const vector& tpos)
{
    target_pos = tpos;
}

// 0x00951350 (.h:113)
inline unsigned int turret::get_hit_points() const
{
    return hit_points;
}

// 0x00A41DA0 (.h:119)
inline bool turret::is_killed() const
{
    return hit_points == 0;
}

// 0x005DD690 (.h:122)
inline bool turret::is_vehicle_turret() const
{
    return turret_flags.vehicle_turret;
}

// 0x006C3ED0 (.h:128)
inline bool turret::turret_on_vehicle(unsigned int vehicle_handle) const
{
    return turret_flags.vehicle_turret && get_parent() == vehicle_handle;
}

// 0x00951360 (.h:131)
inline bool turret::autoaim_has_fired() const
{
    return turret_flags.mp_automated_has_fired;
}

// 0x00951370 (.h:132)
inline void turret::autoaim_clear_fired_flag()
{
    turret_flags.mp_automated_has_fired = false;
}

// 0x005DD6A0 (.h:135)
inline bool turret::is_locked() const
{
    return turret_flags.locked_turret;
}

// 0x005DD6B0 (.h:137)
inline bool turret::is_fixed() const
{
    return turret_flags.fixed_turret;
}

// 0x00A06B20 (.h:144)
inline float turret::get_h_fov_neg() const
{
    return h_fov_neg;
}

// 0x00A06B30 (.h:145)
inline float turret::get_h_fov_pos() const
{
    return h_fov_pos;
}

// 0x00A06B40 (.h:146)
inline float turret::get_v_fov_neg() const
{
    return v_cur_fov_neg;
}

// 0x00A06B50 (.h:147)
inline float turret::get_v_fov_pos() const
{
    return v_cur_fov_pos;
}

// 0x005DD6C0 (.h:158)
inline bool turret::is_stuck(int stuck_time) const
{
    return divergence_time.valid() && stuck_time <= divergence_time.time_since();
}

// 0x00A06B60 (.h:187)
inline bool turret::is_automated_turret() const
{
    return t_info->flags.automated;
}

// 0x00A06B70 (.h:190)
inline bool turret::is_stationary_operator_turret() const
{
    return t_info->flags.stationary_operator;
}

// 0x00A06B80 (.h:193)
inline bool turret::is_manned_vehicle_turret() const
{
    return t_info->flags.manned_vehicle_turret;
}

// 0x007F2520 (.h:196)
inline bool turret::no_operator_animation() const
{
    return t_info->flags.no_operator_animation;
}

// 0x00BD9140 (.h:199)
inline bool turret::limited_ammo_primary() const
{
    return t_info->flags.limited_ammo_primary;
}

// 0x00BD9160 (.h:202)
inline bool turret::limited_ammo_secondary() const
{
    return t_info->flags.limited_ammo_secondary;
}

// 0x006C3EF0 (.h:205)
inline bool turret::force_fine_aim() const
{
    return t_info->flags.force_fine_aim;
}

// 0x00A06BA0 (.h:211)
inline bool turret::center_on_player_entry() const
{
    return t_info->flags.center_on_player_entry;
}

// 0x00A06BC0 (.h:214)
inline bool turret::invulnerable() const
{
    return t_info->flags.invulnerable;
}

// 0x00A06BE0 (.h:217)
inline bool turret::gun_of_tharsis() const
{
    return t_info->flags.gun_of_tharsis;
}

// 0x00A41DB0 (.h:282)
inline int turret::get_vehicle_attach_tag_index() const
{
    return vehicle_attach_tag_index;
}

// 0x00A06C00 (.h:291)
inline void turret::set_roadblock_flag()
{
    turret_flags.roadblock_turret = true;
}

// 0x00A06C10 (.h:292)
inline bool turret::is_roadblock_turret() const
{
    return turret_flags.roadblock_turret;
}

// 0x00A06C20 (.h:347)
inline turret::resource_dependent_data::resource_dependent_data()
    : physics(nullptr)
{
}

// 0x00A2D3F0 (.h:413)
inline turret_physics* turret::get_physics() const
{
    return rdd.is_loaded() ? rdd.get_data()->physics : nullptr;
}
