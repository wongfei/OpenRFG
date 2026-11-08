// Original: volition/anim_lib/code/anim_lib.h
// Implementation: volition/anim_lib/code/anim_lib.cpp
//
// The animation instance runtime of anim_lib and its public API (anim_lib.cpp; the animlib_* and aim functions).
// - anim_instance: the skeleton state of one animated entity: its rig, the cached bone matrices (vertex-to-model and
//   instanced model), bone and morph info, the parent rig mapping and the morph callback. Tags and bones are looked up
//   by name; get_tag_world_coords places a tag with the entity's base transform.
// - anim_instance_manager: an anim_instance plus up to 10 playing animations (anim_instance_info): play / stop /
//   swap, looping animations driven by cyclic_percent, ramps, triggers, bone modifiers, and the per-frame
//   process_animations (advance times, ramp, remove finished) and transform_all_bones_pre/post. Instances are linked
//   in a list (anim_instance_next/prev) and addressed by handle ("aim handle", character_instance::aim_handle).
// Placement of the functions inferred (anim_lib.h has no inline code recorded).
#pragma once

#include "volition/anim_lib/code/anim_lib_types.h"
#include "volition/vlib/code/math/vector3.h"

struct anim_entity_class_bone_info;
class animlib_compressed_data_reader;
struct animlib_page_pool_manager;
struct cpu_job;
union cpu_job_arg;
class event_trigger;
struct human_spinebend_data;
class matrix;
class matrix43;
class mempool;
class quaternion;

// Placement of the following types inferred (rfg.pdb records no inline code for them; player_minimal.md P2).

// How a registered bone modifier changes a bone (anim_instance_manager::process_bone_modifier).
enum bone_modifier
{
    BONE_MODIFIER_PRE_ROTATE = 1,
    BONE_MODIFIER_POST_ROTATE = 2,
    BONE_MODIFIER_ROTATE = 3,
    BONE_MODIFIER_COPY = 4,
    BONE_MODIFIER_FORCE = 5,
    BONE_MODIFIER_POSITION_OFFSET = 6,
    BONE_MODIFIER_ROTATE_RC = 7,
};

// Per entity bone state of an instance, after the two matrix caches in the bone page.
// size 0xC
struct anim_bone_info
{
    struct
    {
        unsigned int instanced_model_matrix_valid : 1; // 0x00:0
        unsigned int vertex_to_model_matrix_valid : 1; // 0x00:1
        unsigned int use_special_func : 1; // 0x00:2, a bone modifier is registered
        unsigned int is_weighted : 1; // 0x00:3, an animation moved the bone this frame
        unsigned int tmp_matrices_valid : 1; // 0x00:4
    } flags; // 0x00
    int data_idx; // 0x04, s_bone_modifier_data::bones index
    bone_modifier modifier; // 0x08
};

// One active morph target of an instance (blend_all_morphs keeps up to 20, sorted by blend_amount).
// size 0xC
struct anim_morph_info
{
    const char* morph_set; // 0x00
    unsigned int name_check; // 0x04
    float blend_amount; // 0x08
};

// size 0x4C
struct bone_modifier_data
{
    bone_modifier_data(); // 0x00C90A70

    matrix old_bone_orient; // 0x00
    matrix desired_bone_orient; // 0x24
    float forced_bone_blend; // 0x48
};

// size 0x480
struct s_bone_modifier_data
{
    s_bone_modifier_data(); // 0x00C94990

    bone_modifier_data bones[15]; // 0x00
    unsigned int padding[3]; // 0x474
};

// One queued per-frame animation job of an instance (animlib_request_batch_start, animlib_process_command).
// size 0xA0
struct animlib_transform_command
{
    human_spinebend_data* spinebend_data; // 0x00
    int num_bone_lods; // 0x04
    int* bone_lods; // 0x08
    unsigned int aim_handle; // 0x0C
    float frametime; // 0x10
    unsigned int bone_lod_level; // 0x14
    unsigned int process_instance : 1; // 0x18:0
    unsigned int transform_all_bones_pre : 1; // 0x18:1
    unsigned int process_spinebending : 1; // 0x18:2
    unsigned int transform_all_bones_post : 1; // 0x18:3
    unsigned int cpu_processing : 1; // 0x18:4, run on the calling thread (no scheduler job)
    unsigned int spu_pause : 1; // 0x18:5
    unsigned int animation_data_sizes[10]; // 0x1C
    unsigned int animation_data_offsets[10]; // 0x44
    unsigned int rig_data_start; // 0x6C
    unsigned int rig_dma_size; // 0x70
    unsigned int parent_transform_data; // 0x74
    unsigned int instanced_model_offset; // 0x78
    unsigned int output_data_start[4]; // 0x7C
    unsigned int output_data_size[4]; // 0x8C
    void* human_ptr; // 0x9C
};

