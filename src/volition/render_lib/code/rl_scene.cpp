// Original: volition/render_lib/code/rl_scene.cpp
//
// rl_scene: a renderable world with its partition systems (renderable instances, lights, low LOD and landmark LOD
// entities, one each), occluders, the visibility manager, the scene renderer and the per-frame render state (fog,
// color correction, HDR, ambient colors, time). Adding and removing entities is deferred (rl_add_deferred_op) while
// render_lib is locked; otherwise visible entities are inserted into or removed from the partition system at once.
// The header-inline members of rl_scene.h are defined here as well.

#include "volition/render_lib/code/rl_scene.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_occluder.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "keen/base/memory.hpp"

#include <math.h>

// <rl_scene::release> 0x004C7480 (:11)
// <rl_scene::create> 0x00550C90 (:11)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_scene)

const int rl_scene::RENDERABLE_INSTANCES_MAX_DEFAULT = 20000; // 0x012BECC8
const int rl_scene::LIGHTS_MAX_DEFAULT = 100; // 0x012BECCC
const int rl_scene::OCCLUDERS_MAX_DEFAULT = 1000; // 0x012BECD0
int rl_scene::ms_renderable_instances_max = 20000; // 0x014FA454
int rl_scene::ms_lights_max = 100; // not in the binary (unreferenced)
int rl_scene::ms_occluders_max = 1000; // 0x014FA458

// 0x0054D5A0 (:32)
// Creates and initializes the visibility manager. The member states start at their defaults (fog, color correction,
// HDR, outline, x-ray); ambient and back ambient are 0.3 grey.
rl_scene::rl_scene()
{
    m_initialized = false;
    m_last_visibility_pass = 0;
    m_ambient_color.set(0.3f, 0.3f, 0.3f, 1.0f);
    m_back_ambient_color.set(0.3f, 0.3f, 0.3f, 1.0f);
    m_largest_light_p = nullptr;
    m_visibility_manager_p = nullptr;
    m_default_scene_renderer_p = nullptr;
    m_scene_renderer_p = nullptr;
    // Magic numbers: instances fade out between 65 and 75 m (opacity distance).
    m_opacity_distance_start = 65.0f;
    m_opacity_distance_end = 75.0f;
    m_skybox_p = nullptr;
    m_render_offset.x = 0.0f;
    m_render_offset.y = 0.0f;
    m_render_offset.z = 0.0f;
    m_last_render_offset_update = 0xFFFFFFFF;
    m_renderable_instance_count = 0;
    m_light_count = 0;
    m_distortion_effect_material = 0xFFFFFFFF;
    m_diffraction_enabled = false;
    m_visibility_manager_p = rl_visibility_manager::create();
    m_visibility_manager_p->init();
    m_render_category_filter = 0xFFFFFFFFFFFFFFFFull;
}

// 0x00517360 (:43)
// Notes (decompiled behaviour, kept as is): only the main partition system is released; the light, low LOD and
// landmark LOD partition systems are not.
rl_scene::~rl_scene()
{
    if (m_largest_light_default_p != nullptr)
    {
        m_largest_light_default_p->release();
    }
    if (m_visibility_manager_p != nullptr)
    {
        m_visibility_manager_p->release();
    }
    if (m_default_scene_renderer_p != nullptr)
    {
        m_default_scene_renderer_p->release();
    }
    if (m_partition_system_p != nullptr)
    {
        m_partition_system_p->release();
    }
}

