// Original: volition/rfg/code/objects/projectile.h
// Implementation: volition/rfg/code/objects/projectile.cpp
//
// projectile, a launched or thrown item (rockets, grenades, mines, charges): flight (ballistic or along a cardinal
// spline), propellant, attachment to what it hits, and the detonation. projectile_info is the launch parameter block
// passed when one is created. projectile_process runs once per frame after the simulation. Placement of projectile_info
// and projectile_flags inferred.
#pragma once

#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct bbox;
struct collision_event;
class hkpRigidBody;
class human;
class matrix;
class obj_prop_block;
class object;
class object_mover;
class rl_renderer;
struct weapon_fire_info;
struct weapon_info;

// size 0x14
struct projectile_info
{
    projectile_info(); // 0x0075C170

    vector launch_direction; // 0x00
    float launch_velocity; // 0x0C
    unsigned int seek_target; // 0x10
};

// size 0x2
struct projectile_flags
{
    projectile_flags(); // 0x005DDDD0 (.h:75)

    unsigned char is_rocket : 1; // 0x00:0
    unsigned char is_attached_backwards : 1; // 0x00:1
    unsigned char propellant_started : 1; // 0x00:2
    unsigned char propellant_expired : 1; // 0x00:3
    unsigned char release_havok_object : 1; // 0x00:4
    unsigned char investigated : 1; // 0x00:5
    unsigned char spotted : 1; // 0x00:6
    unsigned char fading : 1; // 0x00:7
    unsigned char ballistic : 1; // 0x01:0
    unsigned char fired_in_slew_mode : 1; // 0x01:1
    unsigned char no_attach_to_humans : 1; // 0x01:2
    unsigned char spline_based_cloud : 1; // 0x01:3
    unsigned char restart_ballistic : 1; // 0x01:4
};

