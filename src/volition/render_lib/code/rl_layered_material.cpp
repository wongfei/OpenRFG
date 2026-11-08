// Original: volition/render_lib/code/rl_layered_material.cpp
//
// rl_layered_material (VFX.md FX4 item 15): a material drawn as a stack of library materials (layers), each with its
// own passes; the material effects (material_effects.cpp) build one per material of an effect's map. Pass n of the
// layered material is pass n - k of the layer whose passes start at k. Layer flags: 1 = drawn in the depth pass, 8 =
// additive blending, 0x10 = the base surface (its index is kept as m_base_surface_index). Material flags (m_flags,
// names inferred): 1 = transparent, 2 = the base surface has a pass with flag 2 (a stencil pass), 4 = a layer is drawn
// in the depth pass.

#include "volition/render_lib/code/rl_layered_material.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include <malloc.h>
#include <string.h>

// Layer flags (rl_material_layer::m_flags; names inferred).
static const unsigned int RLML_DEPTH_PASS = 0x1;
static const unsigned int RLML_ADDITIVE = 0x8;
static const unsigned int RLML_BASE_SURFACE = 0x10;
// Material flags (m_flags; names inferred).
static const unsigned int RLLM_TRANSPARENT = 0x1;
static const unsigned int RLLM_BASE_STENCIL = 0x2;
static const unsigned int RLLM_DEPTH_LAYERS = 0x4;
// The material flag a pass of the base surface is asked for (get_has_flags) to set RLLM_BASE_STENCIL.
static const unsigned int Base_stencil_pass_flag = 0x2;

const unsigned int rl_layered_material::MAX_LAYERS = 4; // 0x012BEE24

// :51 (class id 0x5CA1AB1E, 256 objects: rfg.exe .data 0x01514C7C)
// <rl_layered_material::factory_init> 0x00510670 (:51)
// <rl_layered_material::release> 0x005107A0 (:51)
// <rl_layered_material::create> 0x00526BA0 (:51)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_layered_material, 0x5CA1AB1E, 256)

// The layer's material for its pass count: the thread's override handle if set, the override material before the
// current one (inlined everywhere).
static rl_material_base* layer_material_for_passes(unsigned int mat_h)
{
    const unsigned int override_h = rl_material_library_get_override();
    return resolve_material_handle(override_h != 0xFFFFFFFF ? override_h : mat_h, true);
}

// 0x004E8AE0 (:54)
rl_layered_material::rl_layered_material()
{
}

// 0x0054BB90 (:59)
rl_layered_material::~rl_layered_material()
{
    implicit_unregister();
}

// 0x005105A0 (:28)
// The CRC of the layers' material hashes; -1 when a layer's material is not in the library.
unsigned int rl_layered_material::hash_internal() const
{
    unsigned int* hashes = (unsigned int*)_alloca(m_num_layers * sizeof(unsigned int));
    for (int i = 0; i < m_num_layers; i++)
    {
        const rl_material_base* mat_p = resolve_material_handle(m_layers[i].m_mat_handle, false);
        if (mat_p == nullptr)
        {
            return 0xFFFFFFFF;
        }
        hashes[i] = mat_p->hash();
    }
    return crc_mem((const unsigned char*)hashes, m_num_layers * (int)sizeof(unsigned int), 0);
}

// 0x00526C10 (:74)
void rl_layered_material::init(const char* name, rl_material_layer* layers, int num_layers)
{
    init(crc_stri(name, 0), layers, num_layers);
}

