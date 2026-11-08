// Original: volition/render_lib/code/rl_skin_instance.cpp
//
// rl_skin_instance: a mesh instance of a character mesh whose vertices are skinned in the vertex shader. It owns a block
// of bone matrices (matrix43, three float4 rows each) from the skin bone manager, which the game fills every render
// update (set_bones_matricies) and which every draw uploads as the skinning vertex constants (rl_renderer::
// set_bone_transforms). An optional dynamic morph instance adds the head's morph targets as a second vertex stream.
//
// The skin bone manager: two page mempools of bone matrix slots, large slots of 64 bones (more than 40 bones) and small
// slots of 2 bones (a small allocation takes consecutive slots).

#include "volition/render_lib/code/rl_skin_instance.h"

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_morph_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/page_mempool.h"

#include "keen/base/trace.hpp"

#include <windows.h>

// The factory: class id 0x57A6199A, 300 objects by default (game_render_system_init sets 500).
// <rl_skin_instance::factory_init> 0x00518CD0 (:42)
// <rl_skin_instance::create> 0x00550D30 (:42)
RL_FACTORY_CLASS_CORE(rl_skin_instance, 0x57A6199A, 300)

// Magic number: the most bones a skin instance can have (a large slot).
const int rl_skin_instance::num_bones_matrices_max = 64; // 0x012BEE4C

// The slot counts (game_render_system_init sets 0x80 and 0x4A0).
static int Rl_skin_bone_manager_num_large_slots = 128; // 0x014FA5F0
static int Rl_skin_bone_manager_num_small_slots = 1480; // 0x014FA5F4
static unsigned char* Rl_skin_bone_manager_pool = nullptr; // 0x01B7BD38
static page_mempool Rl_skin_bone_manager_pool_large; // 0x01C23080
static page_mempool Rl_skin_bone_manager_pool_small; // 0x01C20BD8

// Magic number: an instance of more than 40 bones takes a large slot.
static const int SKIN_SMALL_BONES_MAX = 0x28;

// 0x00530A40 (:42)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool (as rl_mesh_instance::release).
void rl_skin_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_skin_instance();
}

// 0x007C1F10 (.h:27)
void rl_skin_instance::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x006E87F0 (.h:27)
int rl_skin_instance::get_objects_free()
{
    return ms_object_pool.size();
}

// 0x0054E070 (:45)
// The mesh instance part (inlined) without the batchable flag: skinned instances are drawn one by one.
rl_skin_instance::rl_skin_instance()
{
    m_flags &= ~(FLAGS_IS_BATCHABLE | FLAGS_ALPHA_CACHE_VALID);
    m_morph_instance_p = nullptr;
    m_num_bones_matrices = 0;
    m_bone_matrices_p = nullptr;
    m_num_allocated_bones = 0;
}

// 0x0054E0C0 (:61)
rl_skin_instance::~rl_skin_instance()
{
    release_bones();
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->release();
        m_morph_instance_p = nullptr;
    }
}

// 0x004FCCF0 (:67)
void rl_skin_instance::prerender(rl_renderer& renderer)
{
    renderer.set_bone_transforms(m_bone_matrices_p, m_num_bones_matrices);
}

// 0x00530AC0 (:73)
// Only with bones: the bone constants and the morph stream, the mesh instance's draw, then the morph stream is unbound.
void rl_skin_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    if (m_num_allocated_bones <= 0)
    {
        return;
    }
    renderer.set_bone_transforms(m_bone_matrices_p, m_num_bones_matrices);
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->render_setup(renderer);
    }
    rl_mesh_instance::render(renderer, camera, flags);
    if (m_morph_instance_p != nullptr && m_morph_instance_p->m_dynamicVertexDataValid && m_morph_instance_p->m_morph_data_size != 0 && m_morph_instance_p->m_num_morph_verts != 0)
    {
        renderer.m_vertexBufferData[1].pBuffer = nullptr;
        renderer.m_vertexBufferData[1].vertexCount = 0;
        renderer.m_vertexBufferData[1].offset = 0;
    }
}

// 0x00530B40 (:94)
// As render, for the deferred (sorted) list.
void rl_skin_instance::render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags)
{
    if (m_num_allocated_bones <= 0)
    {
        return;
    }
    renderer.set_bone_transforms(m_bone_matrices_p, m_num_bones_matrices);
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->render_setup(renderer);
    }
    rl_mesh_instance::render_for_deferred(renderer, camera, def_id, flags);
    if (m_morph_instance_p != nullptr && m_morph_instance_p->m_dynamicVertexDataValid && m_morph_instance_p->m_morph_data_size != 0 && m_morph_instance_p->m_num_morph_verts != 0)
    {
        renderer.m_vertexBufferData[1].pBuffer = nullptr;
        renderer.m_vertexBufferData[1].vertexCount = 0;
        renderer.m_vertexBufferData[1].offset = 0;
    }
}

