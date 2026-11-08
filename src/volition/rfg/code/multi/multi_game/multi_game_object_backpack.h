// Original: volition/rfg/code/multi/multi_game/multi_game_object_backpack.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp
//
// Multiplayer backpacks (object_backpack): the abilities (jetpack, fleetfoot, rhino, shockwave, thrust, cloak, ...)
// read from the backpack table, attached to a player, with fuel, activation and effects; explosions scale their damage by
// the owner's backpack abilities. Placement of the toggle reasons inferred.
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct camera_shake;
struct effect_info;
struct explosion_info;
class matrix;
class mempool_base;
class multi_object_marker;
class obj_prop_block;
class object;
class human;
class player;
enum player_zoom_state;
struct radial_blur_info;
class rl_renderer;
struct sender;
struct weapon_info;

enum multi_backpack_toggle_reasons
{
    MBTR_OK = 0,
    MBTR_FAILURE = 1,
    MBTR_NON_ACTIVATABLE = 2,
    MBTR_RAMPING_UP = 3,
    MBTR_PLAYER_NON_DEACTIVATABLE = 4,
    MBTR_LOW_FUEL = 5,
    MBTR_RAGDOLLED = 6,
    MBTR_UNATTACHED = 7,
    MBTR_WRECKING_CREW = 8,
    NUM_MULTI_BACKPACK_TOGGLE_REASONS = 9,
};

// size 0x8
struct multi_backpack_effect
{
    const effect_info* effect; // 0x00
    const char* attach_point_name; // 0x04
};

// size 0x70C
struct multi_backpack_info
{
    // size 0xF8
    struct abilities
    {
        const weapon_info* w_info; // 0x00
        float movement_speed_factor; // 0x04
        float damage_taken_factor; // 0x08
        float direct_damage_dealt_factor; // 0x0C
        float area_damage_dealt_factor; // 0x10
        float ammo_factor; // 0x14
        float refire_delay_factor; // 0x18
        float reload_speed_factor; // 0x1C
        bool unlimited_ammo; // 0x20
        float fall_speed_safety_factor; // 0x24
        bool vision; // 0x28
        bool vision_vfx; // 0x29
        int vision_vfx_ramp_in_time_ms; // 0x2C
        int vision_vfx_ramp_out_time_ms; // 0x30
        bool stealth; // 0x34
        float stealth_transparency; // 0x38
        float local_stealth_transparency; // 0x3C
        int stealth_time_ms; // 0x40
        int restealth_time_ms; // 0x44
        bool jump_boost; // 0x48
        float jump_boost_horizontal; // 0x4C
        float jump_boost_vertical; // 0x50
        int jump_boost_timer_ms; // 0x54
        int jump_boost_delay_ms; // 0x58
        const effect_info* jump_boost_downthrust_ground_hit_effect; // 0x5C
        bool shockwave; // 0x60
        const explosion_info* shockwave_explosion_info; // 0x64
        int shockwave_timer_ms; // 0x68
        bool ripple; // 0x6C
        camera_shake* ripple_local_player_camera_shake; // 0x70
        multi_backpack_effect ripple_effect; // 0x74
        int ripple_timer_ms; // 0x7C
        float ripple_range; // 0x80
        float ripple_player_hit_speed; // 0x84
        float ripple_player_minimum_y_velocity; // 0x88
        float ripple_player_damage; // 0x8C
        int ripple_mover_impulse; // 0x90
        bool health_regen; // 0x94
        float health_regen_per_second; // 0x98
        float health_regen_radius; // 0x9C
        bool health_regen_heal_enemies; // 0xA0
        bool repair; // 0xA1
        float repair_offset; // 0xA4
        float repair_radius; // 0xA8
        bool charge; // 0xAC
        float charge_top_speed; // 0xB0
        float charge_ramp_up_time_seconds; // 0xB4
        float charge_ramp_down_time_seconds; // 0xB8
        bool rhino; // 0xBC
        camera_shake* rhino_local_player_camera_shake; // 0xC0
        const effect_info* rhino_mover_hit_effect; // 0xC4
        const effect_info* rhino_player_hit_effect; // 0xC8
        const radial_blur_info* rhino_local_player_hit_radial_blur; // 0xCC
        float rhino_player_hit_damage; // 0xD0
        float rhino_player_hit_speed; // 0xD4
        float rhino_player_minimum_y_velocity; // 0xD8
        const effect_info* additional_bullet_hit_effect_other; // 0xDC
        const effect_info* replacement_bullet_hit_effect_other_player; // 0xE0
        const effect_info* additional_bullet_hit_effect_self; // 0xE4
        const effect_info* replacement_bullet_hit_effect_self; // 0xE8
        const effect_info* bullet_tracer; // 0xEC
        const effect_info* rail_tracer; // 0xF0
        float fov_scale; // 0xF4
    };

