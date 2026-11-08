// Original: volition/render_lib/code/rl_shader.cpp
//
// rl_shader: a shader loaded by rl_render_lib (load_shader for .fxo_kg files, createKgShader for code-built
// pipelines), with lookups into its techniques and constant/texture fixups.

#include "volition/render_lib/code/rl_shader.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

// :6
// <rl_shader::factory_init> 0x005181A0 (:6)
// <rl_shader::release> 0x00518290 (:6)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_shader, 0x09C027DC, 450)

// 0x004C8140 (:21)
rl_shader::rl_shader()
{
    m_shader_data_p = nullptr;
    m_max_user_space_const = 0;
}

// 0x004F61D0 (:27)
rl_shader::~rl_shader()
{
}

// 0x004C8160 (:36)
// Magic number: 512 KB, the size of rl_render_lib's shader base mempool.
int rl_shader::get_base_memory_size()
{
    return 0x80000;
}

// The end of the constant space used by the shader's constants (inlined into load_shader and initKgShader).
static unsigned int get_max_user_space_const(const rl_shader_header_runtime* shader_data_p, unsigned int max_user_space_const)
{
    for (int i = 0; i < shader_data_p->num_const_fixups; i++)
    {
        const rl_shader_const_fixup& fixup = shader_data_p->const_fixups[i];
        const unsigned int end = (unsigned int)fixup.array_size * (unsigned int)fixup.num_rows + (unsigned int)fixup.const_loc;
        if (max_user_space_const < end)
        {
            max_user_space_const = end;
        }
    }
    // Magic number: rounded up to a multiple of 4.
    return (max_user_space_const + 3) & 0xFFFFFFFC;
}

// 0x00546220 (:41)
bool rl_shader::load_shader(char* shader_fname)
{
    m_shader_data_p = rl_render_lib::get_instance()->load_shader(shader_fname);
    if (m_shader_data_p == nullptr)
    {
        return false;
    }
    m_max_user_space_const = get_max_user_space_const(m_shader_data_p, m_max_user_space_const);
    return true;
}

// 0x00518330 (:57)
bool rl_shader::initKgShader(const keen::ShaderPipeline** pPipelines, const rl_shader_tex_fixup* pTexFixups, unsigned int numTex, const rl_shader_const_fixup* pConstFixUps, unsigned int numConsts)
{
    m_shader_data_p = rl_render_lib::get_instance()->createKgShader(pPipelines, pTexFixups, numTex, pConstFixUps, numConsts);
    if (m_shader_data_p == nullptr)
    {
        return false;
    }
    m_max_user_space_const = get_max_user_space_const(m_shader_data_p, m_max_user_space_const);
    return true;
}

// 0x004C8170 (:74)
// The index of a user technique relative to the first one, or -1.
int rl_shader::get_user_technique_index(unsigned int technique_name_crc)
{
    const rl_shader_header_runtime* shader_data_p = m_shader_data_p;
    if (shader_data_p->user_technique_start_index == -1)
    {
        return -1;
    }
    const int start = shader_data_p->user_technique_start_index;
    for (int i = start; i < shader_data_p->num_techniques; i++)
    {
        if (shader_data_p->shader_techniques[i].technique_name_crc == technique_name_crc)
        {
            return i - start;
        }
    }
    return -1;
}

// 0x004F61E0 (:107)
// The technique is clamped to the last one.
void rl_shader::get_shader_pipeline(const int technique, keen::ShaderPipeline*& pipeline, const keen::VertexInputBinding**& bindings_p)
{
    const int last_technique = m_shader_data_p->num_techniques - 1;
    rl_shader_technique_runtime& technique_data = m_shader_data_p->shader_techniques[technique <= last_technique ? technique : last_technique];
    pipeline = &technique_data.pipeline;
    bindings_p = technique_data.bindings_p;
}

// 0x004F6210 (:116)
void rl_shader_const_ref::register_const(unsigned int shader_ref, const char* const_name, rl_shader_const_type const_type)
{
    rl_shader_library* library_p = rl_shader_library::get_instance();
    switch (const_type)
    {
    case rl_shader_const_float:
        m_ref = library_p->get_shader_const_index(shader_ref, crc_stri(const_name, 0));
        break;
    case rl_shader_const_bool:
        m_ref = library_p->get_shader_bool_index(shader_ref, crc_stri(const_name, 0));
        break;
    case rl_shader_const_sampler:
        m_ref = library_p->get_shader_tmu_index(shader_ref, crc_stri(const_name, 0));
        break;
    default:
        break;
    }
}

