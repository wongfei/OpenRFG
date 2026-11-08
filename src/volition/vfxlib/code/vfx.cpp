// Original: volition/vfxlib/code/vfx.cpp
//
// The vfx system (VFX.md FX1): the particle system and effect managers in the caller's mempool, the global wind filter,
// the physics plugin callbacks, the view of the current frame and the class factory list emitters and filters register
// with (their vfx_class_factory_info statics, in the emitter and filter files). The functions of vfx.h are here too
// (the original has them inline in the header).

#include "volition/vfxlib/code/vfx.h"

#include "volition/vfxlib/code/vfx_effect_manager.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_wind.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/vmemory.h"

#include "keen/base/trace.hpp"

// Deviation (build): rfg.exe compiles vfxlib as one unity TU, so class_factories' dynamic initializer (0x0108FB50) runs
// before the emitter and filter factory infos append themselves. Here they are separate TUs; init_seg(lib) runs this
// TU's initializers before theirs, so the list constructor cannot clear entries already appended.
#pragma warning(disable : 4073)
#pragma init_seg(lib)

// Deviation (build): the emitter and filter files are only reached through their factory infos, which nothing
// references by name, so a static library link drops them and their registration (every template using them then
// fails to load, and stream2 skips the rest of its container). Force them in from this always linked TU.
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_emitter_plane@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_emitter_point@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_emitter_sphere@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_emitter_box@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_emitter_cylinder@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_filter_wind@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_filter_magnet@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_filter_turbulence@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_filter_vortex@@2Uvfx_class_factory_info@@A")
#pragma comment(linker, "/include:?m_vfx_class_factory_info@vfx_particle_system_filter_orientor@@2Uvfx_class_factory_info@@A")

// ---- statics
vfx_particle_system_manager* vfx_system::ms_particle_system_manager_instance_p; // 0x03A23C60
vfx_effect_manager* vfx_system::ms_effect_manager_instance_p; // 0x03A23C64
mempool* vfx_system::ms_mempool_p; // 0x03A23C70
rl_handle (*vfx_system::ms_rigid_body_create_cb)(vfx_physics_rigid_body_create_data*); // 0x03A23C74
void (*vfx_system::ms_rigid_body_destroy_cb)(rl_handle); // 0x03A23C78
void (*vfx_system::ms_rigid_body_update_cb)(vfx_physics_rigid_body_update_data&); // 0x03A23C7C
rl_handle (*vfx_system::ms_shape_register_cb)(void*); // 0x03A23C80
void (*vfx_system::ms_shape_unregister_cb)(rl_handle); // 0x03A23C84
matrix vfx_system::ms_view_orientation; // 0x03A254E0
vector vfx_system::ms_view_position; // 0x03A254D4
vfx_particle_system_filter_instance* vfx_system::ms_wind_filter_instance_p; // 0x03A23C68
vfx_particle_system_filter* vfx_system::ms_wind_filter_p; // 0x03A23C6C

// The registered emitter and filter classes (ten, see the emitter and filter files; dynamic initializer 0x0108FB50
// runs before theirs).
linked_list_p<vfx_class_factory_info> vfx_class_factory_info::class_factories; // 0x03A23CB0

// 0x00C9EE90: the shape handle starts invalid (the rest is filled by the caller).
vfx_physics_rigid_body_create_data::vfx_physics_rigid_body_create_data()
{
    m_shape_handle.invalidate();
}

// 0x00CA0C30: the body handle starts invalid (the rest is filled by the caller).
vfx_physics_rigid_body_update_data::vfx_physics_rigid_body_update_data()
{
    m_handle.invalidate();
}

// 0x00CB8130 (:56)
void vfx_system::init(mempool* pool)
{
    ms_mempool_p = pool;
    if (ms_particle_system_manager_instance_p == nullptr)
    {
        ms_particle_system_manager_instance_p = vfx_particle_system_manager::create();
    }
    if (ms_effect_manager_instance_p == nullptr)
    {
        ms_effect_manager_instance_p = vfx_effect_manager::create();
    }
    ms_particle_system_manager_instance_p->init();
    ms_effect_manager_instance_p->init();
    if (ms_wind_filter_instance_p == nullptr)
    {
        ms_wind_filter_instance_p = vfx_particle_system_filter_instance::create();
        ms_wind_filter_p = vfx_particle_system_filter_wind::create();
        ms_wind_filter_instance_p->set_filter(ms_wind_filter_p);
        ms_wind_filter_instance_p->set_force(1.0f);
    }
}

// 0x00CA35E0 (:82): the pools stay in the mempool; the wind filter and its instance are kept.
void vfx_system::release()
{
    ms_particle_system_manager_instance_p->release();
    ms_particle_system_manager_instance_p = nullptr;
    ms_effect_manager_instance_p = nullptr;
}

// 0x006E1810 (.h:118)
vfx_particle_system_manager* vfx_system::get_particle_system_manager()
{
    return ms_particle_system_manager_instance_p;
}

// 0x00C9B520 (.h:119)
vfx_effect_manager* vfx_system::get_effect_manager()
{
    return ms_effect_manager_instance_p;
}

// 0x00C9B530 (.h:121)
mempool* vfx_system::get_mempool()
{
    return ms_mempool_p;
}

// 0x00C9B540 (.h:124)
rl_handle (*vfx_system::get_shape_register_cb())(void*)
{
    return ms_shape_register_cb;
}