// size 0x298
class projectile : public item
{
    // detonate_in_air sets off the owner's mid-air projectiles directly (friends are not recorded in rfg.pdb).
    friend bool detonate_in_air(unsigned int owner_handle);
    friend class ai_perception; // notice_projectile reads the age (inferred)
    friend class cartographer; // the map blinks mines by their age (inferred)
    friend void mission_reclaim_charges_and_mines_impl(class player* pp, class weapon* mine_weapon, class weapon* charge_weapon, class weapon* singularity_weapon); // missions.cpp
public:
    projectile(); // 0x00A1E5D0 (:798)
    static const object_type get_type_static(); // 0x005DDDE0 (.h:91)
    static const object_sub_type get_subtype_static(); // 0x005DDDF0 (.h:91)
    static const char* get_classname_static(); // 0x00A06A90 (.h:91)
    static int get_bpo_index_static(); // 0x00A06AA0 (.h:91)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1E680 (.h:91)
    static unsigned int get_classname_hash(); // 0x00A1E5C0 (:797)
    static int pool_remaining(); // 0x00A40F50 (:797)
    virtual void pool_free(); // vtbl+0x80 0x00A95520 (:797)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAE3C0 (:810)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1E690 (.h:167)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AAE730 (:930)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A95530 (:1172)
    virtual void update_render(rl_renderer* pRenderer); // vtbl+0x44 0x00A72290 (:4061)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A95540 (:1210)
public:
    void process(); // 0x00AE52A0 (:1267)
    void process_explosion_hit(const vector* linear_impulse, const vector* angular_impulse, human* attacker); // 0x00AD8030 (:3085)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00AD8060 (:3102)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& melee_w_info); // vtbl+0x6C 0x00AD80D0 (:3126)
    void collide(const collision_event* c_event, object* other_obj); // 0x00AE3F10 (:2605)
    bool allow_collide_human(const object* target_obj); // 0x00A41540 (:2560)
    void render_debug();
    void set_detonate_time(int ms, unsigned int detonator); // 0x00756BE0 (.h:188)
    bool is_detonation_ts_valid() const; // 0x00756C10 (.h:189)
    int detonate_time_since() const; // 0x00A06AB0 (.h:190)
    vector get_start_pos() const; // 0x005DDE00 (.h:191)
    vector get_start_velocity() const; // 0x005DDE30 (.h:192)
    vector get_start_accel() const;
    void set_heat_seeking_target(unsigned int target); // 0x00756C20 (.h:194)
    void set_secondary_rockets_to_fire(int rockets); // 0x00BD9560 (.h:195)
    int get_secondary_rockets_to_fire(); // 0x00BD9570 (.h:196)
    void set_fade_out_time(int ms); // 0x0094CF70 (.h:198)
    bool is_fading_out() const; // 0x0094CF90 (.h:199)
    bool is_fade_out_complete() const; // 0x00A06AC0 (.h:200)
    bool is_ballistic() const; // 0x0094CFA0 (.h:201)
    void set_ballistic(bool is_ballistic); // 0x0094CFB0 (.h:202)
    bool is_thrown_weapon() const; // 0x005DDE60 (.h:204)
    bool is_rocket_weapon() const; // 0x005DDE90 (.h:206)
    bool is_propellant_started() const; // 0x005DDEA0 (.h:207)
    bool is_propellant_expired() const; // 0x005DDEB0 (.h:208)
    bool is_attached() const; // 0x008147A0 (.h:209)
    bool is_parented() const; // 0x0080D330 (.h:210)
    void mp_restart_effect(); // 0x00A1F340 (:4075)
    bool was_investigated() const; // 0x005DDEC0 (.h:215)
    void set_as_investigated(); // 0x00756C30 (.h:218)
    bool was_spotted() const; // 0x005DDED0 (.h:221)
    void set_as_spotted(); // 0x005DDEE0 (.h:224)
    void invalidate_pending_detonation(); // 0x00A06AF0 (.h:227)
    float get_gravity_value();
    int get_age() const; // 0x005DDEF0 (.h:232)
    void enable_havok_object(); // 0x0094CFD0 (.h:233)
    void enable_ballistic();
    void get_ballistic_position_velocity_time(vector& ballistic_position, vector& ballistic_linear_velocity, float time_sec); // 0x00A1EFE0 (:3988)
    void get_ballistic_position_velocity(vector& ballistic_position, vector& ballistic_linear_velocity); // 0x00A41590 (:3997)
    void restart_ballistic_props(); // 0x00A1F220 (:4005)
    void detach(); // 0x00A95CF0 (:3694)
    void mp_handle_collision_attach_effects(); // 0x00A722E0 (:4081)
    bool get_mp_collision_info(const vector& start_posp, const vector& end_pos, object** collide_obj, collision_event& c_event); // 0x00A95820 (:3237)
    bool mid_air_detonate(const vector* detonate_pos, const vector* surface_normal, object* attacker); // 0x00AE45F0 (:2900)
    void attach(object* other_obj, int attach_index, const vector& surface_normal, const vector& surface_pos, int host_alt_body_index, float attach_offset, const matrix* attach_orient_override); // 0x00AAF750 (:3507)
private:
    void rocket_seek_target(float current_speed, vector current_direction); // 0x00AE0600 (:1456)
    void air_bomb_seek_target(float current_speed, vector current_direction); // 0x00A40F80 (:1486)
    void process_mine(); // 0x00A71B10 (:1538)
    void process_charge(); // 0x00AE0A90 (:1678)
    void process_singularity_bomb(); // 0x00AE0B20 (:1702)
    void process_rocket(); // 0x00AE2810 (:1759)
    void process_grinder(); // 0x00A71ED0 (:1839)
    void process_mass_driver(); // 0x00A1E770 (:1899)
    void process_repair(); // 0x00A41480 (:1951)
    void process_fade_out(); // 0x00A1E6B0 (:1247)
    void collision_attach(const collision_event* c_event, object* other_obj, float attach_offset, const matrix* attach_orient_override); // 0x00AD8100 (:3352)
    void collision_bounce(const collision_event* c_event, object* other_obj); // 0x00A1EC30 (:3774)
    void collision_shield_deflect(const collision_event* c_event, item* shield_item); // 0x00A1EC90 (:3789)
    void collision_detonate(const collision_event* c_event, object* other_obj); // 0x00AD8650 (:3842)
    void collision_damage(const collision_event* c_event, object* other_obj); // 0x00AD89D0 (:3917)
    void impaling_collide(const collision_event* c_event, object* other_obj, const vector& human_hit_pos, int human_hit_bone); // 0x00AE2E50 (:2029)
    void mass_driver_collide(const collision_event* c_event, object* other_obj); // 0x00AD7650 (:2362)
    void nano_rifle_collide(const collision_event* c_event, object* other_obj); // 0x00A1E8F0 (:2488)
    void repair_collide(const collision_event* c_event, object* other_obj); // 0x00A956E0 (:2506)
    void remove_dead_harpoon(); // 0x00A1E8D0 (:2017)
    void handle_collision_attach_effects(); // 0x00A72090 (:3742)
    void detonate(const vector* detonate_pos, const vector* surface_normal, object* attacker); // 0x00AD7BF0 (:2919)
    void attach_to_human(human* target, const vector& hit_pos, const vector& hit_normal, int hit_bone_index); // 0x00ACDDC0 (:3604)
    void attach_common(); // 0x00A95BA0 (:3674)
    void detach_effects(bool detach_effect1, bool detach_effect2, bool detach_effect3, bool detach_effect4); // 0x00A41830 (:4024)
    void get_collision_data(const collision_event* c_event, unsigned char* hit_material, material_shape_type* hit_shape_type, unsigned int* hit_shape_key); // 0x00A1E780 (:1972)
    void set_acceleration(const vector& target_accel); // 0x00A1E800 (:1999)
public:
    static void recompute_safe_bp(); // 0x00A1E390 (:99)
    virtual ~projectile(); // vtbl+0x00 0x00A1E6A0

