// Original: volition/render_lib/code/rl_renderer_core.cpp
//
// render_lib startup and shutdown (rl_init, rl_shutdown, the class factories), the startup parameters and the deferred
// operations the simulation thread queues while the renderer is locked (rl_lock, rl_unlock, rl_add_deferred_op).

#include "volition/render_lib/code/rl_renderer_core.h"

#include "volition/render_lib/code/rl_clb_projector.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_fog_volume.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh_recompile.h"
#include "volition/render_lib/code/rl_meteor_shower.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_morph_weights.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/render_lib/code/rl_skybox_renderable_instance.h"
#include "volition/render_lib/code/rl_star_field.h"
#include "volition/render_lib/code/rl_xray_material_manager.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/lockfree_queues.h"
#include "volition/vlib/code/os/thread.h"
#include "keen/base/trace.hpp"
#include "keen/graphics/graphicssystem.hpp"

#include <stdlib.h>
#include <windows.h>

rl_init_params Init_params; // 0x01C20A60
rl_scene* Active_scene_p = nullptr; // 0x01B7BD04
static bool Rl_is_initialized = false; // 0x01B7BD08
static bool Rl_is_locked = false; // 0x01B7BD09
static mempool Factory_mempool; // 0x01C320E0
static fober_lock_free_queue Deferred_operation_list; // 0x01515198
// Magic number: at most 512 operations queued while the renderer is locked.
static object_pool<rl_deferred_operation, 512, 1> Deferred_op_pool; // 0x01C20C30
static void (*Rl_wait_for_unlock_cb)() = nullptr; // 0x01B69A40

// 0x004C5BA0 (:49)
rl_deferred_operation::rl_deferred_operation()
    : m_type(RLDO_NOP)
    , m_data(nullptr)
{
}

// 0x004C5BB0 (:51)
void rl_deferred_operation::init(rl_deferred_operation_type type, void* data_p)
{
    m_type = type;
    m_data = data_p;
}

// 0x00459660 (.h:81)
rl_init_params::rl_init_params()
{
    m_uses_screen_info = 0;
    m_vsync_enabled = 0;
    m_memtracker_start_cb = nullptr;
    m_memtracker_end_cb = nullptr;
    m_pKeenGraphicsSystem = nullptr;
    m_pBufferAllocator = nullptr;
    m_pVramAllocator = nullptr;
    m_pFramebufferAllocator = nullptr;
    m_pFileSystem = nullptr;
    // m_override_width, m_override_height, m_pDefaultAllocator and m_occlusionQueryGPUMemory are left as is.
}

// 0x004C5BC0 (:76)
bool rl_is_initialized()
{
    return Rl_is_initialized;
}

// 0x004C5BD0 (:81)
void rl_fail_initialization()
{
    ExitProcess(0);
}

// 0x004F3120 (:102)
// One mempool holds the object pools of every factory class: objects_max * (class size + 0x14) bytes per class.
void factory_init()
{
    if (rl_class_factory_info::ms_class_factories == nullptr)
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0);
        keen::debug::nativePrintArgs("[error] Failed to initialize renderer.\n");
        ExitProcess(0);
    }
    unsigned int size = 0;
    rl_class_factory_info* first_p = rl_class_factory_info::ms_class_factories->first();
    rl_class_factory_info* info_p = first_p;
    while (info_p != nullptr)
    {
        // Magic number: 0x14 bytes of pool and alignment overhead per object.
        size += (info_p->m_class_size + 0x14) * info_p->m_objects_max;
        info_p = info_p->next;
        if (info_p == first_p)
        {
            break;
        }
    }
    void* mem = new unsigned char[size];
    if (mem == nullptr)
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0);
        keen::debug::nativePrintArgs("[error] Failed to initialize renderer.\n");
        ExitProcess(0);
    }
    // Magic numbers: 32-byte alignment, mempool flag 8.
    Factory_mempool.init(mem, size, "rl factory", 0x20, 8);
    info_p = first_p;
    while (info_p != nullptr)
    {
        info_p->m_factory_init(Factory_mempool);
        info_p = info_p->next;
        if (info_p == first_p)
        {
            break;
        }
    }
}

// 0x004C5BF0 (:138)
void factory_shutdown()
{
    delete[] (unsigned char*)Factory_mempool.get_base();
}

// 0x004C5C00 (:144)
void rl_track_mem_usage_start(const char* where)
{
    if (Init_params.m_memtracker_start_cb != nullptr)
    {
        Init_params.m_memtracker_start_cb(where);
    }
}

