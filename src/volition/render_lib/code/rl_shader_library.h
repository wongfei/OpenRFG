// Original: volition/render_lib/code/rl_shader_library.h
// Implementation: volition/render_lib/code/rl_shader_library.cpp
//
// rl_shader_library, the singleton registry of loaded shaders (rl_shaderlib_entry by name hash, grouped in
// containers).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/vlib/code/util/hash_table.h"

class rl_class_dynamic_info;
class rl_shader;

// size 0x10
struct rl_shaderlib_container
{
    void init(); // 0x004B6D10 (.h:70)

    rl_shader* configurations[4]; // 0x00
};

// size 0x48
struct rl_shaderlib_entry
{
    void init(); // 0x004B6D20 (.h:92)

    rl_shaderlib_container types[4]; // 0x00
    rl_shader_type lookup_type; // 0x40
    int lookup_configuration; // 0x44
};

// size 0x6020
class rl_shader_library : public rl_base_object, public rl_singleton<rl_shader_library>
{
protected:
    rl_shader_library(); // 0x005183C0 (:125)
    virtual ~rl_shader_library(); // vtbl+0x00 0x004F6310 (:131)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_shader_library* create(); // 0x005186E0 (:440)
    static bool can_create(); // 0x004C8740 (:454)
    virtual void release(); // vtbl+0x04 0x004C8750 (:459)
    virtual void initialize(); // vtbl+0x08 0x00546470 (:137)
    virtual void shutdown(); // vtbl+0x0C 0x004C82D0 (:156)
    bool register_shader(const char* shader_name, rl_shader* shader, bool overrideExisting); // 0x00518480 (:179)
    rl_shader* get_reference_shader(unsigned int handle); // 0x004C8280 (:110)
    unsigned int lookup(unsigned int name_hash); // 0x00518630 (:257)
    unsigned int lookup(const char* name); // 0x00518680 (:273)
    rl_shader* get_rl_shader(unsigned int handle, rl_shader_type type, int flags); // 0x004C8320 (:286)
    bool does_shader_exist(unsigned int handle, rl_shader_type type, int flags); // 0x004C8360 (:295)
    int get_shader_tmu_index(unsigned int handle, int tex_name); // 0x004C83A0 (:322)
    int get_shader_const_index(unsigned int handle, int const_name); // 0x004C8430 (:338)
    int get_shader_bool_index(unsigned int handle, int bool_name); // 0x004C84B0 (:354)
    int get_shader_num_consts(unsigned int handle); // 0x004C8530 (:369)
    int get_shader_user_technique_index(unsigned int handle, unsigned int technique_name_crc); // 0x004C8590 (:385)
    virtual const char* const get_shader_file_extension() const; // vtbl+0x10 0x00518420 (.h:137)
    bool load_shaders(const char* ext); // 0x005462A0 (:63)
    static void get_shader_name_and_type(const char* shader_name, char* name_out, int max_size, rl_shader_type& type_out, int& flags_out); // 0x004C8620 (:403)
    unsigned int get_depth_only_shader() const; // 0x004B6D60 (.h:149)

    static rl_class_dynamic_info ms_class_dynamic_info;
protected:
    rl_shaderlib_entry library_entries[256]; // 0x0C
    int num_library_entries; // 0x480C
    data_hash_table<unsigned int,rl_shaderlib_entry*,&hash_uint,512,unsigned short> library_entry_hash; // 0x4810
    unsigned int m_highlight_shader_to_use; // 0x6018
    unsigned int m_depth_only_shader; // 0x601C
};

void unused_dcf_reload_shaders(); // 0x004F62D0 (:48)
