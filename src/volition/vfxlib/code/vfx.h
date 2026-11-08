// Original: volition/vfxlib/code/vfx.h
// Implementation: volition/vfxlib/code/vfx.cpp
//
// The vfx system: init/release of the effect and particle system managers in a mempool, the physics plugin (shape and
// rigid body callbacks the game registers for mesh debris), the view position and orientation of the current frame,
// and the class factories (class id -> create/serialize) emitters and filters register. Placement of the physics data
// types inferred (vfx_physics_plugin_init and the vfx_system callbacks take them).
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class vfx_effect_manager;
class vfx_particle_system_filter;
class vfx_particle_system_filter_instance;
class vfx_particle_system_manager;

// size 0x6C
struct vfx_physics_rigid_body_create_data
{
    vfx_physics_rigid_body_create_data(); // 0x00C9EE90

    vector m_position; // 0x00
    matrix m_rotation; // 0x0C
    rl_handle m_shape_handle; // 0x30
    unsigned int m_material_index; // 0x34
    float m_volume; // 0x38
    vector m_center_of_mass; // 0x3C
    matrix m_inertia_tensor; // 0x48
};

// size 0x4C
struct vfx_physics_rigid_body_update_data
{
    vfx_physics_rigid_body_update_data(); // 0x00CA0C30

    rl_handle m_handle; // 0x00
    matrix43 m_transform; // 0x04
    vector m_force_vector; // 0x34
    vector m_angular_force_vector; // 0x40
};

// size 0x1
class vfx_system
{
    vfx_system();
    ~vfx_system();
public:
    static void init(mempool* pool); // 0x00CB8130 (:56)
    static void release(); // 0x00CA35E0 (:82)
    static vfx_particle_system_manager* get_particle_system_manager(); // 0x006E1810 (.h:118)
    static vfx_effect_manager* get_effect_manager(); // 0x00C9B520 (.h:119)
    static mempool* get_mempool(); // 0x00C9B530 (.h:121)
    static rl_handle (*get_shape_register_cb())(void*); // 0x00C9B540 (.h:124)
    static void set_shape_register_cb(rl_handle (*shape_register_cb)(void*)); // 0x00C9B550 (.h:125)
    static void (*get_shape_unregister_cb())(rl_handle); // 0x00C9B560 (.h:127)
    static void set_shape_unregister_cb(void (*shape_unregister_cb)(rl_handle)); // 0x00C9B570 (.h:128)
    static rl_handle (*get_rigid_body_create_cb())(vfx_physics_rigid_body_create_data*); // 0x00C9B580 (.h:130)
    static void set_rigid_body_create_cb(rl_handle (*rigid_body_create_cb)(vfx_physics_rigid_body_create_data*)); // 0x00C9B590 (.h:131)
    static void (*get_rigid_body_destroy_cb())(rl_handle); // 0x00C9B5A0 (.h:133)
    static void set_rigid_body_destroy_cb(void (*rigid_body_destroy_cb)(rl_handle)); // 0x00C9B5B0 (.h:134)
    static void (*get_rigid_body_update_cb())(vfx_physics_rigid_body_update_data&); // 0x00C9B5C0 (.h:136)
    static void set_rigid_body_update_cb(void (*rigid_body_update_cb)(vfx_physics_rigid_body_update_data&)); // 0x00C9B5D0 (.h:137)
    static const matrix& get_view_orientation(); // 0x00C9B5E0 (.h:139)
    static void set_view_orientation(const matrix& view_orientation); // 0x00741D50 (.h:140)
    static const vector& get_view_position(); // 0x00C9B5F0 (.h:142)
    static void set_view_position(const vector& view_position); // 0x00741D90 (.h:143)
    static vfx_particle_system_filter_instance* get_global_wind_instance(); // 0x006E1820 (.h:145)

private:
    static vfx_particle_system_manager* ms_particle_system_manager_instance_p; // 0x03A23C60
    static vfx_effect_manager* ms_effect_manager_instance_p; // 0x03A23C64
    static mempool* ms_mempool_p; // 0x03A23C70
    static rl_handle (*ms_rigid_body_create_cb)(vfx_physics_rigid_body_create_data*); // 0x03A23C74
    static void (*ms_rigid_body_destroy_cb)(rl_handle); // 0x03A23C78
    static void (*ms_rigid_body_update_cb)(vfx_physics_rigid_body_update_data&); // 0x03A23C7C
    static rl_handle (*ms_shape_register_cb)(void*); // 0x03A23C80
    static void (*ms_shape_unregister_cb)(rl_handle); // 0x03A23C84
    static matrix ms_view_orientation; // 0x03A254E0
    static vector ms_view_position; // 0x03A254D4
    static vfx_particle_system_filter_instance* ms_wind_filter_instance_p; // 0x03A23C68
    static vfx_particle_system_filter* ms_wind_filter_p; // 0x03A23C6C
};

// size 0x14
struct vfx_class_factory_info
{
    typedef void* (*class_factory)();
    typedef void (*class_serializer)(void*, void*);

    vfx_class_factory_info* get_prev() const;
    void set_prev(vfx_class_factory_info*);
    vfx_class_factory_info* get_next() const;
    void set_next(vfx_class_factory_info*);
    static void register_class(vfx_class_factory_info* class_factory_info_p); // 0x00CA2DB0 (.h:178)
    static vfx_class_factory_info* get_class_factory(unsigned int class_id); // 0x00CA3600 (:97)
    vfx_class_factory_info(unsigned int class_id, void* (*create)(), void (*serialize)(void*, void*)); // 0x00CA2E00 (.h:182)

    vfx_class_factory_info* prev; // 0x00
    vfx_class_factory_info* next; // 0x04
    unsigned int m_class_id; // 0x08
    void* (*m_create)(); // 0x0C
    void (*m_serialize)(void*, void*); // 0x10
    static linked_list_p<vfx_class_factory_info> class_factories; // 0x03A23CB0
};

float vfx_float_random_lerp(float percentage, float a, float b); // 0x00C9B600 (.h:190)
float vfx_float_random(float a, float b); // 0x00C9B650 (.h:202)
float vfx_max(const float& a, const float& b); // 0x00C9B6A0 (.h:219)
void vfx_system_init(mempool* global_mempool); // 0x00CB8200 (:92)
void vfx_physics_plugin_init(rl_handle (*shape_register_cb)(void*), void (*shape_unregister_cb)(rl_handle), rl_handle (*rigid_body_create_cb)(vfx_physics_rigid_body_create_data*), void (*rigid_body_destroy_cb)(rl_handle), void (*rigid_body_update_cb)(vfx_physics_rigid_body_update_data&)); // 0x00C9D700 (:115)