// 0x004C5C10 (:151)
void rl_track_mem_usage_end(bool quiet)
{
    if (Init_params.m_memtracker_end_cb != nullptr)
    {
        Init_params.m_memtracker_end_cb(quiet);
    }
}

// 0x005508E0 (:158)
void rl_init(rl_init_params& params)
{
    Rl_is_initialized = true;
    // The screen size is the back buffer's, whatever the parameters asked for.
    params.m_screen_info.set_screen_width((int)keen::graphics::getRenderTargetWidth(keen::graphics::getBackBufferRenderTarget(params.m_pKeenGraphicsSystem)));
    params.m_screen_info.set_screen_height((int)keen::graphics::getRenderTargetHeight(keen::graphics::getBackBufferRenderTarget(params.m_pKeenGraphicsSystem)));
    params.m_screen_info.set_monitor(0);
    params.m_screen_info.set_window_mode(keen::graphics::getCurrentWindowMode(params.m_pKeenGraphicsSystem, nullptr));
    Init_params = params;

    rl_track_mem_usage_start("RL: factory");
    factory_init();
    rl_skin_bone_manager_init();
    rl_render_lib::create();
    rl_shader_library::create();
    rl_primitive_renderer::create();
    rl_draw::create();
    rl_track_mem_usage_end(false);

    rl_render_lib::get_instance()->initialize(params);

    rl_track_mem_usage_start("RL: shader library");
    rl_shader_library::get_instance()->initialize();
    rl_track_mem_usage_end(false);

    rl_track_mem_usage_start("RL: other systems");
    Matlib_entry_hash.clear();
    rl_render_lib* render_lib_p = rl_render_lib::get_instance();
    const rl_screen_info* screen_info_p = render_lib_p->get_screen_info();
    rl_singleton_phoenix<rl_particle_renderer>::get_instance()->initialize(screen_info_p->get_screen_width(), screen_info_p->get_screen_height(), render_lib_p->get_level_of_detail());
    rl_primitive_renderer* primitive_renderer_p = rl_primitive_renderer::get_instance();
    primitive_renderer_p->initialize_for_thread();
    primitive_renderer_p->initialize();
    rl_draw::get_instance()->initialize();
    primitive_renderer_p->set_mode(RLPRM_IMMEDIATE);
    rl_singleton_phoenix<rl_shadow_system>::get_instance()->initialize();
    rl_clb_projector_system_init();
    rl_morph_system_init();
    rl_singleton_phoenix<rl_star_field>::get_instance()->initialize();
    rl_singleton_phoenix<rl_meteor_shower>::get_instance()->initialize();
    rl_skybox_renderable_instance::initialize();
    rl_fog_volume::system_init();
    rl_track_mem_usage_end(false);

    rl_track_mem_usage_start("RL: gpu visibility");
    rl_gpu_vis_system_init(params.m_pKeenGraphicsSystem);
    rl_track_mem_usage_end(false);

    rl_xray_material_manager_init();
    rl_mesh_recompile_initialize();
}

// 0x0054D100 (:230)
// Notes (decompiled behaviour, kept as is): the fog volume, GPU visibility, CLB projector, morph, star field, meteor
// shower and render lib singletons are not shut down here.
void rl_shutdown()
{
    rl_mesh_recompile_shutdown();
    rl_xray_material_manager_shutdown();
    rl_shader_library::get_instance()->shutdown();
    rl_singleton_phoenix<rl_particle_renderer>::get_instance()->release();
    rl_draw::get_instance()->release();
    rl_primitive_renderer::get_instance()->release();
    rl_shader_library::get_instance()->release();
    rl_singleton_phoenix<rl_shadow_system>::get_instance()->shutdown();
    rl_singleton_phoenix<rl_shadow_system>::get_instance()->release();
    free(rl_class_factory_info::ms_class_factories);
    factory_shutdown();
    rl_skin_bone_manager_shutdown();
    Rl_is_initialized = false;
}

// 0x004C5C20 (:260)
keen::GraphicsSystem* rl_get_graphics_system()
{
    return Init_params.m_pKeenGraphicsSystem;
}

// 0x004C5C30 (:265)
keen::MemoryAllocator* rl_get_buffer_allocator()
{
    return Init_params.m_pBufferAllocator;
}

// 0x004C5C40 (:270)
keen::MemoryAllocator* rl_get_default_allocator()
{
    return Init_params.m_pDefaultAllocator;
}

// 0x004C5C50 (:275)
keen::FileSystem* rl_get_filesystem()
{
    return Init_params.m_pFileSystem;
}

