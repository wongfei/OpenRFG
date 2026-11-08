// Original: volition/render_lib/code/rl_particle_renderer.cpp
//
// The particle renderer: construction, initialization (particle pools, shared index/uv geometry, shader and technique
// lookups, low LOD quarter resolution targets), resource release/renewal, the pool sizes and (VFX.md FX2) the
// particle and corona drawing through keen's immediate vertex buffer. The fillers are in rl_particle_filler_*.cpp, the
// deferred passes in rl_particle_renderer_deferred.cpp.

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_corona_instance.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_particle_lighting_state.h"
#include "volition/render_lib/code/rl_particle_material.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "keen/base/trace.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/vertexformat.hpp"

#include <math.h>
#include <string.h>
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/rendergeometry.hpp"

// The particle pool sizes (counts; game_render_system_init sets 900 iterative, 0xE00 parametric, 0x400 tails).
int rl_particle_renderer::ms_particle_iterative_pool_size = 0x200; // 0x014FA314
int rl_particle_renderer::ms_particle_parametric_pool_size = 0xE00; // 0x014FA318
int rl_particle_renderer::ms_particle_tail_pool_size = 0x1000; // 0x014FA31C
mempool rl_particle_renderer::ms_particle_parametric_pool; // 0x01C209F8
mempool rl_particle_renderer::ms_particle_iterative_pool; // 0x01C1A9A0
mempool rl_particle_renderer::ms_particle_tail_pool; // 0x01C273B0

// :43
// <rl_particle_renderer::release> 0x004C3D80 (:43)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_particle_renderer)

// 0x004C3DA0
rl_particle_renderer::particle_shader_techniques::particle_shader_techniques()
{
}

// 0x004C3DC0
rl_particle_renderer::particle_shader_techniques::~particle_shader_techniques()
{
}

// 0x00529820 (:205)
// The vertex layouts of the particle types (rl_particle_base::render_type), iterative and parametric.
rl_particle_renderer::rl_particle_renderer()
{
    m_quarter_ds_surface.pData = nullptr;
    m_iterative_vertex_info[1].format = RLVF_INVALID;
    m_iterative_vertex_info[3].format = RLVF_INVALID;
    m_iterative_vertex_info[4].format = RLVF_INVALID;
    m_iterative_vertex_info[5].format = RLVF_INVALID;
    m_iterative_vertex_info[1].stride = 0;
    m_iterative_vertex_info[3].stride = 0;
    m_iterative_vertex_info[4].stride = 0;
    m_iterative_vertex_info[5].stride = 0;
    // Magic numbers: the sizes of the vertex structs (parametric_vertex 0x34, billboard_vertex 0x30, ...).
    m_iterative_vertex_info[6].stride = 0x34;
    m_parametric_vertex_info[0].stride = 0x34;
    m_parametric_vertex_info[6].stride = 0x34;
    m_parametric_vertex_info[7].stride = 0x34;
    m_parametric_vertex_info[8].stride = 0x34;
    m_parametric_vertex_info[9].stride = 0x34;
    m_parametric_vertex_info[0].format = RLVF_PARTICLE_PARAMETRIC;
    m_parametric_vertex_info[6].format = RLVF_PARTICLE_PARAMETRIC;
    m_parametric_vertex_info[7].format = RLVF_PARTICLE_PARAMETRIC;
    m_parametric_vertex_info[8].format = RLVF_PARTICLE_PARAMETRIC;
    m_parametric_vertex_info[9].format = RLVF_PARTICLE_PARAMETRIC;
    m_iterative_vertex_info[0].format = RLVF_PARTICLE_BILLBOARD;
    m_iterative_vertex_info[0].stride = 0x30;
    m_iterative_vertex_info[6].format = RLVF_PARTICLE_RADIAL;
    m_iterative_vertex_info[7].format = RLVF_PARTICLE_DROP;
    m_iterative_vertex_info[7].stride = 0x38;
    m_iterative_vertex_info[2].format = RLVF_PARTICLE_RIBBON;
    m_iterative_vertex_info[2].stride = 0x44;
    m_iterative_vertex_info[8].format = RLVF_PARTICLE_ORIENTED;
    m_iterative_vertex_info[8].stride = 0x38;
    m_iterative_vertex_info[9].format = RLVF_PARTICLE_CORONA;
    m_iterative_vertex_info[9].stride = 0xB0;
    m_parametric_vertex_info[2].format = RLVF_PARTICLE_RIBBON_PARAMETRIC;
    m_parametric_vertex_info[2].stride = 0x50;
}

// 0x00529AA0 (:242)
// Notes (decompiled behaviour, kept as is): the CPU copies of the index and uv data (create_buffers) are not freed,
// and the particle pools are not reset.
rl_particle_renderer::~rl_particle_renderer()
{
    keen::GraphicsSystem* graphics_system_p = Init_params.m_pKeenGraphicsSystem;
    keen::graphics::destroyDynamicConstantBuffer(graphics_system_p, m_pFragmentConstants);
    keen::graphics::destroyDynamicConstantBuffer(graphics_system_p, m_pVertexConstants);
    keen::graphics::freeRenderGeometry(&m_billboard, graphics_system_p, nullptr);
    keen::graphics::freeRenderGeometry(&m_drop, graphics_system_p, nullptr);
    keen::graphics::freeRenderGeometry(&m_ribbon, graphics_system_p, nullptr);
    keen::graphics::freeRenderGeometry(&m_uv, graphics_system_p, nullptr);
    if (m_quarter_ds_surface.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_quarter_ds_surface.pData);
    }
    m_quarter_ds_surface.pData = nullptr;
    delete[] (unsigned char*)ms_particle_iterative_pool.get_base();
    delete[] (unsigned char*)ms_particle_parametric_pool.get_base();
    delete[] (unsigned char*)ms_particle_tail_pool.get_base();
}

