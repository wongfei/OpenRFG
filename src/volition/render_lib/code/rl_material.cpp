// Original: volition/render_lib/code/rl_material.cpp
//
// rl_material_base (hash cache and library flags) and rl_material, a shader handle with its constant and texture data
// (rl_material_data, usually inside a loaded mesh). Materials are registered in the material library
// (rl_material_library.cpp) and bound by rl_renderer::set_material.

#include "volition/render_lib/code/rl_material.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "keen/base/trace.hpp"
#include "keen/math/vector3.hpp"

#include <string.h>

// The base_flags bits (RLMBF_*; names inferred except RLMBF_REGISTERED_IN_LIBRARY, quoted by implicit_unregister).
static const unsigned int RLMBF_HASH_CACHED = 0x1;
static const unsigned int RLMBF_REGISTERED_IN_LIBRARY = 0x2;
static const unsigned int RLMBF_OVERRIDE = 0x4;
static const unsigned int RLMBF_BASE_TRANSPARENT = 0x8;

// The mat_flags bits used here (material flags of rl_material_data).
static const unsigned int Mat_flag_transparent = 0x1;
static const unsigned int Mat_flag_constants_modified = 0x100;

static keen::Vector3 s_debugPlayerPosition; // 0x01CA4A10

// :15
// <rl_material::release> 0x004C2A30 (:15)
// <rl_material::create> 0x004E8C90 (:15)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_material)

// (.h) The dynamic class's count of the materials alive (inlined away in the binary; the viewer's census reads it).
int rl_material::get_objects_used()
{
    return ms_class_dynamic_info.m_objects_allocated;
}

// 0x004C2A50 (:27)
rl_material_base::rl_material_base()
{
    base_flags = 0;
    hash_cache = 0;
}

// 0x004C2A70 (:35)
rl_material_base::~rl_material_base()
{
    if ((base_flags & RLMBF_OVERRIDE) != 0)
    {
        keen::debug::nativePrintArgs("rl_material_base::~rl_material_base: get_is_override()\n");
    }
}

// 0x004C2AA0 (:51)
rl_material::rl_material()
{
    m_override_technique_type = rl_shader_technique_none;
    m_override_technique_index = -1;
}

// 0x0054BCE0 (:59)
rl_material::~rl_material()
{
    implicit_unregister();
}

// 0x004C2AF0 (:65)
void rl_material::set_override_technique(rl_shader_technique_type technique_type, int user_technique_index)
{
    m_override_technique_type = technique_type;
    m_override_technique_index = user_technique_index;
}

// 0x004C2B10 (:75)
unsigned int rl_material_base::hash()
{
    if ((base_flags & RLMBF_HASH_CACHED) == 0)
    {
        hash_cache = hash_internal();
        base_flags |= RLMBF_HASH_CACHED;
    }
    return hash_cache;
}

// 0x004C2B30 (:93)
// Not cached.
unsigned int rl_material_base::hash() const
{
    if ((base_flags & RLMBF_HASH_CACHED) == 0)
    {
        return hash_internal();
    }
    return hash_cache;
}

// 0x004C2B50 (:104)
void rl_material::set_material_data(rl_material_data* mat_data)
{
    m_data = mat_data;
    if (mat_data != nullptr)
    {
        set_is_base_transparent((mat_data->mat_flags & Mat_flag_transparent) != 0);
    }
}