// 0x0054D800 (:65)
// Creates the occluder list and the default scene renderer (screen size and LOD of render_lib), and the default
// largest light: a black directional light far away, which is taken out of the scene again (it is only returned by
// get_largest_light when no light is set). The scene is the active scene while its objects are created.
void rl_scene::init(keen::GraphicsCommandWriter* pCommandWriter, keen::MemoryBlock deferredCommandBufferMemory)
{
    rl_scene* previous_active_scene_p = Active_scene_p;
    Active_scene_p = this;
    m_initialized = true;
    m_occluder_list.init_dynamic(ms_occluders_max);
    m_default_scene_renderer_p = rl_scene_renderer::create();
    rl_render_lib* render_lib = rl_render_lib::get_instance();
    m_default_scene_renderer_p->init(pCommandWriter, render_lib->m_screen_info.get_screen_width(), render_lib->m_screen_info.get_screen_height(), render_lib->m_new_level_of_detail, deferredCommandBufferMemory);
    m_scene_renderer_p = m_default_scene_renderer_p;
    m_default_scene_renderer_p->m_scene_p = this;

    // Magic numbers: the default light sits at (1000, 1000, 0), points along that direction, and reaches 10000 m.
    m_largest_light_default_p = rl_light::create();
    vector light_position(1000.0f, 1000.0f, 0.0f);
    m_largest_light_default_p->set_position(light_position);
    m_largest_light_default_p->set_color(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f));
    m_largest_light_default_p->set_attenuation_start(10000.0f);
    m_largest_light_default_p->set_attenuation_end(10001.0f);
    m_largest_light_default_p->set_light_type(rl_light::LIGHT_TYPE_DIRECTIONAL);
    light_position.normalize_safe();
    m_largest_light_default_p->set_light_direction(light_position);
    const float radius = m_largest_light_default_p->get_attenuation_end();
    m_largest_light_default_p->set_local_bounding_sphere(rl_sphere(ZERO_VECTOR, radius));
    m_largest_light_default_p->set_clip_min(vector(radius * -1.0f, radius * -1.0f, radius * -1.0f));
    m_largest_light_default_p->set_clip_max(vector(radius, radius, radius));
    m_largest_light_default_p->update_partition_system();
    remove_light(m_largest_light_default_p);
    Active_scene_p = previous_active_scene_p;

    m_outline_state.layer[0] = RLOL_DISABLED;
    m_outline_state.layer[1] = RLOL_DISABLED;
    m_outline_state.layer[2] = RLOL_DISABLED;
    m_outline_state.layer[3] = RLOL_DISABLED;
    m_outline_state.colors[0].set(0.0f, 0.0f, 0.0f, 1.0f);
    m_outline_state.colors[1].set(0.0f, 0.0f, 0.0f, 1.0f);
}

// 0x004C74B0 (:119)
void rl_scene::set_scene_renderer(rl_scene_renderer* scene_renderer_p)
{
    m_scene_renderer_p = scene_renderer_p;
    scene_renderer_p->m_scene_p = this;
}

// 0x004C74C0 (:125)
// The HDR overrides, then the scene renderer (vtbl+0x0C, rl_scene_renderer::render).
void rl_scene::render(rl_renderer& renderer, rl_camera& camera, rl_renderer& deferredRenderer)
{
    m_hdr_state.update_overridden_values();
    m_scene_renderer_p->render(renderer, deferredRenderer, *this, camera, m_hdr_state, m_color_correct_state);
}

// 0x004F5B90 (:132)
// The offset moves only when it is more than 20 m off and at most every 30 frames (or when the frame counter wrapped).
void rl_scene::set_render_offset(const vector& render_offset)
{
    const unsigned int frame = rl_render_lib::get_instance()->m_current_frame_count;
    const float dx = render_offset.x - m_render_offset.x;
    const float dy = render_offset.y - m_render_offset.y;
    const float dz = render_offset.z - m_render_offset.z;
    // Magic numbers: 20 m, 30 frames.
    if (sqrtf(dy * dy + dx * dx + dz * dz) > 20.0f && (frame < m_last_render_offset_update || m_last_render_offset_update + 30 < frame))
    {
        m_render_offset = render_offset;
        m_last_render_offset_update = frame;
    }
}

// 0x004BBFD0 (.h:51)
rl_xray_render_state::rl_xray_render_state()
{
    // Magic numbers: the x-ray effect fades in from 40 m over 10 m.
    m_xray_effect_fade_start = 40.0f;
    m_xray_effect_enabled = false;
    m_use_terrain_occlusion = true;
    m_xray_effect_fade_dist = 10.0f;
    m_num_xray_outline_points = 0;
}

// 0x004BC000 (.h:207)
void rl_scene::get_xray_render_state(rl_xray_render_state& state)
{
    state = m_xray_render_state;
}

// 0x007C0300 (.h:208)
void rl_scene::set_xray_render_state(const rl_xray_render_state& state)
{
    m_xray_render_state = state;
}

// 0x004BC020 (.h:210)
unsigned int rl_scene::get_distortion_effect_material()
{
    return m_distortion_effect_material;
}

// 0x007C0320 (.h:211)
void rl_scene::set_distortion_effect_material(unsigned int mat_handle)
{
    m_distortion_effect_material = mat_handle;
}

void rl_scene::set_render_category_filter(uint64_t filter)
{
    m_render_category_filter = filter;
}

// 0x004BC030 (.h:215)
uint64_t rl_scene::get_render_category_filter()
{
    return m_render_category_filter;
}

