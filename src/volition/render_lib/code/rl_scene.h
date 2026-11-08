// Original: volition/render_lib/code/rl_scene.h
// Implementation: volition/render_lib/code/rl_scene.cpp
//
// rl_scene, a renderable world: scene entities, lights, occluders, fog, color correction and HDR state, the partition
// system and the visibility passes. The game has one main scene (game_render_get_main_scene).
#pragma once

#include <stdint.h>
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_color_correct_state.h"
#include "volition/render_lib/code/rl_fog_state.h"
#include "volition/render_lib/code/rl_hdr_state.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_particle_lighting_state.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

class rl_camera;
class rl_class_dynamic_info;
class rl_handle;
class rl_light;
class rl_occluder;
class rl_partition_system;
class rl_renderable_instance;
class rl_renderer;
class rl_scene_entity;
class rl_scene_renderer;
class rl_visibility_manager;

namespace keen
{
    class GraphicsCommandWriter;
    struct MemoryBlock;
}

// size 0x50
struct rl_xray_render_state
{
    rl_xray_render_state(); // 0x004BBFD0 (.h:51)

    bool m_xray_effect_enabled; // 0x00
    bool m_use_terrain_occlusion; // 0x01
    float m_xray_effect_fade_start; // 0x04
    float m_xray_effect_fade_dist; // 0x08
    vector2 m_xray_outline_points[8]; // 0x0C
    int m_num_xray_outline_points; // 0x4C
};

// size 0x210
class rl_scene : public rl_base_object
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred: rl_render_lib and rl_renderer read the private members directly.
    friend class rl_render_lib;
    friend class rl_renderer;
public:
    typedef pool_list<rl_renderable_instance*> renderable_instance_list;
    typedef pool_list<rl_light*> light_list;
    typedef pool_list<rl_occluder*> occluder_list;

protected:
    rl_scene(); // 0x0054D5A0 (:32)
    virtual ~rl_scene(); // vtbl+0x00 0x00517360 (:43)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_scene* create(); // 0x00550C90 (:11)
    static bool can_create(); // 0x004C7470 (:11)
    virtual void release(); // vtbl+0x04 0x004C7480 (:11)
    rl_scene* get_prev() const;
    void set_prev(rl_scene*);
    rl_scene* get_next() const;
    void set_next(rl_scene*);
    static void set_renderable_instances_max(int renderable_instances_max); // 0x007C0330 (.h:285)
    static int get_renderable_instances_max();
    void init(keen::GraphicsCommandWriter* pCommandWriter, keen::MemoryBlock deferredCommandBufferMemory); // 0x0054D800 (:65)
    void render(rl_renderer& renderer, rl_camera& camera, rl_renderer& deferredRenderer); // 0x004C74C0 (:125)
    rl_handle add_renderable_instance(rl_renderable_instance* renderable_instance_p); // 0x00537680 (.h:295)
    void remove_renderable_instance(rl_renderable_instance* renderable_instance_p); // 0x00537700 (.h:324)
    const pool_list<rl_renderable_instance*>& get_renderable_instance_list() const;
    rl_handle add_light(rl_light* light_p); // 0x00537770 (.h:355)
    void remove_light(rl_light* light_p); // 0x005377E0 (.h:380)
    rl_handle add_low_lod(rl_scene_entity* entity_p); // 0x00537840 (.h:404)
    void remove_low_lod(rl_scene_entity* entity_p); // 0x005378B0 (.h:427)
    rl_handle add_landmark_lod(rl_scene_entity* entity_p); // 0x00537900 (.h:444)
    void remove_landmark_lod(rl_scene_entity* entity_p); // 0x00537970 (.h:467)
    const pool_list<rl_light*>& get_light_list() const;
    rl_handle add_occluder(rl_occluder* occluder_p); // 0x00509F50 (.h:484)
    void remove_occluder(rl_occluder* occluder_p); // 0x004E2D50 (.h:493)
    const pool_list<rl_occluder*>& get_occluder_list() const; // 0x004BC040 (.h:502)
    unsigned int get_last_visibility_pass() const; // 0x004BC050 (.h:510)
    void set_last_visibility_pass(unsigned int last_visibility_pass); // 0x004BC060 (.h:515)
    const rl_fog_state& get_fog_state() const; // 0x004BC0E0 (.h:562)
    void set_fog_state(const rl_fog_state& fog_state); // 0x004BC0B0 (.h:557)
    const rl_color_correct_state& get_color_correct_state() const; // 0x00893130 (.h:568)
    void set_color_correct_state(const rl_color_correct_state& color_correct_state); // 0x007C0340 (.h:573)
    const rl_hdr_state& get_hdr_state() const;
    void set_hdr_state(const rl_hdr_state& hdr_state); // 0x007C0380 (.h:584)
    const rl_particle_lighting_state& get_particle_lighting_state() const; // 0x004BC0F0 (.h:590)
    void set_particle_lighting_state(const rl_particle_lighting_state& particle_lighting_state); // 0x007C03A0 (.h:595)
    const rl_color_float& get_ambient_color() const; // 0x004BC100 (.h:601)
    void set_ambient_color(const rl_color_float& ambient_color); // 0x007C03F0 (.h:606)
    const rl_color_float& get_back_ambient_color() const; // 0x004BC110 (.h:611)
    void set_back_ambient_color(const rl_color_float& back_ambient_color); // 0x007C0410 (.h:616)
    float get_time_value() const; // 0x004BC120 (.h:621)
    void set_time_value(float time_val); // 0x007C0430 (.h:626)
    float get_time_of_day_value() const; // 0x004BC130 (.h:631)
    void set_time_of_day_value(float tod_val); // 0x007C0450 (.h:636)
    void set_is_visible(rl_scene_entity* scene_entity_p, bool is_visible); // 0x004BC070 (.h:521)
    rl_light* get_largest_light() const; // 0x004BC140 (.h:643)
    void set_largest_light(rl_light* largest_light_p); // 0x007C04A0 (.h:652)
    rl_visibility_manager* get_visibility_manager() const; // 0x004BC160 (.h:662)
    void set_visibility_manager(rl_visibility_manager*);
    rl_scene_renderer* get_scene_renderer() const; // 0x004BC170 (.h:674)
    void set_scene_renderer(rl_scene_renderer* scene_renderer_p); // 0x004C74B0 (:119)
    rl_partition_system* get_partition_system() const; // 0x004BC180 (.h:680)
    void set_partition_system(rl_partition_system* partition_system_p); // 0x007C04D0 (.h:685)
    rl_partition_system* get_light_partition_system() const; // 0x004BC190 (.h:690)
    void set_light_partition_system(rl_partition_system* partition_system); // 0x007C04E0 (.h:695)
    rl_partition_system* get_landmark_lod_partition_system() const; // 0x004BC1B0 (.h:705)
    void set_landmark_lod_partition_system(rl_partition_system* partition_system); // 0x007C0500 (.h:715)
    rl_partition_system* get_low_lod_partition_system() const; // 0x004BC1A0 (.h:700)
    void set_low_lod_partition_system(rl_partition_system* partition_system); // 0x007C04F0 (.h:710)
    void set_render_offset(const vector& render_offset); // 0x004F5B90 (:132)
    unsigned int get_render_offset(vector& render_offset); // 0x004BC1C0 (.h:720)
    int get_renderable_instance_count() const;
    int get_light_count() const;
    void set_skybox(rl_renderable_instance* renderable_instance_p); // 0x007C0510 (.h:738)
    rl_renderable_instance* get_skybox() const; // 0x004BC1F0 (.h:743)
    bool get_outline_pass_enabled(int);
    void set_outline_state(const rl_outline_state& state); // 0x0094BDF0 (.h:761)
    void get_outline_state(rl_outline_state& state); // 0x004BC200 (.h:766)
    void get_xray_render_state(rl_xray_render_state& state); // 0x004BC000 (.h:207)
    void set_xray_render_state(const rl_xray_render_state& state); // 0x007C0300 (.h:208)
    unsigned int get_distortion_effect_material(); // 0x004BC020 (.h:210)
    void set_distortion_effect_material(unsigned int mat_handle); // 0x007C0320 (.h:211)
    void set_render_category_filter(uint64_t);
    uint64_t get_render_category_filter(); // 0x004BC030 (.h:215)
    bool get_diffraction_enabled() const; // 0x004BC260 (.h:772)
    void set_diffraction_enabled(bool diffraction_enabled); // 0x007C0520 (.h:777)
    float get_opacity_distance_start() const; // 0x004BC270 (.h:783)
    void set_opacity_distance_start(float opacity_distance_start); // 0x007C0530 (.h:788)
    float get_opacity_distance_end() const; // 0x004BC280 (.h:794)
    void set_opacity_distance_end(float opacity_distance_end); // 0x007C0550 (.h:799)

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01515214
    rl_scene* prev; // 0x08
    rl_scene* next; // 0x0C
    static const int RENDERABLE_INSTANCES_MAX_DEFAULT; // 0x012BECC8
    static const int LIGHTS_MAX_DEFAULT; // 0x012BECCC
    static const int OCCLUDERS_MAX_DEFAULT; // 0x012BECD0