    // size 0x22C
    struct effects_alr
    {
        // size 0x10
        struct effects
        {
            const effect_info* attack_effect; // 0x00
            const effect_info* loop_effect; // 0x04
            const effect_info* release_effect; // 0x08
            const char* attach_point_name; // 0x0C
        };

        int sfx_alr; // 0x00
        int sfx_npc_alr; // 0x04
        effects player_effects[8]; // 0x08
        int num_player_effects; // 0x88
        effects backpack_effects[8]; // 0x8C
        int num_backpack_effects; // 0x10C
        effects alternate_backpack_effects[8]; // 0x110
        int num_alternate_backpack_effects; // 0x190
        effects weapon_effects[8]; // 0x194
        int num_weapon_effects; // 0x214
        bool loop_waits_for_attack; // 0x218
        const effect_info* arc_effect; // 0x21C
        const char* arc_backpack_attach_point_name; // 0x220
        const char* arc_weapon_attach_point_name; // 0x224
        float arc_travel_speed; // 0x228
    };

    // size 0x10
    struct ramp_up_effects
    {
        const effect_info* ramp_up_player_effect; // 0x00
        const effect_info* ramp_up_backpack_effect; // 0x04
        const char* ramp_up_effect_attach_point_name; // 0x08
        camera_shake* ramp_up_camera_shake; // 0x0C
    };

    multi_backpack_info(); // 0x0096BC20

    char internal_name[32]; // 0x00
    checksum_stri internal_name_crc; // 0x20
    char display_name[32]; // 0x24
    const char* icon_name; // 0x44
    const char* pickup_tooltip; // 0x48
    item_info* item_infop; // 0x4C
    int unique_id; // 0x50
    unsigned int hint_text_crc; // 0x54
    abilities passive_abilities; // 0x58
    abilities active_abilities; // 0x150
    bool jetpack; // 0x248
    bool kaboom; // 0x249
    const explosion_info* kaboom_explosion_info; // 0x24C
    float kaboom_death_chance; // 0x250
    int kaboom_delay_ms; // 0x254
    bool kaboom_damages_user; // 0x258
    camera_shake* kaboom_camera_shake; // 0x25C
    float kaboom_camera_shake_user_intensity; // 0x260
    float kaboom_camera_shake_falloff; // 0x264
    bool kaboom_camera_shake_ignore_disabled; // 0x268
    effects_alr pickup_effects; // 0x26C
    effects_alr activation_effects; // 0x498
    ramp_up_effects ramp_up; // 0x6C4
    ramp_up_effects alternate_ramp_up; // 0x6D4
    camera_shake* activation_camera_shake; // 0x6E4
    int failed_activation_foley; // 0x6E8
    bool no_fine_aim; // 0x6EC
    bool no_cover; // 0x6ED
    bool activatable; // 0x6EE
    bool deactivatable; // 0x6EF
    bool player_deactivatable; // 0x6F0
    bool uses_fuel; // 0x6F1
    float fuel_use_per_second; // 0x6F4
    float fuel_regen_per_second; // 0x6F8
    float fuel_activation_cost; // 0x6FC
    float minimum_fuel_to_activate; // 0x700
    int fuel_refill_foley; // 0x704
    bool mat_fx_on_backpack; // 0x708
    bool mat_fx_on_human; // 0x709
    bool mat_fx_on_weapon; // 0x70A
};

// size 0xC
class obj_backpack_props : public obj_item_props
{
public:
    obj_backpack_props(const multi_backpack_info* backpack_info, const vector& pos, const matrix& orient, const obj_item_flags& flags); // 0x0097C110 (:120)
    ~obj_backpack_props(); // 0x0097D4A0

};

// size 0x10
struct ripple_hit_info
{
    ripple_hit_info(); // 0x00958320

    object* victim; // 0x00
    vector velocity; // 0x04
};

// size 0x694
class multi_object_backpack : public item
{
    // human.cpp and player.cpp read activated, fuel and backpack_info (friends are not recorded in rfg.pdb)
    friend void human_maybe_play_falling_voice(human* hp);
    friend bool gameplay_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type);
    friend void player_do_frame(player* pp);
    friend void player_update_jetpack(player* pp, bool boost_active);
    friend void player_set_zoom_state(player* pp, player_zoom_state zoom, bool instant_change);
    friend void player_get_max_movement_speeds(player* pp, float& max_movement_speed, float& max_movement_speed_base);
