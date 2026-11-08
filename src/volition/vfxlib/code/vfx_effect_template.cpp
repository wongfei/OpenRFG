// Original: volition/vfxlib/code/vfx_effect_template.cpp
//
// Effect templates (VFX.md FX1): serialize fixes up the loaded .cefct_pc (every et_sized_native_pointer holds an offset
// from the start of vfx_data; 0 and -1 mean null) and the GPU half (meshes and materials), then init creates the
// emitter and filter objects through the class factories and copies the template data into them. The accessors of
// vfx_effect_template.h are here too (the original has them inline in the header).

#include "volition/vfxlib/code/vfx_effect_template.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/quaternion.h"

#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-static globals
static bool Vfx_force_parametric; // 0x03A23C89, debug: every emitter parametric
// Byte 0x016F211A (initial value 1) gates the shape registration of the effect meshes; rfg.pdb has no symbol for it
// (Ghidra shows it as the tail of an RTTI name), so the name is inferred. No code writes it.
static bool Vfx_register_mesh_shapes = true; // 0x016F211A

// Magic numbers: the file signature ('71BW' read as a little-endian int) and the oldest version this loader accepts.
static const int VFX_SIGNATURE = 0x57423137;
static const int VFX_VERSION_MIN = 0x15;

// The class factory (vfx_effect_template.cpp:34): class id 0xE17C8FA5, 350 templates.
// <vfx_effect_template::factory_init> 0x00CAEF40 (:34)
// <vfx_effect_template::create> 0x00CAF030 (:34)
// <vfx_effect_template::release> 0x00CAF0E0 (:34)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_effect_template, 0xE17C8FA5, 350)

// An offset from the start of the vfx data, made a pointer in place (0 and -1 are null).
template<typename T>
static void vfx_fixup(et_sized_native_pointer<T>& pointer, vfx_effect_template::vfx_data* vfx_data_p)
{
    T* const offset = pointer.to_pointer();
    if (offset == (T*)0xFFFFFFFF || offset == nullptr)
    {
        pointer = (T*)nullptr;
    }
    else
    {
        pointer = (T*)((char*)vfx_data_p + (int)offset);
    }
}

// An offset from the start of the vfx data, made self-relative (-1 stays: et_ptr_offset<T,1> reads it as null).
template<typename T, int NULLABLE>
static void vfx_fixup_self_relative(et_ptr_offset<T, NULLABLE>& pointer, vfx_effect_template::vfx_data* vfx_data_p)
{
    const int offset = pointer.get_offset();
    if (offset != -1)
    {
        pointer.set_offset((int)vfx_data_p + offset - (int)&pointer);
    }
}

// 0x00C9F040 (:26): the PC textures of a ".tgn" name are ".tga" (LTCG: filename in ESI; inlined in serialize).
static void massage_tgn_filename_in_place(char* filename)
{
    const int length = (int)strlen(filename);
    if (length != 0 && filename[length - 1] == 'n')
    {
        filename[length - 1] = 'a';
    }
}

// 0x00CA45C0 (:38)
vfx_effect_template::vfx_effect_template()
{
    m_initialized = false;
    m_instance_handle = -1;
    m_vfx_data_p = nullptr;
}

// 0x00CB4EC0 (:46)
vfx_effect_template::~vfx_effect_template()
{
    finalize();
}