private:
    static int ms_renderable_instances_max; // 0x014FA454
    static int ms_lights_max;
    static int ms_occluders_max; // 0x014FA458
    bool m_initialized; // 0x10
    pool_list<rl_occluder*> m_occluder_list; // 0x14
    unsigned int m_last_visibility_pass; // 0x2C
    rl_fog_state m_fog_state; // 0x30
    rl_color_correct_state m_color_correct_state; // 0x50
    rl_hdr_state m_hdr_state; // 0x78
    rl_particle_lighting_state m_particle_lighting_state; // 0xE0
    rl_color_float m_ambient_color; // 0x10C
    rl_color_float m_back_ambient_color; // 0x11C
    rl_light* m_largest_light_default_p; // 0x12C
    rl_light* m_largest_light_p; // 0x130
    rl_visibility_manager* m_visibility_manager_p; // 0x134
    rl_scene_renderer* m_default_scene_renderer_p; // 0x138
    rl_scene_renderer* m_scene_renderer_p; // 0x13C
    rl_partition_system* m_partition_system_p; // 0x140
    rl_partition_system* m_light_partition_system_p; // 0x144
    rl_partition_system* m_low_lod_partition_system_p; // 0x148
    rl_partition_system* m_landmark_lod_partition_system_p; // 0x14C
    rl_outline_state m_outline_state; // 0x150
    float m_opacity_distance_start; // 0x180
    float m_opacity_distance_end; // 0x184
    rl_renderable_instance* m_skybox_p; // 0x188
    vector m_render_offset; // 0x18C
    unsigned int m_last_render_offset_update; // 0x198
    int m_renderable_instance_count; // 0x19C
    int m_light_count; // 0x1A0
    unsigned int m_distortion_effect_material; // 0x1A4
    float m_time_val; // 0x1A8
    float m_time_of_day_val; // 0x1AC
    uint64_t m_render_category_filter; // 0x1B0
    rl_xray_render_state m_xray_render_state; // 0x1B8
    bool m_diffraction_enabled; // 0x208
};