    const weapon_info* w_info; // 0x140
    projectile* p_prev; // 0x144
    projectile* p_next; // 0x148
    unsigned int mp_create_id; // 0x14C
    multi_sim_state<multi_projectile_state>* mp_sim_state; // 0x150
private:
    timestamp age_timer; // 0x154
    vector start_pos; // 0x158
    vector start_velocity; // 0x164
    vector start_accel; // 0x170
    vector target_pos; // 0x17C
    vector current_accel; // 0x188
    vector ballistic_accel; // 0x194
    unsigned int ballistic_ignore_handle; // 0x1A0
    vector previous_velocity; // 0x1A4
    int attach_point_index; // 0x1B0
    int blinking_light_point_index; // 0x1B4
    int sound_info; // 0x1B8
    unsigned int effect_obj_handle; // 0x1BC
    unsigned int effect_obj_handle2; // 0x1C0
    unsigned int effect_obj_handle3; // 0x1C4
    unsigned int effect_obj_handle4; // 0x1C8
    vector inaccuracy_vector; // 0x1CC
    cardinal_spline spline; // 0x1D8
    vector spline_pts[7]; // 0x1E4
    float spline_t; // 0x238
    float spline_length; // 0x23C
    // Private in rfg.pdb; multi_game_mode_common.cpp's static helpers use pflags through fade_stamp (friends are not
    // recorded in rfg.pdb).
public:
    projectile_flags pflags; // 0x240
    unsigned int detonator_handle; // 0x244
    timestamp detonate_stamp; // 0x248
    timestamp active_stamp; // 0x24C
    bool repair_collided_this_frame; // 0x250
    vector repair_collision_point; // 0x254
    signed char repair_passes_left; // 0x260
    bool exploded; // 0x261
    timestamp fade_stamp; // 0x264
private:
    float opacity; // 0x268
    unsigned int heat_seeking_target; // 0x26C
    bool is_primary_rocket; // 0x270
    int secondary_rockets_to_fire; // 0x274
public:
    vector ca_create_pos; // 0x278
    float ca_time_offset_sec; // 0x284
    timestamp ca_time_offset_stamp; // 0x288
    unsigned int client_prediction_net_time; // 0x28C
    unsigned int constraint_handle; // 0x290
    timestamp mp_no_sync_timer; // 0x294
private:
    static bbox proj_safe_broadphase; // 0x03067ABC
};

