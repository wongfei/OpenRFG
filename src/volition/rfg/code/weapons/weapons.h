// Original: volition/rfg/code/weapons/weapons.h
// Implementation: volition/rfg/code/weapons/weapons.cpp
//
// weapon_info, the table entry of a weapon class (weapons.xtbl, weapons_read_table in weapons_table_parser.cpp), and
// weapon, the weapon object (an item held by a human or mounted on a turret).
// - weapon_info: classification, the item it uses (weapon_obj_item_info) and its inventory entry, effects, sounds,
//   ranges, ammo, spread, damage scaling, and the projectile parameters of thrown and launched weapons. Lookup by name,
//   crc, handle, item info or unique id (weapons_get_weapon_info).
// - weapon_fire_info: one shot (owner, weapon, muzzle position and direction, target, flags), passed to the hit
//   callbacks (object::process_bullet_hit).
// - weapon: ammo, fire timing, the owner, effects; the arc welder caches its hit (arc_welder_cache_info).
// Placement of the small types inferred.
#pragma once

#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/weapons/weapons_enums.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct aim_drift_profile;
struct camera_shake;
struct collision_event;
struct explosion_info;
class ai_combat_action_parameters;
class human;
class npc;
class player;
class turret;
struct inv_item_info;
class material_effect_parameters;
class matrix;
class obj_prop_block;
class object;
class projectile;
class rl_renderer;
class weapon;
// Indices of weapon::cb (weapon::set_callback): turrets and scripted weapons hook the trigger and firing. Placement
// inferred.
enum weapon_callback_idx
{
    INVALID_WEAPON_CB_IDX = -1,
    WEAPON_CB_IDX_PRESS_TRIGGER = 0,
    WEAPON_CB_IDX_RELEASE_TRIGGER = 1,
    WEAPON_CB_IDX_READY_TO_FIRE = 2,
    WEAPON_CB_IDX_CAN_FIRE = 3,
    WEAPON_CB_IDX_POST_FIRE = 4,
    NUM_WEAPON_CB_IDX = 5,
};
// The damage table column of a target (weapons_get_damage_category). Placement inferred.
enum weapon_damage_category
{
    WEAPON_DAMAGE_CATEGORY_NPC = 0,
    WEAPON_DAMAGE_CATEGORY_PLAYER = 1,
    WEAPON_DAMAGE_CATEGORY_VEHICLE = 2,
    WEAPON_DAMAGE_CATEGORY_PLAYER_VEHICLE = 3,
    NUM_WEAPON_DAMAGE_CATEGORIES = 4,
};
struct weapon_firing_pattern;
enum weapon_range_categories;
enum weapon_tracer_type;

// size 0x3
struct weapon_info_flags
{
    bool armor_piercing : 1; // 0x00:0
    bool can_fine_aim : 1; // 0x00:1
    bool can_scope : 1; // 0x00:2
    bool shatter : 1; // 0x00:3
    bool held_in_left_hand : 1; // 0x00:4
    bool draw_proj_in_opposite_hand : 1; // 0x00:5
    bool silent_bullets : 1; // 0x00:6
    bool penetrating_bullets : 1; // 0x00:7
    bool non_inventory : 1; // 0x01:0
    bool use_even_spread : 1; // 0x01:1
    bool disable_player_cover : 1; // 0x01:2
    bool mp_selectable : 1; // 0x01:3
    bool is_obvious_weapon : 1; // 0x01:4
    bool auto_aim_curved_trail : 1; // 0x01:5
    bool looping_effects : 1; // 0x01:6
    bool never_in_cabinet : 1; // 0x01:7
    bool use_secondary_weapon_heat : 1; // 0x02:0
    bool dont_drop_on_death : 1; // 0x02:1
};

// size 0x14
struct damage_scaling_info
{
    float scale_value; // 0x00
    float damage[4]; // 0x04
};

// size 0x230
struct weapon_info
{
    bool is_firearm() const; // 0x005DC660 (.h:344)
    bool is_charge_weapon() const; // 0x005DC690 (.h:345)
    bool is_melee_weapon() const; // 0x005DC6A0 (.h:346)
    bool is_thrown_weapon() const; // 0x005DC6B0 (.h:347)
    bool is_repair_tool() const; // 0x00942EE0 (.h:348)
    bool is_explosive() const; // 0x00BD97B0 (:667)