// 0x00510D50 (:121)
// Same shader, constants and textures (the textures in any order); exhaustive also compares the name checksums.
bool rl_material::compare(const rl_material_base& other_mat, bool exhaustive) const
{
    if (other_mat.get_type() != RLMT_LOADED_MATERIAL)
    {
        return false;
    }
    const rl_material_data* other_p = static_cast<const rl_material&>(other_mat).m_data;
    const rl_material_data* data_p = m_data;
    if (data_p->shader_handle != other_p->shader_handle || (exhaustive && data_p->name_checksum != other_p->name_checksum) || data_p->num_textures != other_p->num_textures || data_p->num_constants != other_p->num_constants)
    {
        return false;
    }
    const rl_material_const* constants_p = data_p->constant_block.to_pointer();
    const rl_material_const* other_constants_p = other_p->constant_block.to_pointer();
    for (int i = 0; i < data_p->num_constants; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (constants_p[i].elem[j] != other_constants_p[i].elem[j])
            {
                return false;
            }
        }
    }
    if (exhaustive)
    {
        const unsigned int* checksums_p = data_p->constant_name_checksum.to_pointer();
        const unsigned int* other_checksums_p = other_p->constant_name_checksum.to_pointer();
        for (int i = 0; i < data_p->num_constants; i++)
        {
            if (checksums_p[i] != other_checksums_p[i])
            {
                return false;
            }
        }
    }
    const rl_texture_desc* textures_p = data_p->textures.to_pointer();
    const rl_texture_desc* other_textures_p = other_p->textures.to_pointer();
    for (int i = 0; i < data_p->num_textures; i++)
    {
        const rl_texture_desc& texture = textures_p[i];
        int j = 0;
        for (;;)
        {
            if (j >= other_p->num_textures)
            {
                return false;
            }
            const rl_texture_desc& other_texture = other_textures_p[j];
            if (texture.data.rt.texture_stage == other_texture.data.rt.texture_stage && texture.texture_handle == other_texture.texture_handle && texture.data.rt.texture_flags == other_texture.data.rt.texture_flags && (!exhaustive || texture.name_checksum == other_texture.name_checksum))
            {
                break;
            }
            j++;
        }
    }
    return true;
}

// 0x00510FC0 (:218)
// CRC of the shader, the counts, the constants, the flags and the textures by stage.
// Notes (decompiled behaviour, kept as is): the constants are hashed from the address of the constant_block offset
// field (the offset and the bytes after the material data), not from the constants it points to.
unsigned int rl_material::hash_internal() const
{
    if (m_data == nullptr)
    {
        keen::debug::nativePrintArgs("Trying to create material from NULL shader!");
        return 0;
    }
    unsigned int crc = crc_mem((const unsigned char*)&m_data->shader_handle, 4, 0);
    crc = crc_mem(&m_data->num_constants, 1, crc);
    crc = crc_mem((const unsigned char*)&m_data->num_textures, 2, crc);
    crc = crc_mem((const unsigned char*)&m_data->constant_block, m_data->num_constants * sizeof(rl_material_const), crc);
    crc = crc_mem((const unsigned char*)&m_data->mat_flags, 4, crc);
    int textures_hashed = 0;
    // Magic number: 16 texture stages.
    for (unsigned int stage = 0;;)
    {
        const rl_texture_desc* textures_p = m_data->textures.to_pointer();
        for (unsigned int i = 0; i < m_data->num_textures; i++)
        {
            if (textures_p[i].data.rt.texture_stage == stage)
            {
                crc = crc_mem((const unsigned char*)&textures_p[i].texture_handle, 4, crc);
                const unsigned int flags = textures_p[i].data.rt.texture_flags;
                crc = crc_mem((const unsigned char*)&flags, 2, crc);
                textures_hashed++;
                break;
            }
        }
        if (m_data->num_textures <= textures_hashed)
        {
            return crc;
        }
        stage++;
        if (stage > 15)
        {
            return crc;
        }
    }
}

// 0x0053EE70 (:274)
void rl_material::set_as_render_material(rl_renderer& renderer, rl_material_state_store*, int) const
{
    renderer.set_material(m_override_technique_type, m_override_technique_index, m_data);
}

// 0x004E8D20 (:279)
// rl_renderer::cleanup_material inlined.
void rl_material::cleanup_render_material(rl_renderer& renderer, rl_material_state_store*, int) const
{
    renderer.cleanup_material();
}

// 0x004C2B70 (:286)
bool rl_material::get_is_transparent()
{
    if (m_data == nullptr)
    {
        return false;
    }
    return (m_data->mat_flags & Mat_flag_transparent) != 0;
}

// 0x004C2B90 (:292)
void rl_material::set_is_transparent(bool is_transparent)
{
    if (m_data == nullptr)
    {
        return;
    }
    if (is_transparent)
    {
        m_data->mat_flags |= Mat_flag_transparent;
    }
    else
    {
        m_data->mat_flags &= ~Mat_flag_transparent;
    }
    set_is_base_transparent(is_transparent);
}

// 0x004C2BC0 (:304)
bool rl_material::get_has_flags(int, unsigned int flags, bool)
{
    if (m_data == nullptr)
    {
        return false;
    }
    return (m_data->mat_flags & flags) == flags;
}