// 0x0054FBE0 (:273)
// Creates the particle pools and buffers, looks up the particle shaders and their techniques, and creates the
// quarter resolution targets (low LOD only).
void rl_particle_renderer::initialize(const int width, const int height, const rl_level_of_detail& lod)
{
    m_level_of_detail = lod.m_particle_lod;
    // Magic numbers: 64 bytes per parametric particle, 128 per iterative particle, 40 per tail (computed once).
    static const int particle_parametric_pool_size = ms_particle_parametric_pool_size << 6;
    static const int particle_iterative_pool_size = ms_particle_iterative_pool_size << 7;
    static const int particle_tail_pool_size = ms_particle_tail_pool_size * 0x28;
    // Magic numbers: 128-byte alignment, mempool flag 8.
    ms_particle_iterative_pool.init(new unsigned char[particle_iterative_pool_size], particle_iterative_pool_size, "rl_particle_global_mempool", 0x80, 8);
    ms_particle_parametric_pool.init(new unsigned char[particle_parametric_pool_size], particle_parametric_pool_size, "rl_particle_global_mempool", 0x80, 8);
    ms_particle_tail_pool.init(new unsigned char[particle_tail_pool_size], particle_tail_pool_size, "rl_particle_tail_global_mempool", 0x80, 8);
    create_buffers();

    rl_shader_library* library_p = rl_shader_library::get_instance();
    m_particle_type_shaders[0] = library_p->lookup("rl_particle_standard_billboard");
    m_particle_type_shaders[1] = 0xFFFFFFFF;
    m_particle_type_shaders[2] = library_p->lookup("rl_particle_standard_ribbon");
    m_particle_type_shaders[3] = 0xFFFFFFFF;
    m_particle_type_shaders[4] = 0xFFFFFFFF;
    m_particle_type_shaders[5] = 0xFFFFFFFF;
    m_particle_type_shaders[6] = library_p->lookup("rl_particle_standard_radial");
    m_particle_type_shaders[7] = library_p->lookup("rl_particle_standard_drop");
    m_particle_type_shaders[8] = library_p->lookup("rl_particle_standard_oriented");
    m_particle_type_shaders[9] = 0xFFFFFFFF;
    // Magic number: 10 particle types.
    for (int i = 0; i < 10; i++)
    {
        rl_shader_technique_ref* refs_p = m_particle_shader_techniques[i].m_technique_refs;
        const unsigned int shader = m_particle_type_shaders[i];
        if (shader == 0xFFFFFFFF)
        {
            refs_p[0].set_ref(-1);
            refs_p[1].set_ref(-1);
            refs_p[2].set_ref(-1);
            refs_p[3].set_ref(-1);
            continue;
        }
        refs_p[0].set_ref(library_p->get_shader_user_technique_index(shader, crc_stri("Base", 0)));
        refs_p[1].set_ref(library_p->get_shader_user_technique_index(shader, crc_stri("Base_parametric", 0)));
        refs_p[2].set_ref(library_p->get_shader_user_technique_index(shader, crc_stri("Soft", 0)));
        refs_p[3].set_ref(library_p->get_shader_user_technique_index(shader, crc_stri("Soft_parametric", 0)));
    }
    // The corona type (9) has its own techniques.
    m_particle_type_shaders[9] = library_p->lookup("rl_corona");
    m_particle_shader_techniques[9].m_technique_refs[0].set_ref(library_p->get_shader_user_technique_index(m_particle_type_shaders[9], crc_stri("Base", 0)));
    m_particle_shader_techniques[9].m_technique_refs[1].set_ref(library_p->get_shader_user_technique_index(m_particle_type_shaders[9], crc_stri("Visibility", 0)));

    m_quarter_width = width >> 2;
    m_quarter_height = height >> 2;
    create_render_targets();

    m_downsample_depth_effect = library_p->lookup("rl_particle_downsample_depth_pc");
    m_downscale_sampling_offsets_const.set_ref(library_p->get_shader_const_index(m_downsample_depth_effect, crc_stri("Sampling_offsets", 0)));
    m_downscale_z_min_and_range_const.set_ref(library_p->get_shader_const_index(m_downsample_depth_effect, crc_stri("Z_min_and_range", 0)));
    m_downscale_depth_sampler_const.set_ref(library_p->get_shader_tmu_index(m_downsample_depth_effect, crc_stri("depth_sampler", 0)));
    m_up_res_effect = library_p->lookup("rl_particle_composite_low_res");
    m_up_res_sampler_const.set_ref(library_p->get_shader_tmu_index(m_up_res_effect, crc_stri("backbuffer_sampler", 0)));
    m_particle_brightpass_effect = library_p->lookup("rl_particle_brightpass");
    // A 4x4 box of texel offsets around the center (rl_downscale_sampling_offsets<16>::init inlined, x and y only).
    // Magic number: 1.5 texels, the center of the 4x4 box.
    for (int x = 0; x < 4; x++)
    {
        for (int y = 0; y < 4; y++)
        {
            m_brightpass_downscale_sampling.m_values[x + y * 4].x = ((float)x - 1.5f) * (1.0f / (float)width);
            m_brightpass_downscale_sampling.m_values[x + y * 4].y = ((float)y - 1.5f) * (1.0f / (float)height);
        }
    }
    m_brightpass_downscale_sampling_offsets_const.set_ref(library_p->get_shader_const_index(m_particle_brightpass_effect, crc_stri("Sampling_offsets", 0)));
    m_brightpass_sampler.set_ref(library_p->get_shader_tmu_index(m_particle_brightpass_effect, crc_stri("base_sampler", 0)));
}