// 0x00CB87D0 (:53): fixes up the template in place, creates the effect meshes (CPU data from vfx_data, vertex and index
// buffers from gpu_data), loads the particle and corona materials and resolves the expressions, then init().
bool vfx_effect_template::serialize(vfx_data* vfx_data_p, unsigned char* gpu_data, bool load, int& approx_gpu_size)
{
    if (vfx_data_p->signature != VFX_SIGNATURE || vfx_data_p->version < VFX_VERSION_MIN)
    {
        return false;
    }
    m_vfx_data_p = vfx_data_p;
    vfx_fixup(vfx_data_p->effect_name, m_vfx_data_p);
    vfx_fixup(m_vfx_data_p->bitmaps, m_vfx_data_p);
    vfx_fixup(m_vfx_data_p->mesh_bitmaps, m_vfx_data_p);
    vfx_fixup(m_vfx_data_p->meshes, m_vfx_data_p);
    vfx_fixup(m_vfx_data_p->emitters, m_vfx_data_p);
    vfx_fixup(m_vfx_data_p->lights, m_vfx_data_p);
    // Magic number: before version 0x19 every mesh is a normal mesh.
    if (m_vfx_data_p->version < 0x19)
    {
        for (int i = 0; i < m_vfx_data_p->num_meshes; i++)
        {
            m_vfx_data_p->meshes[i].type = MESH_TYPE_NORMAL;
        }
    }
    if (m_vfx_data_p->num_expressions > 0)
    {
        vfx_fixup(m_vfx_data_p->expressions, m_vfx_data_p);
    }
    for (int i = 0; i < m_vfx_data_p->num_bitmaps; i++)
    {
        vfx_fixup(m_vfx_data_p->bitmaps[i], m_vfx_data_p);
        massage_tgn_filename_in_place(m_vfx_data_p->bitmaps[i]);
    }

    int gpu_offset = 0;
    for (int i = 0; i < m_vfx_data_p->num_meshes; i++)
    {
        mesh& mesh_r = m_vfx_data_p->meshes[i];
        vfx_fixup(mesh_r.name, m_vfx_data_p);
        vfx_fixup(mesh_r.position_keys, m_vfx_data_p);
        vfx_fixup(mesh_r.rotation_keys, m_vfx_data_p);
        vfx_fixup(mesh_r.scale_keys, m_vfx_data_p);
        vfx_fixup(mesh_r.opacity_keys, m_vfx_data_p);
        vfx_fixup(mesh_r.mesh_data_p, m_vfx_data_p);
        vfx_fixup(mesh_r.material_map_data_p, m_vfx_data_p);
        vfx_fixup(mesh_r.materials, m_vfx_data_p);
        vfx_fixup(mesh_r.collision_model_data_p, m_vfx_data_p);
        if (Vfx_register_mesh_shapes)
        {
            // The collision model data pointer becomes the shape handle (get_shape_handle reads it back).
            const rl_handle shape_handle = vfx_system::get_shape_register_cb()(mesh_r.collision_model_data_p);
            mesh_r.collision_model_data_p = (unsigned char*)shape_handle.get();
        }

        int cpu_offset = 0;
        LoadedMeshInfo mesh_info(nullptr, nullptr);
        memset(&mesh_info, 0, sizeof(mesh_info));
        keen::copyString(mesh_info.name, sizeof(mesh_info.name), mesh_r.name);
        mesh_r.mesh_data_p = rl_mesh_data_load_validated((unsigned char*)(rl_mesh_data*)mesh_r.mesh_data_p, gpu_data, cpu_offset, gpu_offset, &mesh_info);
        if ((rl_mesh_data*)mesh_r.mesh_data_p == nullptr)
        {
            return false;
        }
        rl_mesh* mesh_p = rl_mesh::create();
        mesh_p->init(mesh_r.mesh_data_p, mesh_info, false);
        m_mesh_renderables.append(mesh_p);
        // Magic number: the next mesh's GPU data starts on a 16-byte boundary.
        int misalignment = gpu_offset % 16;
        gpu_offset += misalignment == 0 ? 0 : 16 - misalignment;

        int map_offset = 0;
        rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)(rl_material_map_data*)mesh_r.material_map_data_p, map_offset);
        mesh_r.material_map_p = rl_material_map::create();
        ((rl_material_map*)mesh_r.material_map_p)->set_map_data(map_data);
        for (int j = 0; j < (int)map_data->num_materials; j++)
        {
            // Magic number: each material's data starts on a 16-byte boundary.
            misalignment = map_offset % 16;
            map_offset += misalignment == 0 ? 0 : 16 - misalignment;
            rl_material_data* material_data = rl_material_data_load((unsigned char*)(rl_material_map_data*)mesh_r.material_map_data_p, map_offset, m_vfx_data_p->mesh_bitmaps, true);
            rl_material* material_p = rl_material::create();
            material_p->set_material_data(material_data);
            map_data->materials[j] = rl_material_library_register(material_p);
            ((et_sized_native_pointer<rl_material>*)(mesh_r.materials))[j] = material_p;
        }
    }

    vfx_fixup(m_vfx_data_p->filters, m_vfx_data_p);
    for (int i = 0; i < m_vfx_data_p->num_filters; i++)
    {
        filter& filter_r = m_vfx_data_p->filters[i];
        vfx_fixup(filter_r.name, m_vfx_data_p);
        vfx_fixup(filter_r.unique_data, m_vfx_data_p);
        vfx_fixup(filter_r.position_keys, m_vfx_data_p);
        vfx_fixup(filter_r.rotation_keys, m_vfx_data_p);
        vfx_fixup(filter_r.affected_emitters, m_vfx_data_p);
        vfx_fixup(filter_r.affected_meshes, m_vfx_data_p);
    }

    // Magic number: coronas came with version 0x17.
    if (m_vfx_data_p->version < 0x17)
    {
        m_vfx_data_p->num_coronas = 0;
    }
    else
    {
        vfx_fixup(m_vfx_data_p->coronas, m_vfx_data_p);
        for (int i = 0; i < m_vfx_data_p->num_coronas; i++)
        {
            corona& corona_r = m_vfx_data_p->coronas[i];
            vfx_fixup(corona_r.name, m_vfx_data_p);
            vfx_fixup(corona_r.position_keys, m_vfx_data_p);
            vfx_fixup(corona_r.rotation_keys, m_vfx_data_p);
            vfx_fixup(corona_r.scale_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_start_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_end_keys, m_vfx_data_p);
            vfx_fixup(corona_r.width_scale_keys, m_vfx_data_p);
            vfx_fixup(corona_r.visibility_radius_keys, m_vfx_data_p);
            vfx_fixup(corona_r.color_start_keys, m_vfx_data_p);
            vfx_fixup(corona_r.color_end_keys, m_vfx_data_p);
            vfx_fixup(corona_r.position_offset_keys, m_vfx_data_p);
            vfx_fixup(corona_r.saturation_keys, m_vfx_data_p);
            vfx_fixup(corona_r.rotation_scale_keys, m_vfx_data_p);
            vfx_fixup(corona_r.intensity_scale_keys, m_vfx_data_p);
            vfx_fixup(corona_r.intensity_exponential_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.opacity_exponential_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_distance_start_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_distance_end_keys, m_vfx_data_p);
            vfx_fixup(corona_r.color_distance_start_keys, m_vfx_data_p);
            vfx_fixup(corona_r.color_distance_end_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_exponential_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.view_fade_scale_keys, m_vfx_data_p);
            vfx_fixup(corona_r.view_fade_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.opacity_visibility_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.radius_visibility_decay_keys, m_vfx_data_p);
            vfx_fixup(corona_r.rotation_offset_keys, m_vfx_data_p);
            // Magic number: before version 0x1A the position offset was not exported; it is forced to one key of 0.
            // A single key of 1 in newer files is the exporter's old default and is read as 0 as well.
            // Notes (decompiled behaviour, kept as is): the old-version branch writes the key without checking that
            // the file has one.
            if (m_vfx_data_p->version < 0x1A)
            {
                corona_r.num_position_offset_keys = 1;
                ((vfx_float_key*)corona_r.position_offset_keys)->value = 0.0f;
            }
            else if (corona_r.num_position_offset_keys == 1 && ((vfx_float_key*)corona_r.position_offset_keys)->value == 1.0f)
            {
                ((vfx_float_key*)corona_r.position_offset_keys)->value = 0.0f;
            }
            load_particle_material(corona_r.material);
        }
    }

    for (int i = 0; i < m_vfx_data_p->num_emitters; i++)
    {
        emitter& emitter_r = m_vfx_data_p->emitters[i];
        vfx_fixup(emitter_r.name, m_vfx_data_p);
        vfx_fixup(emitter_r.unique_data, m_vfx_data_p);
        vfx_fixup(emitter_r.position_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.rotation_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.scale_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.emission_rate_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.angular_speed_start_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.angular_speed_end_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.speed_keys, m_vfx_data_p);
        // Notes (decompiled behaviour, kept as is): without keys the texture animation offset stays unconverted.
        if (emitter_r.num_texture_anim_rate_keys > 0)
        {
            vfx_fixup(emitter_r.texture_anim_rate_keys, m_vfx_data_p);
        }
        vfx_fixup(emitter_r.emit_node_emitters, m_vfx_data_p);
        vfx_fixup(emitter_r.size_start_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_start_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_mida_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_mida_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_mida_offset_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_midb_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_midb_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_midb_offset_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_end_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.size_end_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_start_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_start_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_mida_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_mida_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_mida_offset_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_midb_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_midb_var_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_midb_offset_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_end_keys, m_vfx_data_p);
        vfx_fixup(emitter_r.length_end_var_keys, m_vfx_data_p);
        load_particle_material(emitter_r.material);
    }

    for (int i = 0; i < m_vfx_data_p->num_lights; i++)
    {
        light& light_r = m_vfx_data_p->lights[i];
        vfx_fixup(light_r.name, m_vfx_data_p);
        vfx_fixup(light_r.position_keys, m_vfx_data_p);
        vfx_fixup(light_r.fvec_keys, m_vfx_data_p);
        vfx_fixup(light_r.intensity_keys, m_vfx_data_p);
        vfx_fixup(light_r.clr_keys, m_vfx_data_p);
    }

    // The expressions; a function the game did not register drops all of them.
    for (int i = 0; i < m_vfx_data_p->num_expressions; i++)
    {
        expression& expression_r = m_vfx_data_p->expressions[i];
        if (get_expression_function(expression_r.m_expression_function_name_crc) == nullptr)
        {
            m_vfx_data_p->num_expressions = 0;
            break;
        }
        vfx_fixup(expression_r.m_params, m_vfx_data_p);
        for (int j = 0; j < expression_r.m_num_expression_params; j++)
        {
            expression_param& param_r = ((expression_param*)expression_r.m_params)[j];
            vfx_fixup(param_r.m_data_p_index, m_vfx_data_p);
            if (param_r.m_variable)
            {
                // A variable parameter keeps the index of its expression variable instead of a data pointer.
                param_r.m_data_p_index = (unsigned char*)get_expression_variable_index(param_r.m_variable_crc);
            }
        }
    }
    return init();
}