    char* name; // 0x00
    checksum_stri name_crc; // 0x04
    int unique_id; // 0x08
    weapon_info_flags flags; // 0x0C
    weapon_class_type weapon_class; // 0x10
    item_info* weapon_obj_item_info; // 0x14
    inv_item_info* weapon_inv_item_info; // 0x18
    human_teams default_team; // 0x1C
    char* icon_name; // 0x20
    char* small_icon_name; // 0x24
    char* reticule_name; // 0x28
    char* fine_aim_reticule_name; // 0x2C
    char* mp_kill_phrase; // 0x30
    animation_group weapon_anim_group; // 0x34
    unsigned int muzzle_flash_effect; // 0x38
    unsigned int muzzle_smoke_effect; // 0x3C
    unsigned int special_hit_effect; // 0x40
    unsigned int special_effect; // 0x44
    unsigned int secondary_special_effect; // 0x48
    unsigned int overheated_effect; // 0x4C
    unsigned int tracer_effect; // 0x50
    camera_shake* fire_camera_shake; // 0x54
    bool fire_camera_shake_ignore_disabled; // 0x58
    camera_shake* secondary_camera_shake; // 0x5C
    camera_shake* player_hit_camera_shake; // 0x60
    char attachment_point[32]; // 0x64
    int fire_sound; // 0x84
    int secondary_sound; // 0x88
    int upgrade_sound; // 0x8C
    int reload_sound; // 0x90
    int reload_sound_delay; // 0x94
    int no_ammo_sound; // 0x98
    int special_sound; // 0x9C
    int flyby_sound; // 0xA0
    int num_weapon_personas; // 0xA4
    int npc_fire_sounds[16]; // 0xA8
    float max_range; // 0xE8
    float red_range; // 0xEC
    float max_engagement_dist; // 0xF0
    float min_engagement_dist; // 0xF4
    float max_ai_penetrating_dist; // 0xF8
    const weapon_firing_pattern* npc_firing_pattern; // 0xFC
    const aim_drift_profile* npc_aim_drift_profile; // 0x100
    weapon_trigger_type trigger_type; // 0x104
    weapon_ammo_type ammo_type; // 0x108
    unsigned short magazine_size; // 0x10C
    unsigned short magazine_start_num; // 0x10E
    unsigned short max_rounds; // 0x110
    unsigned short max_rounds_upgrade; // 0x112
    unsigned short ammo_box_restock; // 0x114
    unsigned short to_min_spread; // 0x116
    unsigned short to_max_spread; // 0x118
    unsigned char melee_group_index; // 0x11A
    unsigned char bullet_group_index; // 0x11B
    unsigned char tracer_frequency; // 0x11C
    unsigned char shots_per_round; // 0x11D
    float firing_sound_radius; // 0x120
    float npc_refire_delay; // 0x124
    float default_refire_delay; // 0x128
    float prefire_delay; // 0x12C
    int default_reload_delay; // 0x130
    damage_scaling_info low_scale_damage; // 0x134
    damage_scaling_info high_scale_damage; // 0x148
    const explosion_info* m_explosion_info; // 0x15C
    const explosion_info* m_ai_explosion_info; // 0x160
    float fire_cone_dot; // 0x164
    float even_spread_accuracy_dot; // 0x168
    float max_spread; // 0x16C
    float min_spread; // 0x170
    float fine_aim_max_spread; // 0x174
    float fine_aim_min_spread; // 0x178
    float npc_max_spread; // 0x17C
    float npc_min_spread; // 0x180
    float spread_multiplier_run; // 0x184
    float ragdoll_force_shoot; // 0x188
    float ragdoll_chance; // 0x18C
    float recoil_camera_kick; // 0x190
    float recoil_impulse; // 0x194
    int out_of_ammo_reload_delay; // 0x198
    float overheat_cool_down_time; // 0x19C
    float overheat_percent_per_shot; // 0x1A0
    float dropped_ammo_scale; // 0x1A4
    float bullet_hole_scale; // 0x1A8
    float headshot_multiplier; // 0x1AC
    float zoom_magnification; // 0x1B0
    float autoaim_override; // 0x1B4
    float npc_autoaim; // 0x1B8
    float aim_assist; // 0x1BC
    float player_move_speed_multiplier; // 0x1C0
    float npc_move_speed_multiplier; // 0x1C4
    float alert_multiplier; // 0x1C8
    struct
    {
        item_info* obj_item_info; // 0x00
        float start_speed; // 0x04
        float max_speed; // 0x08
        float acceleration; // 0x0C
        unsigned short fuse_time; // 0x10
        float max_throw_dist; // 0x14
        float gravity; // 0x18
        int sound; // 0x1C
        unsigned int effect1; // 0x20
        unsigned int effect2; // 0x24
        unsigned int effect3; // 0x28
        unsigned int effect4; // 0x2C
        unsigned int flags; // 0x30
        float inaccurate_flight; // 0x34
        float time_until_propelled; // 0x38
        float time_until_prop_expire; // 0x3C
        float time_until_drop; // 0x40
        unsigned int damage_effect; // 0x44
        camera_shake* hit_camera_shake; // 0x48
    } projectile_info; // 0x1CC
    human_melee_combat_move_id standing_primary_melee_attack; // 0x218
    human_melee_combat_move_id standing_secondary_melee_attack; // 0x21C
    human_melee_combat_move_id standing_tertiary_melee_attack; // 0x220
    human_melee_combat_move_id crouching_primary_melee_attack; // 0x224
    human_melee_combat_move_id crouching_secondary_melee_attack; // 0x228
    human_melee_combat_move_id crouching_tertiary_melee_attack; // 0x22C
};

// size 0x1
struct mp_fire_info_flags
{
    void clear(); // 0x00942EF0 (.h:366)

    union
    {
        struct
        {
            unsigned char local_fire : 1; // 0x00:0
            unsigned char trigger_single : 1; // 0x00:1
            unsigned char hit_player : 1; // 0x00:2
            unsigned char from_server : 1; // 0x00:3
            unsigned char sim_bullet : 1; // 0x00:4
            unsigned char send_packet : 1; // 0x00:5
        };
        unsigned char flag_data; // 0x00
    };
};

// size 0x54
struct weapon_fire_info
{
    weapon_fire_info(); // 0x005DC700 (.h:396)
    weapon_fire_info(const vector& start_pos_in, const vector& end_pos_in, const weapon_info* w_info_in); // 0x005DC6C0 (.h:391)
    void init(const vector& start_pos_in, const vector& end_pos_in, const weapon_info* w_info_in); // 0x00BD97E0 (:679)