// 0x0053EE90 (:313)
// Copies the material data, constants and textures into the pool (the clone owns no library registration).
// Notes (decompiled behaviour, kept as is): max_constants becomes num_constants and the name checksums are shared.
rl_material_base* rl_material::clone(mempool_base* pool) const
{
    rl_material* clone_p = rl_material::create();
    rl_material_data* data_p = (rl_material_data*)pool->alloc(sizeof(rl_material_data), 0);
    clone_p->m_data = data_p;
    memcpy(data_p, m_data, sizeof(rl_material_data));
    rl_material_const* constants_p = (rl_material_const*)pool->alloc(m_data->num_constants * sizeof(rl_material_const), 0);
    data_p->constant_block.from_pointer(constants_p);
    memcpy(data_p->constant_block.to_pointer(), m_data->constant_block.to_pointer(), m_data->num_constants * sizeof(rl_material_const));
    data_p->max_constants = data_p->num_constants;
    rl_texture_desc* textures_p = (rl_texture_desc*)pool->alloc(m_data->num_textures * sizeof(rl_texture_desc), 0);
    data_p->textures.from_pointer(textures_p);
    memcpy(data_p->textures.to_pointer(), m_data->textures.to_pointer(), m_data->num_textures * sizeof(rl_texture_desc));
    return clone_p;
}

// 0x004E8D30 (:334)
bool rl_material::get_constant_data(float* data, unsigned int which, int numFloats)
{
    if (m_data == nullptr)
    {
        return false;
    }
    memcpy(data, &((float*)m_data->constant_block.to_pointer())[which], numFloats * sizeof(float));
    return true;
}

// 0x004E8D70 (:351)
bool rl_material::get_constant_data(float* data, const char* name, int numFloats)
{
    if (m_data == nullptr)
    {
        return false;
    }
    const int index = rl_shader_library::get_instance()->get_shader_const_index(m_data->shader_handle, crc_stri(name, 0));
    memcpy(data, &((float*)m_data->constant_block.to_pointer())[index], numFloats * sizeof(float));
    return true;
}

// 0x004E8DD0 (:362)
// The const locations (rl_shader_const_ref, get_const_index) count floats, not rl_material_const rows.
void rl_material::set_constant_data(unsigned int which, float* data, int numFloats)
{
    if (m_data == nullptr)
    {
        return;
    }
    m_data->mat_flags |= Mat_flag_constants_modified;
    memcpy(&((float*)m_data->constant_block.to_pointer())[which], data, numFloats * sizeof(float));
}

// 0x004E8E10 (:380)
void rl_material::set_constant_data(const char* name, float* data, int numFloats)
{
    if (m_data == nullptr)
    {
        return;
    }
    const int index = rl_shader_library::get_instance()->get_shader_const_index(m_data->shader_handle, crc_stri(name, 0));
    m_data->mat_flags |= Mat_flag_constants_modified;
    memcpy(&((float*)m_data->constant_block.to_pointer())[index], data, numFloats * sizeof(float));
}

// 0x00511120 (:393)
// Sets the texture of the stage the shader binds the name to.
// Notes (decompiled behaviour, kept as is): m_data is not checked, and a null textures table writes to address 0.
void rl_material::set_texture_data(const char* name, int tex_handle)
{
    const unsigned int tmu = (unsigned int)rl_shader_library::get_instance()->get_shader_tmu_index(m_data->shader_handle, crc_stri(name, 0));
    rl_texture_desc* textures_p = m_data->textures.to_pointer();
    for (int i = 0; i < m_data->num_textures; i++)
    {
        if (textures_p[i].data.rt.texture_stage == tmu)
        {
            textures_p[i].texture_handle = tex_handle;
            return;
        }
    }
}

// 0x005111C0 (:407)
bool rl_material::get_texture_data(int& tex_handle, int tmu_index)
{
    if (m_data == nullptr)
    {
        return false;
    }
    for (int i = 0; i < m_data->num_textures; i++)
    {
        if ((int)m_data->textures[i].data.rt.texture_stage == tmu_index)
        {
            tex_handle = m_data->textures[i].texture_handle;
            return true;
        }
    }
    return false;
}

// 0x00511220 (:422)
bool rl_material::get_texture_data(int& tex_handle, const char* name)
{
    if (m_data == nullptr)
    {
        return false;
    }
    const int tmu = rl_shader_library::get_instance()->get_shader_tmu_index(m_data->shader_handle, crc_stri(name, 0));
    return get_texture_data(tex_handle, tmu);
}

