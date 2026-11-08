// Original: volition/rfg/code/objects/humans/human.h
// Implementation: volition/rfg/code/objects/humans/human.cpp
//
// human, the base of npcs and players (object type OT_HUMAN). It owns a character instance (skeleton and animation,
// char_inst) and a Havok character controller (in the stream resource rdd), the state machine (cur_state,
// cur_movement_state, stance, movement mode), path following (pf), ragdoll, IK, spine bending, head morphing, combat
// data, health and knockdown, inventory (inventory_item list), the vehicle it uses, voice and look-at.
// Per frame (gamestate_gp_simulate_frame, decompiled): human_process_all runs npc_process_all and human_fire_do_frame;
// the player and npc passes call human_do_frame; human_process_controllers steps the character controllers;
// the *_all_postsim / _post_transform passes finish the frame. human_controller_* wrap the character controller.
// Placement of nano_callback_info and ik_type inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/humans/human_heads_and_props.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_action_node_enum.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_render_distance.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_enums.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/rfg/code/weapons/weapons_enums.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"
#include <stdint.h>

class ambient_prop_info;
class anim;
class anim_set;
class character_controller;
class character_instance;
class cloth_simulation;
class hkpSimpleShapePhantom;
class hkpWorldObject;
class human;
class human_info;
enum human_melee_attack_types;

enum human_ik_reason
{
    IK_REASON_NO_IK = 0,
    IK_REASON_TWO_HANDED_WEAPON = 1,
    IK_REASON_USING_TURRET = 2,
    IK_REASON_RIGHT_HAND_TO_WEAPON = 3,
    IK_REASON_USING_SHIELD = 4,
    IK_REASON_GENERIC_OBJECT = 5,
    NUM_IK_REASONS = 6,
};
// size 0xC
struct human_move_speed_info
{
    float min_speed; // 0x00
    float average_speed; // 0x04
    float max_speed; // 0x08
};
class human_voice_persona;
struct ik_trigger;
struct inv_item_info;
struct inventory_item;
class matrix;
class object_action_node;
enum play_voice_status;
class rl_renderer;
class rl_skin_instance;
class vehicle;
class weapon;
struct weapon_fire_info;
struct weapon_info;

// size 0x10
struct human_flags
{
    human_flags(); // 0x005FB420 (.h:299)
    void reset(); // 0x005DBFB0 (.h:465)

    unsigned int bored_headtrack_disabled : 1; // 0x00:0
    unsigned int hidden : 1; // 0x00:1
    unsigned int cap_speed : 1; // 0x00:2
    unsigned int was_rendered : 1; // 0x00:3
    unsigned int locked_controller : 1; // 0x00:4
    unsigned int invulnerable : 1; // 0x00:5
    unsigned int mission_invulnerable : 1; // 0x00:6
    unsigned int no_damage : 1; // 0x00:7
    unsigned int activity_engage : 1; // 0x00:8
    unsigned int considered_armed : 1; // 0x00:9
    unsigned int riot_shield : 1; // 0x00:10
    unsigned int safehouse_vip : 1; // 0x00:11
    unsigned int radio_operator : 1; // 0x00:12
    unsigned int activity_raid_required : 1; // 0x00:13
    unsigned int activity_raid_optional : 1; // 0x00:14
    unsigned int activity_ha_hostage : 1; // 0x00:15
    unsigned int raid_introduction_lines : 1; // 0x00:16
    unsigned int miner_persona_lines : 1; // 0x00:17
    unsigned int damaged_by_player : 1; // 0x00:18
    unsigned int ai_ignore : 1; // 0x00:19
    unsigned int casts_shadows : 1; // 0x00:20
    unsigned int casts_drop_shadows : 1; // 0x00:21
    unsigned int is_turning : 1; // 0x00:22
    unsigned int is_falling : 1; // 0x00:23
    unsigned int dont_deform_bones : 1; // 0x00:24
    unsigned int dont_lod_bones : 1; // 0x00:25
    unsigned int playing_equip_anim : 1; // 0x00:26
    unsigned int playing_unequip_anim : 1; // 0x00:27
    unsigned int do_instant_equip : 1; // 0x00:28
    unsigned int anim_direct_blend : 1; // 0x00:29
    unsigned int start_jump : 1; // 0x00:30
    unsigned int super_jump : 1; // 0x00:31
    unsigned int processed_this_frame : 1; // 0x04:0
    unsigned int silent_vehicle_start : 1; // 0x04:1
    unsigned int suppress_flee_on_vehicle_exit : 1; // 0x04:2
    unsigned int death_func_done : 1; // 0x04:3
    unsigned int fading_in : 1; // 0x04:4
    unsigned int fading_out : 1; // 0x04:5
    unsigned int fading_out_from_nano : 1; // 0x04:6
    unsigned int is_nano_effect_currently_applied : 1; // 0x04:7
    unsigned int on_fire : 1; // 0x04:8
    unsigned int dropped_cash : 1; // 0x04:9
    unsigned int on_mover : 1; // 0x04:10
    unsigned int recalculate_at_node : 1; // 0x04:11
    unsigned int spinebending_done : 1; // 0x04:12
    unsigned int last_dest_in_repulsor : 1; // 0x04:13
    unsigned int jumping_from_building : 1; // 0x04:14
    unsigned int is_on_ladder : 1; // 0x04:15
    unsigned int ladder_force_slide : 1; // 0x04:16
    unsigned int ladder_force_exit : 1; // 0x04:17
    unsigned int ladder_reequip_weapon : 1; // 0x04:18
    unsigned int allow_flying_equips : 1; // 0x04:19
    unsigned int corpse_spotted : 1; // 0x04:20
    unsigned int corpse_spotted_by_enemy : 1; // 0x04:21
    unsigned int death_reported : 1; // 0x04:22
    unsigned int raid_death_ack : 1; // 0x04:23
    unsigned int anchor_orient : 1; // 0x04:24
    unsigned int bones_transformed_this_frame : 1; // 0x04:25
    unsigned int use_current_velocity : 1; // 0x04:26
    unsigned int use_as_final_velocity : 1; // 0x04:27
    unsigned int aim_at_pos : 1; // 0x04:28
    unsigned int block_forced_movement : 1; // 0x04:29
    unsigned int cancelling_melee_attack : 1; // 0x04:30
    unsigned int do_continuous_melee_damage : 1; // 0x04:31
    unsigned int in_air_melee : 1; // 0x08:0
    unsigned int cant_hit_with_melee : 1; // 0x08:1
    unsigned int invulnerable_to_debris : 1; // 0x08:2
    unsigned int override_default_anim_state : 1; // 0x08:3
    unsigned int override_default_fire_anim : 1; // 0x08:4
    unsigned int moving_and_transitioning_states : 1; // 0x08:5
    unsigned int check_for_cover : 1; // 0x08:6
    unsigned int jump_take_off : 1; // 0x08:7
    unsigned int rotate_internal_heading_updated : 1; // 0x08:8
    unsigned int crouch_cover : 1; // 0x08:9
    unsigned int cover_crouch_high_only : 1; // 0x08:10
    unsigned int cover_crouch_no_lean : 1; // 0x08:11
    unsigned int side_firing : 1; // 0x08:12
    unsigned int side_firing_walk_back : 1; // 0x08:13
    unsigned int ragdoll_on_impact_all_collisions : 1; // 0x08:14
    unsigned int ragdoll_on_impact_use_ragdoll_pos : 1; // 0x08:15
    unsigned int dive_capsule : 1; // 0x08:16
    unsigned int maintain_ambient_props : 1; // 0x08:17
    unsigned int leaning_disabled : 1; // 0x08:18
    unsigned int override_steering_heading_offset : 1; // 0x08:19
    unsigned int pushes_other_humans : 1; // 0x08:20
    unsigned int pushes_debris_scripted : 1; // 0x08:21
    unsigned int allow_steep_slopes : 1; // 0x08:22
    unsigned int external_force_applied : 1; // 0x08:23
    unsigned int ragdoll_shot : 1; // 0x08:24
    unsigned int saved_pushes_debris_scripted : 1; // 0x08:25
    unsigned int filter_handle_valid : 1; // 0x08:26
    unsigned int just_got_up_from_ragdoll : 1; // 0x08:27
    unsigned int disable_path_smoothing_for_request : 1; // 0x08:28
    unsigned int disable_all_path_smoothing : 1; // 0x08:29
    unsigned int in_fetal_position : 1; // 0x08:30
    unsigned int disallow_vehicle_exit : 1; // 0x08:31
    unsigned int limited_vehicle_exit : 1; // 0x0C:0
    unsigned int driverless_exit_only : 1; // 0x0C:1
    unsigned int stuck_in_vehicle : 1; // 0x0C:2
    unsigned int convoy_vehicle_exit : 1; // 0x0C:3
    unsigned int disallow_vehicle_entry : 1; // 0x0C:4
    unsigned int disallow_vehicle_drive : 1; // 0x0C:5
    unsigned int ambient_edf : 1; // 0x0C:6
    unsigned int bashed_character_controller : 1; // 0x0C:7
    unsigned int head_loaded : 1; // 0x0C:8
    unsigned int lod_head_loaded : 1; // 0x0C:9
    unsigned int in_vehicle_invisible : 1; // 0x0C:10
    unsigned int high_priority_target : 1; // 0x0C:11
    unsigned int health_change_was_negative : 1; // 0x0C:12
    unsigned int voice_line_play_2d : 1; // 0x0C:13
    unsigned int voice_line_pain_only : 1; // 0x0C:14
    unsigned int killed_by_killzone : 1; // 0x0C:15
    unsigned int first_time_streamed : 1; // 0x0C:16
    unsigned int tired : 1; // 0x0C:17
    unsigned int use_bigsteps : 1; // 0x0C:18
    unsigned int stuck : 1; // 0x0C:19
    unsigned int last_pf_failed : 1; // 0x0C:20
    unsigned int extended_stuck : 1; // 0x0C:21
    unsigned int xray_visible : 1; // 0x0C:22
    unsigned int was_gibbed : 1; // 0x0C:23
    unsigned int prevent_ragdoll_sfx : 1; // 0x0C:24
    unsigned int always_show_on_minimap : 1; // 0x0C:25
    unsigned int used_death_buffer : 1; // 0x0C:26
    unsigned int do_not_convert_to_guerrilla : 1; // 0x0C:27
    unsigned int do_not_play_ambient_or_greet_lines : 1; // 0x0C:28
    unsigned int disallow_flinches_and_ragdolls : 1; // 0x0C:29
    unsigned int only_use_action_nodes : 1; // 0x0C:30
    unsigned int complain_when_shot : 1; // 0x0C:31
};