    vector start_pos; // 0x00
    vector end_pos; // 0x0C
    unsigned int hit_handle; // 0x18
    vector hit_pos; // 0x1C
    vector hit_normal; // 0x28
    unsigned int hit_shape_key; // 0x34
    unsigned int hit_alt_body_idx; // 0x38
    int hit_bone; // 0x3C
    unsigned int nearest_human; // 0x40
    const weapon_info* w_info; // 0x44
    unsigned char hit_physical_material; // 0x48
    unsigned char hit_effect_material; // 0x49
    object_hit_location hit_location; // 0x4C
    mp_fire_info_flags mp_flags; // 0x50
};

enum electricity_hit_rating
{
    ELECTRICITY_HIT_RATING_NONE = 0,
    ELECTRICITY_HIT_RATING_ALLIED_HUMAN = 1,
    ELECTRICITY_HIT_RATING_WORLD = 2,
    ELECTRICITY_HIT_RATING_FRIENDLY_HUMAN = 3,
    ELECTRICITY_HIT_RATING_OBJECT = 4,
    ELECTRICITY_HIT_RATING_DEAD_HUMAN = 5,
    ELECTRICITY_HIT_RATING_HUMAN = 6,
    ELECTRICITY_HIT_RATING_ELECTRIFIABLE_OBJECT = 7,
    ELECTRICITY_HIT_RATING_VEHICLE = 8,
    ELECTRICITY_HIT_RATING_UNFRIENDLY_HUMAN = 9,
    ELECTRICITY_HIT_RATING_HOSTILE_HUMAN = 10,
    NUM_ELECTRICITY_HIT_RATINGS = 11,
};

// size 0x24
struct arc_welder_cache_info
{
    arc_welder_cache_info(); // 0x00BDB0D0

    vector hit_pos; // 0x00
    vector hit_normal; // 0x0C
    unsigned int hit_handle; // 0x18
    unsigned int hit_shape_key; // 0x1C
    electricity_hit_rating hit_rating; // 0x20
};

// size 0x8
struct weapon_callbacks
{
    bool (*func)(weapon_callback_idx, weapon*, unsigned int); // 0x00
    unsigned int user_handle; // 0x04
};

// size 0x2FC
class weapon : public item
{
public:
    // size 0x1
    struct weapon_flags
    {
        bool fire_info_is_new : 1; // 0x00:0
        bool trigger_down : 1; // 0x00:1
        bool is_linked : 1; // 0x00:2
        bool delay_fire : 1; // 0x00:3
        bool processed_since_last_trigger_pull : 1; // 0x00:4
        bool played_dry_fire : 1; // 0x00:5
        bool playing_pickup_effect : 1; // 0x00:6
    };