// 0x00510820 (:88)
// Counts the base passes (every layer's) and the depth passes: the depth layers', or with none the base surface's,
// plus one (the depth-only base surface draw of set_as_render_material) when both a depth layer and a stencil pass
// on the base surface exist.
void rl_layered_material::init(unsigned int name_checksum, rl_material_layer* layers, int num_layers)
{
    m_num_layers = (short)num_layers;
    m_layers = layers;
    m_flags = 0;
    m_total_base_passes = 0;
    m_total_depth_passes = 0;
    m_base_surface_index = 0;
    short base_surface_passes = 0;
    for (int i = 0; i < m_num_layers; i++)
    {
        rl_material_base* mat_p = layer_material_for_passes(layers[i].m_mat_handle);
        const int num_passes = mat_p != nullptr ? mat_p->get_num_passes(false) : 0;
        m_total_base_passes += (short)num_passes;
        if ((layers[i].m_flags & RLML_DEPTH_PASS) != 0)
        {
            m_total_depth_passes += (short)num_passes;
            m_flags |= RLLM_DEPTH_LAYERS;
        }
        if ((layers[i].m_flags & RLML_BASE_SURFACE) != 0)
        {
            rl_material_base* base_p = resolve_material_handle(layers[i].m_mat_handle, true);
            if (base_p != nullptr && base_p->get_is_base_transparent())
            {
                m_flags |= RLLM_TRANSPARENT;
            }
            base_surface_passes += (short)num_passes;
            m_base_surface_index = (short)i;
            for (int pass = 0; pass < num_passes; pass++)
            {
                base_p = resolve_material_handle(layers[i].m_mat_handle, true);
                if (base_p != nullptr && base_p->get_has_flags(pass, Base_stencil_pass_flag, false))
                {
                    m_flags |= RLLM_BASE_STENCIL;
                    m_total_depth_passes += (short)num_passes;
                    break;
                }
            }
        }
    }
    if ((m_flags & RLLM_BASE_STENCIL) != 0 && (m_flags & RLLM_DEPTH_LAYERS) != 0)
    {
        m_total_depth_passes += 1;
        m_total_base_passes += 1;
    }
    if ((m_flags & RLLM_BASE_STENCIL) == 0 && (m_flags & RLLM_DEPTH_LAYERS) == 0)
    {
        m_total_depth_passes += base_surface_passes;
    }
    // Original bug kept: name_checksum is never stored, so m_name_checksum keeps whatever the pool slot held (the
    // factory objects are not cleared by create()); compare(exhaustive) and get_name_checksum read that.
    (void)name_checksum;
}

// 0x004C2310 (:145)
bool rl_layered_material::layer_needed_in_depth_only(int layer_index) const
{
    if ((m_flags & RLLM_DEPTH_LAYERS) == 0)
    {
        return (m_layers[layer_index].m_flags & RLML_BASE_SURFACE) != 0;
    }
    const unsigned int layer_flags = m_layers[layer_index].m_flags;
    return (layer_flags & RLML_DEPTH_PASS) != 0 || ((m_flags & RLLM_BASE_STENCIL) != 0 && (layer_flags & RLML_BASE_SURFACE) != 0);
}

// 0x004C2360 (:158)
bool rl_layered_material::compare(const rl_material_base& other_mat, bool exhaustive) const
{
    if (other_mat.get_type() != RLMT_LAYERED_MATERIAL)
    {
        return false;
    }
    const rl_layered_material& other = (const rl_layered_material&)other_mat;
    if (exhaustive && other.m_name_checksum != m_name_checksum)
    {
        return false;
    }
    if (other.m_num_layers != m_num_layers || other.m_total_base_passes != m_total_base_passes)
    {
        return false;
    }
    for (int i = 0; i < m_num_layers; i++)
    {
        if (other.m_layers[i].m_mat_handle != m_layers[i].m_mat_handle)
        {
            return false;
        }
    }
    return true;
}