// size 0x4
struct human_mp_flags
{
    void set_charge_weapon(unsigned int _charge_weapon); // 0x00942F60 (.h:470)
    unsigned int get_charge_weapon() const; // 0x00942F80 (.h:471)
    void set_running_attack(unsigned int _running_attack); // 0x00942F90 (.h:474)
    unsigned int get_running_attack() const; // 0x00942FB0 (.h:475)
    void set_in_air_detonate(unsigned int _in_air_detonate); // 0x006F6950 (.h:476)
    unsigned int get_in_air_detonate() const; // 0x00942FC0 (.h:477)
    void set_detonated_charges(unsigned int _detonated_charges); // 0x006F6970 (.h:478)
    unsigned int get_detonated_charges() const; // 0x00942FD0 (.h:479)
    void set_jetpack(unsigned int _jetpack); // 0x00942FE0 (.h:480)
    unsigned int get_jetpack() const; // 0x00943000 (.h:481)

    unsigned int is_multiplayer : 1; // 0x00:0
    unsigned int melee_primary_id : 2; // 0x00:1
    unsigned int melee_secondary_id : 2; // 0x00:3
    unsigned int melee_tertiary_id : 2; // 0x00:5
    unsigned int melee_firearm_id : 2; // 0x00:7
    unsigned int jump_id : 2; // 0x00:9
    unsigned int vehicle : 1; // 0x00:11
    unsigned int turret : 1; // 0x00:12
    unsigned int swap_weapon : 1; // 0x00:13
    unsigned int firearm_trigger_down_id : 2; // 0x00:14
    unsigned int firearm_trigger_up_id : 2; // 0x00:16
    unsigned int throw_attack_id : 2; // 0x00:18
private:
    unsigned int charge_weapon : 2; // 0x00:20
    unsigned int running_attack : 1; // 0x00:22
    unsigned int in_air_detonate : 1; // 0x00:23
    unsigned int detonated_charges : 1; // 0x00:24
    unsigned int jetpack : 1; // 0x00:25
};

// size 0x4
struct weapon_anim_flags
{
    weapon_anim_flags(); // 0x00A2C5F0 (.h:529)
    void clear(); // 0x00A056A0 (.h:530)

    unsigned int prone : 1; // 0x00:0
    unsigned int crouch : 1; // 0x00:1
    unsigned int down_ready : 1; // 0x00:2
    unsigned int ready : 1; // 0x00:3
    unsigned int aim : 1; // 0x00:4
    unsigned int fine_aim : 1; // 0x00:5
    unsigned int no_ammo : 1; // 0x00:6
    unsigned int walk : 1; // 0x00:7
    unsigned int run : 1; // 0x00:8
    unsigned int left : 1; // 0x00:9
    unsigned int right : 1; // 0x00:10
    unsigned int back : 1; // 0x00:11
    unsigned int backpedal : 1; // 0x00:12
    unsigned int switching : 1; // 0x00:13
    unsigned int drive : 1; // 0x00:14
    unsigned int ride : 1; // 0x00:15
    unsigned int ride_left : 1; // 0x00:16
};

enum ik_type
{
    ANIMATION_IK_LEFT_HAND = 0,
    ANIMATION_IK_RIGHT_HAND = 1,
    ANIMATION_IK_LEFT_FOOT = 2,
    ANIMATION_IK_RIGHT_FOOT = 3,
    NUM_ANIMATION_IK_TYPES = 4,
};