// 0x00CAF160 (:307): the material's offsets are relative to the vfx data; they become self-relative, the shader and
// textures are looked up.
void vfx_effect_template::load_particle_material(rl_particle_material_data& material)
{
    vfx_fixup_self_relative(material.shader_consts, m_vfx_data_p);
    vfx_fixup_self_relative(material.textures, m_vfx_data_p);
    vfx_fixup_self_relative(material.vs_const_info.const_refs, m_vfx_data_p);
    vfx_fixup_self_relative(material.vs_const_info.engine_consts, m_vfx_data_p);
    vfx_fixup_self_relative(material.ps_const_info.const_refs, m_vfx_data_p);
    vfx_fixup_self_relative(material.ps_const_info.engine_consts, m_vfx_data_p);
    rl_shader_library* library_p = rl_shader_library::get_instance();
    material.effect_ref = (int)library_p->lookup((unsigned int)material.effect_ref);
    if (material.effect_ref < 0)
    {
        keen::debug::nativePrintArgs("Could not find shader for particle effect!");
        return;
    }
    for (int i = 0; i < (int)material.num_shader_consts; i++)
    {
        rl_particle_shader_const& shader_const = material.shader_consts[i];
        vfx_fixup_self_relative(shader_const.keys, m_vfx_data_p);
        shader_const.sc.const_loc = (short)library_p->get_shader_const_index((unsigned int)material.effect_ref, shader_const.sc.var_ref);
    }
    for (int i = 0; i < material.num_textures; i++)
    {
        gr_shader_texture_ref& texture = material.textures[i];
        texture.tex_ref = bm_load(m_vfx_data_p->bitmaps[texture.tex_ref], false);
        if (texture.tex_ref < 0)
        {
            keen::debug::nativePrintArgs("Could not find texture image for particle effect!");
            return;
        }
        texture.tex_stage = (short)library_p->get_shader_tmu_index((unsigned int)material.effect_ref, texture.var_ref);
    }
}