// 0x004F03C0 (:46)
// The shared particle geometry: index strips for billboards (quads of 4 vertices stitched with a degenerate pair),
// drops (20 vertices) and ribbons (a straight sequence), and the corner uvs of the parametric particles.
// Notes (decompiled behaviour, kept as is): the CPU copies of the data stay allocated from the buffer allocator.
void rl_particle_renderer::create_buffers()
{
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    keen::GraphicsSystem* graphics_system_p = Init_params.m_pKeenGraphicsSystem;
    keen::RenderGeometryDescription description;
    description.primitiveType = keen::PrimitiveType_Invalid;
    description.pVertexFormat = nullptr;
    description.indexFormat = keen::IndexFormat_Uint16;
    description.vertexCount = 0;
    description.indexCount = 0;
    description.cpuAccessMode = 0;
    description.streamIndex = 0;
    keen::RenderGeometryInitializationData init_data;
    init_data.pVertexData = nullptr;
    init_data.pIndexData = nullptr;
    init_data.pSkinningData = nullptr;
    init_data.pMorphingData = nullptr;
    init_data.pPlatformData = nullptr;

    // Magic numbers: 500 billboards of 6 indices (4 vertices, the last repeated, then the next first).
    short* billboard_indices_p = (short*)allocator_p->allocate(6000, 16, 0, nullptr);
    const short billboard_pattern[6] = {0, 1, 2, 3, 3, 4};
    for (int i = 0; i < 3000; i++)
    {
        billboard_indices_p[i] = (short)((i / 6) * 4) + billboard_pattern[i % 6];
    }
    init_data.pIndexData = billboard_indices_p;
    description.indexFormat = keen::IndexFormat_Uint16;
    description.indexCount = 3000;
    description.primitiveType = keen::PrimitiveType_TriangleStrip;
    keen::graphics::initializeRenderGeometry(&m_billboard, graphics_system_p, description, init_data, "rl_particle_renderer billboard indices");

    // Magic numbers: 500 drops of 22 indices (20 vertices, the last repeated, then the next first).
    short* drop_indices_p = (short*)allocator_p->allocate(22000, 16, 0, nullptr);
    short drop_pattern[22];
    for (short i = 0; i < 22; i++)
    {
        drop_pattern[i] = i < 20 ? i : (short)(19 - 20 + i);
    }
    for (int i = 0; i < 11000; i++)
    {
        drop_indices_p[i] = (short)((i / 22) * 20) + drop_pattern[i % 22];
    }
    init_data.pIndexData = drop_indices_p;
    description.indexFormat = keen::IndexFormat_Uint16;
    description.indexCount = 11000;
    description.primitiveType = keen::PrimitiveType_TriangleStrip;
    keen::graphics::initializeRenderGeometry(&m_drop, graphics_system_p, description, init_data, "rl_particle_renderer billboard indices");

    // Magic number: 1000 ribbon indices.
    short* ribbon_indices_p = (short*)allocator_p->allocate(2000, 16, 0, nullptr);
    for (short i = 0; i < 1000; i++)
    {
        ribbon_indices_p[i] = i;
    }
    description.indexCount = 1000;
    init_data.pIndexData = ribbon_indices_p;
    description.indexFormat = keen::IndexFormat_Uint16;
    description.primitiveType = keen::PrimitiveType_TriangleStrip;
    keen::graphics::initializeRenderGeometry(&m_ribbon, graphics_system_p, description, init_data, "rl_particle_renderer billboard indices");

    // Magic numbers: 500 quads of corner uvs (0,0) (0,1) (1,0) (1,1) as 16-bit pairs.
    short* uvs_p = (short*)allocator_p->allocate(8000, 16, 0, nullptr);
    for (int i = 0; i < 500; i++)
    {
        short* quad_p = uvs_p + i * 8;
        quad_p[0] = 0;
        quad_p[1] = 0;
        quad_p[2] = 0;
        quad_p[3] = 1;
        quad_p[4] = 1;
        quad_p[5] = 0;
        quad_p[6] = 1;
        quad_p[7] = 1;
    }
    init_data.pVertexData = uvs_p;
    description.indexFormat = keen::IndexFormat_Invalid;
    description.primitiveType = keen::PrimitiveType_TriangleStrip;
    description.pVertexFormat = rl_render_lib::get_instance()->m_vertex_formats_p[RLVF_PARTICLE_PARAMETRIC];
    description.vertexCount = 2000;
    init_data.pIndexData = nullptr;
    keen::graphics::initializeRenderGeometry(&m_uv, graphics_system_p, description, init_data, "rl_particle_renderer billboard indices");

    m_pVertexConstants = keen::graphics::createDynamicConstantBuffer(graphics_system_p, 0, sizeof(keen::ParticleVertexShaderConstants));
    m_pFragmentConstants = keen::graphics::createDynamicConstantBuffer(graphics_system_p, 0, sizeof(keen::ParticleFragmentShaderConstants));
}

// 0x0054C6C0 (:1255)
// Only the low particle LOD renders into the quarter resolution targets.
void rl_particle_renderer::create_render_targets()
{
    if (m_level_of_detail != RL_PARTICLE_LOD_LOW)
    {
        return;
    }
    m_quarter_res_rt.init(m_quarter_width, m_quarter_height, RL_FORMAT_BLEND_LDR, 0, "ParticleQuarterRes", 0);
    m_quarter_depth_target.init(m_quarter_width, m_quarter_height, RL_FORMAT_R32_FLOAT, 0, "ParticleQuarterDepth", 0);
    m_quarter_ds_surface = rl_render_lib::get_instance()->create_depth_stencil_target(m_quarter_width, m_quarter_height, 0, "ParticleQuarterDS", 0);
}

// 0x00513E80 (:1240)
void rl_particle_renderer::release_resources()
{
    m_quarter_res_rt.destroy();
    m_quarter_depth_target.destroy();
    if (m_quarter_ds_surface.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_quarter_ds_surface.pData);
    }
    m_quarter_ds_surface.pData = nullptr;
}

// 0x00550470 (:1250)
// create_render_targets inlined.
void rl_particle_renderer::renew_resources()
{
    create_render_targets();
}

// 0x004C4120 (:1270)
void rl_particle_renderer::resize(int w, int h)
{
    m_quarter_width = w >> 2;
    m_quarter_height = h >> 2;
}

// 0x004C4140 (:1276)
void rl_particle_renderer::set_level_of_detail(const rl_level_of_detail& lod)
{
    m_level_of_detail = lod.m_particle_lod;
}

// 0x004C3ED0 (:402)
void rl_particle_renderer::set_particle_iterative_pool_size(int particle_iterative_pool_size)
{
    ms_particle_iterative_pool_size = particle_iterative_pool_size;
}

// 0x004C3EE0 (:407)
int rl_particle_renderer::get_particle_iterative_pool_size()
{
    return ms_particle_iterative_pool_size;
}

// 0x004C3EF0 (:413)
void rl_particle_renderer::set_particle_parametric_pool_size(int particle_parametric_pool_size)
{
    ms_particle_parametric_pool_size = particle_parametric_pool_size;
}

// 0x004C3F00 (:418)
int rl_particle_renderer::get_particle_parametric_pool_size()
{
    return ms_particle_parametric_pool_size;
}

// 0x004C3F10 (:423)
void rl_particle_renderer::set_particle_tail_pool_size(int particle_tail_pool_size)
{
    ms_particle_tail_pool_size = particle_tail_pool_size;
}

// 0x004C3F20 (:428)
int rl_particle_renderer::get_particle_tail_pool_size()
{
    return ms_particle_tail_pool_size;
}

// 0x00513C10 (:552)
// The deferred particle lists emptied for the next frame.
void rl_particle_renderer::reset_deferred()
{
    m_half_res_entries.remove_all();
    m_quarter_res_entries.remove_all();
    m_full_res_entries.remove_all();
    m_distortion_entries.remove_all();
    m_corona_entries.remove_all();
}

// ---- FX2: particle drawing (VFX.md FX2)

// Coronas adapt their visibility toward the occlusion query result (initialized data in rfg.exe, never written).
bool RL_corona_adaption = true; // 0x014FA301
float RL_corona_adaption_rate = 0.6f; // 0x014FA310

// 0x004C3BF0
rl_particle_renderer::parametric_vertex::parametric_vertex()
{
}

// 0x004C3BE0
rl_particle_renderer::billboard_vertex::billboard_vertex()
{
}

// 0x004C3C20
rl_particle_renderer::radial_vertex::radial_vertex()
{
}