// 0x007C0330 (.h:285)
void rl_scene::set_renderable_instances_max(int renderable_instances_max)
{
    ms_renderable_instances_max = renderable_instances_max;
}

int rl_scene::get_renderable_instances_max()
{
    return ms_renderable_instances_max;
}

// 0x00537680 (.h:295)
// Notes (decompiled behaviour, kept as is): the handle stays invalid; while locked, the scene pointer is set at once
// and the insertion is deferred.
rl_handle rl_scene::add_renderable_instance(rl_renderable_instance* renderable_instance_p)
{
    rl_handle result;
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_ADD_RI, renderable_instance_p);
        renderable_instance_p->scene_added(this);
        return result;
    }
    rl_partition_system* partition_system_p = m_partition_system_p;
    if (partition_system_p != nullptr && renderable_instance_p->get_is_visible())
    {
        partition_system_p->insert(renderable_instance_p);
        renderable_instance_p->set_partition_system(partition_system_p);
    }
    m_renderable_instance_count++;
    renderable_instance_p->scene_added(this);
    return result;
}

// 0x00537700 (.h:324)
void rl_scene::remove_renderable_instance(rl_renderable_instance* renderable_instance_p)
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_REMOVE_RI, renderable_instance_p);
        return;
    }
    rl_partition_system* partition_system_p = renderable_instance_p->get_partition_system();
    if (partition_system_p != nullptr && renderable_instance_p->get_is_visible())
    {
        partition_system_p->remove(renderable_instance_p);
        renderable_instance_p->set_partition_system(nullptr);
    }
    m_renderable_instance_count--;
    renderable_instance_p->scene_removed();
}

// 0x00537770 (.h:355)
rl_handle rl_scene::add_light(rl_light* light_p)
{
    rl_handle result;
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_ADD_LIGHT, light_p);
        return result;
    }
    rl_partition_system* partition_system_p = m_light_partition_system_p;
    if (partition_system_p != nullptr && light_p->get_is_visible())
    {
        partition_system_p->insert(light_p);
        light_p->set_partition_system(partition_system_p);
    }
    m_light_count++;
    return result;
}

// 0x005377E0 (.h:380)
// Notes (decompiled behaviour, kept as is): a hidden light loses its partition system pointer without being removed
// (hidden entities are not in the partition system).
void rl_scene::remove_light(rl_light* light_p)
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_REMOVE_LIGHT, light_p);
        return;
    }
    rl_partition_system* partition_system_p = light_p->get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (light_p->get_is_visible())
        {
            partition_system_p->remove(light_p);
        }
        light_p->set_partition_system(nullptr);
    }
    m_light_count--;
}

// 0x00537840 (.h:404)
rl_handle rl_scene::add_low_lod(rl_scene_entity* entity_p)
{
    rl_handle result;
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_ADD_LOW_LOD, entity_p);
        return result;
    }
    rl_partition_system* partition_system_p = m_low_lod_partition_system_p;
    if (partition_system_p != nullptr && entity_p->get_is_visible())
    {
        partition_system_p->insert(entity_p);
        entity_p->set_partition_system(partition_system_p);
    }
    return result;
}

// 0x005378B0 (.h:427)
void rl_scene::remove_low_lod(rl_scene_entity* entity_p)
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_REMOVE_LOW_LOD, entity_p);
        return;
    }
    rl_partition_system* partition_system_p = entity_p->get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (entity_p->get_is_visible())
        {
            partition_system_p->remove(entity_p);
        }
        entity_p->set_partition_system(nullptr);
    }
}

// 0x00537900 (.h:444)
rl_handle rl_scene::add_landmark_lod(rl_scene_entity* entity_p)
{
    rl_handle result;
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_ADD_LANDMARK_LOD, entity_p);
        return result;
    }
    rl_partition_system* partition_system_p = m_landmark_lod_partition_system_p;
    if (partition_system_p != nullptr && entity_p->get_is_visible())
    {
        partition_system_p->insert(entity_p);
        entity_p->set_partition_system(partition_system_p);
    }
    return result;
}

// 0x00537970 (.h:467)
void rl_scene::remove_landmark_lod(rl_scene_entity* entity_p)
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_SCENE_REMOVE_LANDMARK_LOD, entity_p);
        return;
    }
    rl_partition_system* partition_system_p = entity_p->get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (entity_p->get_is_visible())
        {
            partition_system_p->remove(entity_p);
        }
        entity_p->set_partition_system(nullptr);
    }
}

