// Original: volition/anim_lib/code/anim_lib_types.h
//
// Animation data types (data side; the instance runtime comes with Tier 6).
// - anim_rig: a skeleton (.rig_pc inside the animation packfiles): bones (anim_bone: name checksum, inverse
//   transform, parent), tags (anim_tag: attachment points relative to a bone) and the bone name checksums. The file is
//   an anim_rig_serialized_data whose et_ptr_offsets set_rig_data turns into the pointers of anim_rig; rigs are kept by
//   file name (anim_rig_new, animlib_get_loaded_rig).
// - anim_animation: one loaded animation (.anim_pc): file name and hash, morph set, triggers and the compressed data
//   (anim_file). Animations are addressed by handle (get_animation_from_handle) and paged in through the GSP_ANIM_FILE
//   primitive (anim_file_loaded_callback in geom/anim.cpp).
// - animlib_bones_used_info: the bones an entity class animates (vehicle_info::bones_used_info,
//   register_entity_base_bone_set).
// - anim_instance_info (Tier 6): one animation playing on an anim_instance_manager (handle, times, priority, speed,
//   mix and the ramp in/out state). Its inline operator= is recorded here, which places the instance types in this
//   header.
// Placement of the other types inferred (the PDB records no inline code for them).
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/util/endian_type.h"

class mempool;
class animlib_compressed_data_reader;

// size 0x24
class anim_bone
{
public:
    anim_bone();

    et_ptr_offset<char,0> name; // 0x00
    vector inv_translation; // 0x04
    vector rel_bone_translation; // 0x10
    int parent_index; // 0x1C
    int vid; // 0x20
};

// size 0x3C
struct anim_tag
{
    et_ptr_offset<char,0> name; // 0x00
    matrix43 transformation; // 0x04
    int parent_index; // 0x34
    int vid; // 0x38
};

// size 0x8
struct anim_trigger
{
    int id; // 0x00
    int ticktime; // 0x04
};

// size 0x30
struct anim_file
{
    int id; // 0x00
    unsigned char version; // 0x04
    unsigned char flags; // 0x05
    unsigned short end_tock; // 0x06
    unsigned char ramp_in; // 0x08
    unsigned char ramp_out; // 0x09
    unsigned char num_bones; // 0x0A
    unsigned char num_rig_bones; // 0x0B
    quaternion total_rotation; // 0x0C
    vector total_translation; // 0x1C
    et_ptr_offset<unsigned char,1> anim_to_rig_bone_mapping; // 0x28
    et_ptr_offset<unsigned char,0> root_controller_bone; // 0x2C
};

// size 0x48
class anim_rig
{
public:
    // size 0x40
    struct anim_rig_serialized_data
    {
        anim_rig_serialized_data(); // 0x00C92F70

        char name[32]; // 0x00
        unsigned int flags; // 0x20
        int num_bones; // 0x24
        int num_common_bones; // 0x28
        int num_virtual_bones; // 0x2C
        int num_tags; // 0x30
        et_ptr_offset<unsigned int,0> bone_name_chksums; // 0x34
        et_ptr_offset<anim_bone,0> bones; // 0x38
        et_ptr_offset<anim_tag,0> tags; // 0x3C
    };

    void clear(); // 0x00C8ACF0 (anim_lib_base.cpp:33)
    int find_bone_by_substring(const char* str) const; // 0x00C95920 (anim_lib_base.cpp:45)
    int find_bone_by_name(const char* bone_name) const; // 0x00C95970 (anim_lib_base.cpp:65)
    int find_tag_by_name(const char* tag_name) const; // 0x00C959C0 (anim_lib_base.cpp:79)
    void get_bone_tree_list(int root_bone_index, const signed char* entity_rig_bones, int max_tree_bones, int& num_tree_bones, int* bone_tree_list) const; // 0x00C8AD10 (anim_lib_base.cpp:112)
    void set_rig_data(unsigned char* data, unsigned int l_data_size); // 0x00C95A10 (anim_lib_base.cpp:181)
    anim_rig* duplicate_rig(mempool* pool) const; // 0x00C9B210 (anim_lib_base.cpp:139)

    char name[32]; // 0x00
    unsigned int flags; // 0x20
    int num_bones; // 0x24
    int num_common_bones; // 0x28
    int num_virtual_bones; // 0x2C
    int num_tags; // 0x30
    unsigned int* bone_name_chksums; // 0x34
    anim_bone* bones; // 0x38
    anim_tag* tags; // 0x3C
    unsigned int data_size; // 0x40
    unsigned char* original_buffer; // 0x44
};