// size 0x28
class ik_joint
{
public:
    bool process(human* hp, vector& target_pos); // 0x00A7D4B0 (:14396)
    void do_ik(human* hp, const vector& target_pos); // 0x00A08820 (:14088)
private:
    ik_trigger* find_ik_trigger(human* hp); // 0x00A09110 (:14257)
    ik_trigger* find_elapsed_ik_trigger(human* hp, anim* ik_animp, unsigned int handle); // 0x00A09070 (:14245)
    bool get_ik_pos(human* hp, vector& ik_pos, human_ik_reason reason); // 0x00A7D2B0 (:14305)
public:
    ik_joint(); // 0x00A0F220

    int anchor_tag; // 0x00
    int joint_tag; // 0x04
    int ik_tag; // 0x08
    float ik_strength; // 0x0C
    float ik_rate_of_change; // 0x10
    unsigned int freeze_obj_handle; // 0x14
    vector freeze_offset_pos; // 0x18
    ik_type location; // 0x24
};

// size 0xC
struct nano_callback_info
{
    unsigned int target_handle; // 0x00
    bool killed_by_nano; // 0x04
    int human_nano_index; // 0x08
};

// size 0xCE0
class human : public object
{
public:
    // size 0x14
    struct resource_dependent_data
    {
        character_controller* controller; // 0x00
        hkpSimpleShapePhantom* hit_detect_phantom; // 0x04
        rl_skin_instance* head_renderable_instance_p; // 0x08
        int head_aim_handle; // 0x0C
        int head_manager_idx; // 0x10
    };

    // size 0x2C
    struct raycast_hit_info_data
    {
        raycast_hit_info_data(); // 0x00A0F210

        vector fire_pos; // 0x00
        vector target_pos; // 0x0C
        vector hit_point; // 0x18
        object_hit_location hit_location; // 0x24
        int hit_bone; // 0x28
    };