// size 0x130
class anim_instance
{
public:
    void get_virtual_bone_pos(int bone_index, vector& pos); // 0x00C8AE50 (anim_lib_instance.cpp:26)
    matrix43 calculate_final_transformation_matrix(int tag_bone_index); // 0x00C8AF10 (anim_lib_instance.cpp:49)
    matrix43 get_bone_transformation(int bone_index); // 0x00C8AFD0 (anim_lib_instance.cpp:94)
    void set_bone_transformation(int entity_bone_index, const vector& pos, const matrix& orient); // 0x00C8AFF0 (anim_lib_instance.cpp:106)
    int find_bone(const char* name); // 0x00C95DC0 (anim_lib_instance.cpp:134)
    int find_tag(const char* name); // 0x00C95E10 (anim_lib_instance.cpp:155)
    int find_nth_tag(const char* name, int num_tags); // 0x00C95E80 (anim_lib_instance.cpp:171)
    int find_tag_with_active_vids(const char* name, int num_active_vids, const int* active_vids); // 0x00C95F00 (anim_lib_instance.cpp:190)
    void get_tag_local_coords(int tag_index, vector& pos, matrix* orient); // 0x00C8B260 (anim_lib_instance.cpp:235)
    void get_tag_world_coords(int tag_index, const matrix& base_orient, const vector& base_pos, vector& pos, matrix* orient); // 0x00C8B300 (anim_lib_instance.cpp:271)

    matrix43* cache_vertex_to_model; // 0x00
    matrix43* cache_instanced_model; // 0x04
    anim_bone_info* bone_info; // 0x08
    anim_morph_info* morph_info; // 0x0C
    animlib_page_pool_manager* page_pool_manager; // 0x10
    animlib_page_pool_manager* morph_anim_pool_manager; // 0x14
    anim_rig* rig; // 0x18
    int parent_rig_aim_handle; // 0x1C
    int parent_rig_bone_indecies[64]; // 0x20
    bool cache_instanced_model_valid; // 0x120
    anim_entity_class_bone_info* entity_bone_info; // 0x124
    void (*morph_func)(unsigned char&, anim_morph_info*, void*); // 0x128
    void* morph_data; // 0x12C
};