// 0x004C3C00
rl_particle_renderer::drop_vertex::drop_vertex()
{
}

// 0x004C3C30
rl_particle_renderer::ribbon_parametric_vertex::ribbon_parametric_vertex()
{
}

// 0x004C3C40
rl_particle_renderer::ribbon_vertex::ribbon_vertex()
{
}

// 0x004C3C10
rl_particle_renderer::oriented_vertex::oriented_vertex()
{
}

// 0x004C3F30
rl_particle_renderer::corona_vertex::corona_vertex()
{
}


// 0x004BD710 (.h:434)
float rl_particle_renderer::get_uv_anim_time(float elapsed_time, float random, float anim_rate)
{
    return elapsed_time * anim_rate + random;
}

// 0x004E3EA0 (.h:423): the particle's age over its lifetime (clamped) times the animation rate, offset by its random
// frame when the renderable has random frames.
float rl_particle_renderer::get_uv_anim_time(rl_renderable_particles_instance* instance_p, const rl_particle_iterative& particle)
{
    // Magic number: keeps a zero lifetime from dividing by zero.
    float age = (instance_p->get_current_elapsed_time() - particle.time_created) / (particle.lifetime + 1e-06f);
    if (age - 0.0f < 0.0f)
    {
        age = 0.0f;
    }
    if (0.0f <= age - 1.0f)
    {
        age = 1.0f;
    }
    float random = 0.0f;
    if (instance_p->get_renderable_particles()->get_texture_anim_random_frames())
    {
        random = particle.random.w;
    }
    return instance_p->get_texture_anim_rate() * age + random;
}

// 0x004BD730 (.h:440): empty in rfg.exe (the shader animates the uvs).
void rl_particle_renderer::uvs_animate(vector2& uvs, const rl_particle_iterative& particle)
{
}

// 0x004C3DD0 (:349): the particle pools are reset every frame (the instances' particle arrays are only valid until the
// next reset).
void rl_particle_renderer::particle_pool_reset()
{
    ms_particle_iterative_pool.set_thread_ownership();
    ms_particle_iterative_pool.clear();
    ms_particle_parametric_pool.set_thread_ownership();
    ms_particle_parametric_pool.clear();
    ms_particle_tail_pool.set_thread_ownership();
    ms_particle_tail_pool.clear();
}

// 0x004C3E10 (:359): 64 bytes per particle (sizeof(rl_particle_parametric)).
rl_particle_parametric* rl_particle_renderer::alloc_parametric_particles(int count)
{
    if (count != 0 && ms_particle_parametric_pool.can_alloc(count << 6, 0))
    {
        return (rl_particle_parametric*)ms_particle_parametric_pool.alloc(count << 6, 0);
    }
    return nullptr;
}

// 0x004C3E50 (:373): 128 bytes per particle (sizeof(rl_particle_iterative)).
rl_particle_iterative* rl_particle_renderer::alloc_iterative_particles(int count)
{
    if (count != 0 && ms_particle_iterative_pool.can_alloc(count << 7, 0))
    {
        return (rl_particle_iterative*)ms_particle_iterative_pool.alloc(count << 7, 0);
    }
    return nullptr;
}

// 0x004C3E90 (:387): 40 bytes per tail (sizeof(rl_particle_base::tail_data)).
rl_particle_base::tail_data* rl_particle_renderer::alloc_particle_tails(int count)
{
    if (count != 0 && ms_particle_tail_pool.can_alloc(count * 0x28, 0))
    {
        return (rl_particle_base::tail_data*)ms_particle_tail_pool.alloc(count * 0x28, 0);
    }
    return nullptr;
}

// 0x004C3F40 (:1166): the immediate vertices (and, for parametric particles, the shared corner uv stream) with the
// type's index buffer. keen's immediate geometry end is inlined here with the second stream and the index buffer
// (keen has no such command), so the D3D11 calls are made directly.
void rl_particle_renderer::draw_particles(rl_renderer& renderer, bool is_iterative, rl_particle_base::render_type current_render_type, unsigned int, unsigned int num_prims)
{
    keen::RenderGeometry* geometry_p = nullptr;
    switch (current_render_type)
    {
    case rl_particle_base::RENDER_TYPE_BILLBOARD:
    case rl_particle_base::RENDER_TYPE_RADIAL:
    case rl_particle_base::RENDER_TYPE_ORIENTED:
    case rl_particle_base::RENDER_TYPE_CORONA:
        geometry_p = &m_billboard;
        break;
    case rl_particle_base::RENDER_TYPE_RIBBON:
        geometry_p = &m_ribbon;
        break;
    case rl_particle_base::RENDER_TYPE_DROP:
        geometry_p = &m_drop;
        break;
    default:
        // Original bug kept: the null geometry is dereferenced below.
        keen::debug::nativePrintArgs("should never get here");
        break;
    }
    if (num_prims == 0)
    {
        num_prims = m_uv.pIndexBuffer == nullptr ? m_uv.vertexCount : m_uv.indexCount;
    }
    keen::GraphicsCommandBuffer* buffer_p = renderer.getCommandWriter()->getBuffer();
    ID3D11DeviceContext* context_p = buffer_p->pContext;
    context_p->Unmap(buffer_p->pImmediateVertexData, 0);
    ID3D11Buffer* buffers[2] = { buffer_p->pImmediateVertexData, m_uv.pVertexBuffer };
    UINT strides[2] = { buffer_p->immediateVertexDataStride, 0 };
    UINT offsets[2] = { buffer_p->immediateVertexBufferOffset, 0 };
    const UINT num_streams = is_iterative ? 1 : 2;
    if (!is_iterative)
    {
        strides[1] = buffer_p->pCurrentlyBoundVertexFormat->streamStride[1];
    }
    context_p->IASetVertexBuffers(0, num_streams, buffers, strides, offsets);
    context_p->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)m_uv.primitiveTopology);
    if (geometry_p->pIndexBuffer == nullptr)
    {
        context_p->Draw(num_prims, 0);
    }
    else
    {
        context_p->IASetIndexBuffer(geometry_p->pIndexBuffer, (DXGI_FORMAT)geometry_p->indexFormat, 0);
        context_p->DrawIndexed(num_prims, 0, 0);
    }
    buffer_p->immediateVertexBufferOffset += buffer_p->immediateVertexCount * buffer_p->immediateVertexDataStride;
    buffer_p->immediateVertexDataStride = 0;
}