    static const object_type get_type_static() { return OT_HUMAN; } // 0x005DBFC0 (.h:560)
    static const object_sub_type get_subtype_static() { return OT_SUB_UNDEFINED; } // 0x005DBFD0 (.h:560)
    static const char* get_classname_static();
    static int get_bpo_index_static(); // 0x005DBFE0 (.h:560)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A83C40 (.h:560)
    virtual void pool_free(); // vtbl+0x80 0x00A83C50 (.h:560)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A9BB90 (:15159)
    virtual bool enable_object_outline(int layer); // vtbl+0x48 0x00A2EE80 (:15461)
    virtual bool disable_object_outline(); // vtbl+0x4C 0x00A2EF00 (:15482)
    void human_path_data_init(); // 0x00A07790 (:6980)
    int human_get_state_animation_index(const char*);
    void human_map_animations();
    bool is_transformed() const; // 0x00A078A0 (:7601)
    float calc_stopping_distance(); // 0x00A2E530 (:7613)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A2E4B0 (:7556)
    virtual bool is_offscreen() const; // vtbl+0x84 0x00A07840 (:7582)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A07F90 (:12408)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A08060 (:12422)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A9AF70 (:12429)
    virtual bool is_killed() const; // vtbl+0x10 0x00A07450 (:4917)
    bool is_invulnerable() const; // 0x00A07470 (:4923)
    void get_center_pos(vector& output) const; // 0x00A99F90 (:4940)
    virtual float get_mass() const; // vtbl+0x58 0x00A080B0 (:12467)
    human_teams get_team() const; // 0x005DBFF0 (.h:630)
    human_teams get_base_team() const; // 0x005DC000 (.h:631)
    virtual bool set_team(human_teams new_team); // vtbl+0x88 0x00A07F30 (:11963)
    human_teams get_undercover_team() const { return undercover_team; } // 0x005DC010 (.h:637)
    void set_undercover_team(human_teams new_undercover_team); // 0x00A07F50 (:11973)
    bool is_officer() const; // 0x0083F5B0 (.h:643)
    void set_casts_transparent_shadows_plus_child_items(bool do_cast); // 0x00A54700 (:15511)
    void set_opacity_plus_child_items(float alpha); // 0x00A7E1D0 (:15527)
    void set_stealth_plus_child_items(float percent); // 0x00A54760 (:15546)
    void request_avoidance(const unsigned int handlep); // 0x00A083C0 (:12600)
    void request_avoidance(const human* hp, const vector& request_dir, const vector& hint_dir); // 0x00A080C0 (:12569)
    bool update_avoidance(); // 0x00A08430 (:12614)
    void do_avoidance_pause(int ms); // 0x00A08500 (:12630)
    bool do_avoidance_pause_frame(); // 0x00A08540 (:12641)
    unsigned int get_avoidance_handle() const; // 0x005DC020 (.h:931)
    void get_avoidance_info(vector& request_dir, vector& hint_dir) const; // 0x005DC030 (.h:932)
    bool can_do_avoidance() const; // 0x005DC070 (.h:933)
    bool should_do_avoidance() const; // 0x005DC080 (.h:934)
    void end_avoidance(); // 0x005DC0B0 (.h:935)
    void refill_health(float percent); // 0x00A075E0 (:5922)
    void start_nano_rifle_fade_out(bool death_full_fade, human* attacker, bool apply_default_damage, const vector& world_hit_pos); // 0x00AD33A0 (human_combat.cpp:2591)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00AD37B0 (human_combat.cpp:2667)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00AD2A10 (human_combat.cpp:2339)
    virtual bool is_in_combat() const; // vtbl+0x8C 0x00A07650 (:5935)
    bool has_personal_shield() const; // 0x005DC0C0 (.h:963)
    bool is_shielded(const vector& source_pos, bool physical_shield_only) const; // 0x00A9A1A0 (:5950)
    bool create_shield_item(const item_info* i_info, bool visible); // 0x00AB14C0 (:5987)
    bool init_shield(item* const shield_inst); // 0x00A7B3E0 (:6048)
    void destroy_virtual_shield(); // 0x00A2DE80 (:6084)
    bool render_on_minimap() const; // 0x00A074B0 (:4948)
    inventory_item* get_last_equipped_weapon() const; // 0x00A074E0 (:5172)
    inventory_item* get_desired_inv_item() const; // 0x00A07500 (:5182)
    inventory_item* get_equipped_inv_item() const; // 0x00A074F0 (:5177)
    void set_equipped_inv_item(inventory_item* iip); // 0x00AB4D20 (:5031)
    weapon* get_equipped_weapon() const; // 0x00A7B390 (:5187)
    bool is_armed(bool ranged_only) const; // 0x00A52EA0 (:4907)
    bool wait_for_vehicle_stream_enter() const; // 0x00A2DD80 (:5288)
    bool fade_with_vehicle() const; // 0x00A2DC70 (:5202)
    float get_vehicle_opacity() const; // 0x00A2DD50 (:5264)
    void sync_to_vehicle_opacity(); // 0x00A2DCB0 (:5241)
    void fade_in(int new_fade_time); // 0x00A07510 (:5220)
    void fade_out(int new_fade_time, bool fading_from_nano); // 0x00A07540 (:5229)
    float get_health_percentage() const; // 0x00753D30 (.h:1014)
    bool is_npc() const; // 0x005DC0E0 (.h:1017)
    virtual bool is_undercover() const; // vtbl+0x90 0x00A07330 (:4530)
    bool has_special_voice_flags() const; // 0x00A07DF0 (:11187)
    play_voice_status voice_do_frame(); // 0x00A9AC70 (:11194)
    bool voice_persona_finished() const; // 0x00A2E670 (:10854)
    play_voice_status play_voice_line(voice_line_handle line_handle, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay, bool play_intro, bool play_outro); // 0x00A9A8F0 (:10932)
    play_voice_status play_voice_line(const char* line_name, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay); // 0x00A9ABB0 (:11067)
    play_voice_status play_voice_line(voice_lines line, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay); // 0x00A9AC00 (:11089)
    void play_vehicle_impact_sound(const vector& posp, float volume, bool run_over); // 0x00A07D50 (:11163)
    bool stop_voice(voice_line_priorities priority); // 0x00A2E6C0 (:10879)
    voice_lines get_current_voice_line() const; // 0x00A07E30 (:11381)
    voice_line_handle get_current_voice_line_handle() const; // 0x00A07E10 (:11346)
    bool is_playing_any_voice() const; // 0x005DC0F0 (.h:1063)
    bool is_playing_spot_line() const; // 0x00A2E7A0 (:11357)
    unsigned short get_radio_msg_inst() const; // 0x00B1FAF0 (.h:1067)
    int get_voice_play_inst() const; // 0x00A056B0 (.h:1068)
    int get_voice_line_play_id(); // 0x00A07E50 (:11423)
    void set_voice_play_inst(int play_id); // 0x00A07E80 (:11435)
    const human_voice_persona* get_voice() const; // 0x005DC100 (.h:1074)
    void set_voice(const human_voice_persona* new_voice); // 0x00943010 (.h:1077)
    int time_since_last_voice() const; // 0x00A07EB0 (:11449)
    const timestamp* get_voice_delay_timer() const; // 0x00A07ED0 (:11459)
    bool has_been_acknowledged() const; // 0x005DC110 (.h:1093)
    void acknowledge_human(int ack_ms); // 0x005DC130 (.h:1096)
    bool has_been_spotted() const;
    void flag_as_spotted(); // 0x005DC140 (.h:1105)
    virtual int get_tag_index_from_name(const char* tag_name) const; // vtbl+0x34 0x00A07590 (:5294)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00A2DDC0 (:5309)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00A2DE50 (:5332)
    virtual unsigned char get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x5C 0x00A07EE0 (:11467)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00A07EF0 (:11475)
    bool init_from_h_info(); // 0x00A7C900 (:11480)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00ADB030 (:11582)
    virtual bool stream(bool load); // vtbl+0x1C 0x00ABFA70 (:11989)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00AB1FB0 (:12472)
protected:
    virtual void free_this(); // vtbl+0x28 0x00AC84F0 (:12373)
public:
    bool has_entered_head_action_node() const; // 0x00A7B120 (:4540)
    bool at_entry_point(unsigned int node_handle) const; // 0x00AB1450 (:4555)
    bool has_entered_action_node(const object_action_node* anp) const; // 0x00A07380 (:4577)
    bool has_entered_action_node(unsigned int action_node_handle) const; // 0x00A07360 (:4569)
    bool reserve_action_node(const farray<unsigned int,4>& action_node_set); // 0x00A7B170 (:4605)
    void release_action_node(object_action_node* anp); // 0x00A7B2E0 (:4691)
    void release_action_node(unsigned int action_node_handle); // 0x00A7B260 (:4659)
    void release_all_action_nodes(); // 0x00A2DAB0 (:4700)
    bool has_node_reserved(const farray<unsigned int,4>& action_node_set) const; // 0x00A2DB50 (:4743)
    bool has_node_reserved(const object_action_node* anp) const; // 0x00A2DB40 (:4736)
    bool has_node_reserved(unsigned int action_node_handle) const; // 0x00A2DAE0 (:4709)
    int get_action_node_index(const object_action_node* anp) const; // 0x00A52E30 (:4767)
    object_action_node* get_desired_action_node() const; // 0x00A52E70 (:4813)
    unsigned int get_desired_action_node_handle() const; // 0x00A2DC00 (:4819)
    object_action_node* get_current_action_node(bool get_even_if_dead) const; // 0x00A2DC20 (:4834)
    unsigned int get_current_action_node_handle() const; // 0x00A073F0 (:4845)
    bool completed_action_node(const object_action_node* anp) const; // 0x00A07400 (:4851)
    bool can_reserve_action_node(const object_action_node* anp) const; // 0x00A7B300 (:4868)
    bool has_any_action_node_reserved() const; // 0x00A2DC60 (:4885)
    void get_reserved_nodes(farray<unsigned int,4>& action_node_set) const; // 0x00A7B370 (:4891)
    bool enter_action_node(const object_action_node* anp, int duration_ms); // 0x00A2DBB0 (:4778)
    void exit_action_node(const object_action_node* anp); // 0x00A073A0 (:4796)
    int get_lifetime() const; // 0x00A073D0 (:4805)
    human(); // 0x00A83AA0
    virtual ~human(); // vtbl+0x00 0x00A83C60

