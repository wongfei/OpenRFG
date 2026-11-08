// Original: volition/render_lib/code/rl_shader.h
// Implementation: volition/render_lib/code/rl_shader.cpp
//
// rl_shader, a loaded shader with its techniques, constant and texture fixups; rl_shader_const_ref and
// rl_shader_technique_ref resolve names to indices; constant/texture binding records. Placement of the binding records
// inferred.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_shader_defines.h"

class mempool;
class rl_class_factory_info;

namespace keen
{
    struct ShaderPipeline;
    struct VertexInputBinding;
}

// size 0x10
class rl_shader : public rl_base_object
{
    // Access inferred from rl_renderer::set_current_shader.
    friend class rl_renderer;
protected:
    rl_shader(); // 0x004C8140 (:21)
    virtual ~rl_shader(); // vtbl+0x00 0x004F61D0 (:27)
private:
    static const pool_list<rl_shader*>& get_object_pool(); // 0x004B7540 (.h:266)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004B7550 (.h:266)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x005181A0 (:6)
    static rl_shader* create_placement(rl_shader* _data_p); // 0x004F6190 (:6)
    static rl_shader* create(); // 0x0052EAF0 (:6)
    static bool can_create(); // 0x004F61C0 (:6)
    virtual void release(); // vtbl+0x04 0x00518290 (:6)
    bool load_shader(char* shader_fname); // 0x00546220 (:41)
    bool initKgShader(const keen::ShaderPipeline** pPipelines, const rl_shader_tex_fixup* pTexFixups, unsigned int numTex, const rl_shader_const_fixup* pConstFixUps, unsigned int numConsts); // 0x00518330 (:57)
    int get_num_consts(); // 0x004B75E0 (.h:362)
    int get_const_index(int const_name_crc); // 0x004B75F0 (.h:373)
    int get_const_num_rows(int const_name_crc); // 0x004B7630 (.h:389)
    int get_max_num_consts(); // 0x004B7700 (.h:483)
    int get_num_bool_consts();
    int get_bool_const_index(int const_name_crc); // 0x004B7680 (.h:414)
    int get_bool_const_num_rows(int);
    int get_num_techniques();
    int get_technique_index(unsigned int);
    int get_tmu_index(int tex_name_crc); // 0x004B76C0 (.h:447)
    void get_shader_pipeline(const int technique, keen::ShaderPipeline*& pipeline, const keen::VertexInputBinding**& bindings_p); // 0x004F61E0 (:107)
    unsigned int get_flags();
    int get_user_technique_index(unsigned int technique_name_crc); // 0x004C8170 (:74)
    int find_technique_to_use(rl_shader_technique_type technique_type, int user_technique_idx); // 0x004C81C0 (:140)
    const rl_shader_tex_fixup_runtime* get_tex_fixup(int tex_name_crc); // 0x004C8240 (:171)
    static int get_base_memory_size(); // 0x004C8160 (:36)

    static const unsigned int m___class_id; // 0x012B8080
    static rl_class_factory_info ms_class_factory_info; // 0x01516248
protected:
    static pool_list<rl_shader*> ms_object_pool; // 0x01516278
private:
    rl_shader_header_runtime* m_shader_data_p; // 0x08
    unsigned int m_max_user_space_const; // 0x0C
};

// size 0x4
class rl_shader_const_ref
{
public:
    rl_shader_const_ref(); // 0x004B7560 (.h:318)
    ~rl_shader_const_ref(); // 0x004B7570 (.h:319)
    void register_const(unsigned int shader_ref, const char* const_name, rl_shader_const_type const_type); // 0x004F6210 (:116)
    unsigned int get_ref() const; // 0x004B7580 (.h:323)
    void set_ref(unsigned int ref); // 0x004B7590 (.h:328)

private:
    unsigned int m_ref; // 0x00
};

// size 0x4
class rl_shader_technique_ref
{
public:
    rl_shader_technique_ref(); // 0x004B75A0 (.h:338)
    ~rl_shader_technique_ref(); // 0x004B75B0 (.h:339)
    void register_technique(unsigned int shader_ref, const char* technique_name); // 0x004F62A0 (:134)
    int get_ref() const; // 0x004B75C0 (.h:344)
    void set_ref(int ref); // 0x004B75D0 (.h:349)

private:
    int m_ref; // 0x00
};

// size 0x14
struct rl_constant_binding
{
    float m_constant[4]; // 0x00
    int m_register; // 0x10
};

// size 0x8
struct rl_texture_binding
{
    int m_handle; // 0x00
    int m_tmu; // 0x04
};