// 0x00CB4EE0 (:353): one emitter object and one renderable per template emitter, the emit node lists, one filter object
// per template filter.
bool vfx_effect_template::init()
{
    for (int i = 0; i < m_vfx_data_p->num_emitters; i++)
    {
        rl_renderable_particles* renderable_p = rl_renderable_particles::create();
        if (renderable_p == nullptr)
        {
            return false;
        }
        emitter& emitter_r = m_vfx_data_p->emitters[i];
        if (Vfx_force_parametric)
        {
            emitter_r.is_iterative = false;
        }
        vfx_class_factory_info* factory_p = vfx_class_factory_info::get_class_factory(emitter_r.class_id);
        if (factory_p == nullptr)
        {
            return false;
        }
        emitter_r.particle_system_emitter_p = (vfx_particle_system_emitter*)factory_p->m_create();
        vfx_particle_system_emitter* emitter_p = emitter_r.particle_system_emitter_p;
        if (emitter_p == nullptr)
        {
            return false;
        }

        // The renderable (render flags: 0x10 default blend unless 0x08 or 0x10 is set, 0x20 half resolution, 0x40
        // quarter resolution, 0x80 always on top, 0x100 soft).
        renderable_p->m_particle_type = (rl_particle_base::render_type)emitter_r.particle_type;
        if ((emitter_r.material.flags & 0x18) == 0)
        {
            emitter_r.material.flags |= 0x10;
        }
        if (emitter_r.is_half_res)
        {
            emitter_r.material.flags |= 0x20;
        }
        if (emitter_r.is_quarter_res)
        {
            emitter_r.material.flags |= 0x40;
        }
        if (emitter_r.is_always_on_top)
        {
            emitter_r.material.flags |= 0x80;
        }
        if (emitter_r.is_soft_render)
        {
            emitter_r.material.flags |= 0x100;
        }
        // Magic number: the soft particle depth fade distance of every vfx renderable.
        renderable_p->m_soft_thickness = 0.8f;
        renderable_p->m_material_p = &emitter_r.material;
        // Renderable flags: 1 rotation, 2 local space, 4 reverse render, 8 iterative, 0x40 color correction.
        if (emitter_r.is_reverse_render)
        {
            renderable_p->m_flags |= 4;
        }
        else
        {
            renderable_p->m_flags &= ~4u;
        }
        if (emitter_r.local_space_emission)
        {
            renderable_p->m_flags |= 2;
        }
        else
        {
            renderable_p->m_flags &= ~2u;
        }
        renderable_p->m_tail_length = emitter_r.num_tails;
        if (emitter_r.rotation_enabled)
        {
            renderable_p->m_flags |= 1;
        }
        else
        {
            renderable_p->m_flags &= ~1u;
        }
        if (emitter_r.is_iterative)
        {
            renderable_p->m_flags |= 8;
        }
        else
        {
            renderable_p->m_flags &= ~8u;
        }
        renderable_p->m_texture_anim_random_frames = emitter_r.texture_anim_random_frames;
        if (emitter_r.color_correction_matrix.to_pointer() == nullptr)
        {
            renderable_p->m_flags &= ~0x40u;
        }
        else
        {
            vfx_fixup(emitter_r.color_correction_matrix, m_vfx_data_p);
            renderable_p->m_flags |= 0x40;
            renderable_p->set_color_correction_matrix(emitter_r.color_correction_matrix);
        }
        renderable_p->m_lod_large_particle_fade_min = emitter_r.lod_large_particle_fade_min;
        renderable_p->m_lod_large_particle_fade_max = emitter_r.lod_large_particle_fade_max;
        // Magic number: lighting came with version 0x16; older effects are fully self-lit.
        if (m_vfx_data_p->version < 0x16)
        {
            renderable_p->m_self_illum = 1.0f;
            renderable_p->m_ambient_color = rl_color_float(rl_color::BLACK);
            renderable_p->m_diffuse_color = rl_color_float(rl_color::BLACK);
        }
        else
        {
            renderable_p->m_self_illum = emitter_r.self_illum;
            renderable_p->m_ambient_color = rl_color_float(emitter_r.ambient_color, 1.0f);
            renderable_p->m_diffuse_color = rl_color_float(emitter_r.diffuse_color, 1.0f);
        }
        renderable_p->m_effect_material_percent = emitter_r.effect_material_percent;

        // The emitter.
        emitter_p->set_renderable(renderable_p);
        emitter_p->set_animation_duration(m_vfx_data_p->duration);
        // Magic number: velocity inheritance came with version 0x13.
        if (m_vfx_data_p->version < 0x13)
        {
            emitter_r.inherit_velocity = false;
        }
        emitter_p->set_inherit_velocity(emitter_r.inherit_velocity);
        emitter_p->set_lod_emission_distance(emitter_r.lod_emission_distance);
        emitter_p->set_position_keys(emitter_r.position_keys, emitter_r.num_position_keys);
        emitter_p->set_rotation_keys(emitter_r.rotation_keys, emitter_r.num_rotation_keys);
        emitter_p->set_scale_keys(emitter_r.scale_keys, emitter_r.num_scale_keys);
        emitter_p->set_emission_rate_keys(emitter_r.emission_rate_keys, emitter_r.num_emission_rate_keys);
        emitter_p->set_angular_speed_start_keys(emitter_r.angular_speed_start_keys, emitter_r.num_angular_speed_start_keys);
        emitter_p->set_angular_speed_end_keys(emitter_r.angular_speed_end_keys, emitter_r.num_angular_speed_end_keys);
        emitter_p->set_speed_keys(emitter_r.speed_keys, emitter_r.num_speed_keys);
        if (emitter_r.texture_anim_rate_keys.to_pointer() != nullptr)
        {
            emitter_p->set_texture_anim_rate_keys(emitter_r.texture_anim_rate_keys, emitter_r.num_texture_anim_rate_keys);
        }
        emitter_p->set_size_start_keys(emitter_r.size_start_keys, emitter_r.num_size_start_keys);
        emitter_p->set_size_start_var_keys(emitter_r.size_start_var_keys, emitter_r.num_size_start_var_keys);
        emitter_p->set_size_mida_keys(emitter_r.size_mida_keys, emitter_r.num_size_mida_keys);
        emitter_p->set_size_mida_var_keys(emitter_r.size_mida_var_keys, emitter_r.num_size_mida_var_keys);
        emitter_p->set_size_mida_offset_keys(emitter_r.size_mida_offset_keys, emitter_r.num_size_mida_offset_keys);
        emitter_p->set_size_midb_keys(emitter_r.size_midb_keys, emitter_r.num_size_midb_keys);
        emitter_p->set_size_midb_var_keys(emitter_r.size_midb_var_keys, emitter_r.num_size_midb_var_keys);
        emitter_p->set_size_midb_offset_keys(emitter_r.size_midb_offset_keys, emitter_r.num_size_midb_offset_keys);
        emitter_p->set_size_end_keys(emitter_r.size_end_keys, emitter_r.num_size_end_keys);
        emitter_p->set_size_end_var_keys(emitter_r.size_end_var_keys, emitter_r.num_size_end_var_keys);
        emitter_p->set_length_start_keys(emitter_r.length_start_keys, emitter_r.num_length_start_keys);
        emitter_p->set_length_start_var_keys(emitter_r.length_start_var_keys, emitter_r.num_length_start_var_keys);
        emitter_p->set_length_mida_keys(emitter_r.length_mida_keys, emitter_r.num_length_mida_keys);
        emitter_p->set_length_mida_var_keys(emitter_r.length_mida_var_keys, emitter_r.num_length_mida_var_keys);
        emitter_p->set_length_mida_offset_keys(emitter_r.length_mida_offset_keys, emitter_r.num_length_mida_offset_keys);
        emitter_p->set_length_midb_keys(emitter_r.length_midb_keys, emitter_r.num_length_midb_keys);
        emitter_p->set_length_midb_var_keys(emitter_r.length_midb_var_keys, emitter_r.num_length_midb_var_keys);
        emitter_p->set_length_midb_offset_keys(emitter_r.length_midb_offset_keys, emitter_r.num_length_midb_offset_keys);
        emitter_p->set_length_end_keys(emitter_r.length_end_keys, emitter_r.num_length_end_keys);
        emitter_p->set_length_end_var_keys(emitter_r.length_end_var_keys, emitter_r.num_length_end_var_keys);
        emitter_p->set_is_iterative(emitter_r.is_iterative);
        emitter_p->set_is_reverse_render(emitter_r.is_reverse_render);
        emitter_p->set_iterations_per_sec(emitter_r.interations_per_second);
        emitter_p->set_local_space_emission(emitter_r.local_space_emission);
        emitter_p->set_use_accel_values(emitter_r.acceleration_enabled);
        emitter_p->set_acceleration_start(emitter_r.acceleration_start);
        emitter_p->set_acceleration_end(emitter_r.acceleration_end);
        emitter_p->set_use_accel_variance(emitter_r.acceleration_variance_enabled);
        emitter_p->set_acceleration_start_variance(emitter_r.acceleration_start_variance);
        emitter_p->set_acceleration_end_variance(emitter_r.acceleration_end_variance);
        emitter_p->set_use_color_variance(true);
        emitter_p->set_use_color_mid_value(emitter_r.use_color_mid_value);
        emitter_p->set_is_color_variance_by_value(emitter_r.is_color_variance_by_value);
        emitter_p->set_color_start(emitter_r.color_start);
        emitter_p->set_color_start_variance(emitter_r.color_start_variance);
        emitter_p->set_color_end(emitter_r.color_end);
        emitter_p->set_color_end_variance(emitter_r.color_end_variance);
        emitter_p->set_color_mid_A(emitter_r.color_mid_A);
        emitter_p->set_color_mid_A_variance(emitter_r.color_mid_A_variance);
        emitter_p->set_color_mid_A_offset(emitter_r.color_mid_A_offset);
        emitter_p->set_color_mid_B(emitter_r.color_mid_B);
        emitter_p->set_color_mid_B_variance(emitter_r.color_mid_B_variance);
        emitter_p->set_color_mid_B_offset(emitter_r.color_mid_B_offset);
        emitter_p->set_alpha_mid_A_offset(emitter_r.alpha_mid_A_offset);
        emitter_p->set_alpha_mid_B_offset(emitter_r.alpha_mid_B_offset);
        emitter_p->set_use_color_saturation_value(emitter_r.use_color_saturation_value);
        emitter_p->set_color_saturation_start(emitter_r.color_saturation_start);
        emitter_p->set_color_saturation_start_variance(emitter_r.color_saturation_start_variance);
        emitter_p->set_color_saturation_end(emitter_r.color_saturation_end);
        emitter_p->set_color_saturation_end_variance(emitter_r.color_saturation_end_variance);
        emitter_p->set_color_saturation_mid_A(emitter_r.color_saturation_mid_A);
        emitter_p->set_color_saturation_mid_A_variance(emitter_r.color_saturation_mid_A_variance);
        emitter_p->set_color_saturation_mid_B(emitter_r.color_saturation_mid_B);
        emitter_p->set_color_saturation_mid_B_variance(emitter_r.color_saturation_mid_B_variance);
        emitter_p->set_use_additive_key_value(emitter_r.use_additive_key_value);
        emitter_p->set_additive_key_start(emitter_r.additive_key_start);
        emitter_p->set_additive_key_start_variance(emitter_r.additive_key_start_variance);
        emitter_p->set_additive_key_end(emitter_r.additive_key_end);
        emitter_p->set_additive_key_end_variance(emitter_r.additive_key_end_variance);
        emitter_p->set_additive_key_mid_A(emitter_r.additive_key_mid_A);
        emitter_p->set_additive_key_mid_A_variance(emitter_r.additive_key_mid_A_variance);
        emitter_p->set_additive_key_mid_B(emitter_r.additive_key_mid_B);
        emitter_p->set_additive_key_mid_B_variance(emitter_r.additive_key_mid_B_variance);
        emitter_p->set_weight_start(emitter_r.weight_start);
        emitter_p->set_weight_end(emitter_r.weight_end);
        emitter_p->set_density(emitter_r.density);
        emitter_p->set_density_variance(emitter_r.density_variance);
        emitter_p->set_use_density_variance(emitter_r.density_variance > 0.0f);
        emitter_p->set_num_tails(emitter_r.num_tails);
        emitter_p->set_particle_type((rl_particle_base::render_type)emitter_r.particle_type);
        emitter_p->set_yaw_variance(emitter_r.yaw_variance);
        emitter_p->set_pitch_variance(emitter_r.pitch_variance);
        emitter_p->set_max_particles(emitter_r.max_particles);
        emitter_p->set_use_size_variance(true);
        emitter_p->set_use_size_mid_value(emitter_r.use_size_mid_value);
        emitter_p->set_use_length_mid_value(emitter_r.use_length_mid_value);
        emitter_p->set_use_length(emitter_r.use_length);
        emitter_p->set_use_lifetime_variance(emitter_r.use_lifetime_variance);
        emitter_p->set_lifetime(emitter_r.lifetime);
        emitter_p->set_lifetime_variance(emitter_r.lifetime_variance);
        emitter_p->set_speed_variance(emitter_r.speed_variance);
        emitter_p->set_is_rotation_enabled(emitter_r.rotation_enabled);
        emitter_p->set_angular_speed_start_variance(emitter_r.angular_speed_start_variance);
        emitter_p->set_angular_speed_end_variance(emitter_r.angular_speed_end_variance);
        emitter_p->set_is_random_start_rotation(emitter_r.is_random_start_rotation);
        factory_p->m_serialize(emitter_p, emitter_r.unique_data);
        emitter_p->init();
        // Magic number: a density within 0.0001 of zero turns the iterations off.
        if (emitter_p->get_density() - 0.0f < 0.0001f && -(emitter_p->get_density() - 0.0f) < 0.0001f)
        {
            emitter_p->set_iterations_per_sec(0.0f);
        }
    }

    for (int i = 0; i < m_vfx_data_p->num_emitters; i++)
    {
        emitter& emitter_r = m_vfx_data_p->emitters[i];
        vfx_particle_system_emitter* emitter_p = emitter_r.particle_system_emitter_p;
        // Magic number: version 0x11 files have no emit node data.
        if (m_vfx_data_p->version == 0x11)
        {
            emitter_r.is_emit_node = false;
            continue;
        }
        // The emit node list holds emitter indices; they become the emitters.
        et_sized_native_pointer<vfx_particle_system_emitter>* node_emitters = emitter_r.emit_node_emitters;
        for (int j = 0; j < emitter_r.num_emit_node_emitters; j++)
        {
            const int index = (int)node_emitters[j].to_pointer();
            node_emitters[j] = m_vfx_data_p->emitters[index].particle_system_emitter_p.to_pointer();
        }
        emitter_p->set_emit_node_emitter_list(node_emitters, emitter_r.num_emit_node_emitters);
    }

    for (int i = 0; i < m_vfx_data_p->num_filters; i++)
    {
        filter& filter_r = m_vfx_data_p->filters[i];
        // Notes (decompiled behaviour, kept as is): a missing factory is printed and then called through null.
        vfx_class_factory_info* factory_p = vfx_class_factory_info::get_class_factory(filter_r.class_id);
        filter_r.particle_system_filter_p = (vfx_particle_system_filter*)factory_p->m_create();
        vfx_particle_system_filter* filter_p = filter_r.particle_system_filter_p;
        factory_p->m_serialize(filter_p, filter_r.unique_data);
        filter_p->set_animation_duration(m_vfx_data_p->duration);
        filter_p->set_num_position_keys(filter_r.num_position_keys);
        filter_p->set_position_keys(filter_r.position_keys);
        filter_p->set_num_rotation_keys(filter_r.num_rotation_keys);
        filter_p->set_rotation_keys(filter_r.rotation_keys);
    }
    m_initialized = true;
    return true;
}