// 0x00526C40 (:195)
// Finds the layer and local pass of the requested pass (in the depth pass only the layers needed there count), sets
// the render state for it (saving the old one in pStateStore) and lets the layer's material set itself up. Past the
// last pass (the extra depth pass of init) the base surface is drawn depth-only with the depth-only shader.
void rl_layered_material::set_as_render_material(rl_renderer& renderer, rl_material_state_store* pStateStore, int requested_pass_index) const
{
    const bool depth_only = renderer.get_depth_only();
    const bool base_stencil = (m_flags & RLLM_BASE_STENCIL) != 0;
    const bool depth_layers = (m_flags & RLLM_DEPTH_LAYERS) != 0;
    int pass_index = 0;
    for (int i = 0; i < m_num_layers; i++)
    {
        rl_material_base* mat_p = layer_material_for_passes(m_layers[i].m_mat_handle);
        const int num_passes = mat_p != nullptr ? mat_p->get_num_passes(renderer.get_depth_only()) : 0;
        if (depth_only && !layer_needed_in_depth_only(i))
        {
            continue;
        }
        for (int layer_pass = 0; layer_pass < num_passes; layer_pass++, pass_index++)
        {
            if (pass_index != requested_pass_index)
            {
                continue;
            }
            rl_material_base* layer_mat_p = resolve_material_handle(m_layers[i].m_mat_handle, false);
            rl_primitive_state state = renderer.get_macro_state();
            pStateStore->old_state[i] = state;
            unsigned int bits = state.get_bits();
            const unsigned int layer_flags = m_layers[i].m_flags;
            // Magic numbers: the alpha mode field (bits 0-4): 2 additive, 5 the transparent blend.
            if (layer_mat_p->get_is_base_transparent() || (layer_flags & RLML_ADDITIVE) != 0)
            {
                if ((layer_flags & RLML_ADDITIVE) != 0)
                {
                    bits = bits & 0xFFFFFFE2 | 0x2;
                }
                else
                {
                    bits = bits & 0xFFFFFFE5 | 0x5;
                }
            }
            // Magic numbers: the stencil mode field (bits 9-12, with bit 15/16 of the color write mode): the depth
            // layers write the stencil (8c00 with color writes off), the base surface's stencil pass tests it (e00),
            // the other layers test it and clear it on their last pass (1000/1200, 1400/1600 without the stencil pass).
            if ((base_stencil || !depth_only) && depth_layers)
            {
                if ((layer_flags & RLML_DEPTH_PASS) != 0)
                {
                    bits = bits & 0xFFFCEDFF | 0x8C00;
                }
                else if (base_stencil && (layer_flags & RLML_BASE_SURFACE) != 0)
                {
                    bits = bits & 0xFFFFEFFF | 0xE00;
                }
                else
                {
                    const bool last_pass = !depth_only && requested_pass_index == m_total_base_passes - 1;
                    if (base_stencil)
                    {
                        bits = last_pass ? (bits & 0xFFFFF1FF | 0x1000) : (bits & 0xFFFFF3FF | 0x1200);
                    }
                    else
                    {
                        bits = last_pass ? (bits & 0xFFFFF5FF | 0x1400) : (bits & 0xFFFFF7FF | 0x1600);
                    }
                }
            }
            // Magic numbers: the z-buffer mode field (bits 6-8): 2 test only for the overlays, 1 test and write for
            // the depth layers.
            if ((layer_flags & RLML_BASE_SURFACE) == 0 && !(depth_only && !base_stencil && (layer_flags & RLML_DEPTH_PASS) != 0))
            {
                if ((layer_flags & RLML_DEPTH_PASS) == 0)
                {
                    bits = bits & 0xFFFFFEBF | 0x80;
                }
                else
                {
                    bits = bits & 0xFFFFFE7F | 0x40;
                }
            }
            state.set_bits(bits);
            renderer.set_macro_state(state, false);
            layer_mat_p->set_as_render_material(renderer, &renderer.m_state_store, layer_pass);
            return;
        }
    }

    const rl_primitive_state implicit_state = renderer.get_macro_state();
    pStateStore->implicit_state = implicit_state;
    const unsigned int old_bits = implicit_state.get_bits();
    // Magic numbers: stencil test (0x1800) and, by the z-buffer mode, test-and-write (4 → 1) or no write (3 → 0);
    // color writes off (0x8000).
    unsigned int bits = old_bits & 0xFFFFF9FF | 0x1800;
    const unsigned int zbuf_mode = old_bits >> 6 & 7;
    if (zbuf_mode == 4)
    {
        bits = bits & 0xFFFFFE7F | 0x40;
    }
    else if (zbuf_mode == 3)
    {
        bits &= 0xFFFFFE3F;
    }
    rl_primitive_state state;
    state.set_bits(bits & 0xFFFCFFFF | 0x8000);
    renderer.set_macro_state(state, false);
    renderer.m_override_shader = renderer.get_depth_only_shader();
    // Original: no null check (a base surface missing from the library crashes).
    rl_material_base* base_p = resolve_material_handle(m_layers[m_base_surface_index].m_mat_handle, false);
    base_p->set_as_render_material(renderer, nullptr, 0);
}