// 0x004F62A0 (:134)
void rl_shader_technique_ref::register_technique(unsigned int shader_ref, const char* technique_name)
{
    m_ref = rl_shader_library::get_instance()->get_shader_user_technique_index(shader_ref, crc_stri(technique_name, 0));
}

// 0x004C81C0 (:140)
// Missing techniques fall back to technique 0.
int rl_shader::find_technique_to_use(rl_shader_technique_type technique_type, int user_technique_idx)
{
    int technique;
    switch (technique_type)
    {
    case rl_shader_technique_base:
        technique = m_shader_data_p->base_technique_index;
        break;
    case rl_shader_technique_lighting:
        technique = m_shader_data_p->lighting_technique_index;
        break;
    case rl_shader_technique_depth_only:
        technique = m_shader_data_p->depth_only_technique_index;
        break;
    case rl_shader_technique_user:
        technique = m_shader_data_p->user_technique_start_index + user_technique_idx;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        return 0;
    }
    if (technique == -1 || m_shader_data_p->num_techniques <= technique)
    {
        technique = 0;
    }
    return technique;
}

// 0x004C8240 (:171)
const rl_shader_tex_fixup_runtime* rl_shader::get_tex_fixup(int tex_name_crc)
{
    for (int i = 0; i < m_shader_data_p->num_tex_fixups; i++)
    {
        if (m_shader_data_p->tex_fixups[i].crc_value == tex_name_crc)
        {
            return &m_shader_data_p->tex_fixups[i];
        }
    }
    return nullptr;
}

// 0x004B75E0 (.h:362)
int rl_shader::get_num_consts()
{
    return m_shader_data_p->num_const_fixups;
}

// 0x004B75F0 (.h:373)
int rl_shader::get_const_index(int const_name_crc)
{
    for (int i = 0; i < m_shader_data_p->num_const_fixups; i++)
    {
        if (m_shader_data_p->const_fixups[i].crc_value == const_name_crc)
        {
            return m_shader_data_p->const_fixups[i].const_loc;
        }
    }
    return -1;
}

// 0x004B7630 (.h:389)
int rl_shader::get_const_num_rows(int const_name_crc)
{
    for (int i = 0; i < m_shader_data_p->num_const_fixups; i++)
    {
        if (m_shader_data_p->const_fixups[i].crc_value == const_name_crc)
        {
            return (unsigned int)m_shader_data_p->const_fixups[i].array_size * (unsigned int)m_shader_data_p->const_fixups[i].num_rows;
        }
    }
    return -1;
}

// 0x004B7680 (.h:414)
int rl_shader::get_bool_const_index(int const_name_crc)
{
    for (int i = 0; i < m_shader_data_p->num_bool_fixups; i++)
    {
        if (m_shader_data_p->bool_fixups[i].crc_value == const_name_crc)
        {
            return m_shader_data_p->bool_fixups[i].const_loc;
        }
    }
    return -1;
}

// 0x004B76C0 (.h:447)
int rl_shader::get_tmu_index(int tex_name_crc)
{
    for (int i = 0; i < m_shader_data_p->num_tex_fixups; i++)
    {
        if (m_shader_data_p->tex_fixups[i].crc_value == tex_name_crc)
        {
            return m_shader_data_p->tex_fixups[i].constLocData.const_loc;
        }
    }
    return -1;
}

// 0x004B7700 (.h:483)
int rl_shader::get_max_num_consts()
{
    return m_max_user_space_const;
}

// 0x004B7560 (.h:318)
rl_shader_const_ref::rl_shader_const_ref()
{
    m_ref = 0xFFFFFFFF;
}

// 0x004B7570 (.h:319)
rl_shader_const_ref::~rl_shader_const_ref()
{
}

// 0x004B7580 (.h:323)
unsigned int rl_shader_const_ref::get_ref() const
{
    return m_ref;
}

// 0x004B7590 (.h:328)
void rl_shader_const_ref::set_ref(unsigned int ref)
{
    m_ref = ref;
}

// 0x004B75A0 (.h:338)
rl_shader_technique_ref::rl_shader_technique_ref()
{
    m_ref = -1;
}

// 0x004B75B0 (.h:339)
rl_shader_technique_ref::~rl_shader_technique_ref()
{
}

// 0x004B75C0 (.h:344)
int rl_shader_technique_ref::get_ref() const
{
    return m_ref;
}

// 0x004B75D0 (.h:349)
void rl_shader_technique_ref::set_ref(int ref)
{
    m_ref = ref;
}