// 0x004FCD10 (:127)
// The character shaders; configuration 2 with a valid morph stream.
void rl_skin_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_character);
    if (m_morph_instance_p != nullptr && m_morph_instance_p->m_dynamicVertexDataValid && m_morph_instance_p->m_morph_data_size != 0 && m_morph_instance_p->m_num_morph_verts != 0)
    {
        renderer.set_shader_configuration(2);
        return;
    }
    renderer.set_shader_configuration(0);
}

// 0x004D1380 (:144)
// Copies the matrices; more than were allocated is ignored.
void rl_skin_instance::set_bones_matricies(const matrix43* bone_matrices, int num_bone_matrices)
{
    if (m_num_allocated_bones < num_bone_matrices)
    {
        return;
    }
    m_num_bones_matrices = num_bone_matrices;
    for (int i = 0; i < m_num_bones_matrices; i++)
    {
        m_bone_matrices_p[i] = bone_matrices[i];
    }
}

// 0x004D1400 (:169)
void rl_skin_instance::release_morph_data()
{
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->release();
        m_morph_instance_p = nullptr;
    }
}

// 0x00550DE0 (:177)
// The first morph creates the dynamic morph instance with the vertex count of the mesh's sub mesh (12 bytes of morph
// data per vertex).
bool rl_skin_instance::activate_morph(rl_morph_container* container_p, int target, float weight)
{
    if (m_morph_instance_p == nullptr)
    {
        m_morph_instance_p = rl_dynamic_morph_instance::create();
        if (m_morph_instance_p == nullptr)
        {
            return false;
        }
        // Notes (decompiled behaviour, kept as is): the mesh data (rl_mesh::get_mesh_data, inlined) is not checked.
        const rl_mesh* mesh_p = (const rl_mesh*)get_renderable();
        const unsigned int num_verts = (unsigned int)mesh_p->get_mesh_data()->vertex_buffer.num_verts;
        m_morph_instance_p->m_num_morph_verts = num_verts;
        // Magic number: 12 bytes of morph data per vertex.
        m_morph_instance_p->m_morph_data_size = num_verts * 0xC;
    }
    return m_morph_instance_p->add_active_morph(container_p, target, weight);
}

// 0x00530BC0 (:244)
// The skinned decal pass (VFX.md FX7): the bone constants (and the morph stream), the decal pass state (alpha mode 2,
// 5 in the base pass; depth read; state bit 20, meaning inferred), then per decal the whole instance again
// (render_internal) with the decal material as the material library override; no scissor, no double sided state. The
// decal constants are set when the handle changes. The override, the state and the morph stream are restored after.
void rl_skin_instance::render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p)
{
    renderer.set_bone_transforms(m_bone_matrices_p, m_num_bones_matrices);
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->render_setup(renderer);
    }
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state state;
    // Magic numbers: the bits of the mesh decal state without bit 18 (rl_decal_internal.h).
    if (!renderer.get_is_base_pass())
    {
        state.set_bits(saved_state.get_bits() & 0xFF9FFE62 | 0x100042);
    }
    else
    {
        state.set_bits(saved_state.get_bits() & 0xFF9FFE65 | 0x100045);
    }
    renderer.set_macro_state(state, false);
    unsigned int last_material = 0xFFFFFFFF;
    unsigned int last_handle = 0xFFFFFFFF;
    for (int i = 0; i < num_decals; i++)
    {
        rl_decal* decal_p = decal_list_p[i];
        const unsigned int material = decal_p->get_material();
        const unsigned int handle = decal_p->get_decal_handle();
        if (material != last_material)
        {
            rl_material_library_set_override(material);
            last_material = material;
        }
        if (handle != last_handle)
        {
            last_handle = handle;
            rl_decal_setup_decal_for_render(renderer, decal_p);
        }
        rl_decal_setup_decal_instance_for_render(renderer, decal_p);
        render_internal(renderer, 0xFFFFFFFF);
    }
    rl_material_library_set_override(0xFFFFFFFF);
    renderer.set_macro_state(saved_state, false);
    if (m_morph_instance_p != nullptr && m_morph_instance_p->m_dynamicVertexDataValid && m_morph_instance_p->m_morph_data_size != 0 && m_morph_instance_p->m_num_morph_verts != 0)
    {
        renderer.m_vertexBufferData[1].pBuffer = nullptr;
        renderer.m_vertexBufferData[1].vertexCount = 0;
        renderer.m_vertexBufferData[1].offset = 0;
    }
}