public:
    // size 0x194
    struct effects_alr_handles
    {
        // size 0x10
        struct effects_handles
        {
            int tag_index; // 0x00
            unsigned int host; // 0x04
            unsigned int attack_effect_handle; // 0x08
            unsigned int loop_effect_handle; // 0x0C
        };

        int sfx_alr_handle; // 0x00
        effects_handles player_effects_handles[8]; // 0x04
        effects_handles backpack_effects_handles[8]; // 0x84
        effects_handles weapon_effects_handles[8]; // 0x104
        int arc_backpack_tag_index; // 0x184
        int arc_weapon_tag_index; // 0x188
        unsigned int arc_handle; // 0x18C
        float arc_position; // 0x190
    };

    multi_object_backpack(); // 0x0097CFA0 (:2414)
    static const object_type get_type_static(); // 0x00884550 (.h:218)
    static const object_sub_type get_subtype_static(); // 0x00884560 (.h:218)
    static const char* get_classname_static(); // 0x00941D10 (.h:218)
    static int get_bpo_index_static(); // 0x00941D20 (.h:218)
    virtual const char* get_classname() const; // vtbl+0x0C 0x0097D010 (.h:218)
    static unsigned int get_classname_hash(); // 0x009587B0 (:2413)
    static int pool_remaining(); // 0x0097CF90 (:2413)
    virtual void pool_free(); // vtbl+0x80 0x009B0850 (:2413)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x009B0640 (:1381)
    virtual bool stream(bool load); // vtbl+0x1C 0x009580C0 (:1429)
protected:
    virtual void free_this(); // vtbl+0x28 0x009B07B0 (:1478)
public:
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00958190 (:1463)
    void process(); // 0x009F2A70 (:3321)
    virtual void update_render(rl_renderer* pRenderer); // vtbl+0x44 0x0097D210 (:3853)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x0097D330 (:3893)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x0097D3E0 (:3906)
    const multi_backpack_info* get_backpack_info() const; // 0x006F6AB0 (.h:248)
    const multi_backpack_info::abilities* get_backpack_abilities() const; // 0x006C3F10 (.h:249)
    player* get_player_attached_to() const; // 0x00941D30 (.h:250)
    multi_object_marker* get_backpack_rack() const; // 0x00941D40 (.h:251)
    int get_backpack_rack_index() const; // 0x00941D50 (.h:252)
    int get_backpack_rack_attach_tag_index() const; // 0x00941D60 (.h:253)
    float get_fuel() const; // 0x00A06A10 (.h:254)
    bool is_activated() const; // 0x0075E5B0 (.h:255)
    float get_jump_boost_horizontal() const; // 0x00941D70 (.h:256)
    float get_jump_boost_vertical() const; // 0x00941D80 (.h:257)
    bool get_jump_boost_direction_up() const; // 0x00941D90 (.h:258)
    bool is_ramping_up() const; // 0x0075E5C0 (.h:259)
    bool has_enough_fuel_to_activate() const; // 0x009587D0 (:2419)
    multi_backpack_toggle_reasons can_activate() const; // 0x00958850 (:2435)
    multi_backpack_toggle_reasons can_deactivate() const; // 0x00958930 (:2476)
    void toggle_activation(int activation_delay_ms); // 0x009F84B0 (:2497)
    void set_backpack_rack(multi_object_marker* backpack_rack_in, int backpack_rack_index_in); // 0x0097D050 (:2737)
    bool event_player_touched(player* pp, bool& ask_pickup); // 0x009F8570 (:2747)
    void event_player_picked_up(player* pp, bool server_sync, bool migrating); // 0x009F26A0 (:2823)
    void event_player_dropped(bool server_sync); // 0x009DF480 (:2963)
    void event_player_left(); // 0x009B0860 (:3012)
    void event_player_died(); // 0x009F2810 (:2891)
    void event_player_respawned(); // 0x009F2960 (:2933)
    void event_player_attacked(damaged_by_types attack_type); // 0x009DF420 (:2945)
    void start_ramp_up(int delay_ms); // 0x0097D070 (:3028)
    void process_ramp_up(); // 0x009F2990 (:3068)
    void stop_ramp_up(); // 0x0097D1A0 (:3089)
    bool is_fully_stealthed() const; // 0x00958B50 (:3164)
    bool is_stealth_active() const; // 0x00958BB0 (:3172)
    void attempt_shockwave(const vector& posp); // 0x00958C40 (:3210)
    void attempt_ripple(const vector& posp); // 0x009F2A00 (:3230)
    bool did_rhino_hit_mover_this_run() const; // 0x00941DA0 (.h:314)
    void prepare_type_specific_info_for_toggle_packet(); // 0x00958CB0 (:3251)
    void write_type_specific_info_to_toggle_packet(unsigned char* packet_data, unsigned short& packet_size) const; // 0x00958D00 (:3278)
    void read_type_specific_info_from_toggle_packet(const unsigned char* packet_data, unsigned short& packet_offset); // 0x00958D60 (:3300)
    void process_equip_item(); // 0x0097C8E0 (:1744)
    void remote_charge_change(bool held); // 0x00958210 (:1787)
    const vector* get_player_offset() const; // 0x0097C9C0 (:1814)