    object_stream_resource<resource_dependent_data> rdd; // 0x9C
    human_flags hflags; // 0xB4
    human_mp_flags mp_flags; // 0xC4
    int last_trigger_down_frame; // 0xC8
    const human_info* h_info; // 0xCC
    object_render_distance render_distance; // 0xD0
    unsigned char xray_material; // 0xD8
    unsigned char xray_render_alpha; // 0xD9
    int rank; // 0xDC
    vector approx_local_bmin; // 0xE0
    vector approx_local_bmax; // 0xEC
    vector last_pos; // 0xF8
    human_path_data pf; // 0x108
    vector turnto_target; // 0x228
    unsigned int nav_cell_detour_request_handle; // 0x234
    raycast_hit_info_data raycast_hit_info; // 0x238
    vector velocity; // 0x264
    vector actual_velocity; // 0x270
    int transform_frames_skipped; // 0x27C
    vector steering_vector; // 0x280
    float move_speed; // 0x28C
    float steering_heading_offset; // 0x290
    human_rotate_dir rotate_dir; // 0x294
    float state_speed_throttle; // 0x298
    float rotate_heading_left; // 0x29C
    float rotate_heading_right; // 0x2A0
    float rotate_internal_heading; // 0x2A4
    vector inertial_motion; // 0x2A8
    unsigned int ground_obj_handle; // 0x2B4
    vector ground_obj_contact_pos; // 0x2B8
    int ground_obj_alt_body_idx; // 0x2C4
    int ground_obj_shape_key; // 0x2C8
    float ground_obj_normal; // 0x2CC
    unsigned int object_filter_handle; // 0x2D0
    int block_movement_anim; // 0x2D4
    vector block_steering_vector; // 0x2D8
    block_movement_types block_movement_type; // 0x2E4
    float block_steering_heading_offset; // 0x2E8
    int released_movement_anim; // 0x2EC
    block_movement_types released_movement_type; // 0x2F0
    human_states cur_state; // 0x2F4
    human_movement_states cur_movement_state; // 0x2F8
    human_stances cur_stance; // 0x2FC
    human_stances last_stance; // 0x300
    human_move_modes movement_mode; // 0x304
    human_move_submodes movement_submode; // 0x308
    human_move_modes previous_movement_mode; // 0x30C
    animation_action custom_fire_animation; // 0x310
    float walk_animation_speed_pct; // 0x314
    float frametime_tally; // 0x318
    float actual_velocity_mag; // 0x31C
    float max_speed; // 0x320
    float falling_vel_mag; // 0x324
    timestamp not_moving_update; // 0x328
    character_instance* char_inst; // 0x32C
    cloth_simulation* cloth_sim; // 0x330
    int bone_lod_level; // 0x334
    weapon_anim_flags animation_flags; // 0x338
    human_ragdoll_state ragdoll_state; // 0x33C
    unsigned int ragdoll_source_human_handle; // 0x340
    int ragdoll_idx; // 0x344
    float ragdoll_blend_pct; // 0x348
    vector ragdoll_last_frame_velocity; // 0x34C
    float ragdoll_on_impact_time; // 0x358
    float ragdoll_on_impact_min_vel; // 0x35C
    timestamp ragdoll_damaged; // 0x360
    float root_bone_offset; // 0x364
    unsigned int nano_mat_fx[16]; // 0x368
    unsigned int mat_fx_handle_for_head_skin; // 0x3A8
    unsigned int last_valid_pos_before_ragdoll_index; // 0x3AC
    vector last_valid_pos_before_ragdoll[3]; // 0x3B0
    int ragdoll_num_collisions_with_walker; // 0x3D4
    ik_joint ik_joints[4]; // 0x3D8
    human_spinebend_data spinebend_data; // 0x480
    human_head_morphing head_morphs; // 0x890
    int initial_max_hit_points; // 0x90C
    int max_hit_points; // 0x910
    float hit_points; // 0x914
    int max_knockdown_hits; // 0x918
    float knockdown_hits; // 0x91C
    timestamp knockdown_timestamp; // 0x920
    timestamp knockdown_timeout_ts; // 0x924
    human_combat combat; // 0x928
    timestamp collision_damage_timer; // 0xA14
    float current_collision_damage; // 0xA18
    timestamp do_ragdoll_ts; // 0xA1C
    timestamp facial_pose_ts; // 0xA20
    timestamp turret_hold_animations; // 0xA24
    float cash; // 0xA28
    inventory_item* inventory; // 0xA2C
    inventory_item* desired_equipped_inv_item; // 0xA30
    inventory_item* last_equipped_weapon; // 0xA34
    inventory_item* second_last_equipped_weapon; // 0xA38
    inventory_item* grenade_weapon; // 0xA3C
    item_info* anim_prop_info; // 0xA40
    unsigned int offhand_projectile_handle; // 0xA44
    unsigned int shield_handle; // 0xA48
    timestamp reload_timer; // 0xA4C
    int equip_tag_index; // 0xA50
    int equip_offhand_tag_index; // 0xA54
    int root_bone_index; // 0xA58
    int left_foot_bone_index; // 0xA5C
    int right_foot_bone_index; // 0xA60
    int left_shoulder_bone_index; // 0xA64
    int right_shoulder_bone_index; // 0xA68
    int left_hand_bone_index; // 0xA6C
    int right_hand_bone_index; // 0xA70
    int left_hip_bone_index; // 0xA74
    int right_hip_bone_index; // 0xA78
    float run_stand_blend_pose_weight; // 0xA7C
    float lean_weight; // 0xA80
    vector lean_dir; // 0xA84
    timestamp lean_disabled; // 0xA90
    vector lean_last_steering_vector; // 0xA94
    foot_plant foot_planted; // 0xAA0
    timestamp foot_plant_time; // 0xAA4
    bool is_turning; // 0xAA8
    bool starting_turn; // 0xAA9
    float render_alpha; // 0xAAC
    float camera_alpha_override; // 0xAB0
    bool casts_transparent_shadows; // 0xAB4
    timestamp fade_timer; // 0xAB8
    int fade_time; // 0xABC
    float m_stealth_percent; // 0xAC0
    unsigned int vehicle_handle; // 0xAC4
    unsigned int reserved_vehicle_handle; // 0xAC8
    vehicle_seat_index vehicle_seat_idx; // 0xACC
    timestamp bored_idle_timestamp; // 0xAD0
    timestamp corpse_cleanup_timer; // 0xAD4
    timestamp turn_off_fire_timestamp; // 0xAD8
    timestamp health_restore_timestamp; // 0xADC
    float health_restore_hit_points; // 0xAE0
    float health_restore_min_hit_points; // 0xAE4
    vector look_at_pos; // 0xAE8
    unsigned int look_at_handle; // 0xAF4
    float look_at_speed; // 0xAF8
    vector aim_override_dir; // 0xAFC
    float damage_percent; // 0xB08
    unsigned short damage_func_handle; // 0xB0C
    unsigned short death_func_handle; // 0xB0E
    timestamp breath_timer; // 0xB10
    timestamp crouch_to_stand_test_ts; // 0xB14
    unsigned int ladder_handle; // 0xB18
    float ladder_slide_speed; // 0xB1C
    int ladder_grab_rung; // 0xB20
    int ladder_slide_play_id; // 0xB24
    bool code_driven_start_jump; // 0xB28
    float code_driven_jump_timer; // 0xB2C
    float code_driven_jump_height; // 0xB30
    timestamp jump_state_timer; // 0xB34
    timestamp last_supported; // 0xB38
    timestamp air_time; // 0xB3C
    timestamp update_timer; // 0xB40
    object_importance_level importance_level; // 0xB44
    unsigned int scripted_action_node_handle; // 0xB48
    nano_callback_info nano_cb_info[16]; // 0xB4C
    int nano_index; // 0xC0C
    human_action_node_data action_node_data; // 0xC10
    unsigned int light_effects[2]; // 0xC48
    int light_tags[2]; // 0xC50
    timestamp avoidance_check_timer; // 0xC58
    // The rest is protected/private in rfg.pdb; human.cpp's free functions (friends, not recorded in the PDB) use it.
    timestamp avoidance_request_timer; // 0xC5C
    unsigned int avoidance_request_human; // 0xC60
    vector avoidance_move_dir; // 0xC64
    vector avoidance_hint_dir; // 0xC70
    timestamp avoidance_pause_timer; // 0xC7C
    human_movement_states avoidance_original_movement_state; // 0xC80
public:
    int minimap_flags; // 0xC84
    const ambient_prop_info* amb_prop_info; // 0xC88
    inventory_item* equipped_inv_item_last_frame; // 0xC8C
    inventory_item* equipped_inv_item; // 0xC90
    const human_voice_persona* voice; // 0xC94
    human_teams current_team; // 0xC98
    human_teams undercover_team; // 0xC9C
    int dialogue_foley_info; // 0xCA0
    voice_line_handle queued_voice_line; // 0xCA4
    voice_lines situational_voice_line; // 0xCA8
    voice_line_priorities voice_priority; // 0xCAC
public:
    audiolib_cue_priority voice_cue_priority; // 0xCB0
    unsigned short radio_instance; // 0xCB4
    int voice_instance; // 0xCB8
    timestamp voice_time_since_finish; // 0xCBC
    lipsync_data_handle lipsync_handle; // 0xCC0
    timestamp voice_delay_time; // 0xCC4
    bool (*voice_delay_cb)(const human*, void*); // 0xCC8
    void* voice_delay_cb_data; // 0xCCC
    timestamp acknowledged_time; // 0xCD0
    timestamp reported_timer; // 0xCD4
    timestamp lifetime; // 0xCD8
};

