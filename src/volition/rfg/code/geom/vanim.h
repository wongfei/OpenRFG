// Original: volition/rfg/code/geom/vanim.h
// Implementation: volition/rfg/code/geom/vanim.cpp
//
// The animation state machine of a character (v_animate): the animation set (with its parent sets and variants)
// maps animation groups, states and actions to anims; v_animate keeps up to max_simultaneous_anims playing v_anims
// (states, actions, poses, blend trees, variants) and blends them, moves the character to an action's end position and
// orientation, and fires the triggers. vanim_play_parameters correct the start and end of an interaction animation.
// The tables are parsed from the anim set xtbl files (vanim_parse_anim_tables). Blend trees blend the states of an
// anim by a control input (speed, lean). Placement of state_enum, anim_set, anim_group and the blend tree types
// inferred.
#pragma once

#include <string.h>

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
class index_alias;
class object;
struct anim_variant;
struct prop_trigger;

enum state_enum
{
    VANIM_STATE_INVALID = -1,
    VANIM_STATE_HIGHEST_BLEND = 0,
    VANIM_STATE_SECOND_HIGHEST_BLEND = 1,
    VANIM_STATE_THIRD = 2,
    VANIM_STATE_FOURTH = 3,
    VANIM_STATE_FIFTH = 4,
    VANIM_STATE_DESIRED = 1033,
    VANIM_STATE_SECONDARY = 1034,
};

// size 0xC
struct anim_foley
{
    int foley_id; // 0x00
    bool looping; // 0x04
    int frame; // 0x08
};

// size 0x38 (anim_foley.xtbl)
struct anim_foley_set
{
    unsigned int chcksum; // 0x00, crc of the name
    anim_foley foleys[4]; // 0x04
    unsigned short num_foleys; // 0x34
};

// size 0x10
class anim_set
{
public:
    char* name; // 0x00
    char* parent_name; // 0x04
    anim_set* parent; // 0x08
    anim_set_variants variant_type; // 0x0C
};

// size 0xB8C
struct anim_group
{
    anim_group* parent; // 0x00
    unsigned char num_anims[983]; // 0x04
    unsigned short lookup_index[983]; // 0x3DC
};

// size 0x20
class anim_blend_tree_info
{
public:
    // size 0x3C
    struct blend_tree_state
    {
        char* anim_filename; // 0x00
        int animlib_handle; // 0x04
        int anim_handle; // 0x08
        float blend_time; // 0x0C
        int num_control_points; // 0x10
        float control_point_range[5]; // 0x14
        float control_point_value[5]; // 0x28
    };

    // size 0x44
    struct blend_tree_action
    {
        char* anim_filename; // 0x00
        int anim_handle; // 0x04
        bool use_ramped_input; // 0x08
        float start_range_min; // 0x0C
        float start_range_max; // 0x10
        float end_range_min; // 0x14
        float end_range_max; // 0x18
        float time_variance; // 0x1C
        float end_state_percent; // 0x20
        float cap_ramp_min; // 0x24
        float cap_ramp_max; // 0x28
        bool always_cap; // 0x2C
        float start_state_percent_min; // 0x30
        float start_state_percent_max; // 0x34
        float cancel_range_min; // 0x38
        float cancel_range_max; // 0x3C
        bool speed_throttling; // 0x40
    };

    char* name; // 0x00
    float control_input_low; // 0x04
    float control_input_high; // 0x08
    float control_ramp_speed; // 0x0C
    int num_states; // 0x10
    blend_tree_state* states; // 0x14
    int num_actions; // 0x18
    blend_tree_action* actions; // 0x1C
};

// size 0xF0
struct anim_blend_tree_instance
{
    // size 0x14
    struct action_instance
    {
        float time_variance; // 0x00
        float current_blend; // 0x04
        float cur_time_percent; // 0x08
        unsigned int instance; // 0x0C
        unsigned int is_active : 1; // 0x10:0
        unsigned int is_blending_out : 1; // 0x10:1
    };