// size 0x410
class anim_instance_manager : public anim_instance
{
public:
    void init(); // 0x00C8BF70 (anim_lib_instance_manager.cpp:880)
    bool allocate_bone_matrices(); // 0x00C8E710 (anim_lib_instance_manager.cpp:4112)
    void allocate_morph_info(); // 0x00C8E940 (anim_lib_instance_manager.cpp:4182)
    void clear_bone_info(); // 0x00C8E690 (anim_lib_instance_manager.cpp:4069)
    void clear_morph_info(); // 0x00C8E6C0 (anim_lib_instance_manager.cpp:4081)
    int get_morph_info(anim_morph_info** morph_info_ptr); // 0x00C8E6E0 (anim_lib_instance_manager.cpp:4096)
    void get_bone_velocities(int num_bones, int* entity_bone_indices, vector* rotational_vel, vector* translational_vel); // 0x00C9A140 (anim_lib_instance_manager.cpp:3956)
    void reset_looping_animations(); // 0x00C980B0 (anim_lib_instance_manager.cpp:3126)
    void clear_looping_animations(bool immediate); // 0x00C93DD0 (anim_lib_instance_manager.cpp:3136)
    void set_cyclic_percent(float _cyclic_percent); // 0x00C8DC90 (anim_lib_instance_manager.cpp:3161)
    unsigned int play_animation(int handlep, unsigned int flags, float start_time_sec, float base_mix_percent, float ramp_in_sec, float speed_multiplier); // 0x00C94270 (anim_lib_instance_manager.cpp:3373)
    unsigned int play_animation_looping(int handlep, unsigned int flags, float base_mix_percent, float ramp_in_sec, float speed_multiplier); // 0x00C8DE00 (anim_lib_instance_manager.cpp:3237)
    void stop_animation(unsigned int handlep, float blend_out_time_sec); // 0x00C980D0 (anim_lib_instance_manager.cpp:3619)
    unsigned int swap_animation(unsigned int handlep, int new_anim_handle); // 0x00C94630 (anim_lib_instance_manager.cpp:3646)
    int get_animation_time_since_start(const anim_instance_info* anim_info) const; // 0x00C93BE0 (anim_lib_instance_manager.cpp:2954)
    void clear_non_looping_animations(); // 0x00C93FA0 (anim_lib_instance_manager.cpp:3177)
    unsigned int get_hold_animation_handle() const; // 0x00C8DCB0 (anim_lib_instance_manager.cpp:3166)
    void set_current_time(unsigned int handlep, float time_sec); // 0x00C94210 (anim_lib_instance_manager.cpp:3351)
    void set_current_time(anim_instance_info* anim_info, float time_sec); // 0x00C94160 (anim_lib_instance_manager.cpp:3328)
    void set_trigger_flags(unsigned int instance_handle, int trigger_num, bool just_triggered, bool triggered, bool supress_trigger); // 0x00C8DFB0 (anim_lib_instance_manager.cpp:3470)
    bool trigger_should_have_elapsed(unsigned int instance_handle, int trigger_id); // 0x00C94430 (anim_lib_instance_manager.cpp:3502)
    void transform_all_bones_pre(matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level); // 0x00C99060 (anim_lib_instance_manager.cpp:1136)
    void transform_all_bones_post(matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level); // 0x00C964A0 (anim_lib_instance_manager.cpp:1142)
    void get_local_bone_matrices(matrix43* bone_matrix, int* rig_bone_transform_list, int bone_lod_level); // 0x00C964D0 (anim_lib_instance_manager.cpp:1156)
    void instance_bone_matrices(matrix43* bone_matrix, int* bone_transform_list, int bone_lod_level); // 0x00C93230 (anim_lib_instance_manager.cpp:1488)
    void update_vertex_to_model_matrices(int* bone_transform_list, int bone_lod_level); // 0x00C8C940 (anim_lib_instance_manager.cpp:1611)
    void process_ramp(anim_instance_info* anim_info, int tick_delta); // 0x00C97310 (anim_lib_instance_manager.cpp:1725)
    int get_looping_animation_time(float cyclic_percentp, anim_instance_info* anim_info, anim_animation* anim) const; // 0x00C8CE70 (anim_lib_instance_manager.cpp:1793)
    void process_animation_looping(int* looping_indices, int num_animations, int tick_delta, bool allow_state_change); // 0x00C974A0 (anim_lib_instance_manager.cpp:1946)
    void process_animation_non_looping(int* non_looping_indices, int num_animations, int tick_delta, bool allow_state_change); // 0x00C97D10 (anim_lib_instance_manager.cpp:2138)
    void clear_trigger_just_elapsed_flags(); // 0x00C8D250 (anim_lib_instance_manager.cpp:2226)
    void process_animations(float frametime, bool allow_state_change); // 0x00C99070 (anim_lib_instance_manager.cpp:2239)
    void remove_inactive_anims(); // 0x00C97EF0 (anim_lib_instance_manager.cpp:2300)
    void process_bone_modifier(bone_modifier modifier, matrix43& mat, const matrix43& parent_mat, matrix43& local_matrix, bone_modifier_data* data); // 0x00C8D280 (anim_lib_instance_manager.cpp:2313)
    void get_ramp_in_out_times(anim_instance_info* anim_info, int& ramp_in, int& ramp_out) const; // 0x00C8D760 (anim_lib_instance_manager.cpp:2415)
    int get_animation_time_to_end(const anim_instance_info* anim_info) const; // 0x00C93A30 (anim_lib_instance_manager.cpp:2451)
    void remove_active_anim(unsigned int instance_handle); // 0x00C93A80 (anim_lib_instance_manager.cpp:2478)
    int find_active_anim(unsigned int anim_handle) const; // 0x00C8D840 (anim_lib_instance_manager.cpp:2534)
    void register_bone_modifier(bone_modifier modifier, int bone_index, int storage_idx, matrix& delta_orient, matrix* old_bone_orient, float blend_pct); // 0x00C984E0 (anim_lib_instance_manager.cpp:4493)
    void get_bone_matrices_from_instanced(matrix43* bone_matrix); // 0x00C8C4E0 (anim_lib_instance_manager.cpp:1081)
    void invalidate_cache_matrix(int bone_index, bool instanced_model, bool vertex_to_model); // 0x00C8BFE0 (anim_lib_instance_manager.cpp:1011)
    void invalidate_cache_matrix_list(int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model); // 0x00C8C080 (anim_lib_instance_manager.cpp:1043)
    void validate_cache_matrix(int bone_index, bool instanced_model, bool vertex_to_model); // 0x00C8C040 (anim_lib_instance_manager.cpp:1028)
    void validate_cache_matrix_list(int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model); // 0x00C8C100 (anim_lib_instance_manager.cpp:1050)
    void blend_all_morphs(); // 0x00C96080 (anim_lib_instance_manager.cpp:898)
    void add_morph_special_func(void (*func)(unsigned char&, anim_morph_info*, void*), void* data); // 0x00C8EA30 (anim_lib_instance_manager.cpp:4219)
    void disable_instance_bone_special_func(int bone_index); // 0x00C8EA50 (anim_lib_instance_manager.cpp:4226)
    bool is_bone_animating(unsigned int handlep, int bone_index); // 0x00C983D0 (anim_lib_instance_manager.cpp:4347)
    bool is_bone_animating(int bone_index); // 0x00C982E0 (anim_lib_instance_manager.cpp:4305)
    float get_bone_animating_fraction(int bone_index); // 0x00C98180 (anim_lib_instance_manager.cpp:4256)
    vector get_base_pose_bone_pos(int entity_bone_index); // 0x00C8EA70 (anim_lib_instance_manager.cpp:4231)
    vector get_base_pose_tag_pos(int entity_tag_index); // 0x00C8EB60 (anim_lib_instance_manager.cpp:4389)
    matrix43* get_render_matrices(); // 0x00C8EC00 (anim_lib_instance_manager.cpp:4406)
    void copy_render_matrices(matrix43* dest); // 0x00C8EC10 (anim_lib_instance_manager.cpp:4413)
    unsigned int get_anim_flags(unsigned int handlep) const; // 0x00C8DC70 (anim_lib_instance_manager.cpp:3096)
    bool is_anim_looping(unsigned int handlep) const; // 0x00C8E3B0 (anim_lib_instance_manager.cpp:3876)
    void set_speed_multiplier(unsigned int handlep, float speed_multiplier); // 0x00C8E400 (anim_lib_instance_manager.cpp:3885)
    void set_start_frame(unsigned int handlep, int start_frame); // 0x00C8E450 (anim_lib_instance_manager.cpp:3893)
    void set_looping_anim_start_percent(unsigned int handlep, float init_cyclic_percent); // 0x00C8E4C0 (anim_lib_instance_manager.cpp:3901)
    float get_init_cyclic_percent(unsigned int handlep); // 0x00C8E5A0 (anim_lib_instance_manager.cpp:3915)
    void set_init_cyclic_percent(unsigned int handlep, float init_cyclic_percent); // 0x00C8E5F0 (anim_lib_instance_manager.cpp:3923)
    void sync_anims(anim_instance_manager* aim_dst, unsigned int src_handle, unsigned int dst_handle); // 0x00C8DCD0 (anim_lib_instance_manager.cpp:3202)
    float get_heading_change(int future_ticks) const; // 0x00C99930 (anim_lib_instance_manager.cpp:2671)
    float get_root_heading() const; // 0x00C99D20 (anim_lib_instance_manager.cpp:2756)
    void get_position_change(vector& action_delta_pos, vector& state_delta_pos, vector& anim_local_delta_pos) const; // 0x00C991D0 (anim_lib_instance_manager.cpp:2555)
    int get_parent_bone_index(int entity_bone_index) const; // 0x00C8DB60 (anim_lib_instance_manager.cpp:2921)
    float anim_length(int handlep) const; // 0x00C93D70 (anim_lib_instance_manager.cpp:3112)
    float time_till_animation_finished(unsigned int handlep) const; // 0x00C93C30 (anim_lib_instance_manager.cpp:2985)
    float get_current_time(unsigned int handlep) const; // 0x00C93D10 (anim_lib_instance_manager.cpp:3042)
    void get_animation_times(unsigned int handlep, int& prev_time, int& cur_time); // 0x00C8DBC0 (anim_lib_instance_manager.cpp:3047)
    int get_current_frame(unsigned int handlep) const; // 0x00C93D40 (anim_lib_instance_manager.cpp:3064)
    float get_current_mix_percent(unsigned int handlep) const; // 0x00C8DC20 (anim_lib_instance_manager.cpp:3069)
    float get_current_speed_multiplier(unsigned int handlep) const; // 0x00C8DC40 (anim_lib_instance_manager.cpp:3079)
    int get_current_ticks(unsigned int handlep) const; // 0x00C93CB0 (anim_lib_instance_manager.cpp:3023)
    bool trigger_has_elapsed(int trigger_id, unsigned int handlep, int trigger_index) const; // 0x00C8E030 (anim_lib_instance_manager.cpp:3718)
    bool trigger_just_elapsed(int trigger_id, unsigned int handlep, int trigger_index) const; // 0x00C8E150 (anim_lib_instance_manager.cpp:3764)
    bool looping_trigger_has_elapsed(int trigger_id) const; // 0x00C8E270 (anim_lib_instance_manager.cpp:3810)
    float time_till_trigger(int trigger_id, unsigned int handlep) const; // 0x00C8E2F0 (anim_lib_instance_manager.cpp:3836)
    bool has_trigger(int trigger_id, unsigned int handlep) const; // 0x00C8E290 (anim_lib_instance_manager.cpp:3816)
    int trigger_time(int, unsigned int) const;
    bool has_frame_elapsed(unsigned int handlep, int frame) const; // 0x00C8E370 (anim_lib_instance_manager.cpp:3866)
    bool anim_is_ramping_out(unsigned int handlep) const; // 0x00C8ECE0 (anim_lib_instance_manager.cpp:4439)
    bool active_anim_is_looping(unsigned int handlep) const; // 0x00C8ECC0 (anim_lib_instance_manager.cpp:4428)
    int anim_handle_from_instance_handle(unsigned int handlep); // 0x00C8ED00 (anim_lib_instance_manager.cpp:4453)
    bool anim_is_playing(unsigned int handlep); // 0x00C8ED60 (anim_lib_instance_manager.cpp:4466)
    anim_priority get_anim_priority(unsigned int handlep); // 0x00C8EDA0 (anim_lib_instance_manager.cpp:4474)
    void allocate_bone_modifiers(); // 0x00C984C0 (anim_lib_instance_manager.cpp:4483)
    void process_triggers(); // 0x00C8CFF0 (anim_lib_instance_manager.cpp:1863)
private:
    anim_instance_info* add_active_anim(int anim_handle, int priority); // 0x00C8D930 (anim_lib_instance_manager.cpp:2849)
    const anim_instance_info* get_active_anim(unsigned int handlep) const; // 0x00C8D880 (anim_lib_instance_manager.cpp:2818)
    anim_instance_info* get_active_anim(unsigned int handlep); // 0x00C8D8E0 (anim_lib_instance_manager.cpp:2832)
    void stop_animation_internal(anim_instance_info* cur_anim, float blend_out_time_sec); // 0x00C944F0 (anim_lib_instance_manager.cpp:3543)
public:
    anim_instance_manager(); // 0x00C8B3E0