    weapon(); // 0x00BE0F00 (:383)
    static const object_type get_type_static(); // 0x005DC720 (.h:415)
    static const object_sub_type get_subtype_static(); // 0x005DC730 (.h:415)
    static const char* get_classname_static(); // 0x00BD90D0 (.h:415)
    static int get_bpo_index_static(); // 0x00BD90E0 (.h:415)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00BE0F80 (.h:415)
    static unsigned int get_classname_hash(); // 0x00BD97A0 (:382)
    static int pool_remaining(); // 0x00BDB820 (:382)
    virtual void pool_free(); // vtbl+0x80 0x00BE4F00 (:382)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00BE5030 (:698)
    virtual bool stream(bool load); // vtbl+0x1C 0x00BDBAD0 (:928)
    virtual bool is_owned() const; // vtbl+0x84 0x00BD98F0 (:1083)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_infop); // vtbl+0x68 0x00BE2E50 (:5074)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_infop); // vtbl+0x6C 0x00BE2E60 (:5079)
    virtual void update_render(rl_renderer* pRenderer); // vtbl+0x44 0x00BDD030 (:5093)
    virtual void precache_audio_cues(); // vtbl+0x90 0x00BDA080 (:5102)
private:
    bool do_callback(weapon_callback_idx cb_idx) const; // 0x00BD9820 (:1011)
    bool has_callback(weapon_callback_idx cb_idx) const; // 0x00BD9850 (:1028)
public:
    void set_callback(weapon_callback_idx cb_idx, bool (*func)(weapon_callback_idx, weapon*, unsigned int), unsigned int user_handle); // 0x00BD9870 (:1039)
    bool get_callback(weapon_callback_idx cb_idx, bool (** funcptr)(weapon_callback_idx, weapon*, unsigned int), unsigned int* handleptr); // 0x00BD9890 (:1051)
    void clear_callback(weapon_callback_idx cb_idx); // 0x00BD98D0 (:1072)
    bool ready_to_fire() const; // 0x00BE2670 (:2700)
private:
    void get_spread(float& min_spread, float& max_spread) const; // 0x00BDCB10 (:3122)
public:
    float get_spread() const;
    const weapon_fire_info& get_fire_info() const;
    const weapon_fire_info* get_new_fire_info(); // 0x00BD9BA0 (:2262)
    void reset_fire_delay(); // 0x005DC740 (.h:479)
    bool do_one_shot_delay(); // 0x00BD9BC0 (:2716)
    int time_since_fired() const; // 0x00BD9C00 (:2947)
    weapon_range_categories get_range_category(float distance) const; // 0x00BD9C50 (:2961)
    bool in_range(float distance) const; // 0x005DC750 (.h:486)
    bool in_optimal_range(float) const;
    bool is_queueing_multi_rockets() const; // 0x00884570 (.h:489)
    bool can_fire() const; // 0x00BE26D0 (:2839)
    bool is_overheated() const; // 0x008845A0 (.h:492)
    bool played_dry_fire() const; // 0x00A05E90 (.h:493)
    float get_overheat_percent() const; // 0x008845B0 (.h:494)
    bool set_overheat_percent(float new_percent); // 0x00BE2740 (:2871)
    float get_weapon_info_refire_delay() const; // 0x00BDC9E0 (:2897)
    virtual projectile* launch_thrown_weapon(const vector& start_pos, const matrix& start_orient, const vector& start_velocity); // vtbl+0x94 0x00BE79A0 (:2574)
    void start_reload(int delay_time); // 0x00BDC910 (:2814)
    void cancel_reload(); // 0x00BD9BF0 (:2914)
    void do_reload(); // 0x00BE2850 (:2919)
    bool is_reloading() const; // 0x005DC770 (.h:508)
    bool can_reload() const; // 0x006FB830 (.h:509)
    bool uses_ammo() const; // 0x00BD9930 (:1094)
    bool has_ammo() const; // 0x00A2C6C0 (.h:514)
    bool has_unlimited_ammo() const; // 0x00BDBB90 (:1108)
    bool has_unlimited_magazine_ammo() const; // 0x00BDBC10 (:1133)
    int get_rounds_in_magazine() const; // 0x005DC780 (.h:517)
    int get_rounds_in_reserve() const; // 0x005DC790 (.h:518)
    int get_rounds_total() const; // 0x006F69C0 (.h:519)
    bool is_loaded() const; // 0x005DC7A0 (.h:520)
    bool is_fully_loaded() const; // 0x005DC7E0 (.h:521)
    void add_rounds_to_reserve(int rounds); // 0x00A518B0 (.h:522)
    void fill_ammo(); // 0x00BE6D20 (:1209)
    int add_ammo(int rounds); // 0x00BE56B0 (:1217)
    void fill_magazine(); // 0x00A51900 (.h:525)
    void adjust_dropped_ammo(); // 0x00BE15F0 (:1255)
    bool process_multi_rocket_trigger_down(); // 0x00BEC0F0 (:4446)
    unsigned int get_clip_handle(); // 0x00BD9FA0 (:4486)
    int get_magazine_size() const; // 0x00BDCEE0 (:5029)
    int get_max_rounds() const; // 0x00BDCF60 (:5043)
    void set_rounds_in_reserve(int rounds); // 0x00769D20 (.h:536)
    void set_rounds_in_magazine(int rounds); // 0x005FB5F0 (.h:541)
    void get_muzzle_transform(vector& muzzle_pos, matrix* muzzle_orient) const; // 0x00BDCA70 (:2974)
    void get_adjusted_muzzle_transform(vector& adjusted_muzzle_pos, vector& adjusted_muzzle_fvec, vector& muzzle_pos) const; // 0x00BEAAF0 (:2997)
    int get_grip_tag_index() const; // 0x00A05EA0 (.h:551)
    int get_secondary_grip_tag_index() const; // 0x00A05EB0 (.h:552)
    int get_destruction_tag_index() const; // 0x00A05EC0 (.h:553)
    int get_secondary_destruction_tag_index() const; // 0x00A05ED0 (.h:554)
    int get_muzzle_tag_index() const; // 0x00A05EE0 (.h:555)
    int get_melee_blood_effect_tag_index() const;
    void equip(); // 0x00BD9EC0 (:3963)
    void equip_non_inventory_item(const human* hp); // 0x00BD9ED0 (:3969)
    void unequip(); // 0x00BDCC80 (:3977)
    void collision_damage(const collision_event* event, object* other_obj, float damage_multiplier); // 0x00BDD060 (:5129)
    void collide(const collision_event* event, object* other_obj); // 0x00BE2EB0 (:5202)
    void explode(); // 0x00BDD240 (:5305)
    bool is_trigger_pulled() const; // 0x005DC810 (.h:572)
    void set_trigger_pulled(bool pulled); // 0x00BD9F00 (:4429)
    bool charge_grinder(bool charge_up); // 0x00BE4940 (grinder.cpp:55)
    void stop_all_grinder_effects(); // 0x00BDB6C0 (grinder.cpp:203)
    int get_num_rounds_fired() const; // 0x005DC820 (.h:576)
    bool is_shot_delayed() const; // 0x006F69D0 (.h:577)
    void set_shot_delayed(bool delayed); // 0x006F69E0 (.h:578)
    void set_dry_fire_played(bool played); // 0x00A05EF0 (.h:579)
    bool is_playing_pickup_effect(); // 0x00BD90F0 (.h:580)
    void set_playing_pickup_effect(bool playing); // 0x00BD9100 (.h:581)
    void set_linked(const human* operator_hp); // 0x00BDA010 (:4589)
    bool is_linked() const; // 0x00A05F10 (.h:585)
    bool should_play_visual_effects() const; // 0x00BD9FE0 (:4502)
    void play_single_fire_effects(); // 0x00BE65A0 (:4600)
    void start_looping_effects(); // 0x00BE2B50 (:4754)
    void end_looping_effects(bool hard_stop); // 0x00BDCE60 (:4823)
    void play_individual_fire_effects(); // 0x00BE8CF0 (:4512)
    void do_frame_pre(); // 0x00BEC530 (:4849)
    void do_frame_post(); // 0x00BE6790 (:4872)
    void do_frame_pickup_effect_post(); // 0x00BE2D20 (:4995)
    void fire(vector aim_override); // 0x00BEB270 (:4220)
    bool can_play_muzzle_flash() const; // 0x00BDCDF0 (:4683)
    bool can_unequip() const; // 0x00BDA040 (:4697)
    float get_grinder_speed_percent() const; // 0x008845C0 (.h:600)
    void process_grinder_effects(bool charge_up); // 0x00BE0B80 (grinder.cpp:221)
    void grinder_update_render(); // 0x00BDB730 (grinder.cpp:304)
    int get_weapon_persona(); // 0x00A05F20 (.h:604)
    void clear_weapon_persona(); // 0x00A05F30 (.h:605)
    void set_new_weapon_persona(unsigned int squad_handle); // 0x00BE2980 (:4706)
    void fire_repair(const vector& repair_pos); // 0x00BE2310 (:2429)
    void fire_flamethrower(const vector& flame_pos); // 0x00BE24E0 (:2485)
private:
    bool bullet_autoaim(const human* owner, bool& bullet_hit); // 0x00BE9DF0 (:1318)
    void fire_bullet(const vector& start, const vector& end, const vector& muzzle_pos); // 0x00BEA0C0 (:1751)
    void fire_penetrating_bullet(const vector& start, const vector& end, const vector& muzzle_pos); // 0x00BE6D40 (:1947)
    void fire_projectile(const vector& start, const vector& end); // 0x00BE1C30 (:2271)
    void fire_electricity(const vector& start_pos, const vector& end_pos); // 0x00BE0970 (arc_welder.cpp:310)
    void create_curved_tracers(const human* hp, const vector& muzzle_pos, const vector& end_pos, weapon_tracer_type tracer_type); // 0x00BDC710 (:1638)
    void create_tracers(const human* hp, const vector& muzzle_pos, const vector& end_pos, weapon_tracer_type tracer_type); // 0x00BDC830 (:1679)
    void lock_sounds(bool);
    void process_arc_welder(); // 0x00BE96E0 (arc_welder.cpp:752)
    void arc_welder_stop_sounds(); // 0x00BD9510 (arc_welder.cpp:740)
    void process_repair(); // 0x00BEC320 (:5423)
    void process_flame(); // 0x00BDD520 (:5477)
    void pseudoseek_find_custom_accel(vector&, const vector&, const vector&, const vector&);
protected:
    virtual void free_this(); // vtbl+0x28 0x00BE1480 (:972)
public:
    virtual ~weapon(); // vtbl+0x00 0x00BE0F90