// 0x00CAF350 (:646): releases the emitters with their renderables, the filters, the effect meshes with their material
// maps and materials, and unregisters the mesh shapes.
// Notes (decompiled behaviour, kept as is): the material maps and materials are released per mesh renderable, the i-th
// renderable taking the i-th mesh's.
void vfx_effect_template::finalize()
{
    if (!m_initialized)
    {
        return;
    }
    if (m_vfx_data_p != nullptr)
    {
        for (int i = 0; i < m_vfx_data_p->num_emitters; i++)
        {
            vfx_particle_system_emitter* emitter_p = m_vfx_data_p->emitters[i].particle_system_emitter_p;
            rl_renderable_particles* renderable_p = emitter_p->get_renderable_particles();
            if (renderable_p != nullptr)
            {
                renderable_p->release();
            }
            emitter_p->release();
        }
        for (int i = 0; i < m_vfx_data_p->num_filters; i++)
        {
            ((vfx_particle_system_filter*)m_vfx_data_p->filters[i].particle_system_filter_p)->release();
        }
    }
    int mesh_index = 0;
    rl_renderable* renderable_p = m_mesh_renderables.first();
    while (renderable_p != nullptr)
    {
        rl_renderable* next_p = renderable_p->next != m_mesh_renderables.first() ? renderable_p->next : nullptr;
        renderable_p->release();
        m_mesh_renderables.remove(renderable_p);
        mesh& mesh_r = m_vfx_data_p->meshes[mesh_index];
        ((rl_material_map*)mesh_r.material_map_p)->release();
        for (int j = 0; j < (int)mesh_r.num_materials; j++)
        {
            rl_material* material_p = ((et_sized_native_pointer<rl_material>*)mesh_r.materials)[j];
            rl_material_library_unregister(material_p);
            material_p->release();
        }
        mesh_index++;
        renderable_p = next_p;
    }
    for (int i = 0; i < m_vfx_data_p->num_meshes; i++)
    {
        rl_handle shape_handle;
        shape_handle.set((unsigned int)(unsigned char*)m_vfx_data_p->meshes[i].collision_model_data_p);
        vfx_system::get_shape_unregister_cb()(shape_handle);
    }
}