// 0x00526FE0 (:306)
void rl_layered_material::cleanup_render_material(rl_renderer& renderer, rl_material_state_store* pStateStore, int requested_pass_index) const
{
    const bool depth_only = renderer.get_depth_only();
    int pass_index = 0;
    for (int i = 0; i < m_num_layers; i++)
    {
        rl_material_base* mat_p = layer_material_for_passes(m_layers[i].m_mat_handle);
        const int num_passes = mat_p != nullptr ? mat_p->get_num_passes(renderer.get_depth_only()) : 0;
        if (depth_only && !layer_needed_in_depth_only(i))
        {
            continue;
        }
        for (int layer_pass = 0; layer_pass < num_passes; layer_pass++, pass_index++)
        {
            if (pass_index == requested_pass_index)
            {
                rl_material_base* layer_mat_p = resolve_material_handle(m_layers[i].m_mat_handle, false);
                renderer.set_macro_state(pStateStore->old_state[i], false);
                layer_mat_p->cleanup_render_material(renderer, &renderer.m_state_store, layer_pass);
                return;
            }
        }
    }
    // Original: no null check (a base surface missing from the library crashes).
    rl_material_base* base_p = resolve_material_handle(m_layers[m_base_surface_index].m_mat_handle, false);
    base_p->cleanup_render_material(renderer, nullptr, 0);
    renderer.set_macro_state(pStateStore->implicit_state, false);
    renderer.m_override_shader = 0xFFFFFFFF;
}

// 0x00510A00 (:353)
bool rl_layered_material::get_has_flags(int pass_index, unsigned int flags, bool depthPass)
{
    int first_pass = 0;
    for (int i = 0; i < m_num_layers; i++)
    {
        rl_material_base* mat_p = layer_material_for_passes(m_layers[i].m_mat_handle);
        const int num_passes = mat_p != nullptr ? mat_p->get_num_passes(depthPass) : 0;
        if (pass_index >= first_pass && pass_index < num_passes + first_pass)
        {
            rl_material_base* layer_mat_p = resolve_material_handle(m_layers[i].m_mat_handle, true);
            if (layer_mat_p == nullptr)
            {
                return false;
            }
            return layer_mat_p->get_has_flags(pass_index - first_pass, flags, depthPass);
        }
        first_pass += num_passes;
    }
    return false;
}

// 0x005271D0 (:375)
// A copy with its own layer array from pool.
rl_material_base* rl_layered_material::clone(mempool_base* pool) const
{
    rl_layered_material* clone_p = create();
    if (clone_p != nullptr)
    {
        rl_material_layer* layers = (rl_material_layer*)pool->alloc(m_num_layers * sizeof(rl_material_layer), 0);
        memcpy(layers, m_layers, m_num_layers * sizeof(rl_material_layer));
        clone_p->init(m_name_checksum, layers, m_num_layers);
    }
    return clone_p;
}

// 0x004C23D0 (:388)
int rl_layered_material::get_num_passes(bool depthPass)
{
    return depthPass ? m_total_depth_passes : m_total_base_passes;
}

// 0x004E8B00 (.h:45)
// Not with a depth layer and a stencil pass (the extra depth-only pass).
bool rl_layered_material::can_combine_drawcalls()
{
    return (m_flags & RLLM_BASE_STENCIL) == 0 || (m_flags & RLLM_DEPTH_LAYERS) == 0;
}

// 0x004E8B20 (.h:52)
unsigned int rl_layered_material::get_name_checksum() const
{
    return m_name_checksum;
}

// 0x004E8B30 (.h:58)
bool rl_layered_material::get_is_transparent()
{
    return (m_flags & RLLM_TRANSPARENT) != 0;
}

// 0x004E8B40 (.h:59)
void rl_layered_material::set_is_transparent(bool is_transparent)
{
    if (is_transparent)
    {
        m_flags |= RLLM_TRANSPARENT;
    }
    else
    {
        m_flags &= ~RLLM_TRANSPARENT;
    }
    set_is_base_transparent(is_transparent);
}

// 0x004E8B60 (.h:68)
rl_material_type rl_layered_material::get_type() const
{
    return RLMT_LAYERED_MATERIAL;
}