// 0x00509F50 (.h:484)
// The handle is the index in the occluder list (invalid when the list is full).
rl_handle rl_scene::add_occluder(rl_occluder* occluder_p)
{
    rl_handle result;
    const int index = m_occluder_list.add(occluder_p);
    if (index >= 0)
    {
        result.set(index);
    }
    return result;
}

// 0x004E2D50 (.h:493)
// The last occluder moves into the removed one's slot and takes its handle.
void rl_scene::remove_occluder(rl_occluder* occluder_p)
{
    const unsigned int index = occluder_p->m_handle.get();
    m_occluder_list.remove_unsorted(index);
    // Notes (decompiled behaviour, kept as is): when the removed occluder was the last one, this writes its handle
    // through the slot past the end.
    (&m_occluder_list[0])[index]->m_handle.set(index);
    occluder_p->scene_removed();
}

// 0x004BC040 (.h:502)
const pool_list<rl_occluder*>& rl_scene::get_occluder_list() const
{
    return m_occluder_list;
}

// 0x004BC050 (.h:510)
unsigned int rl_scene::get_last_visibility_pass() const
{
    return m_last_visibility_pass;
}

// 0x004BC060 (.h:515)
void rl_scene::set_last_visibility_pass(unsigned int last_visibility_pass)
{
    m_last_visibility_pass = last_visibility_pass;
}

// 0x004BC070 (.h:521)
// Inserts a shown entity into its partition system and removes a hidden one. Notes (decompiled behaviour, kept as
// is): the entity's own visibility flag is not changed (the caller does that).
void rl_scene::set_is_visible(rl_scene_entity* scene_entity_p, bool is_visible)
{
    rl_partition_system* partition_system_p = scene_entity_p->get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (is_visible)
        {
            if (!scene_entity_p->get_is_visible())
            {
                partition_system_p->insert(scene_entity_p);
            }
        }
        else if (scene_entity_p->get_is_visible())
        {
            partition_system_p->remove(scene_entity_p);
        }
    }
}

// 0x004BC0B0 (.h:557)
void rl_scene::set_fog_state(const rl_fog_state& fog_state)
{
    m_fog_state = fog_state;
}

// 0x004BC0E0 (.h:562)
const rl_fog_state& rl_scene::get_fog_state() const
{
    return m_fog_state;
}

// 0x00893130 (.h:568)
const rl_color_correct_state& rl_scene::get_color_correct_state() const
{
    return m_color_correct_state;
}

// 0x007C0340 (.h:573)
void rl_scene::set_color_correct_state(const rl_color_correct_state& color_correct_state)
{
    m_color_correct_state = color_correct_state;
}

const rl_hdr_state& rl_scene::get_hdr_state() const
{
    return m_hdr_state;
}

// 0x007C0380 (.h:584)
void rl_scene::set_hdr_state(const rl_hdr_state& hdr_state)
{
    m_hdr_state = hdr_state;
}

// 0x004BC0F0 (.h:590)
const rl_particle_lighting_state& rl_scene::get_particle_lighting_state() const
{
    return m_particle_lighting_state;
}

// 0x007C03A0 (.h:595)
void rl_scene::set_particle_lighting_state(const rl_particle_lighting_state& particle_lighting_state)
{
    m_particle_lighting_state = particle_lighting_state;
}

// 0x004BC100 (.h:601)
const rl_color_float& rl_scene::get_ambient_color() const
{
    return m_ambient_color;
}

// 0x007C03F0 (.h:606)
void rl_scene::set_ambient_color(const rl_color_float& ambient_color)
{
    m_ambient_color = ambient_color;
}

// 0x004BC110 (.h:611)
const rl_color_float& rl_scene::get_back_ambient_color() const
{
    return m_back_ambient_color;
}

// 0x007C0410 (.h:616)
void rl_scene::set_back_ambient_color(const rl_color_float& back_ambient_color)
{
    m_back_ambient_color = back_ambient_color;
}

// 0x004BC120 (.h:621)
float rl_scene::get_time_value() const
{
    return m_time_val;
}

// 0x007C0430 (.h:626)
void rl_scene::set_time_value(float time_val)
{
    m_time_val = time_val;
}

// 0x004BC130 (.h:631)
float rl_scene::get_time_of_day_value() const
{
    return m_time_of_day_val;
}

// 0x007C0450 (.h:636)
// Clamped to [0, 1].
void rl_scene::set_time_of_day_value(float tod_val)
{
    if (tod_val - 0.0f < 0.0f)
    {
        tod_val = 0.0f;
    }
    if (tod_val - 1.0f >= 0.0f)
    {
        tod_val = 1.0f;
    }
    m_time_of_day_val = tod_val;
}