// size 0xC
class obj_human_props : public obj_prop_block
{
public:
    obj_human_props(const human_info* h_info, const vector& pos, const matrix& orient, const char* name); // 0x00A52D70 (:4519)
    ~obj_human_props(); // 0x00760B90

};

// size 0x18
struct human_integration_parameters
{
    void initialize(human* p_human); // 0x00A06C40 (:278)
    human_integration_parameters(); // 0x00A07830

    human* m_human; // 0x00
    vector m_desired_velocity; // 0x04
    float m_start_jump_height; // 0x10
    bool m_integrate : 1; // 0x14:0
    bool m_check_support : 1; // 0x14:1
    bool m_use_velocity : 1; // 0x14:2
    bool m_ignore_y : 1; // 0x14:3
    bool m_move_character_controller : 1; // 0x14:4
    bool m_check_fall : 1; // 0x14:5
    bool m_refresh_manifold : 1; // 0x14:6
};

bool check_if_pos_is_invalid(const vector&, void*, const char*); // 0x00A06C30 (:153)
void human_get_animation_flags(const human* hp, human_states state, human_movement_states m_state, weapon_anim_flags& animation_flags); // 0x00ABAC10 (:2273)
void human_get_turret_state_anim(human* hp); // 0x00AC6510 (:2734)
void human_set_and_process_animation_states(human* hp); // 0x00ABEDF0 (:3943)
bool ix_sphere_cylinder(const vector& sphere_pos, const float sphere_radius, const vector& cylinder_base, const vector& cylinder_axis, const float cylinder_height, const float cylinder_radius); // 0x00A06E90 (:4242)
void human_play_pain_sound(human* target, const human* attacker, const weapon_info* w_info, damaged_by_types damage_type, bool silent); // 0x00AB1260 (:4304)
const human_move_speed_info* human_get_move_speed_info(const human* hp, human_movement_states m_state); // 0x00A07300 (:4501)
extern const human_move_speed_info Default_move_speed_block[4]; // 0x01310E2C
const human_move_speed_info (&human_get_move_speed_block(const human* hp))[4]; // 0x00A07320 (:4507)
void human_do_equip_unequip(human* hp, bool play_animation); // 0x00ABC490 (:5346)
void human_controller_synch(human* hp); // 0x00A537E0 (:7006)
bool human_controller_make_jump(human* hp); // 0x00A077F0 (:7030)
bool human_controller_make_super_jump(human* hp); // 0x00A07810 (:7037)
bool human_controller_is_on_ground(human* hp); // 0x00A2E3A0 (:7048)
bool human_controller_is_sliding(human* hp); // 0x00A2E3D0 (:7057)
void human_controller_get_pos(const human* hp, vector& pos); // 0x00A2E400 (:7065)
void human_controller_set_pos(human* hp, const vector& pos); // 0x00A2E430 (:7076)
void human_controller_get_vel(const human* hp, vector& vel); // 0x00A2E450 (:7087)
void human_controller_set_vel(human* hp, const vector& vel); // 0x00A7B510 (:7099)
void human_controller_reset(human* hp); // 0x00A2E480 (:7122)
void human_controller_process_pre(human* hp, bool resimulating); // 0x00A7B580 (:7133)
void human_process_controllers(float time_step); // 0x00AE4BD0 (:7452)
bool human_stick_to_ground(human* hp); // 0x00A7C540 (:7706)
void human_teleport_unsafe(human* hp, vector pos, matrix orient); // 0x00A7C660 (:7737)
void human_teleport(human* hp, vector& pos, matrix& orient, float placement_range, bool allow_fail); // 0x00A9A7A0 (:7773)
void human_set_state(human* hp, human_states state, human_movement_states movement_state, float move_speed); // 0x00A078B0 (:7841)
void human_clear_bored_idle(human* hp); // 0x00A078F0 (:7881)
void human_set_stance(human* hp, human_stances stance); // 0x00A07910 (:7885)
void human_set_movement_mode(human* hp, human_move_modes move_mode, human_move_submodes move_submode); // 0x00A07930 (:7914)
human_move_modes human_get_movement_mode(const human* hp); // 0x00A07AC0 (:8014)
bool human_is_in_movement_mode(human* hp, human_move_modes mode, human_move_submodes submode); // 0x00A07AD0 (:8026)
bool human_is_moving(const human* hp, bool* running); // 0x00A07B10 (:8044)
bool human_pathfind_to(human* hp, const vector& pos, human_states state, human_movement_states m_state, uint64_t end_nav_cell_hint, bool out_of_chunk); // 0x00A53920 (:8077)
bool human_pathfind_in_direction(human* hp, const vector& dir, human_states state, human_movement_states m_state); // 0x00A53AA0 (:8101)
void human_do_frame(human* hp, bool do_animation); // 0x00AE5D40 (:8129)
void human_do_frame_no_animation(human* hp); // 0x00AB1660 (:8432)
void human_process_post_transform(human* hp); // 0x00ADA0E0 (:8559)
void human_process_all(); // 0x00AE7860 (:8697)
void human_process_all_no_animation(); // 0x00AB5200 (:8717)
void human_process_all_animations_start(); // 0x00A07B60 (:8722)
void human_process_all_animations_end(); // 0x00A07B70 (:8727)
void human_process_all_post_transform(); // 0x00ADA8E0 (:8732)
void human_process_all_postsim(); // 0x00ADA950 (:8745)
void human_do_ik_process(); // 0x00A9A890 (:8913)
void human_jump(human* hp, bool from_building, bool leap_over, bool dropping_down); // 0x00AC7690 (:8935)
void human_melee_cancel_attack(human* hp); // 0x00A2E5C0 (:9104)
bool human_melee_attack(human* hp, human_melee_attack_types attack_type, bool from_server, bool force_running); // 0x00AB1B50 (:9123)
void human_set_default_inventory(human* hp); // 0x00ADAED0 (:9252)
bool human_inventory_slot_is_available(human* hp, int slot); // 0x00A07B80 (:9288)
inventory_item* human_get_inventory_item(const human* hp, int selection_slot); // 0x00A07BB0 (:9300)
inventory_item* human_add_item_to_inventory(human* hp, const inv_item_info* info, int count, int ammo_count, int slot, bool add_to_weapon_cabinet, bool demolitions_master_override, bool show_inventory); // 0x00AB5210 (:9319)
inventory_item* human_add_item_to_inventory(human* hp, const char* name, int count, int ammo_count, int slot, bool add_to_weapon_cabinet, bool demolitions_master_override, bool show_inventory); // 0x00AB5690 (:9501)
inventory_item* human_add_item_to_inventory(human* hp, item* obj_itemp, bool add_to_weapon_cabinet, bool demolitions_master_override, bool show_inventory); // 0x00AB56B0 (:9518)
void human_transfer_item_from_inventory(human* src, human* target, inventory_item* iip, int how_many); // 0x00ABCE50 (:9699)
void human_add_all_items_to_inventory(human* hp); // 0x00AB5BC0 (:9716)
void human_delete_all_items_from_inventory(human* hp, bool from_server, bool skip_grenades); // 0x00ACEC90 (:9764)
void human_disarm(human* hp, bool from_server); // 0x00ACED10 (:9781)
int human_count_inventory_items(human* hp); // 0x00A07BE0 (:9794)
bool human_remove_item_from_inventory(human* hp, inventory_item* iip, int how_many, bool from_server); // 0x00AB5CC0 (:9815)
void human_delete_item_from_inventory(human* hp, inventory_item* iip, bool from_server, bool owner_being_deleted); // 0x00AC7A90 (:9894)
void human_validate_inventory(human* hp); // 0x00AB5EE0 (:9974)
inventory_item* human_has_item(const human* hp, const inv_item_info* item_info); // 0x00A07C10 (:10049)
bool human_has_weapon(const human* hp, unsigned int weapon_handle); // 0x00A07C40 (:10062)
inventory_item* human_has_weapon_of_type(const human* hp, weapon_class_type weapon_class); // 0x00A07C70 (:10082)
bool human_has_ammo(const human* hp, bool include_thrown_charges, bool include_non_inventory); // 0x00A7C860 (:10105)
inventory_item* human_has_item(const human* hp, const char* name); // 0x00A2E630 (:10134)
bool human_has_all_items(const human* hp); // 0x00A07CB0 (:10141)
bool human_has_equipped(const human* hp, const inv_item_info* item_info); // 0x00A07D00 (:10155)
bool human_has_equipped(const human* hp, const char* item_name); // 0x00A07D20 (:10160)
void human_equip_inventory_item(human* hp, inventory_item* iip); // 0x00ABF800 (:10165)
void human_unequip_inventory_item(human* hp); // 0x00A53BC0 (:10279)
void human_died(human* hp, human* attacker, damaged_by_types damage_type, const weapon_info* w_info, const object* owner); // 0x00AC7C90 (:10313)
float human_damage(human* hp, human* attacker, float damage_amount, damaged_by_types damage_type, const weapon_info* w_info, const object* owner, bool play_pain_sound, bool silent_attack); // 0x00ACED70 (:10582)
// Deviation (not in rfg.exe): the regeneration delay a multiplayer client starts when the server's state lowers a
// player's hit points (human_damage applies no damage on a client, so rfg.exe never starts it there).
void human_health_damage_register_from_network(human* hp);
bool human_is_tied_up(const human* hp); // 0x00A2E7D0 (:12553)
vehicle* human_in_vehicle(const human* hp); // 0x00A53C40 (:12667)
bool human_in_vehicle_interaction(const human* hp, const vehicle* vp); // 0x00A2E800 (:12676)
bool human_in_vehicle(const human* hp, const vehicle* vp); // 0x00A2E890 (:12721)
bool human_in_vehicle(const human* hp, unsigned int vehicle_handle); // 0x00A2E900 (:12777)
bool human_in_flyer(const human* p_human); // 0x00A53C90 (:12783)
bool human_in_vehicle_transition(const human* hp, const vehicle* vp, vi_interaction interaction); // 0x00A2E930 (:12801)
bool human_in_players_vehicle(const human* const hp); // 0x00A2E9D0 (:12851)
bool human_is_leaning_out_vehicle_window(const human* p_human, bool& left_side); // 0x00A53CF0 (:12873)
void human_switch_to_vehicle_camera(human* hp, vehicle* target_vp); // 0x00A9AFC0 (:12908)
void human_switch_to_on_foot_camera(human* new_target); // 0x00A7CB90 (:12942)
bool human_is_combat_ready(const human* hp); // 0x005DC160 (.h:1367), inline in the original header
bool human_is_combat_aiming(const human* hp); // 0x005DC190 (.h:1368), inline in the original header
// 0x005DC1C0 (.h:1414)
// Missing humans count as dead.
inline bool human_is_dead(const human* hp)
{
    return hp == nullptr || hp->is_killed();
}
bool human_is_dead(unsigned int handle); // 0x00A2EA00 (:12964)
bool human_is_ragdolled(const human* hp); // 0x00A085D0 (:12971)
bool human_is_ragdolled_or_getting_up(const human* hp); // 0x00A08610 (:12979)
bool human_is_immune_to_ragdoll(const human* hp); // 0x00A2EA40 (:12986)
void human_set_combat_ready(human* hp, bool ready, int additional_time); // 0x00A2EAE0 (:13018)
void human_override_default_animation_state(human* hp, bool _override); // 0x00A08650 (:13167)
void human_override_default_state_and_fire_anims(human* hp, animation_action custom_fire_anim); // 0x00A08690 (:13180)
bool human_is_playing_turn(human* hp); // 0x00A086D0 (:13189)
void human_stop_playing_turn(human* hp, int ramp_out_frames, bool pause); // 0x00A08710 (:13194)
bool human_get_standing_on_material(const human* hp, unsigned char& mat_index, material_shape_type& shape_type); // 0x00A7CC30 (:13203)
void human_switch_to_dead_state(human* hp); // 0x00ACF520 (:13241)
void human_set_look_at_target(human* hp, const vector& target, bool should_aim_at, float spinebend_rate); // 0x00A08750 (:13421)
void human_set_look_at_target(human* hp, unsigned int handle, const vector& offset, bool should_aim_at, float spinebend_rate); // 0x00A7CD70 (:13445)
void human_get_look_at_vector_from_eye_pos(const human* hp, vector eye_pos, vector& look_at_vec); // 0x00A9B0E0 (:13485)
void human_get_look_at_vector(const human* hp, vector& look_at_vec); // 0x00AB6020 (:13558)
void human_get_head_pos(const human* hp, vector& head_pos); // 0x00A7CE60 (:13575)
void human_get_eye_pos_orient(const human* hp, vector& eye_pos, matrix* eye_orient); // 0x00AB2260 (:13604)
void human_get_head_pos_orient(const human* hp, vector& head_pos, matrix& head_orient); // 0x00A9B5D0 (:13701)
void human_hide(human* hp, bool hide); // 0x00A9B7E0 (:13812)
void human_set_render(human* hp, bool render); // 0x00A9B8C0 (:13839)
void human_set_cast_shadows(human* hp, bool cast_shadows, bool include_children); // 0x00A7CF80 (:13850)
void human_set_cast_drop_shadows(human* hp, bool cast_drop_shadows, bool include_children); // 0x00A7D050 (:13874)
void xray_my_children(object* the_parent, bool xray_visible, unsigned char xray_material, const unsigned char xray_alpha, bool force_dirty); // 0x00A7D120 (:13892)
void human_set_xray_visible(human* hp, bool xray_visible); // 0x00A7D200 (:13919)
void human_update_collision_filter(human* hp, bool enable_collisions); // 0x00A53D90 (:13978)
bool human_get_collision_filter(unsigned int& filter_type, const human* hp); // 0x00A2EB50 (:14011)
void human_enable_gravity(human* p_human, bool enable); // 0x00A2EB90 (:14038)
bool human_contacting_non_stationary_object(const human* p_human); // 0x00A087C0 (:14052)
void human_enable_character_scaling_this_frame(human* hp); // 0x00A09230 (:14491)
bool human_is_driving(const human* hp); // 0x00A09270 (:14497)
bool human_is_passenger(const human* hp); // 0x00A09290 (:14502)
bool human_is_falling(const human* hp); // 0x00A092B0 (:14507)
bool human_is_jumping(const human* hp); // 0x00A092D0 (:14512)
bool human_is_in_air(const human* hp); // 0x00A53DF0 (:14517)
bool human_is_tired(const human* hp); // 0x00A2EBC0 (:14536)
unsigned int human_get_vehicle_handle(const human* hp); // 0x00A092F0 (:14556)
object_hit_location human_get_hit_location(const human* hp, const vector& start_pos, const vector& direction, vector& intersection, float radius, int* bone_index, vector* hit_normal); // 0x00A7D740 (:14609)
void human_get_unanimated_hit_location(const human* hp, const vector& hit_pos, const vector& hit_normal, int hit_bone_index, vector& out_pos, vector& out_norm); // 0x00A2EC40 (:14788)
object_hit_location human_get_point_hit_location(const human* hp, const vector& position, float radius, int* bone_index); // 0x00A53EF0 (:14818)
object_hit_location human_get_hit_location_from_bone_index(const human* hp, int bone_index); // 0x00A545B0 (:14927)
item* human_get_prop(human* hp, prop_type ptype, const char* pNameToCheck); // 0x00A2ED40 (:14970)
bool human_wear_prop(human* hp, item* ip); // 0x00AB2680 (:14992)
bool human_freeze_ik(human* hp, ik_type body_part, object* obj); // 0x00A2EDD0 (:15068)
void human_maybe_play_falling_voice(human* hp); // 0x00A9B920 (:15112)
bool human_is_flinching(const human* hp); // 0x00A09300 (:15503)
// size 0xC
struct facial_anim_level
{
    int anim; // 0x00, the facial pose state
    float min_morale; // 0x04
    float max_morale; // 0x08
};