    weapon* prev; // 0x140
    weapon* next; // 0x144
    const weapon_info* w_info; // 0x148
    unsigned int turret_handle; // 0x14C
    timestamp weapon_fire_sound_timestamp; // 0x150
    timestamp weapon_hit_sound_timestamp; // 0x154
    timestamp one_shot_delay_timestamp; // 0x158
    int melee_combat_move_past; // 0x15C
    bool melee_combat_in_combo; // 0x160
    timestamp arc_welder_post_arc_timer; // 0x164
    timestamp multi_rocket_trigger_down_timer; // 0x168
    int secondary_rockets_to_fire; // 0x16C
    unsigned int mat_fx_handle[4]; // 0x170
    unsigned int current_mat_fx; // 0x180
    unsigned int mat_fx_clip_handle; // 0x184
    unsigned int mat_fx_handle_for_clip; // 0x188
    // control_gameplay.cpp reads and clears delay_fire (friends are not recorded in rfg.pdb)
    friend void control_gameplay_process();
    friend void player_cover_do_frame(player* pp); // reads trigger_down (inferred)
    friend void player_process_cover_movement(player* pp, float h, float v); // clears delay_fire (inferred)
    // multi_game_player.cpp's apply_weapon_state (a file static) clears delay_fire too (port-only access struct)
    friend struct multi_game_player_weapon_access;
private:
    weapon_flags flags; // 0x18C
    weapon_callbacks cb[5]; // 0x190
    weapon_fire_info fire_info; // 0x1B8
    int rounds_in_magazine; // 0x20C
    int rounds_in_reserve; // 0x210
    float current_spread; // 0x214
    unsigned char tracer_count; // 0x218
    friend class attack_action; // reads the rounds (inferred)
    friend bool ai_combat_fire_burst(npc* np, const weapon* wp, const weapon_firing_pattern* wfpp, float eff_range_pct, bool secondary_weapon); // (inferred)
    friend float get_effective_range_percent(const npc* np, float distance, const weapon* wp, const human* target, const weapon_firing_pattern* wfpp, bool do_adjustments); // (inferred)
    friend bool ai_combat_do_reload(npc* np); // (inferred)
    friend bool ai_combat_fire_at_target(const ai_combat_action_parameters& params, bool ignore_range); // (inferred)
    friend void ai_combat_fire_turret_at_target(npc* np, turret* tp, const human* target_hp, bool alternate_fire); // (inferred)
    friend class turret_destroy_cover_action; // do_action clears the refire delay (inferred)
    friend void ai_combat_fire_turret_at_pos(npc* np, turret* tp, const vector& pos, bool alternate_fire); // (inferred)
    float refire_delay_time; // 0x21C
    float overheat_percent; // 0x220
    bool overheated; // 0x224
    unsigned int overheated_effect_handle; // 0x228
    timestamp reload_delay_timer; // 0x22C
    timestamp last_fired_timer; // 0x230
    float muzzle_flash_intensity; // 0x234
    unsigned int muzzle_flash_effect_handle; // 0x238
    unsigned int muzzle_smoke_effect_handle; // 0x23C
    timestamp muzzle_smoke_timestamp; // 0x240
    int grip_tag_index; // 0x244
    int secondary_grip_tag_index; // 0x248
    int destruction_tag_index; // 0x24C
    int secondary_destruction_tag_index; // 0x250
    int muzzle_tag_index; // 0x254
    int melee_blood_effect_tag_index; // 0x258
    int num_rounds_fired; // 0x25C
    int fire_sound_alr_id; // 0x260
    int secondary_sound_id; // 0x264
    int special_sound_id; // 0x268
    int weapon_persona; // 0x26C
    float grinder_speed_percent; // 0x270
    float grinder_heat_effect; // 0x274
    unsigned int special_effect_handle; // 0x278
    unsigned int secondary_special_effect_handle; // 0x27C
    farray<arc_welder_cache_info,3> arc_welder_best_point; // 0x280
};