// 0x00C9D200 (.h:48) and 0x00C9D210 (.h:48) are RL_FACTORY_CLASS_IMPLEMENTATION's get_object_pool and get_objects_max.

// 0x00C9D220 (.h:429)
void vfx_effect_template::set_handle(int insthandle)
{
    m_instance_handle = insthandle;
}

// 0x00C9D230 (.h:430)
int vfx_effect_template::get_handle() const
{
    return m_instance_handle;
}

// 0x00741DF0 (.h:432)
float vfx_effect_template::get_duration() const
{
    return m_vfx_data_p->duration;
}

// 0x00741E00 (.h:433)
float vfx_effect_template::get_bounding_radius() const
{
    return m_vfx_data_p->radius;
}

// 0x00CA3130 (.h:519)
int vfx_effect_template::get_mesh_type(int mesh_index)
{
    return (int)m_vfx_data_p->meshes[mesh_index].type;
}

// 0x00CA3150 (.h:525)
vfx_vector_key* vfx_effect_template::get_mesh_position_keys(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].position_keys;
}

// 0x00CA3170 (.h:531)
int vfx_effect_template::get_mesh_position_keys_count(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].num_position_keys;
}

// 0x00CA3190 (.h:537)
vfx_quat_key* vfx_effect_template::get_mesh_rotation_keys(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].rotation_keys;
}

