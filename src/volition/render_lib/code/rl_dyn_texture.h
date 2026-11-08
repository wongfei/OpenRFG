// Original: volition/render_lib/code/rl_dyn_texture.h
// Implementation: volition/render_lib/code/rl_dyn_texture.cpp
//
// rl_dyn_texture_base and rl_dyn_texture, textures created and updated at run time.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_device.h"

class rl_class_dynamic_info;
class rl_renderer;

// size 0xC
class rl_dyn_texture_base : public rl_base_object
{
public:
    rl_dyn_texture_base(); // 0x0050DEC0 (:48)
    virtual ~rl_dyn_texture_base(); // vtbl+0x00 0x005240C0 (:66)
    const int get_texture_handle() const; // 0x004B7500 (.h:26)
    virtual const rl_dev_texture& get_texture_data() const = 0; // vtbl+0x08
    virtual void get_dimensions(int&, int&, int&) = 0; // vtbl+0x0C
    static rl_dyn_texture_base* find_by_handle(int tex_handle); // 0x004E4D90 (:77)
    static void release_resources(); // 0x0050DF50 (:89)
    static void renew_resources(rl_renderer& renderer); // 0x0050DFC0 (:98)
protected:
    virtual void release_resource(); // vtbl+0x10 0x0050DF30 (.h:39)
    virtual void renew_resource(rl_renderer&); // vtbl+0x14 0x0050DF40 (.h:40)

    int m_tex_handle; // 0x08
};

// size 0x10
class rl_dyn_texture : public rl_dyn_texture_base
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred from rl_hdr_compositor::create_render_textures.
    friend class rl_hdr_compositor;
    // Access inferred from rl_outline_compositor::initialize.
    friend class rl_outline_compositor;
protected:
    rl_dyn_texture(); // 0x00524120 (:110)
    virtual ~rl_dyn_texture(); // vtbl+0x00 0x00524150 (:117)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_dyn_texture* create(); // 0x0053AA20 (:106)
    static bool can_create(); // 0x004BEBB0 (:106)
    virtual void release(); // vtbl+0x04 0x004BEBC0 (:106)
    void set_texture_data(const rl_dev_texture& texture); // 0x004B7510 (.h:53)
    virtual const rl_dev_texture& get_texture_data() const; // vtbl+0x08 0x00524140 (.h:54)
    virtual void get_dimensions(int& width, int& height, int& mip_levels); // vtbl+0x0C 0x004E4DD0 (:122)

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514A98
protected:
    rl_dev_texture m_dev_texture; // 0x0C
};