// size 0x1E8
struct bullet_callback_info
{
    bullet_callback_info(const object* attacker_); // 0x00BE0EA0 (:162)
    ~bullet_callback_info(); // 0x00BE2970

    const object* attacker; // 0x00
    const human* attacker_hp; // 0x04
    farray<human*,116> hit_humans; // 0x08
};

// size 0xC
class obj_weapon_props : public obj_item_props
{
public:
    obj_weapon_props(const weapon_info* w_info, const item_info* info, const vector& pos, const matrix& orient, obj_item_flags& flags, unsigned int collision_type, float mass_override); // 0x006FB890 (.h:740)
    obj_weapon_props(char* w_info_name, char* item_info_name, const vector& pos, const matrix& orient, obj_item_flags& flags, unsigned int collision_type, float mass_override); // 0x0098E5D0 (.h:729)
    ~obj_weapon_props(); // 0x006F6AA0

};

void unused_dcf_jetter_rocket_delay(); // 0x00BD95A0 (:340)
void types_weapon(signed char& type, signed char& subtype); // 0x00BD9770 (:382)
object* alloc_weapon(); // 0x00BE0EF0 (:382)
int bpo_index_weapon(); // 0x00BD9780 (:382)
const char* classname_weapon(); // 0x00BD9790 (:382)
void propagate_bullet_whizz_sound(const human* attacker, human* target, const vector& sound_loc, const weapon_info* w_info); // 0x00BDB830 (:397)
void do_frame_weapon_material_effect_post(weapon* wp); // 0x00BDB930 (:611)
void weapons_create_bullet_hit_effects(human* attacker, const weapon_fire_info& fire_info, float damage); // 0x00BDBDC0 (:1463)
void weapon_create_curved_tracer_base(const human* hp, const vector& muzzle_pos, const vector& end_pos, const weapon_info* w_info, weapon_tracer_type tracer_type); // 0x00BDC110 (:1602)
void weapon_create_tracer_base(const human* hp, const vector& muzzle_pos, const vector& end_pos, const weapon_info* w_info, weapon_tracer_type tracer_type); // 0x00BDC780 (:1650)
float weapons_get_bullet_damage(const weapon_info* w_info, const object* target, float distance); // 0x00BE1AF0 (:2193)
weapon_damage_category weapons_get_damage_category(const object* target); // 0x00BDC8A0 (:2237)
bool weapon_projectile_throw_collide(const vector& start_pos, const vector& end_pos, projectile* proj); // 0x00BE77D0 (:2534)
extern const char* const Weapon_class_names[17]; // 0x0133AD70
extern const char* const Weapon_ammo_type_names[8]; // 0x0133ADB4
extern int Num_weapon_infos; // 0x03882C94
extern weapon_info* Weapon_infos; // 0x03882C9C