// 0x004C5C60 (:280)
void rl_set_active_scene(rl_scene* scene_p)
{
    Active_scene_p = scene_p;
}

// 0x004C5C70 (:285)
rl_scene* rl_get_active_scene()
{
    return Active_scene_p;
}

// 0x0054D200 (:290)
// LTCG: type arrives in EAX and data in ECX.
void process_deferred_op(rl_deferred_operation_type type, void* data)
{
    switch (type)
    {
    case RLDO_SCENE_ADD_RI:
        Active_scene_p->add_renderable_instance((rl_renderable_instance*)data);
        return;
    case RLDO_SCENE_ADD_LIGHT:
        Active_scene_p->add_light((rl_light*)data);
        return;
    case RLDO_SCENE_REMOVE_RI:
        Active_scene_p->remove_renderable_instance((rl_renderable_instance*)data);
        return;
    case RLDO_SCENE_REMOVE_LIGHT:
        Active_scene_p->remove_light((rl_light*)data);
        return;
    case RLDO_CREATE_DECAL:
        rl_decal_process_deferred_create((rl_decal_deferred_info*)data);
        return;
    case RLDO_RELEASE_DECAL:
        rl_decal_release((unsigned int)data);
        return;
    case RLDO_RELEASE_OBJECT:
        ((rl_base_object*)data)->release();
        return;
    case RLDO_SET_FRAMEBUFFER_RESOLVE:
        // Not supported on this platform: the release build only logs the failed check.
        VLIB_CRITICAL_FAILURE();
        return;
    case RLDO_SCENE_ADD_LOW_LOD:
        Active_scene_p->add_low_lod((rl_scene_entity*)data);
        return;
    case RLDO_SCENE_REMOVE_LOW_LOD:
        Active_scene_p->remove_low_lod((rl_scene_entity*)data);
        return;
    case RLDO_OVERRIDE_MATERIAL_HANDLE:
        rl_material_library_override_handle((rl_deferred_material_handle_override*)data);
        return;
    default:
        return;
    }
}

// 0x0054D2F0 (:334)
// Runs the operations queued while the renderer was locked, oldest first.
void rl_unlock()
{
    Rl_is_locked = false;
    lf_node<void*>* pnode = (lf_node<void*>*)Deferred_operation_list.pop();
    while (pnode != nullptr)
    {
        rl_deferred_operation* op_p = (rl_deferred_operation*)pnode->value;
        process_deferred_op(op_p->m_type, op_p->m_data);
        Deferred_op_pool.free(op_p);
        free_utility_lockfree_node(pnode);
        pnode = (lf_node<void*>*)Deferred_operation_list.pop();
    }
}

// 0x004C5C80 (:351)
void rl_lock()
{
    Rl_is_locked = true;
}

// 0x004C5C90 (:356)
bool rl_is_locked()
{
    return Rl_is_locked;
}

// 0x0052DC50 (:362)
// Queues an operation for rl_unlock (newest at the head; rl_unlock pops from the tail).
void rl_add_deferred_op(rl_deferred_operation_type type, void* data)
{
    lf_node<void*>* pnode = get_utility_lockfree_node();
    rl_deferred_operation* op_p = Deferred_op_pool.alloc(0);
    do
    {
        if (pnode == nullptr)
        {
            if (op_p != nullptr)
            {
                Deferred_op_pool.free(op_p);
            }
        }
        else
        {
            if (op_p != nullptr)
            {
                op_p->m_data = data;
                op_p->m_type = type;
                pnode->value = op_p;
                Deferred_operation_list.push(&pnode->base_node);
                return;
            }
            free_utility_lockfree_node(pnode);
        }
        thread_yield();
        pnode = get_utility_lockfree_node();
        op_p = Deferred_op_pool.alloc(0);
    } while (pnode != nullptr && op_p != nullptr);
    // Notes (decompiled behaviour, kept as is): despite the loop, there is a single retry; when it fails too, the
    // operation is dropped.
    keen::debug::nativePrintArgs("[renderer core] rl_add_deferred_op() failed allocating either lf_node or deferred_op, this is probably bad!\n");
    if (pnode != nullptr)
    {
        free_utility_lockfree_node(pnode);
    }
    if (op_p != nullptr)
    {
        Deferred_op_pool.free(op_p);
    }
}

// 0x004C5CA0 (:429)
void rl_set_wait_for_unlock_cb(void (*callback)())
{
    Rl_wait_for_unlock_cb = callback;
}