extern int Bone_lods[100]; // 0x03023690
extern facial_anim_level Facial_anims[10]; // 0x01694288
extern int Num_facial_anims; // 0x01694300
extern bool No_npc_npc_damage; // 0x0302F065
extern anim_set* NPC_male_guerrilla_anim_set; // 0x0302F0A4
extern const vector Default_target_object_offset; // 0x016947A0
extern int Ai_reuse_turret_after_nearby_explosive; // 0x01694324
void human_system_init(); // 0x00AB6080 (:16437)
void human_level_init_post(); // 0x00ADBA90 (:16571)
void human_level_shutdown(); // 0x00AB2A70 (:16604)
void human_validate_respawn_point(vector& respawn_pos); // 0x00A54870 (:16618)
bool human_is_in_cover(const human* hp); // 0x00A09320 (:16637)
bool human_is_leaning_from_cover(human* hp); // 0x00A09340 (:16646)
bool human_can_ordinarily_be_pushed(const human* hp); // 0x00A2EF50 (:16677)
bool human_can_currently_be_pushed(const human* hp); // 0x00A7E250 (:16721)
bool human_should_ragdoll_other_human(const human* hp, const human* other_hp); // 0x00A7E3A0 (:16784)
bool human_should_push_other_human(const human* hp, const human* other_hp); // 0x00A7E490 (:16841)
void human_flags_bitwise_or(const human_flags& s1, const human_flags& s2, human_flags& return_value); // 0x00A09380 (:16928)