void weapons_system_init(); // 0x00BE2900 (:3267)
void weapons_level_init_post(); // 0x00BD9CC0 (:3292)
unsigned int weapons_get_weapon_info_handle(const char* name); // 0x00BDCB80 (:3308)
unsigned int weapons_get_weapon_info_handle(checksum_stri name_crc); // 0x00BD9CD0 (:3314)
unsigned int weapons_get_weapon_info_handle(const weapon_info* w_info); // 0x00BD9D10 (:3325)
unsigned int weapons_get_first_weapon_info_handle(); // 0x00BD9D40 (:3338)
unsigned int weapons_get_last_weapon_info_handle(); // 0x00BD9D60 (:3347)
unsigned int weapons_get_next_weapon_info_handle(unsigned int handle); // 0x00BD9DA0 (:3356)
unsigned int weapons_get_prev_weapon_info_handle(unsigned int handle); // 0x00BD9DC0 (:3365)
const weapon_info* weapons_get_weapon_info(const char* name); // 0x00BDCBD0 (:3374)
const weapon_info* weapons_get_weapon_info(checksum_stri name_crc); // 0x00BDCC30 (:3379)
const weapon_info* weapons_get_weapon_info(unsigned int handle); // 0x00BD9DE0 (:3384)
const weapon_info* weapons_get_weapon_info(const item_info* weapon_obj_item_info); // 0x00BD9E10 (:3395)
const weapon_info* weapons_get_weapon_info(const inv_item_info* weapon_inv_item_info); // 0x00BD9E40 (:3409)
const weapon_info* weapons_get_weapon_info_from_weapon_unique_id(int weapon_unique_id); // 0x00BD9E70 (:3423)
float weapons_get_projectile_gravity(const weapon_info* w_info); // 0x00BD9EB0 (:3446)
bool weapons_test_bullet(weapon_fire_info& fire_info, const object* attacker, bool ignore_destroyable_shatter, float override_radius, vector muzzle_pos); // 0x00BE7E90 (:3459)
void weapons_test_penetrating_bullet(weapon_fire_info& fire_info, sorted_list<weapon_fire_info,50,float>& hit_list, const object* attacker, bool ignore_destroyable_shatter); // 0x00BE5C80 (:3748)
void cancel_unequip_on_collide(weapon* wp); // 0x00BDD1B0 (:5178)
void weapons_process_bullet_hit(human* attacker, const weapon_fire_info& fire_info, float damage); // 0x00BDD610 (:5504)
void weapons_set_unlimited_ammo(bool unlimited_ammo); // 0x00BDA140 (:5589)
float weapons_get_autoaim_radius(const human* hp); // 0x00BDD920 (:5596)
unsigned int weapons_get_nano_fade_effect(); // 0x00BDA150 (:5628)
int weapons_get_nano_material_effect(bool& invert_render); // 0x00BDA160 (:5633)
unsigned int weapons_get_repair_fade_effect(); // 0x00BDA170 (:5639)
void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal); // 0x00BE3530 (:5645)
void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal); // 0x00BE3CC0 (:5772)
void weapon_start_material_effect(weapon* wp, human* hp_owner, unsigned int effect, material_effect_parameters mat_fx_params); // 0x00BDD9C0 (:5880)
bool weapon_stop_material_effect(weapon* wp); // 0x00BDA180 (:6006)
void weapon_start_pickup_effect(weapon* wp); // 0x00BDDCE0 (:6028)
void weapon_stop_pickup_effect(weapon* wp); // 0x00BDA1F0 (:6061)
void weapons_read_table(bool mp_override, bool refresh, int dlc_id); // 0x00BDF8A0 (weapons_table_parser.cpp:1202)

// 0x005DC6C0 (.h:391): no hit yet
inline weapon_fire_info::weapon_fire_info(const vector& start_pos_in, const vector& end_pos_in, const weapon_info* w_info_in)
    : start_pos(start_pos_in)
    , end_pos(end_pos_in)
    , hit_handle(0xFFFFFFFF)
    , w_info(w_info_in)
{
}

// 0x005DC700 (.h:396)
inline weapon_fire_info::weapon_fire_info()
    : hit_handle(0xFFFFFFFF)
    , w_info(nullptr)
{
}

// 0x00BDB0D0: nothing to construct.
inline arc_welder_cache_info::arc_welder_cache_info()
{
}

// 0x005DC660 (.h:344): every ammo type but thrown and melee.
inline bool weapon_info::is_firearm() const
{
    return ammo_type == WEAPON_AMMO_TYPE_BULLET || ammo_type == WEAPON_AMMO_TYPE_PROJECTILE || ammo_type == WEAPON_AMMO_TYPE_ELECTRICITY || ammo_type == WEAPON_AMMO_TYPE_REPAIR ||
           ammo_type == WEAPON_AMMO_TYPE_BLANKS || ammo_type == WEAPON_AMMO_TYPE_FLAME;
}

// 0x005DC690 (.h:345). As rfg.exe: the same test as is_thrown_weapon.
inline bool weapon_info::is_charge_weapon() const
{
    return ammo_type == WEAPON_AMMO_TYPE_THROWN;
}

// 0x005DC6A0 (.h:346)
inline bool weapon_info::is_melee_weapon() const
{
    return ammo_type == WEAPON_AMMO_TYPE_MELEE;
}

// 0x005DC6B0 (.h:347)
inline bool weapon_info::is_thrown_weapon() const
{
    return ammo_type == WEAPON_AMMO_TYPE_THROWN;
}

// 0x00942EE0 (.h:348)
inline bool weapon_info::is_repair_tool() const
{
    return ammo_type == WEAPON_AMMO_TYPE_REPAIR;
}

// 0x00942EF0 (.h:366)
inline void mp_fire_info_flags::clear()
{
    flag_data = 0;
}

// 0x005DC740 (.h:479)
inline void weapon::reset_fire_delay()
{
    refire_delay_time = 0.0f;
}

// 0x005DC750 (.h:486)
inline bool weapon::in_range(float distance) const
{
    return (int)get_range_category(distance) != 3; // WEAPON_OUT_OF_RANGE (the enum is in ai/combat/combat_common.h)
}

// 0x00884570 (.h:489)
// 0x008845A0 (.h:492), a second copy
inline bool weapon::is_queueing_multi_rockets() const
{
    return multi_rocket_trigger_down_timer.valid() || secondary_rockets_to_fire > 0;
}

// 0x008845B0 (.h:494)

inline float weapon::get_overheat_percent() const
{
    return overheat_percent;
}

// 0x008845C0 (.h:600)

inline float weapon::get_grinder_speed_percent() const
{
    return grinder_speed_percent;
}

inline bool weapon::is_overheated() const
{
    return overheated;
}

// 0x005DC770 (.h:508)
inline bool weapon::is_reloading() const
{
    return reload_delay_timer.valid();
}

// 0x006FB830 (.h:509): not while reloading, with a full magazine, without ammo use or without rounds to load.
inline bool weapon::can_reload() const
{
    if (reload_delay_timer.valid() || rounds_in_magazine == get_magazine_size() || !uses_ammo())
    {
        return false;
    }
    return rounds_in_reserve > 0 || has_unlimited_ammo() || has_unlimited_magazine_ammo();
}