    int handle; // 0x130
    int num_active_anims; // 0x134
    int animation_to_freeze_sig; // 0x138
    float cyclic_percent; // 0x13C
    float old_cyclic_percent; // 0x140
    float cyclic_speed; // 0x144
    unsigned char frozen; // 0x148
    unsigned char num_active_morphs; // 0x149
    anim_instance_info active_anims[10]; // 0x14C
    anim_instance_manager* anim_instance_next; // 0x3CC
    anim_instance_manager* anim_instance_prev; // 0x3D0
    s_bone_modifier_data* bone_modifiers; // 0x3D4
    bool do_bone_scaling; // 0x3D8
    vector bone_scaling; // 0x3DC
    vector bone_bbox_min; // 0x3E8
    vector bone_bbox_max; // 0x3F4
    unsigned int locked; // 0x400
    unsigned int padding[1]; // 0x404
    float partial_frame_amount; // 0x408
    unsigned int rig_data_size; // 0x40C
};

// Game trace only (new code): the instance whose commands anim_lib.cpp checks for non-finite bones, -1 for none.
inline int Game_trace_anim_watch_handle = -1;

extern animlib_transform_command Animlib_transform_command_buffer[176]; // 0x038D1DF0
extern int Num_animlib_transform_commands; // 0x039ACB18
extern event_trigger* Anim_scheduler_trigger; // 0x038C288C
extern bool Animlib_multi_thread; // 0x016F0888
extern bool Bash_spinebend_bones; // 0x039ACB1C
extern bool Spinebend_enabled; // 0x016F0889
extern int Bone_parent[9]; // 0x016F0894