// 0x004BC140 (.h:643)
rl_light* rl_scene::get_largest_light() const
{
    return m_largest_light_p != nullptr ? m_largest_light_p : m_largest_light_default_p;
}

// 0x007C04A0 (.h:652)
// Takes a visible light out of the light partition system (the largest light is not a secondary light).
void rl_scene::set_largest_light(rl_light* largest_light_p)
{
    m_largest_light_p = largest_light_p;
    if (largest_light_p != nullptr && largest_light_p->get_is_visible())
    {
        largest_light_p->visible_update(false);
    }
}

// 0x004BC160 (.h:662)
rl_visibility_manager* rl_scene::get_visibility_manager() const
{
    return m_visibility_manager_p;
}

void rl_scene::set_visibility_manager(rl_visibility_manager* visibility_manager_p)
{
    m_visibility_manager_p = visibility_manager_p;
}

// 0x004BC170 (.h:674)
rl_scene_renderer* rl_scene::get_scene_renderer() const
{
    return m_scene_renderer_p;
}

// 0x004BC180 (.h:680)
rl_partition_system* rl_scene::get_partition_system() const
{
    return m_partition_system_p;
}

// 0x007C04D0 (.h:685)
void rl_scene::set_partition_system(rl_partition_system* partition_system_p)
{
    m_partition_system_p = partition_system_p;
}

// 0x004BC190 (.h:690)
rl_partition_system* rl_scene::get_light_partition_system() const
{
    return m_light_partition_system_p;
}

// 0x007C04E0 (.h:695)
void rl_scene::set_light_partition_system(rl_partition_system* partition_system)
{
    m_light_partition_system_p = partition_system;
}

// 0x004BC1A0 (.h:700)
rl_partition_system* rl_scene::get_low_lod_partition_system() const
{
    return m_low_lod_partition_system_p;
}

// 0x004BC1B0 (.h:705)
rl_partition_system* rl_scene::get_landmark_lod_partition_system() const
{
    return m_landmark_lod_partition_system_p;
}

// 0x007C04F0 (.h:710)
void rl_scene::set_low_lod_partition_system(rl_partition_system* partition_system)
{
    m_low_lod_partition_system_p = partition_system;
}

// 0x007C0500 (.h:715)
void rl_scene::set_landmark_lod_partition_system(rl_partition_system* partition_system)
{
    m_landmark_lod_partition_system_p = partition_system;
}

// 0x004BC1C0 (.h:720)
// Returns the frame of the last offset change.
unsigned int rl_scene::get_render_offset(vector& render_offset)
{
    render_offset = m_render_offset;
    return m_last_render_offset_update;
}

int rl_scene::get_renderable_instance_count() const
{
    return m_renderable_instance_count;
}

int rl_scene::get_light_count() const
{
    return m_light_count;
}

// 0x007C0510 (.h:738)
void rl_scene::set_skybox(rl_renderable_instance* renderable_instance_p)
{
    m_skybox_p = renderable_instance_p;
}

// 0x004BC1F0 (.h:743)
rl_renderable_instance* rl_scene::get_skybox() const
{
    return m_skybox_p;
}

// 0x0094BDF0 (.h:761)
void rl_scene::set_outline_state(const rl_outline_state& state)
{
    m_outline_state = state;
}

// 0x004BC200 (.h:766)
void rl_scene::get_outline_state(rl_outline_state& state)
{
    state = m_outline_state;
}

// 0x004BC260 (.h:772)
bool rl_scene::get_diffraction_enabled() const
{
    return m_diffraction_enabled;
}

// 0x007C0520 (.h:777)
void rl_scene::set_diffraction_enabled(bool diffraction_enabled)
{
    m_diffraction_enabled = diffraction_enabled;
}

// 0x004BC270 (.h:783)
float rl_scene::get_opacity_distance_start() const
{
    return m_opacity_distance_start;
}

// 0x007C0530 (.h:788)
void rl_scene::set_opacity_distance_start(float opacity_distance_start)
{
    m_opacity_distance_start = opacity_distance_start;
}

// 0x004BC280 (.h:794)
float rl_scene::get_opacity_distance_end() const
{
    return m_opacity_distance_end;
}

// 0x007C0550 (.h:799)
void rl_scene::set_opacity_distance_end(float opacity_distance_end)
{
    m_opacity_distance_end = opacity_distance_end;
}