// 0x00513990 (:433): once both occlusion queries of the corona have results, the pixel counts are kept and the
// queries freed.
void update_corona(rl_renderer& renderer, rl_corona_instance* instance_p)
{
    if (!instance_p->m_query_depth_handle.is_valid() || !instance_p->m_query_no_depth_handle.is_valid())
    {
        return;
    }
    rl_render_lib* render_lib_p = rl_render_lib::get_instance();
    const int depth_pixels = render_lib_p->occlusion_query_get_pixels_drawn(instance_p->m_query_depth_handle, renderer, false);
    const int no_depth_pixels = render_lib_p->occlusion_query_get_pixels_drawn(instance_p->m_query_no_depth_handle, renderer, false);
    if (depth_pixels == -1 || no_depth_pixels == -1)
    {
        return;
    }
    instance_p->m_depth_pixels_drawn = depth_pixels;
    instance_p->m_no_depth_pixels_drawn = no_depth_pixels;
    render_lib_p->occlusion_query_release_handle(instance_p->m_query_depth_handle);
    render_lib_p->occlusion_query_release_handle(instance_p->m_query_no_depth_handle);
    instance_p->m_query_depth_handle.invalidate();
    instance_p->m_query_no_depth_handle.invalidate();
}

// 0x00513A80 (:527)
void rl_particle_renderer::add_half_res(rl_renderable_particles_instance* instance_p)
{
    m_half_res_entries.add(instance_p);
}

// 0x00513AD0 (:532)
void rl_particle_renderer::add_quarter_res(rl_renderable_particles_instance* instance_p)
{
    m_quarter_res_entries.add(instance_p);
}

// 0x00513B20 (:537)
void rl_particle_renderer::add_distortion(rl_renderable_particles_instance* instance_p)
{
    m_distortion_entries.add(instance_p);
}

// 0x00513B70 (:542)
void rl_particle_renderer::add_full_res(rl_renderable_particles_instance* instance_p)
{
    m_full_res_entries.add(instance_p);
}

// 0x00513BC0 (:547)
void rl_particle_renderer::add_corona(rl_corona_instance* instance_p)
{
    m_corona_entries.add(instance_p);
}

// 0x00513CD0 (:562): by material: distortion, quarter resolution, half resolution, else full resolution.
void rl_particle_renderer::render_for_deferred(rl_renderable_particles_instance* instance_p)
{
    const int flags = instance_p->get_renderable_particles()->get_material()->flags;
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_DISTORTION) != 0)
    {
        m_distortion_entries.add(instance_p);
        return;
    }
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_QUARTER_RES) != 0)
    {
        add_quarter_res(instance_p);
        return;
    }
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_HALF_RES) != 0)
    {
        add_half_res(instance_p);
        return;
    }
    add_full_res(instance_p);
}

// 0x00513D60 (:577)
void rl_particle_renderer::render_for_deferred(rl_corona_instance* instance_p)
{
    m_corona_entries.add(instance_p);
}

// 0x00513DB0 (:1148): the texture, and the uv animation tiling of its peg (1 x 1 unless the peg is an animation
// strip).
void rl_particle_renderer::set_texture(rl_renderer& renderer, int stage, int ref)
{
    renderer.set_texture(stage, get_dev_texture(ref));
    bitmap_entry* entry_p = bm_get_entry(ref, true);
    if (entry_p == nullptr || entry_p->current_peg_entry == nullptr)
    {
        return;
    }
    // Magic number: the peg flag of animation strips.
    if ((entry_p->current_peg_entry->flags & 0x100) != 0)
    {
        m_vertexData.effectUvAnimTiling.x = (float)entry_p->current_peg_entry->anim_tiles_width;
        m_vertexData.effectUvAnimTiling.y = (float)entry_p->current_peg_entry->anim_tiles_height;
        return;
    }
    m_vertexData.effectUvAnimTiling.x = 1.0f;
    m_vertexData.effectUvAnimTiling.y = 1.0f;
}

// 0x00529BE0 (:460): the scene's distortion-only meshes with the distortion material forced (flat fog color, a fog
// height density from the camera height, the user technique 0 for the batch and 3 for the entries).
void rl_particle_renderer::render_mesh_distortion(rl_renderer& renderer, rl_camera& camera, rl_batched_pass& scene_batch, base_array<rl_renderable_instance*>& distortion_only_entries, unsigned int distortion_mat, const unsigned int vis_flags)
{
    if (distortion_mat == 0xFFFFFFFF)
    {
        return;
    }
    rl_material* material_p = (rl_material*)rl_material_library_get_material(distortion_mat);
    rl_material_library_set_override(distortion_mat);
    // Magic numbers: the fog height density falls with the camera height (exp(-0.001 * y) * 0.021640426); the fog
    // color is the "no distortion" offset (0.5, 0.5).
    const float height_density = (float)exp((camera.get_position().y - 0.0) * -0.001) * 0.021640426f;
    renderer.set_fog_color(rl_color_float(0.5019608f, 0.5019608f, 0.0f, 0.0f));
    renderer.set_fog_parameters(height_density, 0.001f, 0.015f, 0.3f);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state batch_state;
    // Magic numbers: the distortion pass state bits (blend, depth read, culling).
    batch_state.set_bits(saved_state.get_bits() & 0xFFBFFF2E | 0x30010E);
    renderer.set_macro_state(batch_state, false);
    material_p->set_override_technique((rl_shader_technique_type)0, -1);
    scene_batch.render(renderer, camera, vis_flags, true);
    rl_material_library_clear_render_material(renderer);
    rl_primitive_state entry_state;
    entry_state.set_bits(batch_state.get_bits() & 0xFFFFFE6F | 0x4F);
    renderer.set_macro_state(entry_state, false);
    material_p->set_override_technique((rl_shader_technique_type)3, 0);
    camera.render_refresh(renderer);
    for (int i = 0; i < distortion_only_entries.size(); i++)
    {
        distortion_only_entries[i]->render(renderer, camera, 0);
    }
    rl_material_library_clear_render_material(renderer);
    renderer.set_macro_state(saved_state, false);
    rl_material_library_set_override(0xFFFFFFFF);
}

// 0x00529E40 (:511): the renderer's states, then the particle vertex and fragment constants (slot 3).
void rl_particle_renderer::flushStates(rl_renderer& renderer)
{
    renderer.flush_states();
    keen::GraphicsCommandWriter* writer_p = renderer.getCommandWriter();
    // Magic number: the particle constant buffers are in slot 3.
    void* vertex_data_p = writer_p->beginDynamicVertexConstants(m_pVertexConstants, 3);
    memcpy(vertex_data_p, &m_vertexData, sizeof(m_vertexData));
    writer_p->endDynamicVertexConstants();
    void* fragment_data_p = writer_p->beginDynamicFragmentConstants(m_pFragmentConstants, 3);
    memcpy(fragment_data_p, &m_fragmentData, sizeof(m_fragmentData));
    writer_p->endDynamicFragmentConstants();
}