private:
    void activate(); // 0x009F2300 (:2515)
    void deactivate(); // 0x0099AAC0 (:2656)
    bool attach_to_player(player* pp); // 0x009581C0 (:1502)
    bool detach_from_player(); // 0x009581E0 (:1512)
    void activate_dual_abilities(); // 0x0097C410 (:1523)
    void deactivate_dual_abilities(); // 0x0097C5E0 (:1569)
    void start_material_effects(bool player_effect_just_stopped); // 0x00999BF0 (:1620)
    void process_material_effects(); // 0x00999DA0 (:1700)
    bool stop_material_effects(); // 0x0097C810 (:1712)
    void use_shockwave(const vector& posp); // 0x009582C0 (:1835)
    void use_ripple(const vector& posp); // 0x009F1B10 (:1849)
    void use_jump_boost(); // 0x00958330 (:1982)
    void use_kaboom(); // 0x00958350 (:1990)
    void stealth_player(); // 0x00958970 (:3113)
    void unstealth_player(); // 0x00958A40 (:3137)
    void reset(); // 0x0099A9A0 (:2367)
    void effects_alr_store_handle_info(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles); // 0x00958590 (:2034)
    void effects_alr_attack(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles); // 0x0097CAC0 (:2066)
    void effects_attack(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles); // 0x00958700 (:2107)
    void effects_alr_loop(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles); // 0x00999DC0 (:2119)
    void effects_loop(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles, bool loop_waits_for_attack); // 0x0097CD10 (:2254)
    void effects_alr_release(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles); // 0x0099A850 (:2284)
    void effects_release(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles); // 0x0097CE40 (:2316)
    void effects_handles_alr_reset(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles); // 0x0097CEF0 (:2342)
    void effects_handles_reset(effects_alr_handles::effects_handles* handles); // 0x00958760 (:2361)
public:
    virtual ~multi_object_backpack(); // vtbl+0x00 0x0097D020

    bool rhino_hit_players[16]; // 0x140
    unsigned int rhino_hit_npcs[64]; // 0x150
    unsigned int num_rhino_hit_npcs; // 0x250
private:
    bool activated; // 0x254
    float fuel_last_frame; // 0x258
    float fuel; // 0x25C
    timestamp_percent fuel_timer; // 0x260
    timestamp_percent activated_time; // 0x268
    const multi_backpack_info* backpack_info; // 0x270
    player* player_attached_to; // 0x274
    multi_object_marker* backpack_rack; // 0x278
    int backpack_rack_index; // 0x27C
    int backpack_rack_attach_tag_index; // 0x280
    timestamp ramp_up_timer; // 0x284
    unsigned int ramp_up_effect_object; // 0x288
    bool in_stealth; // 0x28C
    timestamp_percent stealth_timestamp; // 0x290
    timestamp time_until_restealth; // 0x298
    timestamp jump_boost_timer; // 0x29C
    timestamp jump_boost_delay_timer; // 0x2A0
    float jump_boost_horizontal; // 0x2A4
    float jump_boost_vertical; // 0x2A8
    bool jump_boost_direction_up; // 0x2AC
    bool jump_boost_downthrust_hit_ground; // 0x2AD
    timestamp shockwave_timer; // 0x2B0
    timestamp ripple_timer; // 0x2B4
    timestamp_percent rhino_shape_cutter_timer; // 0x2B8
    timestamp rhino_mover_hit_effect_timer; // 0x2C0
    bool rhino_hit_mover_this_run; // 0x2C4
    float heal_amounts[16]; // 0x2C8
    timestamp kaboom_explosion_delay_timer; // 0x308
    timestamp_percent vision_vfx_timer; // 0x30C
    int fuel_effect_attach_tag_index; // 0x314
    const effect_info* current_fuel_effect; // 0x318
    unsigned int fuel_effect_object; // 0x31C
    bool use_alternate_ramp_up_effects; // 0x320
    bool use_alternate_backpack_activation_effects; // 0x321
    effects_alr_handles pickup_effects_alr_handles; // 0x324
    effects_alr_handles activation_effects_alr_handles; // 0x4B8
    unsigned int mat_fx_handle[16]; // 0x64C
    unsigned int mat_fx_weapon_handle; // 0x68C
    bool mat_fx_succeeded; // 0x690
};