// 0x00CA31B0 (.h:543)
int vfx_effect_template::get_mesh_rotation_keys_count(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].num_rotation_keys;
}

// 0x00CA31D0 (.h:549)
vfx_vector_key* vfx_effect_template::get_mesh_scale_keys(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].scale_keys;
}

// 0x00CA31F0 (.h:555)
int vfx_effect_template::get_mesh_scale_keys_count(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].num_scale_keys;
}

// 0x00CA3210 (.h:561)
vfx_float_key* vfx_effect_template::get_mesh_opacity_keys(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].opacity_keys;
}

// 0x00CA3230 (.h:567)
int vfx_effect_template::get_mesh_opacity_keys_count(int mesh_index)
{
    return m_vfx_data_p->meshes[mesh_index].num_opacity_keys;
}

// 0x00C9D240 (.h:447)
int vfx_effect_template::get_num_lights() const
{
    return m_vfx_data_p->num_lights;
}

// 0x00CA3250 (.h:573)
vfx_effect_template::light* vfx_effect_template::get_light(int light_index) const
{
    return &m_vfx_data_p->lights[light_index];
}

// 0x005D1840 (.h:451)
int vfx_effect_template::get_emitter_count() const
{
    return m_vfx_data_p->num_emitters;
}

// 0x005D7150 (.h:589)
vfx_particle_system_emitter* vfx_effect_template::get_emitter(int emitter_index) const
{
    return m_vfx_data_p->emitters[emitter_index].particle_system_emitter_p;
}

// 0x005D7170 (.h:607)
const char* vfx_effect_template::get_emitter_name(int emitter_index) const
{
    if (emitter_index >= m_vfx_data_p->num_emitters)
    {
        return nullptr;
    }
    return m_vfx_data_p->emitters[emitter_index].name;
}

// 0x00CA3270 (.h:614)
int vfx_effect_template::get_emitter_parent_index(int emitter_index) const
{
    if (emitter_index >= m_vfx_data_p->num_emitters)
    {
        return -1;
    }
    return m_vfx_data_p->emitters[emitter_index].parent_index;
}

// 0x00CA32A0 (.h:620)
int vfx_effect_template::get_filter_parent_index(int filter_index) const
{
    if (filter_index >= m_vfx_data_p->num_filters)
    {
        return -1;
    }
    return m_vfx_data_p->filters[filter_index].parent_index;
}

// 0x00C9D250 (.h:457)
int vfx_effect_template::get_filter_count() const
{
    return m_vfx_data_p->num_filters;
}

// 0x00CAB8B0 (.h:626)
vfx_particle_system_filter* vfx_effect_template::get_filter(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].particle_system_filter_p;
}

// 0x00CA32C0 (.h:643)
vfx_vector_key* vfx_effect_template::get_filter_position_keys(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].position_keys;
}

// 0x00CA32E0 (.h:648)
int vfx_effect_template::get_filter_num_position_keys(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].num_position_keys;
}

// 0x00CA3300 (.h:653)
vfx_quat_key* vfx_effect_template::get_filter_rotation_keys(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].rotation_keys;
}

// 0x00CA3320 (.h:658)
int vfx_effect_template::get_filter_num_rotation_keys(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].num_rotation_keys;
}

