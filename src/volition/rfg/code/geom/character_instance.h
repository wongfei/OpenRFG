// Original: volition/rfg/code/geom/character_instance.h
// Implementation: volition/rfg/code/geom/character_instance.cpp
//
// character_instance, the animated character of a human: its anim_lib instance (aim_handle), the v_animate state
// machine (vp: states, actions and animation groups), scale, LOD and render alpha, and as stream resource the base
// character and its skin renderable. play_action / set_state / set_animation_group forward to v_animate;
// character_instance_animlib_matrices_do_frame computes the bone matrices of all instances once per gameplay frame.
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/object.h"

enum animation_state;
class character;
class index_alias;
struct lod_info;
class matrix;
class mempool;
class rl_renderable_mesh;
class rl_skin_instance;
struct v_animate;
enum vanim_aliases;
class vector;

// size 0x38
class character_instance
{
public:
    // size 0x8
    struct resource_dependent_data
    {
        character* base_character; // 0x00
        rl_skin_instance* renderable_instance_p; // 0x04
    };

    void update_render(const vector& lpos, const matrix& lorient, const vector& bmin, const vector& bmax, int lod_level, bool transform_only); // 0x0080B9A0 (:249)
    unsigned int play_action(animation_action anim_index, vanim_aliases anim_alias, float speed_multiplier, unsigned int flags); // 0x007FB900 (:228)
    void stop_action(int action_alias_index, int ramp_out_frames, bool pause); // 0x007FB950 (:239)
    void set_state(index_alias state_index, index_alias action_index, float blend_time, bool start_at_zero); // 0x00802040 (:139)
    void set_animation_group(animation_group group_index, index_alias state_index, float blend_time, bool start_at_zero); // 0x0080A820 (:133)
    void set_action_dest_position(vector& current_pos, matrix& current_orient, vector world_pos, int action_alias_index, float time, bool ignore_y); // 0x007FB750 (:145)
    void set_action_dest_orient(matrix& current_orient, const matrix& dest_orient, int action_alias_index, float time); // 0x007FB7A0 (:151)
    void set_action_starting_orient(vector& current_pos, matrix& current_orient, vector& dest_pos, matrix& dest_orient, int action_alias_index, float time); // 0x00802080 (:157)
    animation_group get_animation_group(); // 0x007F21C0 (:163)
    animation_state get_animation_state(); // 0x007F21D0 (:168)
    bool is_in_state(int state_alias_index); // 0x007FB7D0 (:177)
    bool is_playing_action(int action_alias_index); // 0x007FB800 (:182)
    float action_percent_done(int action_alias_index); // 0x007FB870 (:197)
    int get_action_anim_handle(int action_alias_index); // 0x007FB820 (:187)
    unsigned int get_action_anim_instance_handle(int action_alias_index); // 0x007FB840 (:192)
    void get_bounding_sphere(vector& center, float& radius); // 0x007FB9C0 (:441)
    void add_prop(const char*, const char*, mempool*);
    void delete_prop(rl_renderable_mesh*);
    void delete_all_props();
    void get_bbox(vector& min, vector& max); // 0x008020C0 (:314)
    void get_exact_local_bbox(vector& bmin, vector& bmax); // 0x007F21F0 (:332)

    static const int c_lod_distance_max; // 0x012C70A4
    static lod_info ms_char_lod_distances[4]; // 0x0165F3AC
    int aim_handle; // 0x00
    v_animate* vp; // 0x04
    float orig_character_scale; // 0x08
    float character_scale; // 0x0C
    unsigned short ci_flags; // 0x10
    unsigned char render_alpha; // 0x12
    unsigned char cache_lod; // 0x13
    int buf_verts_shadow_offset; // 0x14
    unsigned char* buf_frame_verts; // 0x18
    character_instance* next; // 0x1C
    character_instance* prev; // 0x20
    unsigned int variant_index; // 0x24
    float bbox_max_dimension; // 0x28
    object_stream_resource<resource_dependent_data> rdd; // 0x2C
};

character_instance* character_instance_create(float character_scale, const char* anim_set_name); // 0x00801FB0 (:86)
void character_instance_delete(character_instance* ci); // 0x007FB710 (:121)
void character_instance_system_init(int string_pool_size); // 0x007F23A0 (:459)
void character_instance_system_shutdown(); // 0x007F2490 (:474)
void character_instance_render_bones_debug(character_instance* ci, const vector& char_pos, const matrix& char_orient); // 0x007F24A0 (:478)
void character_instance_post_render(); // 0x007F24B0 (:597)
void character_instance_animlib_matrices_do_frame(); // 0x007F24C0 (:606)
bool character_instance_resources_loaded(character_instance* char_inst, character* base_character, unsigned char rl_category); // 0x008021D0 (:624)
void character_instance_resources_unloaded(character_instance* char_inst); // 0x007FBA30 (:692)
void character_instance_serialize(character_instance* char_inst, obj_prop_block& pblock, bool load); // 0x007FBAC0 (:719)
