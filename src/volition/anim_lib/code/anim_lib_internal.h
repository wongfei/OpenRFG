// Original: volition/anim_lib/code/anim_lib_internal.h
//
// The reader over an animation's compressed key data (per bone rotation, translation, scale and weight keys with 8- or
// 16-bit times; anim_get_bone_data / setup_bone_data fill it), the compressed key formats ("afc", animation file
// compressed) and the rig and animation table functions of anim_lib_base.cpp and anim_lib_animation.cpp. Placement of
// the key types and the function declarations inferred (player_minimal.md P2 added the key types from rfg.pdb).
//
// Compressed data of an anim_file (decompiled): after the 0x30 byte header (+8 with anim_file::flags & 0x10, the
// anim_morph table) come the anim-to-rig bone map (num_bones bytes, if any), the per bone weight key counts and keys
// (flags & 0x40) and then per bone: the rotation and translation key counts (8-bit, or 16-bit with flags & 0x80), the
// scale key count (flags & 0x20), the rotation offsets and keys, times, translation offsets and keys, times and the
// scale keys and times. Times are 8-bit (16-bit with flags & 0x02) in units of 80 ticks (4800 ticks per second).
#pragma once

#include "volition/anim_lib/code/anim_lib_types.h"

// A run of rotation keys: the run's base value (7 bits per component, in units of 256 / 16383) and the scaler
// selector (offset_data, built by anim_animation_system_init). The run holds count + 1 keys; each key adds its
// char_quat delta times the component's scaler to the base.
// size 0x5
struct afc_rotation_offset
{
    signed char x : 7; // 0x00:0
    unsigned char x_scaler_bit2 : 1; // 0x00:7
    signed char y : 7; // 0x01:0
    unsigned char y_scaler_bit2 : 1; // 0x01:7
    signed char z : 7; // 0x02:0
    unsigned char z_scaler_bit2 : 1; // 0x02:7
    signed char w : 7; // 0x03:0
    unsigned char w_scaler_bit2 : 1; // 0x03:7
    unsigned char count : 4; // 0x04:0
    unsigned char w_scaler_bit1 : 1; // 0x04:4
    unsigned char z_scaler_bit1 : 1; // 0x04:5
    unsigned char y_scaler_bit1 : 1; // 0x04:6
    unsigned char x_scaler_bit1 : 1; // 0x04:7
};

// size 0x4
struct char_quat
{
    signed char x; // 0x00
    signed char y; // 0x01
    signed char z; // 0x02
    signed char w; // 0x03
};

// size 0x4
struct afc_rotation_key
{
    char_quat rotation; // 0x00
};

// A run of translation keys: num_keys keys around the base value (units of 1 / 64 m); each key adds its own offset
// (units of 1 / 4000 m).
// size 0x8
struct afc_translation_offset
{
    unsigned short num_keys; // 0x00
    short x; // 0x02
    short y; // 0x04
    short z; // 0x06
};

// size 0x3
struct afc_translation_key
{
    char x; // 0x00
    char y; // 0x01
    char z; // 0x02
};

// size 0x4
struct afc_weight_key
{
    float weight; // 0x00
};

// size 0xC
struct afc_scale_key
{
    vector scale; // 0x00
};

// One morph target's weight keys: num_morph_keys weights (0..255) followed by their times (aligned to 2 bytes when
// they are shorts).
// size 0x8
struct afc_morph
{
    unsigned int name_chk; // 0x00
    unsigned short num_morph_keys; // 0x04
    unsigned char data[1]; // 0x06
};

// size 0x1
struct afc_morph_key
{
    unsigned char weight; // 0x00
};

// The morph table after the anim_file header (anim_file::flags & 0x10). morph_offset points to num_morphs offsets of
// afc_morph records, relative to the anim_file (animlib_overlay_anim makes the table offset self-relative).
// size 0x8
struct anim_morph
{
    unsigned int num_morphs; // 0x00
    et_ptr_offset<int,0> morph_offset; // 0x04
};

// size 0x194
class animlib_compressed_data_reader
{
public:
    animlib_compressed_data_reader(); // 0x00C891F0 (.h:153)

    int init_level; // 0x00
    bool weight_as_shorts; // 0x04
    void* num_weight_keys; // 0x08
    afc_weight_key* weight_keys; // 0x0C
    void* weight_times; // 0x10
    int weight_indicies[64]; // 0x14
    char* next_bone_start; // 0x114
    int bone_idx; // 0x118
    bool time_as_shorts; // 0x11C
    signed char rig_to_anim_bone_mapping[64]; // 0x11D
    int num_rotation_keys; // 0x160
    int num_rotation_offsets; // 0x164
    afc_rotation_offset* rotation_offsets; // 0x168
    afc_rotation_key* rotation_keys; // 0x16C
    void* rotation_times; // 0x170
    int num_translation_keys; // 0x174
    int num_translation_offsets; // 0x178
    afc_translation_offset* translation_offsets; // 0x17C
    afc_translation_key* translation_keys; // 0x180
    void* translation_times; // 0x184
    int num_scale_keys; // 0x188
    afc_scale_key* scale_keys; // 0x18C
    void* scale_times; // 0x190
};

extern anim_rig Anim_rig[290]; // 0x038BD6F8
extern anim_animation Animations[3875]; // 0x0398E338
extern int Num_animations; // 0x038BD6F0
extern int Animation_lookup_table[7750]; // 0x038CA4D8
extern int Animation_lookup_table_collisions; // 0x016F088C

void anim_rig_system_init(); // 0x00C8ADA0 (anim_lib_base.cpp:239)
void anim_rig_system_shutdown(); // 0x00C8ADC0 (anim_lib_base.cpp:246)
anim_rig* anim_rig_new(const char* filename, unsigned char* data_buffer, unsigned int buffer_size, bool permanent); // 0x00C95B60 (anim_lib_base.cpp:268)
int anim_rig_find_index(const char* filename); // 0x00C8ADF0 (anim_lib_base.cpp:318)
bool anim_rig_test_existence(const char* filename); // 0x00C8AE30 (anim_lib_base.cpp:339)
void get_rotation_key_scalers(afc_rotation_offset* offset, int& scalers); // 0x00C894B0 (anim_lib_animation.cpp:170)
void anim_animation_system_init(); // 0x00C8A760 (anim_lib_animation.cpp:622)
int animlib_get_num_trigger_types(); // 0x00C8A9C0 (anim_lib_animation.cpp:731)
const anim_morph* get_morph_data(const anim_file* amf); // 0x00C8A9D0 (anim_lib_animation.cpp:736)
void anim_animation_system_shutdown(); // 0x00C8A9F0 (anim_lib_animation.cpp:745)
bool animlib_register_trigger_name(const char* trigger_name); // 0x00C92E20 (anim_lib_animation.cpp:755)
int animlib_get_trigger_id(const char* trigger_name); // 0x00C8AA00 (anim_lib_animation.cpp:785)
void animlib_remove_anim_from_lookup_table(const char* filename); // 0x00C8AA80 (anim_lib_animation.cpp:829)
int animlib_get_anim_slot(const char* filename); // 0x00C92E90 (anim_lib_animation.cpp:905)
bool animlib_add_trigger_to_animation(int handle, int trigger_id, int frame); // 0x00C92F00 (anim_lib_animation.cpp:944)
bool animlib_set_morph_set_for_animation(int handle, const char* morph_set_name); // 0x00C92F40 (anim_lib_animation.cpp:977)
anim_animation* get_animation_from_handle(int handle); // 0x00C8AB90 (anim_lib_animation.cpp:1014)