// 0x00A2C6C0 (.h:514)
inline bool weapon::has_ammo() const
{
    return rounds_in_reserve + rounds_in_magazine > 0 || !uses_ammo() || has_unlimited_ammo() || has_unlimited_magazine_ammo();
}

// 0x005DC7E0 (.h:521): a full magazine, or a weapon without ammo
inline bool weapon::is_fully_loaded() const
{
    return rounds_in_magazine == get_magazine_size() || !uses_ammo();
}

// 0x006F69C0 (.h:519)
inline int weapon::get_rounds_total() const
{
    return rounds_in_reserve + rounds_in_magazine;
}

// 0x005DC780 (.h:517)
inline int weapon::get_rounds_in_magazine() const
{
    return rounds_in_magazine;
}

// 0x005DC790 (.h:518)
inline int weapon::get_rounds_in_reserve() const
{
    return rounds_in_reserve;
}

// 0x005DC7A0 (.h:520): rounds in the magazine (thrown weapons: in reserve too), or unlimited magazines.
inline bool weapon::is_loaded() const
{
    if (rounds_in_magazine < 1 && (rounds_in_reserve < 1 || w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN) && !has_unlimited_magazine_ammo())
    {
        return false;
    }
    return true;
}

// 0x00A518B0 (.h:522): the reserve stays within 0 and the maximum minus a magazine.
inline void weapon::add_rounds_to_reserve(int rounds)
{
    rounds_in_reserve = rounds_in_reserve + rounds;
    const int max_rounds = get_max_rounds();
    const int magazine_size = get_magazine_size();
    if (rounds_in_reserve < 0)
    {
        rounds_in_reserve = 0;
        return;
    }
    if (max_rounds - magazine_size < rounds_in_reserve)
    {
        rounds_in_reserve = max_rounds - magazine_size;
    }
}

// 0x00A51900 (.h:525)
inline void weapon::fill_magazine()
{
    rounds_in_magazine = get_magazine_size();
    const int magazine_size = get_magazine_size();
    if (rounds_in_magazine < 0)
    {
        rounds_in_magazine = 0;
        return;
    }
    if (magazine_size < rounds_in_magazine)
    {
        rounds_in_magazine = magazine_size;
    }
}

// 0x00769D20 (.h:536)
inline void weapon::set_rounds_in_reserve(int rounds)
{
    rounds_in_reserve = rounds;
    const int max_rounds = get_max_rounds();
    const int magazine_size = get_magazine_size();
    if (rounds_in_reserve < 0)
    {
        rounds_in_reserve = 0;
        return;
    }
    if (max_rounds - magazine_size < rounds_in_reserve)
    {
        rounds_in_reserve = max_rounds - magazine_size;
    }
}

// 0x005FB5F0 (.h:541)
inline void weapon::set_rounds_in_magazine(int rounds)
{
    rounds_in_magazine = rounds;
    const int magazine_size = get_magazine_size();
    if (rounds_in_magazine < 0)
    {
        rounds_in_magazine = 0;
        return;
    }
    if (magazine_size < rounds_in_magazine)
    {
        rounds_in_magazine = magazine_size;
    }
}

// 0x00A05EA0 (.h:551)
inline int weapon::get_grip_tag_index() const
{
    return grip_tag_index;
}

// 0x00A05EB0 (.h:552)
inline int weapon::get_secondary_grip_tag_index() const
{
    return secondary_grip_tag_index;
}

// 0x00A05EC0 (.h:553)
inline int weapon::get_destruction_tag_index() const
{
    return destruction_tag_index;
}

// 0x00A05ED0 (.h:554)
inline int weapon::get_secondary_destruction_tag_index() const
{
    return secondary_destruction_tag_index;
}

// 0x00A05EE0 (.h:555)
inline int weapon::get_muzzle_tag_index() const
{
    return muzzle_tag_index;
}

// 0x005DC810 (.h:572)
inline bool weapon::is_trigger_pulled() const
{
    return flags.trigger_down;
}

// 0x005DC820 (.h:576)
inline int weapon::get_num_rounds_fired() const
{
    return num_rounds_fired;
}

// 0x00BD90F0 (.h:580)
inline bool weapon::is_playing_pickup_effect()
{
    return flags.playing_pickup_effect;
}

// 0x00BD9100 (.h:581)
inline void weapon::set_playing_pickup_effect(bool playing)
{
    flags.playing_pickup_effect = playing;
}

// 0x00A05F10 (.h:585)
inline bool weapon::is_linked() const
{
    return flags.is_linked;
}

// 0x006F69D0 (.h:577)
inline bool weapon::is_shot_delayed() const
{
    return flags.delay_fire;
}

// 0x006F69E0 (.h:578)
inline void weapon::set_shot_delayed(bool delayed)
{
    flags.delay_fire = delayed;
}

// 0x00A05E90 (.h:493)
inline bool weapon::played_dry_fire() const
{
    return flags.played_dry_fire;
}

// 0x00A05EF0 (.h:579)
inline void weapon::set_dry_fire_played(bool played)
{
    flags.played_dry_fire = played;
}

// 0x00A05F30 (.h:605)
inline void weapon::clear_weapon_persona()
{
    weapon_persona = -1;
}

// 0x00A05F20 (.h:604)
inline int weapon::get_weapon_persona()
{
    return weapon_persona;
}