void animlib_system_init(); // 0x00C90BF0 (:205)
void animlib_system_close(); // 0x00C90C10 (:215)
void animlib_reset_all_anims(bool including_perm); // 0x00C89210 (:232)
void animlib_reset_anim_system(); // 0x00C90C40 (:250)
void animlib_do_frame_pre_transform(bool clear_morphs); // 0x00C90C50 (:257)
bool animlib_is_rig_loaded(const char* filename); // 0x00C90C60 (:274)
anim_rig* animlib_get_loaded_rig(const char* filename); // 0x00C90C80 (:285)
int animlib_get_rig_index(const char* filename); // 0x00C90CA0 (:299)
anim_rig* animlib_new_anim_rig(const char* filename, unsigned char* data_buffer, unsigned int buffer_size, bool permanent); // 0x00C98800 (:312)
void animlib_reset_all_anim_rig(bool including_perm); // 0x00C90CB0 (:317)
void animlib_delete_anim_rig(anim_rig* rig); // 0x00C90CF0 (:330)
int animlib_find_bone_by_substring(const anim_rig* rig, const char* substr); // 0x00C98810 (:336)
int animlib_find_bone_by_name(const anim_rig* rig, const char* bone_name); // 0x00C98830 (:342)
int animlib_find_tag_by_name(const anim_rig* rig, const char* tag_name); // 0x00C98850 (:348)
void animlib_get_bone_tree_list(int aim_handle, int root_bone_index, int max_bones, int& num_bones, int* bone_list); // 0x00C90D10 (:354)
int animlib_new_anim_instance_manager(anim_rig* rig, int entity_class_id); // 0x00C94A10 (:365)
void animlib_register_entity_base_bone_set(const anim_rig* rig, const animlib_bones_used_info& bones_used_info); // 0x00C90D70 (:370)
void animlib_unregister_entity_base_bone_set(int entity_class_id); // 0x00C90D80 (:375)
void animlib_delete_anim_instance(int aim_handle); // 0x00C9AEA0 (:380)
anim_rig* animlib_get_rig(int aim_handle); // 0x00C90DB0 (:386)
void animlib_set_rig(int aim_handle, anim_rig* _rig); // 0x00C90DF0 (:393)
void animlib_set_parent_rig(int base_aim_handle, int parent_aim_handle); // 0x00C94A20 (:401)
void animlib_get_bone_velocities(int aim_handle, int num_bones, int* entity_bone_indices, vector* rotational_vel, vector* translational_vel); // 0x00C9AEF0 (:422)
void animlib_reset_looping_animations(int aim_handle); // 0x00C98870 (:429)
void animlib_clear_looping_animations(int aim_handle, bool immediate); // 0x00C94B70 (:436)
void animlib_clear_non_looping_animations(int aim_handle); // 0x00C94BB0 (:443)
void animlib_set_cyclic_percent(int aim_handle, float cyclic_percent); // 0x00C90E40 (:455)
unsigned int animlib_play_animation(int aim_handle, int anim_handle, unsigned int flags, float start_time_sec, float base_mix_percent, float speed_multiplier, float ramp_in_sec); // 0x00C94BF0 (:464)
unsigned int animlib_play_animation_looping(int aim_handle, int anim_handle, float base_mix_percent, unsigned int flags, float speed_multiplier, float ramp_in_sec); // 0x00C90EC0 (:472)
void animlib_stop_animation(int aim_handle, unsigned int handle, float blend_out_time_sec); // 0x00C988C0 (:481)
unsigned int animlib_swap_animation(int aim_handle, unsigned int handle, int new_anim_handle); // 0x00C94C80 (:489)
void animlib_sync_anims(int aim_handle_src, int aim_handle_dst, unsigned int src_handle, unsigned int dst_handle); // 0x00C90F40 (:497)
void animlib_process_instance(int aim_handle, float frametime); // 0x00C9AF40 (:505)
void animlib_process_instance_min(int aim_handle); // 0x00C98920 (:512)
int animlib_get_num_active_anims(int aim_handle); // 0x00C90FC0 (:519)
void animlib_get_bone_transform(int aim_handle, int bone_index, matrix43& transform); // 0x00C91000 (:526)
void animlib_set_bone_transform(int aim_handle, unsigned int entity_bone_index, const vector& pos, const matrix& orient); // 0x00C91090 (:533)
void animlib_transform_all_bones_pre(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level); // 0x00C9AF90 (:540)
void animlib_transform_all_bones_post(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level); // 0x00C98960 (:547)
void animlib_blend_all_morphs(int aim_handle); // 0x00C989C0 (:554)
void animlib_instance_bone_matrices(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level); // 0x00C94CD0 (:565)
void animlib_get_bone_matrices_from_instanced(int aim_handle, matrix43* bone_matrices); // 0x00C910E0 (:572)
void animlib_set_special_bone_blend_type(int aim_handle, int bone_index); // 0x00C91120 (:579)
void animlib_set_bone_transform(int aim_handle, int entity_bone_index, const vector& pos, const matrix& orient); // 0x00C91170 (:587)
void animlib_update_vertex_to_model_matrices(int aim_handle); // 0x00C911C0 (:595)
void animlib_invalidate_cache_bone(int aim_handle, int bone_index, bool instanced_model, bool vertex_to_model); // 0x00C91200 (:602)
void animlib_invalidate_cache_bone_list(int aim_handle, int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model); // 0x00C91250 (:609)
void animlib_validate_cache_bone_list(int aim_handle, int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model); // 0x00C912A0 (:616)
bool animlib_allocate_bones(int aim_handle); // 0x00C912F0 (:629)
void animlib_clear_bone_info(int aim_handle); // 0x00C91330 (:644)
bool animlib_allocate_morphs(int aim_handle); // 0x00C91390 (:657)
void animlib_clear_morph_info(int aim_handle); // 0x00C913E0 (:673)
int animlib_get_morph_info(int aim_handle, anim_morph_info** morph_info_ptr); // 0x00C91430 (:680)
void animlib_add_morph_special_func(int aim_handle, void (*func)(unsigned char&, anim_morph_info*, void*), void* data); // 0x00C91490 (:687)
void animlib_disable_instance_bone_special_func(int aim_handle, int bone_index); // 0x00C914E0 (:695)
vector animlib_get_base_pose_bone_pos(int aim_handle, int entity_bone_index); // 0x00C91530 (:703)
bool animlib_is_bone_animating(int aim_handle, int bone_index); // 0x00C98A00 (:710)
bool animlib_is_bone_animating(int aim_handle, unsigned int handle, int bone_index); // 0x00C98A40 (:717)
float animlib_get_bone_animating_fraction(int aim_handle, int bone_index); // 0x00C98A90 (:724)
vector animlib_get_base_pose_tag_pos(int aim_handle, int entity_tag_index); // 0x00C915A0 (:732)
anim_rig* animlib_create_duplicate_rig(int aim_handle, mempool* pool); // 0x00C9B460 (:739)
void animlib_get_tag_world_coords(int aim_handle, int tag_index, const vector& base_pos, const matrix& base_orient, vector& pos, matrix* orient); // 0x00C91690 (:746)
void animlib_get_tag_local_coords(int aim_handle, int tag_index, vector& pos, matrix* orient); // 0x00C916E0 (:753)
float animlib_get_animation_length(int aim_handle, int handle); // 0x00C94D20 (:760)
float animlib_get_animation_length(int anim_handle); // 0x00C91730 (:767)
float animlib_get_current_time(int aim_handle, unsigned int handle); // 0x00C94D60 (:780)
void animlib_get_animation_times(int aim_handle, unsigned int handle, int& prev_time, int& cur_time); // 0x00C91790 (:787)
int animlib_get_current_frame(int aim_handle, unsigned int handle); // 0x00C94DC0 (:794)
float animlib_get_current_mix_percent(int aim_handle, unsigned int handle); // 0x00C917E0 (:801)
float animlib_get_current_speed_multiplier(int aim_handle, unsigned int handle); // 0x00C91840 (:808)
float animlib_anim_is_playing(int aim_handle, int anim_handle); // 0x00C918C0 (:815)
float animlib_get_time_till_animation_finished(int aim_handle, unsigned int handle); // 0x00C94E20 (:822)
bool animlib_is_anim_finished(int aim_handle, unsigned int handle); // 0x00C94E60 (:829)
anim_priority animlib_get_anim_priority(int aim_handle, unsigned int handle); // 0x00C91950 (:836)
bool animlib_trigger_has_elapsed(int aim_handle, int trigger_id, unsigned int handle, int trigger_index); // 0x00C919D0 (:844)
bool animlib_trigger_just_elapsed(int aim_handle, int trigger_id, unsigned int handle, int trigger_index); // 0x00C91A20 (:851)
bool animlib_looping_trigger_has_elapsed(int aim_handle, int trigger_id); // 0x00C91A70 (:858)
float animlib_time_till_trigger(int aim_handle, int trigger_id, unsigned int handle); // 0x00C91AC0 (:865)
bool animlib_anim_has_trigger(int aim_handle, int trigger_id, unsigned int handle); // 0x00C91B10 (:872)
int animlib_get_trigger_time(int trigger_id, int anim_handle, int trigger_idx); // 0x00C91B60 (:879)
bool animlib_has_frame_elapsed(int aim_handle, unsigned int handle, int frame); // 0x00C91BC0 (:896)
void animlib_set_trigger_flags(int aim_handle, unsigned int instance_handle, int trigger_num, bool just_elapsed, bool elapsed, bool suppress_elapsed); // 0x00C91C30 (:903)
bool animlib_trigger_should_have_elapsed(int aim_handle, unsigned int instance_handle, int trigger_id); // 0x00C94EC0 (:910)
bool animlib_is_holding_last_frame(int aim_handle); // 0x00C91C80 (:917)
bool animlib_is_looping(int aim_handle, unsigned int handle); // 0x00C91CC0 (:928)
bool animlib_anim_is_ramping_out(int aim_handle, unsigned int handle); // 0x00C91D10 (:935)
int animlib_get_anim_handle_from_instance_handle(int aim_handle, unsigned int handle); // 0x00C91D60 (:943)
void animlib_get_ramp_in_out_times(int anim_handle, int& ramp_in, int& ramp_out); // 0x00C91DF0 (:950)
int animlib_find_character_tag(int aim_handle, const char* name); // 0x00C98AD0 (:972)
int animlib_find_nth_character_tag(int aim_handle, const char* name, int num_tag); // 0x00C98B10 (:979)
int animlib_find_character_tag_with_active_vids(int aim_handle, const char* name, int num_active_vids, const int* active_vids); // 0x00C98B60 (:986)
int animlib_find_character_bone(int aim_handle, const char* name); // 0x00C98BB0 (:993)
void animlib_get_position_change(vector& action_delta_pos, vector& state_delta_pos, vector& anim_local_delta_pos, int aim_handle); // 0x00C9AFE0 (:1000)
float animlib_get_heading_change(int aim_handle, int future_ticks); // 0x00C9B030 (:1007)
float animlib_get_root_heading(int aim_handle); // 0x00C9B070 (:1014)
void animlib_get_total_translation(int anim_handle, vector& translation, int start_time, int end_time); // 0x00C98BF0 (:1021)
float animlib_get_total_rotation(int anim_handle); // 0x00C91E70 (:1043)
void animlib_set_speed_multiplier(int aim_handle, unsigned int handle, float speed_multiplier); // 0x00C91EF0 (:1056)
void animlib_set_looping_speed_multiplier(int aim_handle, float speed_multiplier); // 0x00C91F70 (:1063)
void animlib_set_start_frame(int aim_handle, unsigned int handle, int start_frame); // 0x00C91FB0 (:1071)
void animlib_set_animation_time(int aim_handle, unsigned int handle, float new_time); // 0x00C94F10 (:1078)
void animlib_set_looping_anim_start_percent(int aim_handle, unsigned int handle, float init_cyclic_percent); // 0x00C92000 (:1085)
float animlib_get_init_cyclic_percent(int aim_handle, unsigned int handle); // 0x00C92050 (:1092)
void animlib_set_init_cyclic_percent(int aim_handle, unsigned int handle, float init_cyclic_percent); // 0x00C920E0 (:1099)
int animlib_get_hold_animation_handle(int aim_handle); // 0x00C92130 (:1106)
matrix43 animlib_get_bone_inv_transformation(int aim_handle, int entity_bone_index); // 0x00C92180 (:1113)
int animlib_get_bone_parent_index(int aim_handle, int entity_bone_index); // 0x00C922C0 (:1135)
int animlib_get_num_bones(int aim_handle); // 0x00C92300 (:1142)
int animlib_get_num_virtual_bones(int aim_handle); // 0x00C92340 (:1149)
int animlib_get_num_animating_bones(int aim_handle); // 0x00C92380 (:1162)
int animlib_get_num_animating_bones(int aim_handle, unsigned int handle); // 0x00C92430 (:1189)
bool animlib_get_instanced_model_matrices(int aim_handle, matrix43** matrices); // 0x00C92500 (:1207)
void animlib_validate_instanced_model_matrices(int aim_handle); // 0x00C92550 (:1218)
matrix43* animlib_get_vertex_to_model_matrices(int aim_handle); // 0x00C92590 (:1225)
matrix43* animlib_get_render_matrices(int aim_handle); // 0x00C925D0 (:1236)
void animlib_copy_render_matrices(int aim_handle, matrix43* dest); // 0x00C92610 (:1244)
vector animlib_get_bone_rel_translation(int aim_handle, int entity_bone_index); // 0x00C92650 (:1251)
int animlib_overlay_anim(int anim_handle, void* data_block, unsigned int data_block_size, bool is_permanent); // 0x00C926F0 (:1269)
int animlib_overlay_anim(const char* filename, void* data_block, unsigned int data_block_size, bool is_permanent); // 0x00C92760 (:1337)
void animlib_unload_anim(int anim_handle); // 0x00C92780 (:1348)
void animlib_unload_anim(const char* filename); // 0x00C927D0 (:1372)
void animlib_unload_anim_data(int anim_handle); // 0x00C92820 (:1383)
void animlib_unload_anim_data(const char* filename); // 0x00C92850 (:1402)
bool animlib_is_paged_in(int anim_handle); // 0x00C92890 (:1415)
bool animlib_get_anim_filename(int anim_handle, char* filename, int max_len); // 0x00C928C0 (:1435)
int animlib_find_anim(const char* filename); // 0x00C892E0 (:1456)
void animlib_set_anim_flag(int anim_handle, unsigned int flag); // 0x00C92910 (:1500)
void animlib_clear_anim_flag(int anim_handle, unsigned int flag); // 0x00C92940 (:1508)
bool animlib_get_active_animations(unsigned int* anim_handles, int& num_active, int num_handles, int aim_handle); // 0x00C92970 (:1527)
int animlib_get_anim_flags(int aim_handle, int anim_handle); // 0x00C92A10 (:1553)
void animlib_get_animation_vel(vector& vel, int anim_handle); // 0x00C92A60 (:1660)
void animlib_get_animation_vel(vector& vel, int anim_handle, int start_time, int end_time); // 0x00C98CA0 (:1675)
bool animlib_is_moving_animation(int anim_handle); // 0x00C92B60 (:1686)
const char* animlib_get_animation_name(int anim_handle); // 0x00C92BD0 (:1705)
float animlib_get_animation_ramp_in_length(int anim_handle); // 0x00C92BF0 (:1721)
float animlib_get_animation_ramp_out_length(int anim_handle); // 0x00C92C70 (:1748)
void animlib_clear_trigger_just_elapsed_flags(int aim_handle); // 0x00C92CF0 (:1777)
void animlib_allocate_bone_modifiers(int aim_handle); // 0x00C98CE0 (:1790)
void animlib_register_bone_modifier(int aim_handle, bone_modifier modifier, int bone_index, int storage_idx, matrix& delta_orient, matrix* old_bone_orient, float blend_pct); // 0x00C98D40 (:1797)
void animlib_enable_scaling(int aim_handle, vector& scale); // 0x00C92D50 (:1805)
void animlib_get_bone_bbox(int aim_handle, vector& bmin, vector& bmax); // 0x00C92DB0 (:1813)
void animlib_process_command_xbox_pc(cpu_job* lj, cpu_job_arg arg1, cpu_job_arg arg2); // 0x00C9B150 (:1844)
void animlib_batch_process_all_start(); // 0x00C9B160 (:2051)
void animlib_batch_process_all_end(); // 0x00C89350 (:2103)
animlib_transform_command* animlib_request_batch_start(); // 0x00C89370 (:2153)
void animlib_request_batch_end(); // 0x00C893C0 (:2170)
anim_instance_manager* get_anim_instance_manager_from_handle(int aim_handle); // 0x00C8B3F0 (anim_lib_instance_manager.cpp:145)
void anim_get_bone_data(anim_file* amf, animlib_compressed_data_reader& bone_data); // 0x00C95FF0 (anim_lib_instance_manager.cpp:332)
void anim_get_rc_bone_data(const anim_file* const amf, animlib_compressed_data_reader& bone_data); // 0x00C92F90 (anim_lib_instance_manager.cpp:363)
void register_entity_base_bone_set(const anim_rig* rig, const animlib_bones_used_info& bones_used_info); // 0x00C8B6F0 (anim_lib_instance_manager.cpp:383)
void unregister_entity_base_bone_set(int entity_class_id); // 0x00C8B7E0 (anim_lib_instance_manager.cpp:450)
void reverse_slerp(quaternion& result, const quaternion& q1, const quaternion& q2, float t); // 0x00C8B840 (anim_lib_instance_manager.cpp:491)
void anim_instance_system_init(); // 0x00C8BA80 (anim_lib_instance_manager.cpp:652)
void anim_instance_system_shutdown(); // 0x00C8BCF0 (anim_lib_instance_manager.cpp:684)
void anim_instance_do_frame(bool clear_morphs); // 0x00C8BD00 (anim_lib_instance_manager.cpp:688)
int anim_instance_manager_create(anim_rig* rig, int entity_class_id); // 0x00C92FC0 (anim_lib_instance_manager.cpp:752)
void anim_instance_manager_delete(anim_instance_manager* aim); // 0x00C98E30 (anim_lib_instance_manager.cpp:819)
unsigned int compose_anim_instance_handle(int signature); // 0x00C8D810 (anim_lib_instance_manager.cpp:2514)
void decompose_anim_instance_handle(unsigned int handle, int& signature); // 0x00C8D820 (anim_lib_instance_manager.cpp:2522)
void force_bone_orient(matrix43& bone_matrix, matrix& force_orient, float blend_force_percent); // 0x00C8EDF0 (anim_lib_instance_manager.cpp:4547)
void modify_bone_orient_pbh(matrix43& bone_matrix, float p, float b, float h); // 0x00C8EEA0 (anim_lib_instance_manager.cpp:4581)
void animlib_set_thread_ownership(); // 0x00C8EF20 (anim_lib_instance_manager.cpp:4596)