// size 0xC
class obj_projectile_props : public obj_item_props
{
public:
    obj_projectile_props(unsigned int owner_handle, item_info* info, const obj_item_flags& flags, const weapon_info* w_info, const vector& pos, const matrix& orient, const vector& velocity, const vector& target, const unsigned int create_id); // 0x006058C0 (.h:320)
    ~obj_projectile_props(); // 0x005ECEB0

};

void unused_dcf_reconstructor_passes(); // 0x00A1E470 (:193)
void unused_dcf_detonate_charges(); // 0x00ACDDA0 (:225)
void unused_dcf_detonate_mines(); // 0x00A95370 (:230)
void unused_dcf_remove_explosives(); // 0x00A718D0 (:237)
int count_charges(unsigned int owner_handle); // 0x00A719A0 (:368)
int count_charges(unsigned int owner_handle, int& num_attached); // 0x00A40EF0 (:374)
void projectile_count_charges_proxy_mines(unsigned int owner_handle, int& num_charges, int& num_proxy_mines); // 0x00A95390 (:393)
void projectile_count_charges_proxy_mines(unsigned int owner_handle, int& num_charges, int& num_attached_charges, int& num_proxy_mines); // 0x00A719C0 (:399)
bool detonate_in_air(unsigned int owner_handle); // 0x00AE5200 (:421)
bool projectile_invalidate_pending_charges(); // 0x00A1E530 (:440)
bool detonate_charges(unsigned int owner_handle); // 0x00AC55D0 (:461)
void detonate_mines(unsigned int owner_handle); // 0x00A71A70 (:555)
bool can_fire_charge(unsigned int owner_handle, bool recent_detonation_override); // 0x00A953B0 (:571)
void singularity_bomb_vacuum(projectile* grenade, float dest_pos_x, float dest_pos_y, float dest_pos_z, float vacuum_multiplier); // 0x00AD69F0 (:641)
const char* classname_projectile(); // 0x00A1E5B0 (:797)
int bpo_index_projectile(); // 0x00A1E5A0 (:797)
object* alloc_projectile(); // 0x00A71B00 (:797)
void types_projectile(signed char& type, signed char& subtype); // 0x00A1E590 (:797)
bool projectile_is_valid_mp_collision_target(const object* collision_object); // 0x00A1E9E0 (:2541)
bool projectile_object_mover_impact_stops_projectile(const projectile* proj, object_mover* target, int shape_key, bool& gmover_shatter, bool& rmover_shatter); // 0x00A1EA20 (:3157)
void projectile_collide_fill_out_collision_event(collision_event* c_event, object* obj_a, object* obj_b, const vector& collision_pos, const vector& collision_normal, int shape_key_a, int shape_key_b, const hkpRigidBody* rigid_body_a, const hkpRigidBody* rigid_body_b); // 0x00A1EB10 (:3210)
void projectile_system_init(); // 0x00A1F390 (:4154)
void projectile_level_init_post(); // 0x00A1F3A0 (:4160)
void projectile_level_shutdown(); // 0x00A95DD0 (:4168)
void projectile_process(); // 0x00AE5C60 (:4226)
projectile* projectile_find_by_create_id(unsigned int& create_id); // 0x00A723E0 (:4249)
void validate_projectile_destruction(const object* obj); // 0x00A41910 (:4266)
void projectile_destroy_all(); // 0x00A72430 (:4277)

// 0x008147A0 (.h:209): stuck to something (a constraint or a parent)
inline bool projectile::is_attached() const
{
    return child_constraint_ptr != nullptr || get_parent() != 0xFFFFFFFF;
}

// 0x00BD9560 (.h:195)
inline void projectile::set_secondary_rockets_to_fire(int rockets)
{
    secondary_rockets_to_fire = rockets;
}

// 0x00BD9570 (.h:196)
inline int projectile::get_secondary_rockets_to_fire()
{
    return secondary_rockets_to_fire;
}