// 0x00546590 (:318)
void rl_skin_instance::on_visible_for_frame(rl_renderer& renderer)
{
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->process_active_morphs(renderer);
    }
}

// 0x004D1420 (:332)
bool rl_skin_instance::can_allocate_bones(int num_bones)
{
    // Magic number: 0x30 bytes per bone (matrix43), 16-byte aligned.
    if (SKIN_SMALL_BONES_MAX < num_bones)
    {
        return Rl_skin_bone_manager_pool_large.can_alloc(num_bones * 0x30, 0x10);
    }
    return Rl_skin_bone_manager_pool_small.can_alloc(num_bones * 0x30, 0x10);
}

// 0x004D1450 (:351)
void rl_skin_instance::release_bones()
{
    if (SKIN_SMALL_BONES_MAX < m_num_allocated_bones)
    {
        Rl_skin_bone_manager_pool_large.release_bytes(m_bone_matrices_p);
    }
    else if (0 < m_num_allocated_bones)
    {
        Rl_skin_bone_manager_pool_small.release_bytes(m_bone_matrices_p);
    }
    m_bone_matrices_p = nullptr;
    m_num_bones_matrices = 0;
}

// 0x004D14B0 (:378)
// Once per instance, 1 to 64 bones, set to identity. Notes (decompiled behaviour, kept as is): release_bones leaves
// m_num_allocated_bones, so an instance never allocates twice.
bool rl_skin_instance::allocate_bones(int num_bones)
{
    if (m_bone_matrices_p != nullptr || m_num_allocated_bones != 0)
    {
        return false;
    }
    if ((unsigned int)(num_bones - 1) >= (unsigned int)num_bones_matrices_max)
    {
        return false;
    }
    page_mempool& pool = num_bones <= SKIN_SMALL_BONES_MAX ? Rl_skin_bone_manager_pool_small : Rl_skin_bone_manager_pool_large;
    m_bone_matrices_p = (matrix43*)pool.alloc(num_bones * 0x30, 0x10);
    if (m_bone_matrices_p == nullptr)
    {
        return false;
    }
    m_num_allocated_bones = num_bones;
    m_num_bones_matrices = num_bones;
    for (int i = 0; i < m_num_allocated_bones; i++)
    {
        m_bone_matrices_p[i].make_identity();
    }
    return true;
}

// 0x004D1550 (:413)
// One allocation, 16-byte aligned, split between the pools.
void rl_skin_bone_manager_init()
{
    // Magic numbers: 0xC00 bytes per large slot (64 bones of 3 float4 rows), 0x60 per small slot (2 bones).
    const unsigned int large_size = Rl_skin_bone_manager_num_large_slots * 0xC00;
    const unsigned int small_size = Rl_skin_bone_manager_num_small_slots * 0x60;
    Rl_skin_bone_manager_pool = new unsigned char[small_size + 0x10 + large_size];
    if (Rl_skin_bone_manager_pool == nullptr)
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0);
        keen::debug::nativePrintArgs("[error] Failed to initialize renderer.\n");
        ExitProcess(0);
    }
    const unsigned int misalignment = (unsigned int)Rl_skin_bone_manager_pool & 0xF;
    unsigned char* mem_p = Rl_skin_bone_manager_pool + (misalignment == 0 ? 0 : 0x10 - misalignment);
    Rl_skin_bone_manager_pool_large.init(mem_p, large_size, 0xC00, "RL_skin large bone pool", 1, 0);
    Rl_skin_bone_manager_pool_small.init(mem_p + large_size, small_size, 0x60, "RL_skin small bone pool", 1, 0);
}

// 0x004D15F0 (:454)
void rl_skin_bone_manager_shutdown()
{
    delete[] Rl_skin_bone_manager_pool;
}

// 0x004D1600 (:471)
void rl_skin_bone_manager_set_pool_sizes(int num_large_slots, int num_small_slots)
{
    Rl_skin_bone_manager_num_large_slots = num_large_slots;
    Rl_skin_bone_manager_num_small_slots = num_small_slots;
}

// 0x004D1620 (:479)
void rl_skin_bone_manager_set_thread_ownership()
{
    Rl_skin_bone_manager_pool_large.set_thread_ownership();
    Rl_skin_bone_manager_pool_small.set_thread_ownership();
}