// 0x00C9B550 (.h:125)
void vfx_system::set_shape_register_cb(rl_handle (*shape_register_cb)(void*))
{
    ms_shape_register_cb = shape_register_cb;
}

// 0x00C9B560 (.h:127)
void (*vfx_system::get_shape_unregister_cb())(rl_handle)
{
    return ms_shape_unregister_cb;
}

// 0x00C9B570 (.h:128)
void vfx_system::set_shape_unregister_cb(void (*shape_unregister_cb)(rl_handle))
{
    ms_shape_unregister_cb = shape_unregister_cb;
}

// 0x00C9B580 (.h:130)
rl_handle (*vfx_system::get_rigid_body_create_cb())(vfx_physics_rigid_body_create_data*)
{
    return ms_rigid_body_create_cb;
}

// 0x00C9B590 (.h:131)
void vfx_system::set_rigid_body_create_cb(rl_handle (*rigid_body_create_cb)(vfx_physics_rigid_body_create_data*))
{
    ms_rigid_body_create_cb = rigid_body_create_cb;
}

// 0x00C9B5A0 (.h:133)
void (*vfx_system::get_rigid_body_destroy_cb())(rl_handle)
{
    return ms_rigid_body_destroy_cb;
}

// 0x00C9B5B0 (.h:134)
void vfx_system::set_rigid_body_destroy_cb(void (*rigid_body_destroy_cb)(rl_handle))
{
    ms_rigid_body_destroy_cb = rigid_body_destroy_cb;
}

// 0x00C9B5C0 (.h:136)
void (*vfx_system::get_rigid_body_update_cb())(vfx_physics_rigid_body_update_data&)
{
    return ms_rigid_body_update_cb;
}

// 0x00C9B5D0 (.h:137)
void vfx_system::set_rigid_body_update_cb(void (*rigid_body_update_cb)(vfx_physics_rigid_body_update_data&))
{
    ms_rigid_body_update_cb = rigid_body_update_cb;
}

// 0x00C9B5E0 (.h:139)
const matrix& vfx_system::get_view_orientation()
{
    return ms_view_orientation;
}

// 0x00741D50 (.h:140)
void vfx_system::set_view_orientation(const matrix& view_orientation)
{
    ms_view_orientation = view_orientation;
}

// 0x00C9B5F0 (.h:142)
const vector& vfx_system::get_view_position()
{
    return ms_view_position;
}

// 0x00741D90 (.h:143)
void vfx_system::set_view_position(const vector& view_position)
{
    ms_view_position = view_position;
}

// 0x006E1820 (.h:145)
vfx_particle_system_filter_instance* vfx_system::get_global_wind_instance()
{
    return ms_wind_filter_instance_p;
}

// 0x00CA2DB0 (.h:178)
void vfx_class_factory_info::register_class(vfx_class_factory_info* class_factory_info_p)
{
    class_factories.append(class_factory_info_p);
}

// 0x00CA2E00 (.h:182)
vfx_class_factory_info::vfx_class_factory_info(unsigned int class_id, void* (*create)(), void (*serialize)(void*, void*))
{
    m_class_id = class_id;
    m_create = create;
    m_serialize = serialize;
    class_factories.append(this);
}

// 0x00CA3600 (:97)
vfx_class_factory_info* vfx_class_factory_info::get_class_factory(unsigned int class_id)
{
    vfx_class_factory_info* info_p = class_factories.first();
    while (info_p != nullptr)
    {
        if (class_id == info_p->m_class_id)
        {
            return info_p;
        }
        info_p = info_p->next;
        if (info_p == class_factories.first())
        {
            break;
        }
    }
    keen::debug::nativePrintArgs("Couldn't find class factory. Class was not registered. This is required for all new classes in vfx.cpp");
    return nullptr;
}

// 0x00C9B600 (.h:190): the order of a and b does not matter.
float vfx_float_random_lerp(float percentage, float a, float b)
{
    if (b < a)
    {
        const float swap = a;
        a = b;
        b = swap;
    }
    return a + percentage * (b - a);
}

// 0x00C9B650 (.h:202): uses the global random generator (VFX.md 2.5).
float vfx_float_random(float a, float b)
{
    if (b < a)
    {
        const float swap = a;
        a = b;
        b = swap;
    }
    return fl_rand() * (b - a) + a;
}

// 0x00C9B6A0 (.h:219)
float vfx_max(const float& a, const float& b)
{
    if (0.0f <= a - b)
    {
        return a;
    }
    return b;
}

// 0x00CB8200 (:92): the same body as vfx_system::init (inlined there in rfg.exe).
void vfx_system_init(mempool* global_mempool)
{
    vfx_system::init(global_mempool);
}

// 0x00C9D700 (:115)
void vfx_physics_plugin_init(rl_handle (*shape_register_cb)(void*), void (*shape_unregister_cb)(rl_handle), rl_handle (*rigid_body_create_cb)(vfx_physics_rigid_body_create_data*), void (*rigid_body_destroy_cb)(rl_handle), void (*rigid_body_update_cb)(vfx_physics_rigid_body_update_data&))
{
    vfx_system::set_rigid_body_update_cb(rigid_body_update_cb);
    vfx_system::set_rigid_body_create_cb(rigid_body_create_cb);
    vfx_system::set_rigid_body_destroy_cb(rigid_body_destroy_cb);
    vfx_system::set_shape_register_cb(shape_register_cb);
    vfx_system::set_shape_unregister_cb(shape_unregister_cb);
}