void unused_dcf_drop_backpack(); // 0x009AFE50 (:168)
void unused_dcf_remove_backpack(); // 0x009F1370 (:202)
void unused_dcf_reload_backpack_table(); // 0x009F1430 (:266)
void multi_game_backpack_parse_backpack_table_file(const char* backpack_table_file_name); // 0x009B0630 (:1372)
object* alloc_multi_object_backpack(); // 0x0099AAB0 (:2413)
void types_multi_object_backpack(signed char& type, signed char& subtype); // 0x00958780 (:2413)
const char* classname_multi_object_backpack(); // 0x009587A0 (:2413)
int bpo_index_multi_object_backpack(); // 0x00958790 (:2413)
void multi_game_backpack_system_init(mempool_base& pool); // 0x009DF590 (:3919)
void multi_game_backpack_system_shutdown(); // 0x009DF5E0 (:3947)
void multi_game_backpack_level_init_pre(); // 0x00958DC0 (:3953)
void multi_game_backpack_level_shutdown(); // 0x00958E50 (:3981)
void multi_game_backpack_process(); // 0x009F41A0 (:3990)
void multi_game_backpack_render(); // 0x00958E90 (:3998)
void multi_game_backpack_round_over(); // 0x009F8790 (:4038)
int multi_game_backpack_get_backpack_id(const char* name); // 0x00958EA0 (:4053)
unsigned short multi_game_backpack_get_loaded_backpack_mask(); // 0x00958F00 (:4068)
const char* multi_game_backpack_get_internal_name(int backpack_type); // 0x00958F80 (:4082)
const char* multi_game_backpack_get_display_name(int backpack_type); // 0x00958FA0 (:4093)
const multi_backpack_info* multi_game_backpack_get_info(const char* name); // 0x0097D470 (:4104)
const multi_backpack_info* multi_game_backpack_get_info(int backpack_type); // 0x00958FD0 (:4117)
int multi_game_backpack_get_next_backpack_id(int backpack_id); // 0x00958FF0 (:4129)
int multi_game_backpack_get_prev_backpack_id(int backpack_id); // 0x00959050 (:4153)
multi_object_backpack* multi_game_backpack_create(int backpack_type, const vector& pos, const matrix& orient); // 0x0099ACE0 (:4178)
void player_toggle_backpack_activation(player* pp, bool from_server, int activation_delay_ms); // 0x009F8800 (:4196)
bool player_swap_backpack(player* pp, bool do_action); // 0x009F41F0 (:4250)
bool player_has_active_backpack(const player* pp); // 0x009590A0 (:4287)
void multi_game_backpack_process_picked_up(unsigned char* packet_data, const sender& from); // 0x009F4270 (:4326)
void multi_game_backpack_process_dropped(unsigned char* packet_data, const sender& from); // 0x009DF630 (:4391)
void multi_game_backpack_process_player_died_with_backpack(unsigned char* packet_data, const sender& from); // 0x009F43E0 (:4431)
void multi_game_backpack_process_toggled_activation(unsigned char* packet_data, const sender& from); // 0x009F8900 (:4484)
void multi_game_backpack_process_rhino_hit(unsigned char* packet_data, const sender& from); // 0x009C7430 (:4531)
void multi_game_backpack_process_ripple_hits(unsigned char* packet_data, const sender& from); // 0x009F44B0 (:4559)
void multi_game_backpack_process_swap(unsigned char* packet_data, const sender& from); // 0x009F4690 (:4654)

// 0x00884550 (.h:218)
inline const object_type multi_object_backpack::get_type_static()
{
    return OT_ITEM;
}

// 0x00884560 (.h:218)
inline const object_sub_type multi_object_backpack::get_subtype_static()
{
    return OT_SUB_ITEM_MULTI_BACKPACK;
}

// 0x00941D30 (.h:250)
inline player* multi_object_backpack::get_player_attached_to() const
{
    return player_attached_to;
}