// The 3x3 color correction matrix into the fragment constants (columns as rows).
static void particle_set_color_correction(keen::ParticleFragmentShaderConstants& data, const matrix& m)
{
    data.colorCorrectionMatrix.row0.x = m.rvec.x;
    data.colorCorrectionMatrix.row0.y = m.uvec.x;
    data.colorCorrectionMatrix.__dummy0 = 0.0f;
    data.colorCorrectionMatrix.row0.z = m.fvec.x;
    data.colorCorrectionMatrix.row1.x = m.rvec.y;
    data.colorCorrectionMatrix.row1.y = m.uvec.y;
    data.colorCorrectionMatrix.__dummy1 = 0.0f;
    data.colorCorrectionMatrix.row1.z = m.fvec.y;
    data.colorCorrectionMatrix.row2.x = m.rvec.z;
    data.colorCorrectionMatrix.row2.y = m.uvec.z;
    data.colorCorrectionMatrix.row2.z = m.fvec.z;
}

// 0x00529F80 (:582): one instance's particles through the immediate buffer. Parametric instances upload their
// constant block (the shader runs the curves); the material's constants and texture, the blend state, the lighting
// (or none in the distortion pass) and the technique (parametric, soft) are set, the filler of the render type fills
// at most 499 particles, and the instance's particle lists are emptied for the next frame.
void rl_particle_renderer::render(rl_renderer& renderer, const rl_camera& camera, rl_renderable_particles_instance* instance_p, const rl_particle_lighting_state& lighting_state, const bool is_distortion_pass)
{
    rl_renderable_particles* renderable_p = instance_p->get_renderable_particles();
    const bool is_iterative = renderable_p->get_is_iterative();
    int count = is_iterative ? instance_p->get_particle_iterative_list().size() : instance_p->get_particle_parametric_list().size();
    if (count == 0)
    {
        return;
    }
    const bool has_alternate_view = instance_p->get_has_alternate_view();
    if (!is_iterative)
    {
        const unsigned char* constants_p = instance_p->get_parametric_constant_data();
        if (constants_p == nullptr)
        {
            return;
        }
        if (instance_p->get_is_local_space())
        {
            renderer.set_world_transform(matrix43(instance_p->get_local_emission_position(), instance_p->get_local_emission_rotation()));
        }
        else
        {
            renderer.set_world_transform(IDENTITY_MATRIX43);
        }
        // The constant block (0x110 bytes) into the material constants.
        renderer.set_vertex_shader_constant(0, (const float*)constants_p, 0x110 / sizeof(float));
    }
    else
    {
        renderer.set_world_transform(IDENTITY_MATRIX43);
    }
    const float opacity = instance_p->get_opacity();
    const rl_particle_base::render_type render_type = renderable_p->get_particle_type();
    const float fade_min = renderable_p->get_lod_large_particle_fade_min();
    const float fade_max = renderable_p->get_lod_large_particle_fade_max();
    const rl_color_float tint = instance_p->get_effect_material_tint();
    if (render_type != rl_particle_base::RENDER_TYPE_RIBBON && render_type != rl_particle_base::RENDER_TYPE_DROP)
    {
        count += instance_p->get_tail_spawn_count();
    }
    // Magic number: at most 499 particles per instance and frame.
    if (499 < count)
    {
        count = 499;
    }
    const unsigned int stride = is_iterative ? m_iterative_vertex_info[render_type].stride : m_parametric_vertex_info[render_type].stride;
    unsigned int num_vertices;
    if (render_type == rl_particle_base::RENDER_TYPE_DROP)
    {
        num_vertices = count * 0x14;
    }
    else if (render_type == rl_particle_base::RENDER_TYPE_BILLBOARD || render_type == rl_particle_base::RENDER_TYPE_RADIAL || render_type == rl_particle_base::RENDER_TYPE_ORIENTED)
    {
        num_vertices = count * 4;
    }
    else if (render_type == rl_particle_base::RENDER_TYPE_RIBBON)
    {
        num_vertices = count * 2 + 2;
    }
    else
    {
        num_vertices = count;
    }
    renderer.set_vertex_format(is_iterative ? m_iterative_vertex_info[render_type].format : m_parametric_vertex_info[render_type].format);
    rl_particle_material_data* material_p = renderable_p->get_material();
    const matrix43& alternate_view = instance_p->get_alternate_view();
    renderer.set_eye_orientation(has_alternate_view ? alternate_view.m_rotation : camera.get_rotation());
    const vector& eye = has_alternate_view ? alternate_view.m_translation : camera.get_position();
    const vector& render_offset = renderer.get_render_offset();
    renderer.set_eye_position(vector(eye.x - render_offset.x, eye.y - render_offset.y, eye.z - render_offset.z));

    // The material's shader constants (vertex, then pixel; a pixel constant at -1 is skipped).
    const rl_particle_shader_const* consts_p = material_p->shader_consts.to_pointer();
    for (int i = 0; i < (int)material_p->vs_const_info.num_const_refs; i++)
    {
        const rl_particle_shader_const& sc = consts_p[material_p->vs_const_info.const_refs.to_pointer()[i].index];
        float value[4];
        memcpy(value, sc.sc.data, sizeof(value));
        renderer.set_vertex_shader_constant(sc.sc.const_loc, value, 4);
    }
    for (int i = 0; i < (int)material_p->ps_const_info.num_const_refs; i++)
    {
        const rl_particle_shader_const& sc = consts_p[material_p->ps_const_info.const_refs.to_pointer()[i].index];
        if (sc.sc.const_loc != -1)
        {
            float value[4];
            memcpy(value, sc.sc.data, sizeof(value));
            renderer.set_pixel_shader_constant(sc.sc.const_loc, value, 4);
        }
    }
    // The texture: in the distortion pass the first (or, for color pass materials, the second) texture on stage 0;
    // else the first texture on its own stage.
    const gr_shader_texture_ref* textures_p = material_p->textures.to_pointer();
    if (is_distortion_pass)
    {
        const int texture_index = (material_p->flags & rl_particle_material_data::MATERIAL_FLAG_COLOR_PASS) != 0 ? 1 : 0;
        set_texture(renderer, 0, textures_p[texture_index].tex_ref);
    }
    else if (-1 < textures_p[0].tex_stage)
    {
        set_texture(renderer, textures_p[0].tex_stage, textures_p[0].tex_ref);
    }

    // The blend state: additive, subtractive or alpha; always-on-top ignores depth; the distortion pass adds bit 17.
    const int flags = material_p->flags;
    unsigned int bits;
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_ADDITIVE) != 0)
    {
        // Magic numbers: the particle primitive state bits (blend mode in the low bits).
        bits = 0xFF800073;
    }
    else
    {
        bits = ((flags & rl_particle_material_data::MATERIAL_FLAG_SUBTRACTIVE) != 0 ? 3u : 0u) + 0xFF800071;
    }
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_ALWAYS_ON_TOP) != 0)
    {
        bits &= 0xFFFFFE3F;
    }
    if (is_distortion_pass)
    {
        bits |= 0x20000;
    }
    rl_primitive_state state;
    state.set_bits(bits);
    renderer.set_macro_state(state, false);

    m_vertexData.effectOpacity.x = opacity;
    m_vertexData.effectOpacity.y = fade_min;
    m_vertexData.effectOpacity.z = fade_max;
    if (is_distortion_pass)
    {
        m_vertexData.effectAmbient = keen::float4{0.0f, 0.0f, 0.0f, 1.0f};
        m_vertexData.effectMaterialTint = keen::float4{0.0f, 0.0f, 0.0f, 0.0f};
        m_fragmentData.effectDiffuse = keen::float4{0.0f, 0.0f, 0.0f, renderable_p->get_soft_thickness()};
    }
    else
    {
        const rl_color_float& ambient = renderable_p->get_ambient_color();
        const float material_percent = renderable_p->get_effect_material_percent();
        m_vertexData.effectAmbient.w = ambient.alpha * lighting_state.m_ambient_color.alpha;
        m_vertexData.effectAmbient.y = ambient.green * lighting_state.m_ambient_color.green;
        m_vertexData.effectAmbient.z = ambient.blue * lighting_state.m_ambient_color.blue;
        m_vertexData.effectAmbient.x = lighting_state.m_ambient_color.red * ambient.red;
        m_vertexData.effectMaterialTint = keen::float4{tint.red, tint.green, tint.blue, material_percent};
        const rl_color_float& diffuse = renderable_p->get_diffuse_color();
        const float red = lighting_state.m_tod_color.red * diffuse.red;
        const float green = lighting_state.m_tod_color.green * diffuse.green;
        const float blue = lighting_state.m_tod_color.blue * diffuse.blue;
        const float inverse = 1.0f - material_percent;
        // The time of day light blended toward the effect material tint.
        m_fragmentData.effectDiffuse.z = blue * inverse + blue * tint.blue * material_percent;
        m_fragmentData.effectDiffuse.w = renderable_p->get_soft_thickness();
        m_fragmentData.effectDiffuse.x = inverse * red + red * tint.red * material_percent;
        m_fragmentData.effectDiffuse.y = green * inverse + green * tint.green * material_percent;
        m_fragmentData.effectLightDir.x = lighting_state.m_tod_light_dir.x;
        m_fragmentData.effectLightDir.y = lighting_state.m_tod_light_dir.y;
        m_fragmentData.effectLightDir.z = lighting_state.m_tod_light_dir.z;
    }
    particle_set_color_correction(m_fragmentData, *renderable_p->get_color_correction_matrix());
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    // The technique: 0 iterative, 1 parametric; +2 for soft particles (not ribbons, radials or distortion).
    int technique = is_iterative ? 0 : 1;
    if ((flags & rl_particle_material_data::MATERIAL_FLAG_SOFT_RENDER) != 0 && !is_distortion_pass && render_type != rl_particle_base::RENDER_TYPE_RIBBON && render_type != rl_particle_base::RENDER_TYPE_RADIAL)
    {
        technique += 2;
    }
    if (!renderer.set_active_shader(material_p->effect_ref, rl_shader_technique_user, m_particle_shader_techniques[render_type].m_technique_refs[technique].get_ref()))
    {
        return;
    }
    unsigned int num_verts = 0;
    unsigned int num_prims = 0;
    flushStates(renderer);
    keen::GraphicsCommandBuffer* buffer_p = renderer.getCommandWriter()->getBuffer();
    unsigned char* vertex_p = (unsigned char*)keen::graphics::beginImmediateGeometryCommand(buffer_p, keen::PrimitiveType_TriangleStrip, num_vertices, stride);
    if (vertex_p != nullptr)
    {
        switch (render_type)
        {
        case rl_particle_base::RENDER_TYPE_BILLBOARD:
            filler_billboard(instance_p, is_iterative, vertex_p, num_verts, num_prims, stride);
            break;
        case rl_particle_base::RENDER_TYPE_RIBBON:
            filler_ribbon(instance_p, is_iterative, vertex_p, num_verts, num_prims, stride);
            break;
        case rl_particle_base::RENDER_TYPE_RADIAL:
            filler_radial(instance_p, is_iterative, vertex_p, num_verts, num_prims, stride);
            break;
        case rl_particle_base::RENDER_TYPE_DROP:
            filler_drop(instance_p, vertex_p, num_verts, num_prims, stride);
            break;
        case rl_particle_base::RENDER_TYPE_ORIENTED:
            filler_oriented(instance_p, is_iterative, vertex_p, num_verts, num_prims, stride);
            break;
        default:
            keen::debug::nativePrintArgs("should never get here");
            break;
        }
        draw_particles(renderer, is_iterative, render_type, num_verts, num_prims);
    }
    instance_p->get_particle_parametric_list().clear();
    instance_p->get_particle_iterative_list().clear();
}