    float last_control_input; // 0x00
    float control_input; // 0x04
    float ramped_input; // 0x08
    float global_init_cyclic_percent; // 0x0C
    float state_weights[5]; // 0x10
    action_instance actions[10]; // 0x24
    bool input_initialized; // 0xEC
};

// size 0x120
struct v_anim
{
    int anim_handle; // 0x00
    unsigned int instance; // 0x04
    int index; // 0x08
    vanim_aliases alias; // 0x0C
    float blend; // 0x10
    unsigned int flags; // 0x14
    anim_blend_tree_instance blend_tree_instance; // 0x18
    anim_blend_tree_info* blend_tree_info; // 0x108
    timestamp variant_freq_ts; // 0x10C
    timestamp variant_length_ts; // 0x110
    v_anim* variant; // 0x114
    unsigned char prop_triggers_elapsed; // 0x118
    unsigned int used : 1; // 0x11C:0
    unsigned int is_state : 1; // 0x11C:1
    unsigned int is_pose : 1; // 0x11C:2
    unsigned int is_blend_tree : 1; // 0x11C:3
    unsigned int is_transition_action : 1; // 0x11C:4
    unsigned int is_variant : 1; // 0x11C:5
    unsigned int is_variant_fullbody : 1; // 0x11C:6
    unsigned int is_variant_cyclic : 1; // 0x11C:7
    unsigned int variant_forced : 1; // 0x11C:8
    unsigned int is_explicit_transition_action : 1; // 0x11C:9
    unsigned int ramping_out : 1; // 0x11C:10
    unsigned int move_to_end_pos : 1; // 0x11C:11
    unsigned int move_to_end_pos_post_process : 1; // 0x11C:12
    unsigned int move_to_end_orient : 1; // 0x11C:13
    unsigned int move_to_starting_orient : 1; // 0x11C:14
    unsigned int move_relative_to_obj : 1; // 0x11C:15
    unsigned int two_object_anim : 1; // 0x11C:16
    unsigned int ignore_y : 1; // 0x11C:17
    unsigned int prop_triggers_done : 1; // 0x11C:18
};

// size 0xF8
struct v_animate
{
    v_animate(); // 0x007F48D0

    v_animate* next; // 0x00
    v_animate* prev; // 0x04
    int aim_handle; // 0x08
    anim_set* animation_set; // 0x0C
    anim_set* base_animation_set; // 0x10
    anim_set_variants base_anim_set_variant; // 0x14
    anim_set_variants temp_anim_set_variant; // 0x18
    animation_group default_group_index; // 0x1C
    int max_simultaneous_anims; // 0x20
    v_anim* anims; // 0x24
    v_anim* desired_state; // 0x28
    v_anim* secondary_state; // 0x2C
    float state_blend_rate; // 0x30
    float parametric_blend_percent; // 0x34
    matrix starting_orient; // 0x38
    vector start_pos; // 0x5C
    vector anim_pos_delta; // 0x68
    vector current_pos_adjustment; // 0x74
    vector pos_adjustment_delta; // 0x80
    vector local_anim_movement; // 0x8C
    v_animate* attached_object; // 0x98
    human* hp; // 0x9C
    vector two_object_offset; // 0xA0
    float vertical_deviation; // 0xAC
    float dest_pos_time; // 0xB0
    float dest_orient_time; // 0xB4
    float end_heading_delta; // 0xB8
    float total_pos_adjustment; // 0xBC
    float rotate_speed; // 0xC0
    float start_heading_delta; // 0xC4
    float start_orient_time; // 0xC8
    float current_pos_time; // 0xCC
    float current_orient_time; // 0xD0
    float transition_ending_percent; // 0xD4
    unsigned int editor_flags; // 0xD8
    unsigned int ramped_editor_flags; // 0xDC
    vector interpolate_dest_uvec; // 0xE0
    float interpolate_time; // 0xEC
    float interpolate_cur_time; // 0xF0
    struct
    {
        unsigned int use_aim_state : 1; // 0x00:0
        unsigned int master_object : 1; // 0x00:1
        unsigned int first_process : 1; // 0x00:2
        unsigned int adjusting_orient : 1; // 0x00:3
        unsigned int set_state_transition_ending_frame : 1; // 0x00:4
        unsigned int use_velocity : 1; // 0x00:5
        unsigned int ignore_y : 1; // 0x00:6
        unsigned int next_state_start_at_zero : 1; // 0x00:7
        unsigned int interpolate_orient : 1; // 0x00:8
        unsigned int move_relative_to_obj : 1; // 0x00:9
    } flags; // 0xF4
};

