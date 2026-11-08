// Original: volition/render_lib/code/rl_renderable_particles_instance_group.h
// Implementation: volition/render_lib/code/rl_renderable_particles_instance_group.cpp
//
// rl_renderable_particles_instance_group, particle instances drawn together.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_corona_instance.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

class mempool;
class rl_batched_pass;
class rl_camera;
class rl_class_factory_info;
class rl_particle_batched_pass;
class rl_renderer;

// size 0x130
class rl_renderable_particles_instance_group : public rl_renderable_instance
{
protected:
    rl_renderable_particles_instance_group(); // 0x00543A50 (:15)
    virtual ~rl_renderable_particles_instance_group(); // vtbl+0x00 0x00543B10 (:20)
private:
    static const pool_list<rl_renderable_particles_instance_group*>& get_object_pool(); // 0x004BD740 (.h:32)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BD750 (.h:32)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00515C50 (:11)
    static rl_renderable_particles_instance_group* create_placement(rl_renderable_particles_instance_group*);
    static rl_renderable_particles_instance_group* create(); // 0x0054CE40 (:11)
    static bool can_create(); // 0x004F1780 (:11)
    virtual void release(); // vtbl+0x04 0x005439D0 (:11)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x0054CED0 (:79)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x00543B00 (.h:39)
    virtual void add_to_batch(rl_particle_batched_pass* pass_p); // vtbl+0x54 0x004C4D50 (:107)
    void add_instance(rl_corona_instance* instance_p); // 0x004F1920 (:67)
    void add_instance(rl_renderable_particles_instance* instance_p); // 0x004F1790 (:25)
    void remove_instance(rl_corona_instance* instance_p); // 0x004F1980 (:72)
    void remove_instance(rl_renderable_particles_instance* instance_p); // 0x004F18A0 (:53)
    const linked_list_p<rl_corona_instance>& get_corona_list(); // 0x00743B50 (.h:66)
    const rl_color_float& get_effect_material_tint() const;
    void set_effect_material_tint(const rl_color_float& effect_material_tint); // 0x00C9E230 (.h:77)

    static const unsigned int m___class_id; // 0x012BEDB0
    static rl_class_factory_info ms_class_factory_info; // 0x0151514C
protected:
    static pool_list<rl_renderable_particles_instance_group*> ms_object_pool; // 0x0151517C
private:
    // vfxlib's vfx_effect (release, update_render) walks the coronas and sets the tint directly (friends are not
    // recorded in rfg.pdb).
    friend class vfx_effect;
    // rfg's object_effect::update_render walks a skybox orbital effect's coronas.
    friend class object_effect;
    linked_list_p<rl_renderable_particles_instance> m_instances; // 0x110
    linked_list_p<rl_corona_instance> m_coronas; // 0x118
    rl_color_float m_effect_material_tint; // 0x120
};