// 0x00A0F220: nothing to construct.
inline ik_joint::ik_joint()
{
}

// 0x00A0F210: nothing to construct.
inline human::raycast_hit_info_data::raycast_hit_info_data()
{
}

// 0x005DC0C0 (.h:963): a riot shield that is still there
inline bool human::has_personal_shield() const
{
    return hflags.riot_shield != 0 && shield_handle != 0xFFFFFFFF;
}

// 0x00A07830: nothing to construct.
inline human_integration_parameters::human_integration_parameters()
{
}

// 0x006F6970 (.h:478)
inline void human_mp_flags::set_detonated_charges(unsigned int _detonated_charges)
{
    detonated_charges = _detonated_charges;
}

// 0x00942FD0 (.h:479)
inline unsigned int human_mp_flags::get_detonated_charges() const
{
    return detonated_charges;
}

// 0x006F6950 (.h:476)
inline void human_mp_flags::set_in_air_detonate(unsigned int _in_air_detonate)
{
    in_air_detonate = _in_air_detonate;
}

// 0x00942FC0 (.h:477)
inline unsigned int human_mp_flags::get_in_air_detonate() const
{
    return in_air_detonate;
}

// 0x00942FE0 (.h:480)
inline void human_mp_flags::set_jetpack(unsigned int _jetpack)
{
    jetpack = _jetpack;
}

// 0x00943000 (.h:481)
inline unsigned int human_mp_flags::get_jetpack() const
{
    return jetpack;
}

// 0x005DC190 (.h:1368): the combat-aim window is running.
inline bool human_is_combat_aiming(const human* hp)
{
    return hp->combat.combat_aim_timestamp.valid() && !hp->combat.combat_aim_timestamp.elapsed();
}

// 0x005DC160 (.h:1367): the combat-ready window is running.
// 0x005DC110 (.h:1093)
inline bool human::has_been_acknowledged() const
{
    return !acknowledged_time.elapsed();
}

// 0x005DC0B0 (.h:935)
inline void human::end_avoidance()
{
    avoidance_request_human = 0xFFFFFFFF;
    avoidance_request_timer.invalidate();
}

inline bool human_is_combat_ready(const human* hp)
{
    return hp->combat.combat_ready_timestamp.valid() && !hp->combat.combat_ready_timestamp.elapsed();
}

// 0x005DBFE0 (.h:560): the human broadphase layer (World.collect_objects<human>).
inline int human::get_bpo_index_static()
{
    return 0;
}