// The force, noise and angular velocity keys sit in the filter's unique data at fixed places (the layout the magnet,
// turbulence and vortex filters serialize: count then pointer, 8 bytes each; decompiled).
static unsigned char* vfx_filter_unique_data(vfx_effect_template::vfx_data* vfx_data_p, int filter_index)
{
    return vfx_data_p->filters[filter_index].unique_data;
}

// 0x00CA3340 (.h:663)
vfx_float_key* vfx_effect_template::get_filter_force_keys(int filter_index) const
{
    return *(vfx_float_key**)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x08);
}

// 0x00CA3360 (.h:669)
int vfx_effect_template::get_filter_num_force_keys(int filter_index) const
{
    return *(int*)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x00);
}

// 0x00CA3380 (.h:675)
vfx_float_key* vfx_effect_template::get_filter_noise_keys(int filter_index) const
{
    return *(vfx_float_key**)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x18);
}

// 0x00CA33A0 (.h:681)
int vfx_effect_template::get_filter_num_noise_keys(int filter_index) const
{
    return *(int*)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x10);
}

// 0x00CA33C0 (.h:687)
vfx_float_key* vfx_effect_template::get_filter_angular_velocity_keys(int filter_index) const
{
    return *(vfx_float_key**)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x28);
}

// 0x00CA33E0 (.h:693)
int vfx_effect_template::get_filter_num_angular_velocity_keys(int filter_index) const
{
    return *(int*)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x20);
}

// 0x00CA3400 (.h:699)
vfx_float_key* vfx_effect_template::get_filter_angular_velocity_variance_keys(int filter_index) const
{
    return *(vfx_float_key**)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x38);
}

// 0x00CA3420 (.h:705)
int vfx_effect_template::get_filter_num_angular_velocity_variance_keys(int filter_index) const
{
    return *(int*)(vfx_filter_unique_data(m_vfx_data_p, filter_index) + 0x30);
}

// 0x00CA3440 (.h:711)
bool vfx_effect_template::get_filter_is_global(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].flags == FILTER_FLAGS_GLOBAL;
}

// 0x00CA3460 (.h:716)
int* vfx_effect_template::get_filter_affected_emitters(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].affected_emitters;
}

// 0x00CA3480 (.h:721)
int vfx_effect_template::get_filter_affected_emitters_count(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].num_affected_emitters;
}

// 0x00CA34A0 (.h:727)
int* vfx_effect_template::get_filter_affected_meshes(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].affected_meshes;
}

// 0x00CA34C0 (.h:732)
int vfx_effect_template::get_filter_affected_meshes_count(int filter_index) const
{
    return m_vfx_data_p->filters[filter_index].num_affected_meshes;
}

// 0x00CA34E0 (.h:749): serialize stored the shape handle in place of the collision model pointer.
rl_handle vfx_effect_template::get_shape_handle(int mesh_index) const
{
    rl_handle result;
    result.set((unsigned int)(unsigned char*)m_vfx_data_p->meshes[mesh_index].collision_model_data_p);
    return result;
}

// 0x00CA3500 (.h:758)
unsigned int vfx_effect_template::get_shape_material_index(int mesh_index) const
{
    return m_vfx_data_p->meshes[mesh_index].shape_material_index;
}

// 0x00CA3520 (.h:764)
float vfx_effect_template::get_shape_volume(int mesh_index) const
{
    return m_vfx_data_p->meshes[mesh_index].shape_volume;
}

// 0x00CA3540 (.h:769)
const vector& vfx_effect_template::get_shape_center_of_mass(int mesh_index) const
{
    return m_vfx_data_p->meshes[mesh_index].shape_center_of_mass;
}

// 0x00CA3560 (.h:774)
const matrix& vfx_effect_template::get_shape_inertia_tensor(int mesh_index) const
{
    return m_vfx_data_p->meshes[mesh_index].shape_inertia_tensor;
}

// 0x005D7140 (.h:482)
char* vfx_effect_template::get_effect_name() const
{
    return m_vfx_data_p->effect_name;
}

// 0x00C9D260 (.h:483)
void vfx_effect_template::set_peg_filename(const char* _peg_filename)
{
    peg_filename = _peg_filename;
}

// 0x00C9D270 (.h:484)
const char* vfx_effect_template::get_peg_filename() const
{
    return peg_filename;
}

// 0x00C9D280 (.h:738)
linked_list_p<rl_renderable>& vfx_effect_template::get_mesh_renderables()
{
    return m_mesh_renderables;
}

// 0x00CAB8D0 (.h:743)
rl_material_map* vfx_effect_template::get_material_map(int mesh_index) const
{
    return m_vfx_data_p->meshes[mesh_index].material_map_p;
}

// 0x00741E10 (.h:780)
int vfx_effect_template::get_expression_count() const
{
    return m_vfx_data_p->num_expressions;
}

// 0x007476C0 (.h:785)
const vfx_effect_template::expression& vfx_effect_template::get_expression(int expression_index) const
{
    return m_vfx_data_p->expressions[expression_index];
}

// 0x00CA3580 (.h:791)
bool vfx_effect_template::get_is_emit_node_emitter(int emitter_index) const
{
    return m_vfx_data_p->emitters[emitter_index].is_emit_node;
}

// 0x00C9D290 (.h:803)
int vfx_effect_template::get_num_coronas() const
{
    return m_vfx_data_p->num_coronas;
}

// 0x00CA35A0 (.h:808)
vfx_effect_template::corona* vfx_effect_template::get_corona(int corona_index) const
{
    return &m_vfx_data_p->coronas[corona_index];
}
