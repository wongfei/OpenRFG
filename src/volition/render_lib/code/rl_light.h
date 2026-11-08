// Original: volition/render_lib/code/rl_light.h
// Implementation: volition/render_lib/code/rl_light.cpp
//
// rl_light, a light source (point, spot, directional) with color, attenuation and shadow settings.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_class_factory_info;
class rl_renderable_instance;

namespace keen
{
    struct LightingFragmentShaderConstants;
    struct LightingVertexShaderConstants;
    struct float4;
}

// size 0x120
class rl_light : public rl_scene_entity
{
    // Access inferred: rl_scene reads and writes the protected members directly (inlined accessors).
    friend class rl_scene;
    // Access inferred: rl_scene_renderer::render_lighting_passes reads the visibility pass index.
    friend class rl_scene_renderer;
    // Access inferred: game_render_set_TOD_light updates the new sun's visibility.
    friend void game_render_set_TOD_light(rl_light* light_p);
    // Access inferred: rl_shadow_system reads the light type and direction.
    friend class rl_shadow_system;
public:
    enum light_type
    {
        LIGHT_TYPE_DIRECTIONAL = 0,
        LIGHT_TYPE_POINT = 1,
        LIGHT_TYPE_SPOT_LIGHT = 2,
        LIGHT_TYPE_FORCE_SIZEOF_INT = -1,
    };

    rl_light* get_prev() const;
    void set_prev(rl_light*);
    rl_light* get_next() const;
    void set_next(rl_light*);
protected:
    rl_light(); // 0x0053E990 (:24)
    virtual ~rl_light(); // vtbl+0x00 0x0053EA10 (:33)
private:
    static const pool_list<rl_light*>& get_object_pool(); // 0x004BBB60 (.h:41)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BBB70 (.h:41)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00510B20 (:11)
    static rl_light* create_placement(rl_light*);
    static rl_light* create(); // 0x0054BBE0 (:11)
    static bool can_create(); // 0x004E8B70 (:11)
    virtual void release(); // vtbl+0x04 0x0053E910 (:11)
    light_type get_light_type() const; // 0x004BBB80 (.h:115)
    void set_light_type(light_type light_type); // 0x004BBB90 (.h:120)
    void set_is_visible(bool is_visible); // 0x007C0220 (.h:181)
    const rl_color_float& get_color() const;
    void set_color(const rl_color_float& color); // 0x004BBBA0 (.h:131)
    float get_attenuation_start() const;
    void set_attenuation_start(float attenuation_start); // 0x004BBBC0 (.h:143)
    float get_attenuation_end() const; // 0x004BBBE0 (.h:149)
    void set_attenuation_end(float attenuation_end); // 0x004BBBF0 (.h:154)
    float get_hotspot_falloff_size() const; // 0x004BBC10 (.h:160)
    void set_hotspot_falloff_size(float hotspot_falloff_size); // 0x00A19970 (.h:165)
    float get_hotspot_size() const;
    void set_hotspot_size(float hotspot_size); // 0x00A19990 (.h:176)
    void get_spotlight_fvec(vector& _fvec) const; // 0x004BBC20 (.h:191)
    void get_light_direction(vector& light_dir) const; // 0x004BBCE0 (.h:253)
    void set_light_direction(const vector& light_dir); // 0x004BBD00 (.h:258)
    void set_vis_pass_indices(unsigned int render_index, unsigned int shadow_index); // 0x004BBC40 (.h:196)
    unsigned int get_shadow_vis_pass_flags() const; // 0x004BBC60 (.h:202)
    unsigned int get_render_vis_pass_flags() const; // 0x004BBC80 (.h:212)
    const vector& get_clip_min() const;
    void set_clip_min(const vector& clip_min); // 0x004BBCA0 (.h:228)
    const vector& get_clip_max() const;
    void set_clip_max(const vector& clip_max); // 0x004BBCC0 (.h:238)
    void set_clip_mesh(rl_renderable_instance* clip_mesh); // 0x00A199B0 (.h:243)
    rl_renderable_instance* get_clip_mesh();
    void fillLightingVertexShaderData(keen::LightingVertexShaderConstants* pVertexConstants, const vector& renderOffset) const; // 0x004C2510 (:134)
    void fillLightingFragmentShaderData(keen::LightingFragmentShaderConstants* pFragmentConstants, float colorScale) const; // 0x004C27E0 (:171)
protected:
    void visible_update(bool is_visible); // 0x004C23F0 (:43)

public:
    rl_light* prev; // 0xC0
    rl_light* next; // 0xC4
    static const unsigned int m___class_id; // 0x012BECC4
    static rl_class_factory_info ms_class_factory_info; // 0x01514CC4
protected:
    static pool_list<rl_light*> ms_object_pool; // 0x01514CF4
    light_type m_light_type; // 0xC8
    rl_color_float m_color; // 0xCC
    float m_attenuation_start; // 0xDC
    float m_attenuation_end; // 0xE0
    float m_hotspot_falloff_size; // 0xE4
    float m_hotspot_size; // 0xE8
    vector m_clip_min; // 0xEC
    vector m_clip_max; // 0xF8
    vector m_light_direction; // 0x104
    rl_renderable_instance* m_clip_mesh; // 0x110
    unsigned short m_vis_pass_index; // 0x114
    unsigned short m_shadow_vis_pass_index; // 0x116
};

keen::float4 createHotSpotInfo(float hotSpotSize, float hotSpotFallOffSize); // 0x004C2460 (:121)