// size 0x6C
struct vanim_play_parameters
{
    vanim_play_parameters(); // 0x00B62F40 (.h:280)

    vector ideal_start_pos; // 0x00
    matrix ideal_start_orient; // 0x0C
    float start_err_corr_length; // 0x30
    vector real_end_pos; // 0x34
    matrix real_end_orient; // 0x40
    float end_err_corr_begin_time; // 0x64
    bool should_initialize; // 0x68
};

void MAT_CHANGE_UVEC(matrix& orient, const vector up_vec); // 0x007F15A0 (.h:34), inline in the original header
void unused_dcf_Print_item_list(); // 0x008097D0 (:218)
int vanim_find_anim_handle(int set_index, int group_index, int anim_index); // 0x007F6C40 (:1914)
char* vanim_add_string(const char* string); // 0x007F6CC0 (:1950)
void vanim_mark_anim_string_pool(); // 0x007F6CD0 (:1955)
void vanim_restore_anim_string_pool_to_mark(); // 0x007F6CE0 (:1960)
void vanim_refresh_all(); // 0x007FDD10 (:1965)
void vanim_dlc_reset(int dlc_id); // 0x007FDD80 (:1984)
int vanim_get_action_handle(const v_animate* vp, int group_index, int action); // 0x007F6CF0 (:2003)
int vanim_get_state_handle(v_animate* vp, int group_index, int state); // 0x007F6D20 (:2011)
v_animate* vanim_create(const char* anim_set_name); // 0x007FDDE0 (:2020)
void vanim_delete(v_animate* vp); // 0x007F6D50 (:2061)
void vanim_system_init(); // 0x007FDE70 (:2076)
animation_state vanim_lookup_state(const char* name); // 0x007F6DB0 (:2093)
animation_action vanim_lookup_action(const char* name); // 0x007F6E20 (:2118)
animation_group vanim_lookup_group(const char* name); // 0x007F6EA0 (:2143)
const char* vanim_get_group_name(animation_group group_index); // 0x007F6EE0 (:2154)
unsigned int vanim_play_action(v_animate* vp, index_alias action_index, unsigned int animlib_flag, float speed_multiplier, float max_mix_percent); // 0x007F6EF0 (:2159)
unsigned int vanim_play_custom_action(v_animate* vp, index_alias action_handle, unsigned int animlib_flag, float speed_multiplier, float max_mix_percent); // 0x007F7100 (:2209)
void vanim_change_group(v_animate* vp, animation_group group_index, index_alias state_index, float blend_time, bool start_at_zero); // 0x008097E0 (:2254)
void vanim_change_state(v_animate* vp, index_alias state_index, index_alias action_index, float blend_time, bool start_at_zero); // 0x007FDF50 (:2264)
void vanim_change_custom_state(v_animate* vp, index_alias state_handle, index_alias action_handle, float blend_time, bool start_at_zero); // 0x007F72B0 (:2327)
v_anim* vanim_set_state_blend_pose(v_animate* vp, index_alias state_index, float blend_percent, unsigned int flags); // 0x00809840 (:2452)
v_anim* vanim_set_state_custom_blend_pose(v_animate* vp, index_alias state_index, float blend_percent, unsigned int flags); // 0x007FE0B0 (:2496)
v_anim* vanim_set_state_directional_blend_pose(v_animate* vp, vector lean, float blend_rate); // 0x007FE220 (:2566)
void vanim_set_transition_ending_percent(v_animate* vp, float percent); // 0x007F7600 (:2678)
void vanim_process_variant(v_animate* vp, int va_index, vanim_variants play_variant, bool immediate); // 0x007FE6A0 (:2690)
void vanim_process_states(v_animate* vp); // 0x007FEFB0 (:2871)
void vanim_process_completed_actions(v_animate* vp); // 0x007FF770 (:3024)
void vanim_ramp_out_action(v_animate* vp, int action_alias_index, int num_frames); // 0x007F7620 (:3044)
void vanim_pause_and_ramp_out_action(v_animate* vp, int action_alias_index, int num_frames); // 0x007F7670 (:3054)
bool vanim_has_trigger_just_elapsed(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx); // 0x007F76E0 (:3065)
bool vanim_has_trigger_elapsed(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx); // 0x007F7730 (:3079)
float vanim_get_trigger_time(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx); // 0x007F7780 (:3093)
float vanim_get_anim_length(v_animate* vp, int action_alias_index); // 0x007F7860 (:3123)
int vanim_get_anim_handle(v_animate* vp, int action_alias_index); // 0x007F79C0 (:3158)
void vanim_get_anim_translation(v_animate* vp, int index, vector& translation); // 0x007F7AF0 (:3181)
float vanim_get_anim_velocity(v_animate* vp, int index); // 0x007F7D80 (:3217)
int vanim_get_anim_index(v_animate* vp, int action_alias_index); // 0x007F7F70 (:3263)
unsigned int vanim_get_anim_instance_handle(v_animate* vp, int action_alias_index); // 0x007F7FA0 (:3273)
bool vanim_is_in_state(v_animate* vp, int state_alias_index); // 0x007F7FD0 (:3282)
bool vanim_is_playing_action(v_animate* vp, int action_alias_index); // 0x007F8000 (:3291)
bool vanim_is_action_ending_this_frame(v_animate* vp, int action_alias_index); // 0x007F8020 (:3301)
bool vanim_is_moving_animation(v_animate* vp, int index); // 0x007F80B0 (:3318)
bool vanim_is_transitioning_to_still_state(v_animate* vp); // 0x007F8130 (:3337)
bool vanim_check_editor_flag(v_animate* vp, unsigned int editor_flag); // 0x007F8180 (:3353)
bool vanim_check_editor_flag_ramped(v_animate* vp, unsigned int editor_flag); // 0x007F81A0 (:3358)
int vanim_get_anim_frame(v_animate* vp, int action_alias_index); // 0x007F81C0 (:3363)
void vanim_process_obj_movements(v_animate* vp, const vector& pos, matrix& orient, vector& velocity); // 0x007F8200 (:3375)
void vanim_process_movement_adjustments(v_animate* vp, const vector& pos, matrix& orient, vector& velocity); // 0x007F8700 (:3473)
void vanim_set_blend_tree_init_control_input(v_animate* vp, int anim_index, float control_value); // 0x007F9390 (:3642)
void vanim_set_blend_tree_control_input(v_animate* vp, int anim_index, float control_value); // 0x007F93D0 (:3652)
void vanim_set_all_blend_tree_control_inputs(v_animate* vp, int anim_index, float control_value); // 0x007F9460 (:3662)
float vanim_get_blend_tree_ramped_input(v_animate* vp, int anim_index); // 0x007F9520 (:3676)
bool vanim_is_blend_tree(v_animate* vp, int anim_index); // 0x007F9560 (:3687)
void vanim_set_variant(v_animate* vp, int anim_index, vanim_variants variant, bool immediate, bool override_anim); // 0x007FF7F0 (:3698)
void vanim_swap_animation(v_animate* vp, int anim_index, index_alias action_index); // 0x007F9590 (:3738)
void vanim_set_action_dest_position(v_animate* vp, vector& current_pos, matrix& current_orient, vector world_pos, int action_alias_index, float time, bool ignore_y); // 0x007F9640 (:3765)
void vanim_set_action_dest_orient(v_animate* vp, matrix& current_orient, const matrix& dest_orient, int action_alias_index, float time); // 0x007F9890 (:3804)
void vanim_set_action_starting_orient(v_animate* vp, vector& current_pos, matrix& current_orient, vector& desired_starting_pos, matrix& desired_starting_orient, int action_alias_index, float time); // 0x007FF8E0 (:3855)
void vanim_set_action_relative_space(v_animate* vp, int action_alias_index, vanim_play_parameters* vpp); // 0x007FFC10 (:3921)
void vanim_interpolate_uvec(v_animate* vp, human* hp, const vector& desired_uvec, const float time); // 0x007F9A40 (:4023)
void vanim_attach_actions(human* hp_1, human* hp_2, int action_alias_index_1, int action_alias_index_2, const vector& initial_offset, float vertical_deviation); // 0x007F9B10 (:4044)
void vanim_detach_actions(v_animate* vp); // 0x007F9C80 (:4085)
void vanim_blend_animations_forming_box(v_animate* vp, int* state_handles, float x, float y); // 0x007F9E20 (:4133)
void vanim_blend_states_linear(v_animate* vp, int state_handle_1, int state_handle_2, float x); // 0x007F9E30 (:4156)
v_anim* vanim_get_state(v_animate* vp, state_enum state); // 0x007F9E40 (:4169)
v_anim* vanim_get_action(v_animate* vp, int action_alias_index); // 0x007F9EC0 (:4201)
void vanim_set_start_time(v_animate* vp, int action_alias_index, float anim_time, float anim_percent); // 0x007F9EF0 (:4212)
float vanim_get_cur_time(v_animate* vp, int action_alias_index); // 0x007F9FB0 (:4229)
void vanim_set_cur_time_and_speed(v_animate* vp, int action_alias_index, float anim_time, float anim_speed); // 0x007FA000 (:4243)
void execute_prop_trigger(prop_trigger* ptrig, object* objp); // 0x00800070 (:4259)
void vanim_fast_forward_prop_triggers(v_animate* vp, int action_alias_index, object* objp); // 0x00800460 (:4464)
void vanim_check_prop_handles(v_animate* vp, object* objp); // 0x00800570 (:4492)
prop_type vanim_get_prop_type_from_name(char* prop_type_name); // 0x007FA080 (:4596)
void vanim_clear_states(v_animate* vp); // 0x00800B60 (:4611)
anim_set* anim_set_find(const char* name); // 0x007FA120 (:4628)
void vanim_set_base_animation_set_variant(v_animate* vp, anim_set_variants variant_type); // 0x007FA270 (:4735)
void vanim_set_temp_animation_set_variant(v_animate* vp, anim_set_variants variant_type); // 0x007FA2D0 (:4752)
anim_set_variants vanim_get_base_animation_set_variant(const v_animate* vp); // 0x007FA2F0 (:4771)
anim_set_variants vanim_get_temp_animation_set_variant(const v_animate* vp); // 0x007FA300 (:4775)
anim_set_variants vanim_get_animation_set_variant_from_name(const char* name); // 0x007FA310 (:4786)
void vanim_set_thread_ownership(); // 0x007FA350 (:4799)

// The parsed animation tables (vanim_parse_anim_tables).
extern anim_set Anim_sets[70]; // 0x025BCF28
extern int Num_anim_sets; // 0x025EF8EC
extern anim_blend_tree_info Blend_trees[300]; // 0x025E03C0
extern int Num_blend_trees; // 0x025EF8F0
extern anim_variant Anim_variants[256]; // 0x025E8098
extern int Num_anim_variants; // 0x025EF8AC
extern anim_foley_set Anim_foley_sets[64]; // 0x025BC118
extern unsigned int Num_anim_foley_sets; // 0x025EF8E8
extern int Num_free_vanims; // 0x025EF8F4

// 0x00B62F40 (.h:280)
inline vanim_play_parameters::vanim_play_parameters()
{
    memset(this, 0, sizeof(vanim_play_parameters));
}