// 0x0052A9F0 (:856): one corona in three phases: 0 draws it faded by its visibility (the depth / no-depth pixel
// ratio of the last queries, adapted toward it when RL_corona_adaption is on); 1 and 2 draw it untextured inside an
// occlusion query, with and without depth testing, unless both queries of this frame stamp are still pending.
void rl_particle_renderer::render_corona_internal(rl_renderer& renderer, const rl_camera& camera, rl_corona_instance* instance_p, int visibility_phase)
{
    // The primitive states of the three phases (function static in rfg.exe).
    static rl_primitive_state mode[3];
    static bool mode_initialized = false;

    renderer.set_world_transform(IDENTITY_MATRIX43);
    const float intensity_scale = instance_p->m_intensity_scale;
    rl_stamped_handle query_handle;
    if (visibility_phase < 1)
    {
        const int no_depth_pixels = instance_p->m_no_depth_pixels_drawn;
        float visibility = 0.0f;
        if (-1 < instance_p->m_depth_pixels_drawn && -1 < no_depth_pixels && no_depth_pixels != 0)
        {
            visibility = (float)instance_p->m_depth_pixels_drawn / (float)no_depth_pixels;
        }
        // Magic numbers: adapt only between 5% and 99% visible.
        if (RL_corona_adaption && visibility <= 0.99f && 0.05f <= visibility)
        {
            const float previous = instance_p->m_visibility;
            visibility = fabsf(previous - visibility) * RL_corona_adaption_rate * (visibility - previous) + previous;
            if (visibility - 0.0f < 0.0f)
            {
                visibility = 0.0f;
            }
            if (0.0f <= visibility - 1.0f)
            {
                visibility = 1.0f;
            }
        }
        instance_p->m_visibility = visibility;
    }
    else
    {
        if (instance_p->m_query_depth_handle.is_valid() && instance_p->m_query_no_depth_handle.is_valid())
        {
            return;
        }
        query_handle = rl_render_lib::get_instance()->occlusion_query_issue_begin(renderer);
        if (visibility_phase == 1)
        {
            instance_p->m_query_depth_handle = query_handle;
        }
        else if (visibility_phase == 2)
        {
            instance_p->m_query_no_depth_handle = query_handle;
        }
        if (!query_handle.is_valid())
        {
            return;
        }
    }
    renderer.set_vertex_format(m_iterative_vertex_info[rl_particle_base::RENDER_TYPE_CORONA].format);
    const unsigned int stride = m_iterative_vertex_info[rl_particle_base::RENDER_TYPE_CORONA].stride;
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    if (!renderer.set_active_shader(m_particle_type_shaders[rl_particle_base::RENDER_TYPE_CORONA], rl_shader_technique_user, m_particle_shader_techniques[rl_particle_base::RENDER_TYPE_CORONA].m_technique_refs[0 < visibility_phase ? 1 : 0].get_ref()))
    {
        return;
    }
    set_texture(renderer, 0, visibility_phase < 1 ? instance_p->m_texture_handle : -1);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    if (!mode_initialized)
    {
        mode_initialized = true;
        // Magic numbers: the corona primitive states (draw, occlusion with depth, occlusion without depth).
        mode[0].set_bits(0x80800031);
        mode[1].set_bits(0x80800061);
        mode[2].set_bits(0x80800021);
    }
    renderer.set_macro_state(mode[visibility_phase], false);
    renderer.set_eye_orientation(camera.get_rotation());
    const vector& render_offset = renderer.get_render_offset();
    const vector& camera_position = camera.get_position();
    renderer.set_eye_position(vector(camera_position.x - render_offset.x, camera_position.y - render_offset.y, camera_position.z - render_offset.z));
    particle_set_color_correction(m_fragmentData, instance_p->m_color_correction_matrix);
    m_vertexData.effectOpacity.x = visibility_phase == 0 ? instance_p->m_opacity : 1.0f;
    // Magic numbers: no large particle fade for coronas.
    m_vertexData.effectOpacity.y = 1e+06f;
    m_vertexData.effectOpacity.z = 1e+06f;

    corona_vertex vertex;
    vertex.pos = instance_p->m_position;
    vertex.type = instance_p->m_infinite_depth ? -1.0f : 1.0f;
    if (visibility_phase == 0)
    {
        vertex.type = (float)(instance_p->m_type + 1) * vertex.type;
    }
    vertex.saturation = instance_p->m_saturation;
    vertex.intensity = intensity_scale;
    vertex.intensity_exp_decay = instance_p->m_intensity_exponential_decay;
    vertex.opacity_exp_decay = instance_p->m_opacity_exponential_decay;
    vertex.view_fade_decay = instance_p->m_view_fade_decay;
    vertex.additive_key = instance_p->m_additive_key;
    vertex.visibility = instance_p->m_visibility * instance_p->m_visibility_fade;
    vertex.opacity_visibility_decay = instance_p->m_opacity_visibility_decay;
    vertex.radius_visibility_decay = instance_p->m_radius_visibility_decay;
    vertex.intensity_view_fade_scale = instance_p->m_view_fade_intensity_enabled ? instance_p->m_view_fade_scale : 0.0f;
    vertex.radius_view_fade_scale = instance_p->m_view_fade_radius_enabled ? instance_p->m_view_fade_scale : 0.0f;
    vertex.opacity_view_fade_scale = instance_p->m_view_fade_opacity_enabled ? instance_p->m_view_fade_scale : 0.0f;
    vertex.color_distance_start = instance_p->m_color_distance_start;
    vertex.color_distance_end = instance_p->m_color_distance_end;
    vertex.radius_distance_start = instance_p->m_radius_distance_start;
    vertex.radius_distance_end = instance_p->m_radius_distance_end;
    vertex.color_start = visibility_phase != 0 ? rl_color_float(0.0f, 0.0f, 0.0f, 1.0f) : instance_p->m_color_start;
    vertex.color_end = instance_p->m_color_end;
    vertex.rotation = visibility_phase != 0 ? 0.0f : instance_p->m_rotation_scale;
    vertex.rotation_offset = instance_p->m_rotation_offset;
    vertex.position_offset = instance_p->m_position_offset;
    vertex.radius_start = instance_p->m_radius_start;
    vertex.radius_end = instance_p->m_radius_end;
    vertex.width_scale = instance_p->m_width_scale;
    vertex.visibility_radius = instance_p->m_visibility_radius;
    vertex.fvec = instance_p->m_rotation.fvec;
    vertex.anim_time = instance_p->m_anim_rate * instance_p->m_current_elapsed_time + (float)instance_p->m_random_anim_frames;
    flushStates(renderer);
    keen::GraphicsCommandBuffer* buffer_p = renderer.getCommandWriter()->getBuffer();
    corona_vertex* vertices_p = (corona_vertex*)keen::graphics::beginImmediateGeometryCommand(buffer_p, keen::PrimitiveType_TriangleStrip, 4, stride);
    if (vertices_p != nullptr)
    {
        for (int i = 0; i < 4; i++)
        {
            vertex.uvs.x = (i & 2) != 0 ? 1.0f : 0.0f;
            vertex.uvs.y = (i & 1) != 0 ? 1.0f : 0.0f;
            vertices_p[i] = vertex;
        }
        // Magic numbers: one quad (4 vertices, 6 indices); drawn as not iterative (the corner uv stream is bound).
        draw_particles(renderer, false, rl_particle_base::RENDER_TYPE_CORONA, 4, 6);
    }
    renderer.set_macro_state(saved_state, false);
    if (0 < visibility_phase)
    {
        rl_render_lib::get_instance()->occlusion_query_issue_end(query_handle, renderer);
    }
}

// 0x0052B280 (:1140): the three phases of a corona.
void rl_particle_renderer::render(rl_renderer& renderer, const rl_camera& camera, rl_corona_instance* instance_p)
{
    for (int visibility_phase = 0; visibility_phase < 3; visibility_phase++)
    {
        render_corona_internal(renderer, camera, instance_p, visibility_phase);
    }
}