// 0x00511270 (:432)
// The shader exists for the type and configuration, and the constant and texture tables are present and in range.
bool rl_material::validate_material(rl_shader_type shader_type, int flags)
{
    const rl_material_data* data_p = m_data;
    // Magic number: 256 shader library entries.
    if (data_p == nullptr || data_p->shader_handle > 0xFF)
    {
        return false;
    }
    if (!rl_shader_library::get_instance()->does_shader_exist(data_p->shader_handle, shader_type, flags))
    {
        return false;
    }
    if (data_p->num_constants != 0 && data_p->constant_block.to_pointer() == nullptr)
    {
        return false;
    }
    // Magic numbers: at most 51 constants and 15 textures.
    if (data_p->num_constants >= 0x34)
    {
        return false;
    }
    if (data_p->num_textures != 0 && data_p->textures.to_pointer() == nullptr)
    {
        return false;
    }
    if (data_p->num_textures >= 0x10)
    {
        return false;
    }
    return data_p->num_constants <= data_p->max_constants;
}

// 0x004C2BE0 (:475)
unsigned int rl_material::get_name_checksum() const
{
    return m_data->name_checksum;
}

// 0x004C2BF0 (:483)
void setDebugPlayerPosition(const keen::Vector3& pos)
{
    s_debugPlayerPosition = pos;
}

// 0x004C2C10 (:488)
void rl_material::draw_debug(const keen::Vector3&) const
{
}

// 0x004C2C20 (:525)
void rl_material_base::invalidate_hash_cache()
{
    base_flags &= ~RLMBF_HASH_CACHED;
}

// 0x0053EFF0 (:531)
void rl_material_base::implicit_unregister()
{
    if ((base_flags & RLMBF_REGISTERED_IN_LIBRARY) != 0)
    {
        keen::debug::nativePrintArgs("rl_material_base::implicit_unregister: base_flags & RLMBF_REGISTERED_IN_LIBRARY\n");
        if ((base_flags & RLMBF_REGISTERED_IN_LIBRARY) != 0)
        {
            rl_material_library_unregister(this);
        }
    }
}

// 0x004E8E80 (:547)
int rl_material::get_constant_loc(unsigned int name_crc)
{
    if (m_data == nullptr)
    {
        return 0;
    }
    return rl_shader_library::get_instance()->get_shader_const_index(m_data->shader_handle, name_crc);
}

// 0x004E8EA0 (:554)
int rl_material::get_constant_loc(const char* name)
{
    const unsigned int name_crc = crc_stri(name, 0);
    if (m_data == nullptr)
    {
        return 0;
    }
    return rl_shader_library::get_instance()->get_shader_const_index(m_data->shader_handle, name_crc);
}

// 0x004B7990 (.h:70)
bool rl_material_base::get_is_base_transparent()
{
    return (base_flags & RLMBF_BASE_TRANSPARENT) != 0;
}

// 0x004B79A0 (.h:71)
void rl_material_base::set_is_base_transparent(bool transparent)
{
    if (transparent)
    {
        base_flags |= RLMBF_BASE_TRANSPARENT;
        return;
    }
    base_flags &= ~RLMBF_BASE_TRANSPARENT;
}

// 0x004B79C0 (.h:89)
void rl_material_base::set_registered(bool is_registered)
{
    if (is_registered)
    {
        base_flags |= RLMBF_REGISTERED_IN_LIBRARY;
        return;
    }
    base_flags &= ~RLMBF_REGISTERED_IN_LIBRARY;
}

// 0x004B79E0 (.h:92)
void rl_material_base::set_is_override(bool is_override)
{
    if (is_override)
    {
        base_flags |= RLMBF_OVERRIDE;
        return;
    }
    base_flags &= ~RLMBF_OVERRIDE;
}

// 0x004B7A00 (.h:93)
bool rl_material_base::get_is_override()
{
    return (base_flags & RLMBF_OVERRIDE) != 0;
}

// 0x004B7A10 (.h:126)
const rl_material_data* rl_material::get_material_data() const
{
    return m_data;
}

// 0x004C2AC0 (.h:141)
rl_material_type rl_material::get_type() const
{
    return RLMT_LOADED_MATERIAL;
}

// 0x004C2AD0 (.h:150)
int rl_material::get_num_passes(bool)
{
    return 1;
}

// 0x004C2AE0 (.h:155)
bool rl_material::can_combine_drawcalls()
{
    return true;
}
