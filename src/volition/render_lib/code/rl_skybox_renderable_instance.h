// Original: volition/render_lib/code/rl_skybox_renderable_instance.h
// Implementation: volition/render_lib/code/rl_skybox_renderable_instance.cpp
//
// rl_skybox_renderable_instance, the sky dome and orbital bodies (rl_skybox_orbital_params).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_renderer;
class rl_shader_const_ref;

// size 0x28
struct rl_skybox_orbital_params
{
    rl_skybox_orbital_params(); // 0x004D16A0

    rl_color_float tint; // 0x00
    vector position; // 0x10
    float scale; // 0x1C
    int texture_handle; // 0x20
    bool alpha_blend; // 0x24
};

// size 0x390
class rl_skybox_renderable_instance : public rl_renderable_instance
{
protected:
    rl_skybox_renderable_instance(); // 0x005465B0 (:81)
    virtual ~rl_skybox_renderable_instance(); // vtbl+0x00 0x005465F0 (:88)
private:
    static const pool_list<rl_skybox_renderable_instance*>& get_object_pool(); // 0x004C5B80 (.h:46)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004C5B90 (.h:46)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00518F50 (:12)
    static rl_skybox_renderable_instance* create_placement(rl_skybox_renderable_instance*);
    static rl_skybox_renderable_instance* create(); // 0x0054E160 (:12)
    static bool can_create(); // 0x004FCDA0 (:12)
    virtual void release(); // vtbl+0x04 0x00531370 (:12)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x00546620 (:93)
    bool add_subpart(rl_renderable_instance* new_subpart_p); // 0x004D16B0 (:183)
    void clear_subparts(); // 0x004D16E0 (:195)
    void set_render_orbitals_before(int before); // 0x00A1C310 (.h:67)
    int get_render_orbitals_before();
    void clear_orbitals(); // 0x004D1710 (:294)
    void add_orbital(const rl_skybox_orbital_params& orbital_params); // 0x004D1720 (:299)
    static void initialize(); // 0x00519050 (:281)
protected:
    void render_orbital_objects(rl_renderer& renderer, rl_camera& camera); // 0x00531B30 (:268)
    void render_orbital_object(rl_renderer& renderer, rl_camera& camera, const rl_skybox_orbital_params& orbital_params); // 0x005313F0 (:204)

public:
    static const unsigned int m___class_id; // 0x012BEE50
    static rl_class_factory_info ms_class_factory_info; // 0x01516338
protected:
    static pool_list<rl_skybox_renderable_instance*> ms_object_pool; // 0x01516368
    rl_renderable_instance* m_skybox_parts[4]; // 0x110
    int m_num_skybox_parts; // 0x120
    rl_skybox_orbital_params m_orbital_params[15]; // 0x124
    int m_num_orbitals; // 0x37C
    int m_render_orbitals_before; // 0x380
    static unsigned int m_orbital_shader; // 0x014FA5F8
    static rl_shader_const_ref m_orbtial_map_ref; // 0x01516380
    static rl_shader_const_ref m_orbital_tint_ref; // 0x01516384
    static bool initialized; // 0x01B7BD35
};