// size 0x20
class anim_animation
{
public:
    anim_file* get_animation_data() const; // 0x00C8AB80 (anim_lib_animation.cpp:1003)
    bool paged_in() const; // 0x00C8ABB0 (anim_lib_animation.cpp:1027)
    int end_time() const; // 0x00C8ABC0 (anim_lib_animation.cpp:1036)
    void find_animation_rotation(quaternion& rotation, int current_time, animlib_compressed_data_reader& bone_data) const; // 0x00C8AC20 (anim_lib_animation.cpp:1317)
    void find_animation_translation(vector& translation, int current_time, animlib_compressed_data_reader& bone_data) const; // 0x00C95590 (anim_lib_animation.cpp:1329)
    void find_animation_scaling(vector& scale, int current_time, animlib_compressed_data_reader& bone_data) const; // 0x00C8AC40 (anim_lib_animation.cpp:1360)
    float find_animation_bone_weight(int rig_bone_index, int current_time, animlib_compressed_data_reader& bone_data) const; // 0x00C956C0 (anim_lib_animation.cpp:1372)
    float find_animation_morph_weight(int morph_index, int current_time) const; // 0x00C95800 (anim_lib_animation.cpp:1459)
    float get_animation_controller_weight(int cur_time) const; // 0x00C98DA0 (anim_lib_animation.cpp:1048)
    void get_anim_displacement(vector& disp) const; // 0x00C8ABE0 (anim_lib_animation.cpp:1269)
    float get_delta_heading(int prv_time, int cur_time) const; // 0x00C94F60 (anim_lib_animation.cpp:1074)
    void get_delta_pos(vector& delta_pos, vector& anim_local_delta_pos, int prv_time, int cur_time) const; // 0x00C95150 (anim_lib_animation.cpp:1155)
    void get_delta_pos_unrotated(vector& delta_pos, int prv_time, int cur_time) const; // 0x00C95360 (anim_lib_animation.cpp:1218)
    void get_velocity(vector& vel, int prv_time, int cur_time) const; // 0x00C954D0 (anim_lib_animation.cpp:1290)
    float get_heading(int cur_time) const; // 0x00C95080 (anim_lib_animation.cpp:1120)
    bool add_trigger(int trigger_id, int frame); // 0x00C8AC60 (anim_lib_animation.cpp:1528)
    void reset_start_time();

    const char* anim_filename; // 0x00
    unsigned int anim_filename_hash; // 0x04
    const char* morph_set_name; // 0x08
    unsigned int flags; // 0x0C
    int num_triggers; // 0x10
    anim_trigger* triggers; // 0x14
    unsigned int data_size; // 0x18
    anim_file* animation_data; // 0x1C
};

// size 0xC
struct animlib_bones_used_info
{
    int unique_id; // 0x00
    int num_bones; // 0x04
    unsigned int* bone_indices; // 0x08
};

enum anim_priority
{
    AP_PRIORITY_NONE = 0,
    AP_PRIORITY_LOW = 1,
    AP_PRIORITY_MEDIUM = 2,
    AP_PRIORITY_HIGH = 3,
    AP_NUM_PRIORITIES = 4,
};

enum anim_ramp_state
{
    ARS_RAMP_IN = 0,
    ARS_RAMP_NONE = 1,
    ARS_RAMP_OUT = 2,
};

// size 0x10
struct anim_ramp_info
{
    int start_time; // 0x00
    int duration; // 0x04
    int cur_time; // 0x08
    anim_ramp_state state; // 0x0C
};

// size 0x40
class anim_instance_info
{
public:
    void operator=(anim_instance_info source); // 0x00C89170 (.h:570)

    int signature; // 0x00
    int anim_handle; // 0x04
    int prev_time; // 0x08
    int cur_time; // 0x0C
    unsigned int flags; // 0x10
    anim_priority priority; // 0x14
    float speed_multiplier; // 0x18
    float init_cyclic_percent; // 0x1C
    float base_mix_percent; // 0x20
    float mix_percent; // 0x24
    float remainder_time; // 0x28
    unsigned int trigger_data; // 0x2C
    anim_ramp_info ramp; // 0x30
};
