// Original: volition/vfxlib/code/vfx_effect.cpp
//
// An effect instance (VFX.md FX1): created from a template out of the effect manager's free list, it builds one emitter
// instance per top-level emitter (with a renderable particles instance in its group), one mesh instance per template
// mesh, the filter and renderable filter (mesh debris) instances, dynamic lights and coronas. update() runs the kill
// list, expressions, filters and emitters; update_render() pushes the result to the renderer (group bounds, parametric
// constants, mesh transforms, rigid bodies, lights and coronas). The functions of vfx_effect.h are here too (the
// original has them inline in the header); set_variable_value is in vfx_effect_expressions.cpp.

#include "volition/vfxlib/code/vfx_effect.h"

#include "volition/render_lib/code/rl_corona_instance.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/render_lib/code/rl_renderable_particles_instance_group.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_effect_manager.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"
#include "volition/vlib/code/math/quaternion.h"

#include <new>

// ---- globals
const int vfx_effect::HANDLE_INVALID = -1; // 0x012C7080

// Coronas are created for new effects (initialized data, 1 in rfg.exe; nothing in rfg.exe writes it).
bool Coronas = true; // 0x016F0A01
// Nudges each effect's group translation by its own epsilon so equal-depth groups sort stably (0 in rfg.exe, never set).
bool Vfx_sort_hack = false; // 0x03A23C88
// The per-effect sort epsilon: each new effect gets Eps_start + Eps_delta, wrapping to 0.0001 past 0.02.
float Eps_delta = 0.001f; // 0x016F1940
float Eps_start = 0.0001f; // 0x016F1944

// Magic numbers of vfx_effect.
static const float VFX_EFFECT_EPS_MAX = 0.02f; // Eps_start wraps past this
static const float VFX_EFFECT_EPS_RESET = 0.0001f; // to this
static const float VFX_EFFECT_WEIGHT_EPSILON = 0.0001f; // a weight within this of zero is "no wind"
static const float VFX_EFFECT_LIGHT_MIN_INTENSITY = 0.001f; // dimmer lights are hidden
static const float VFX_EFFECT_EMIT_NODE_FIRST_DELTA = 0.0001f; // first update of a spawned emit-node emitter
static const float VFX_EFFECT_ROD_DOT_LIMIT = 0.9999f; // a rod mesh facing the camera along its axis keeps its rotation
// The shader treats m_is_local_space > 1 as local space (1.5 is what rfg.exe stores).
static const float VFX_EFFECT_LOCAL_SPACE_TRUE = 1.5f;
static const int VFX_EFFECT_PRIORITY_MAX = 0xFFFF; // the first top-level emitter's render priority
static const int VFX_EFFECT_PRIORITY_STEP = 0xFF; // each next top-level emitter's is this much lower (mod 0x10000)
static const unsigned char VFX_EFFECT_CATEGORY_MESH = 4; // rl_scene_entity category of effect meshes
static const unsigned char VFX_EFFECT_CATEGORY_PARTICLES = 5; // and of the particles group
// update_render's corona keys and the shape handle of a mesh without collision use this.
static const unsigned int VFX_EFFECT_INVALID_HANDLE = 0xFFFFFFFF;

// The mesh instance at index (in creation order, which is the template's mesh order), or null.
static rl_renderable_instance* vfx_effect_get_mesh_instance(const linked_list_p<rl_renderable_instance>& mesh_insts, int index)
{
    rl_renderable_instance* instance_p = mesh_insts.first();
    int i = -1;
    while (instance_p != nullptr)
    {
        i++;
        if (index == i)
        {
            return instance_p;
        }
        instance_p = instance_p->next;
        if (instance_p == mesh_insts.first())
        {
            return nullptr;
        }
    }
    return nullptr;
}

// The emit-node list (emit_node_prev/emit_node_next, circular, no count): append and remove (inlined in rfg.exe).
static void vfx_effect_emit_node_append(vfx_particle_system_emitter_instance*& head_p, vfx_particle_system_emitter_instance* instance_p)
{
    if (head_p == nullptr)
    {
        instance_p->emit_node_next = instance_p;
        instance_p->emit_node_prev = instance_p;
        head_p = instance_p;
        return;
    }
    instance_p->emit_node_prev = head_p->emit_node_prev;
    instance_p->emit_node_next = head_p;
    head_p->emit_node_prev->emit_node_next = instance_p;
    head_p->emit_node_prev = instance_p;
}

static void vfx_effect_emit_node_remove(vfx_particle_system_emitter_instance*& head_p, vfx_particle_system_emitter_instance* instance_p)
{
    if (head_p == instance_p)
    {
        if (instance_p->emit_node_next == instance_p)
        {
            instance_p->emit_node_next = nullptr;
            instance_p->emit_node_prev = nullptr;
            head_p = nullptr;
            return;
        }
        head_p = instance_p->emit_node_next;
    }
    instance_p->emit_node_prev->emit_node_next = instance_p->emit_node_next;
    instance_p->emit_node_next->emit_node_prev = instance_p->emit_node_prev;
    instance_p->emit_node_next = nullptr;
    instance_p->emit_node_prev = nullptr;
}

// The parent transform of an emitter or filter: its parent mesh's world transform, else the effect's.
static void vfx_effect_get_parent_transform(const vfx_effect* effect_p, const rl_renderable_instance* parent_p, matrix& rotation, vector& position)
{
    if (parent_p == nullptr)
    {
        rotation = effect_p->get_rotation();
        position = effect_p->get_position();
    }
    else
    {
        rotation = parent_p->get_rotation();
        position = parent_p->get_position();
    }
}

// 0x00CA3B50 (:53): each new effect takes the next sort epsilon.
vfx_effect::vfx_effect()
    : m_rotation(IDENTITY_MATRIX)
{
    m_instance_handle = -1;
    m_elapsed_time = 0.0f;
    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
    alpha_fade = 1.0f;
    m_is_particle_system_active = false;
    num_particle_systems = 0;
    num_particles = 0;
    m_effect_template_p = nullptr;
    m_render_flags = RENDER_FLAGS_ALL;
    use_alpha_fade = true;
    m_is_visible = true;
    m_effect_material_tint.red = 0.0f;
    m_effect_material_tint.green = 0.0f;
    m_effect_material_tint.blue = 0.0f;
    m_effect_material_tint.alpha = 0.0f;
    m_renderable_particles_instance_group_p = nullptr;
    Eps_start = Eps_delta + Eps_start;
    if (VFX_EFFECT_EPS_MAX < Eps_start)
    {
        Eps_start = VFX_EFFECT_EPS_RESET;
    }
    m_eps = Eps_start;
}

// 0x00CA3C60 (:64)
vfx_effect::~vfx_effect()
{
}

// 0x00CBA670 (:69): placement-constructs an effect from the manager's free list. Original bug kept: with the free list
// empty (512 effects) the null effect is dereferenced.
vfx_effect* vfx_effect::create(vfx_effect_template* template_p)
{
    vfx_effect_manager* manager_p = vfx_system::get_effect_manager();
    vfx_effect* effect_p = manager_p->get_new_effect();
    if (effect_p != nullptr)
    {
        effect_p = new (effect_p) vfx_effect();
    }
    effect_p->m_instance_handle = manager_p->register_effect(effect_p);
    effect_p->m_effect_template_p = template_p;
    effect_p->init();
    return effect_p;
}

// 0x00CB9920 (:86): an emitter instance in the effect's list. An iterative emitter with a wind weight also gets the
// global wind filter. Emit-node emitters (render type NODES) spawn child emitters through the callbacks instead of
// rendering; the others get a renderable particles instance in the group (null is returned, after the emitter
// instance was added, when the emitter has no renderable).
vfx_particle_system_emitter_instance* vfx_effect::create_emitter(vfx_particle_system_emitter* emitter_p, int render_priority)
{
    // Its own counter in rfg.exe, incremented and never read.
    static int priority_seed = 0; // 0x03A25508

    if (vfx_particle_system_emitter_instance::get_objects_free() == 0)
    {
        return nullptr;
    }
    vfx_particle_system_emitter_instance* instance_p = vfx_particle_system_emitter_instance::create();
    if (instance_p == nullptr)
    {
        return nullptr;
    }
    instance_p->init(emitter_p);
    if (emitter_p->is_iterative)
    {
        const float weight_start = emitter_p->m_weight_start - 0.0f;
        const float weight_end = emitter_p->m_weight_end - 0.0f;
        if (VFX_EFFECT_WEIGHT_EPSILON <= weight_start || VFX_EFFECT_WEIGHT_EPSILON <= -weight_start || VFX_EFFECT_WEIGHT_EPSILON <= weight_end || VFX_EFFECT_WEIGHT_EPSILON <= -weight_end)
        {
            instance_p->add_filter(vfx_system::get_global_wind_instance());
        }
    }
    m_emitters.append(instance_p);
    num_particle_systems++;
    if (emitter_p->m_particle_type == rl_particle_base::RENDER_TYPE_NODES)
    {
        instance_p->m_particle_create_cb = emit_node_emitters_spawn_cb;
        instance_p->m_particle_create_cb_data = this;
        instance_p->m_post_update_cb = emit_node_emitters_update_cb;
        instance_p->m_update_cb_data_p = this;
        instance_p->m_renderable_particles_instance_p = nullptr;
    }
    else
    {
        rl_renderable_particles_instance* renderable_instance_p = rl_renderable_particles_instance::create();
        instance_p->m_renderable_particles_instance_p = renderable_instance_p;
        if (renderable_instance_p != nullptr)
        {
            if (emitter_p->m_renderable_p == nullptr)
            {
                renderable_instance_p->release();
                return nullptr;
            }
            renderable_instance_p->m_sort_priority = (unsigned short)render_priority;
            renderable_instance_p->set_renderable_particles(emitter_p->m_renderable_p);
            priority_seed++;
            m_renderable_particles_instance_group_p->add_instance(renderable_instance_p);
        }
    }
    instance_p->m_sort_priority = (unsigned short)render_priority;
    return instance_p;
}

// 0x00CB9AC0 (:150)
void vfx_effect::init()
{
    // The group sort priority of the next effect (wraps with the unsigned short).
    static int group_sort_priority = 0; // 0x03A2550C

    vfx_effect_template::vfx_data* data_p = m_effect_template_p->m_vfx_data_p;

    m_emitters.init();
    m_renderable_particles_instance_group_p = rl_renderable_particles_instance_group::create();
    m_renderable_particles_instance_group_p->set_sort_priority((unsigned short)group_sort_priority);
    m_renderable_particles_instance_group_p->set_sort_method(rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT);
    group_sort_priority++;
    m_renderable_particles_instance_group_p->set_category_id(VFX_EFFECT_CATEGORY_PARTICLES);
    m_renderable_particles_instance_group_p->set_flag(rl_scene_entity::FLAGS_NOT_GPU_OCCLUDED, true);

    // Top-level emitters, each one priority step below the previous; emit-node emitters are spawned per particle.
    unsigned int priority = VFX_EFFECT_PRIORITY_MAX;
    for (int i = 0; i < data_p->num_emitters; i++)
    {
        vfx_effect_template::emitter& emitter_r = data_p->emitters[i];
        vfx_particle_system_emitter* emitter_p = emitter_r.particle_system_emitter_p.to_pointer();
        if (!emitter_r.is_emit_node && emitter_p != nullptr)
        {
            create_emitter(emitter_p, priority & 0xFFFF);
        }
        priority += 0x10000 - VFX_EFFECT_PRIORITY_STEP;
    }
    m_emitter_kill_list = nullptr;

    // One mesh instance per template mesh; additive meshes go to the additive pass, transparent ones sort back to front.
    m_renderable_mesh_insts.init();
    rl_renderable* renderable_p = m_effect_template_p->m_mesh_renderables.first();
    int mesh_index = 0;
    while (renderable_p != nullptr)
    {
        rl_mesh_instance* mesh_instance_p = rl_mesh_instance::create();
        if (mesh_instance_p != nullptr)
        {
            rl_material_map* material_map_p = data_p->meshes[mesh_index].material_map_p.to_pointer();
            mesh_instance_p->init(static_cast<rl_mesh*>(renderable_p), material_map_p, 0);
            mesh_instance_p->calculate_local_aabb();
            mesh_instance_p->set_category_id(VFX_EFFECT_CATEGORY_MESH);
            const bool is_additive = (data_p->meshes[mesh_index].type & vfx_effect_template::MESH_TYPE_ADDITIVE) != 0;
            mesh_instance_p->set_is_additive(is_additive);
            mesh_instance_p->set_sort_method(is_additive ? rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT : rl_renderable_instance::SORT_METHOD_NONE);
            for (int material_index = 0; material_index < material_map_p->get_num_materials(); material_index++)
            {
                if (rl_material_library_material_is_transparent(material_map_p->get_material(material_index)))
                {
                    mesh_instance_p->set_sort_method(rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT);
                    break;
                }
            }
            m_renderable_mesh_insts.append(mesh_instance_p);
        }
        renderable_p = renderable_p->next;
        mesh_index++;
        if (renderable_p == m_effect_template_p->m_mesh_renderables.first())
        {
            break;
        }
    }

    // Filters with targets: one filter instance for the affected emitters (added to their interactors) and one
    // renderable filter instance per affected mesh. Global filters (flags 0) are not instanced here.
    linked_list_p<vfx_renderable_filter_instance> pending_mesh_filters;
    for (int filter_index = 0; filter_index < data_p->num_filters; filter_index++)
    {
        vfx_effect_template::filter& filter_r = data_p->filters[filter_index];
        vfx_particle_system_filter* filter_p = filter_r.particle_system_filter_p.to_pointer();
        if (filter_r.flags == vfx_effect_template::FILTER_FLAGS_GLOBAL)
        {
            continue;
        }
        const int* affected_meshes = filter_r.affected_meshes.to_pointer();
        const int* affected_emitters = filter_r.affected_emitters.to_pointer();
        const int num_affected_emitters = filter_r.num_affected_emitters;
        const int num_affected_meshes = filter_r.num_affected_meshes;
        if (0 < num_affected_emitters)
        {
            vfx_particle_system_filter_instance* filter_instance_p = vfx_particle_system_filter_instance::create();
            if (filter_instance_p != nullptr)
            {
                filter_instance_p->m_filter_p = filter_p;
                if (filter_index < data_p->num_filters && data_p->filters[filter_index].parent_index != -1)
                {
                    const int parent_index = filter_index < data_p->num_filters ? data_p->filters[filter_index].parent_index : -1;
                    filter_instance_p->m_parent_renderable_instance_p = vfx_effect_get_mesh_instance(m_renderable_mesh_insts, parent_index);
                }
                m_filters.append(filter_instance_p);
            }
            // Original bug kept: with the filter instance pool exhausted, the null instance is dereferenced here once
            // an affected emitter is found.
            for (int i = 0; i < num_affected_emitters; i++)
            {
                vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
                if (instance_p == nullptr)
                {
                    continue;
                }
                vfx_particle_system_emitter* target_p = data_p->emitters[affected_emitters[i]].particle_system_emitter_p.to_pointer();
                while (instance_p->m_emitter_p != target_p)
                {
                    instance_p = instance_p->next;
                    if (instance_p == m_emitters.first() || instance_p == nullptr)
                    {
                        instance_p = nullptr;
                        break;
                    }
                }
                if (instance_p != nullptr)
                {
                    instance_p->add_filter(filter_instance_p);
                }
            }
        }
        for (int i = 0; i < num_affected_meshes; i++)
        {
            vfx_renderable_filter_instance* mesh_filter_p = vfx_renderable_filter_instance::create();
            rl_renderable_instance* mesh_instance_p = vfx_effect_get_mesh_instance(m_renderable_mesh_insts, affected_meshes[i]);
            if (mesh_filter_p == nullptr)
            {
                continue;
            }
            pending_mesh_filters.append(mesh_filter_p);
            if (filter_index < data_p->num_filters && data_p->filters[filter_index].parent_index != -1)
            {
                const int parent_index = filter_index < data_p->num_filters ? data_p->filters[filter_index].parent_index : -1;
                mesh_filter_p->m_parent_renderable_instance_p = vfx_effect_get_mesh_instance(m_renderable_mesh_insts, parent_index);
            }
            mesh_filter_p->m_renderable_instance_p = mesh_instance_p;
            // Original bug kept: the effect index (the mesh whose shape update_render registers) is the index in the
            // affected-mesh list, not the mesh index affected_meshes[i].
            mesh_filter_p->m_effect_index = i;
            mesh_filter_p->m_position_keys_p = filter_r.position_keys.to_pointer();
            mesh_filter_p->m_num_position_keys = filter_r.num_position_keys;
            mesh_filter_p->m_rotation_keys_p = filter_r.rotation_keys.to_pointer();
            mesh_filter_p->m_local_rotation_cached_dirty = true;
            mesh_filter_p->m_num_rotation_keys = filter_r.num_rotation_keys;
            mesh_filter_p->m_force_keys_p = m_effect_template_p->get_filter_force_keys(filter_index);
            mesh_filter_p->m_num_force_keys = m_effect_template_p->get_filter_num_force_keys(filter_index);
            mesh_filter_p->m_noise_keys_p = m_effect_template_p->get_filter_noise_keys(filter_index);
            mesh_filter_p->m_num_noise_keys = m_effect_template_p->get_filter_num_noise_keys(filter_index);
            mesh_filter_p->m_angular_velocity_keys_p = m_effect_template_p->get_filter_angular_velocity_keys(filter_index);
            mesh_filter_p->m_num_angular_velocity_keys = m_effect_template_p->get_filter_num_angular_velocity_keys(filter_index);
            mesh_filter_p->m_angular_velocity_variance_keys_p = m_effect_template_p->get_filter_angular_velocity_variance_keys(filter_index);
            mesh_filter_p->m_num_angular_velocity_variance_keys = m_effect_template_p->get_filter_num_angular_velocity_variance_keys(filter_index);
            mesh_filter_p->m_animation_duration = data_p->duration;
        }
    }

    // Each mesh keeps the first renderable filter instance that targets it; the rest go back to the pool.
    rl_renderable_instance* mesh_instance_p = m_renderable_mesh_insts.first();
    while (mesh_instance_p != nullptr)
    {
        vfx_renderable_filter_instance* mesh_filter_p = pending_mesh_filters.first();
        while (mesh_filter_p != nullptr)
        {
            vfx_renderable_filter_instance* next_p = mesh_filter_p->next != pending_mesh_filters.first() ? mesh_filter_p->next : nullptr;
            if (mesh_filter_p->m_renderable_instance_p == mesh_instance_p)
            {
                pending_mesh_filters.remove(mesh_filter_p);
                m_mesh_filters.append(mesh_filter_p);
                break;
            }
            mesh_filter_p = next_p;
        }
        mesh_instance_p = mesh_instance_p->next;
        if (mesh_instance_p == m_renderable_mesh_insts.first())
        {
            break;
        }
    }
    vfx_renderable_filter_instance* unused_p = pending_mesh_filters.first();
    while (unused_p != nullptr)
    {
        vfx_renderable_filter_instance* next_p = unused_p->next;
        unused_p->release();
        unused_p = next_p;
        if (unused_p == pending_mesh_filters.first())
        {
            break;
        }
    }

    // Emitter parents. Original quirk kept: the n-th emitter instance takes the n-th template emitter's parent, which
    // is off once an emit-node emitter (not instanced above) precedes it.
    vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
    int emitter_index = -1;
    while (instance_p != nullptr)
    {
        emitter_index++;
        if (emitter_index < data_p->num_emitters && data_p->emitters[emitter_index].parent_index != -1)
        {
            const int parent_index = emitter_index < data_p->num_emitters ? data_p->emitters[emitter_index].parent_index : -1;
            instance_p->m_parent_renderable_instance_p = vfx_effect_get_mesh_instance(m_renderable_mesh_insts, parent_index);
        }
        instance_p = instance_p->next;
        if (instance_p == m_emitters.first())
        {
            break;
        }
    }

    for (int i = 0; i < data_p->num_lights; i++)
    {
        rl_light* light_p = rl_light::create();
        if (light_p != nullptr)
        {
            m_lights.append(light_p);
        }
    }

    if (Coronas)
    {
        for (int i = 0; i < data_p->num_coronas; i++)
        {
            rl_corona_instance* corona_p = rl_corona_instance::create();
            if (corona_p == nullptr)
            {
                return;
            }
            // The first texture of the corona material (no texture is a null dereference in rfg.exe as well).
            corona_p->m_texture_handle = data_p->coronas[i].material.textures.to_pointer()->tex_ref;
            m_renderable_particles_instance_group_p->add_instance(corona_p);
        }
    }
}

// 0x00CA3C70 (:375): the emitter instance and its renderable go back to their pools.
void vfx_effect::release_emitter(vfx_particle_system_emitter_instance* emitter_instance_p)
{
    if (m_emitters.first() == nullptr)
    {
        return;
    }
    m_emitters.remove(emitter_instance_p);
    rl_renderable_particles_instance* renderable_instance_p = emitter_instance_p->m_renderable_particles_instance_p;
    emitter_instance_p->release();
    if (renderable_instance_p != nullptr)
    {
        m_renderable_particles_instance_group_p->remove_instance(renderable_instance_p);
        renderable_instance_p->release();
    }
    num_particle_systems--;
}

// 0x00C9EE20 (:390): stops emission; update() releases the emitter once its particles are gone.
void vfx_effect::kill_emitter(vfx_particle_system_emitter_instance* emitter_instance_p)
{
    emitter_instance_p->emission_enabled = false;
    vfx_effect_emit_node_append(m_emitter_kill_list, emitter_instance_p);
}

// 0x00CB1BC0 (:396): everything goes back to its pool and the effect to the manager's free list. Original quirk kept:
// the renderable filter instances are released in place (their list is left as is, the next create reinitializes it).
void vfx_effect::release()
{
    vfx_system::get_effect_manager()->unregister_effect(this);

    vfx_particle_system_emitter_instance* emitter_instance_p = m_emitters.first();
    while (emitter_instance_p != nullptr)
    {
        vfx_particle_system_emitter_instance* next_p = emitter_instance_p->next != m_emitters.first() ? emitter_instance_p->next : nullptr;
        release_emitter(emitter_instance_p);
        emitter_instance_p = next_p;
    }
    m_renderable_particles_instance_group_p->release();

    vfx_particle_system_filter_instance* filter_instance_p = m_filters.first();
    while (filter_instance_p != nullptr)
    {
        vfx_particle_system_filter_instance* next_p = filter_instance_p->next != m_filters.first() ? filter_instance_p->next : nullptr;
        m_filters.remove(filter_instance_p);
        filter_instance_p->release();
        filter_instance_p = next_p;
    }

    vfx_renderable_filter_instance* mesh_filter_p = m_mesh_filters.first();
    while (mesh_filter_p != nullptr)
    {
        vfx_system::get_rigid_body_destroy_cb()(mesh_filter_p->m_rigid_body_handle);
        mesh_filter_p->release();
        mesh_filter_p = mesh_filter_p->next;
        if (mesh_filter_p == m_mesh_filters.first())
        {
            break;
        }
    }

    rl_renderable_instance* mesh_instance_p = m_renderable_mesh_insts.first();
    while (mesh_instance_p != nullptr)
    {
        rl_renderable_instance* next_p = mesh_instance_p->next != m_renderable_mesh_insts.first() ? mesh_instance_p->next : nullptr;
        m_renderable_mesh_insts.remove(mesh_instance_p);
        mesh_instance_p->release();
        mesh_instance_p = next_p;
    }

    kill_lights();

    // Original quirk kept: the group was released above; its corona list is still walked (the pool keeps the memory).
    rl_corona_instance* corona_p = m_renderable_particles_instance_group_p->m_coronas.first();
    while (corona_p != nullptr)
    {
        rl_corona_instance* next_p = corona_p->next;
        rl_corona_instance* head_p = m_renderable_particles_instance_group_p->m_coronas.first();
        m_renderable_particles_instance_group_p->remove_instance(corona_p);
        corona_p->release();
        corona_p = next_p != head_p ? next_p : nullptr;
    }
}

// 0x00CA3CC0 (:450)
void vfx_effect::kill_lights()
{
    rl_light* light_p = m_lights.first();
    while (light_p != nullptr)
    {
        rl_light* next_p = light_p->next != m_lights.first() ? light_p->next : nullptr;
        m_lights.remove(light_p);
        light_p->release();
        light_p = next_p;
    }
}

// 0x00CB1E20 (:461): each expression runs on the instance of its emitter (stopping at the first one without an
// instance or a registered function) and marks its parametric constants for update.
void vfx_effect::update_expressions()
{
    vfx_effect_template::vfx_data* data_p = m_effect_template_p->m_vfx_data_p;
    const int num_expressions = data_p->num_expressions;
    for (int i = 0; i < num_expressions; i++)
    {
        const vfx_effect_template::expression& expression_r = m_effect_template_p->m_vfx_data_p->expressions[i];
        vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
        if (instance_p == nullptr)
        {
            return;
        }
        vfx_particle_system_emitter* target_p = m_effect_template_p->m_vfx_data_p->emitters[expression_r.object_index].particle_system_emitter_p.to_pointer();
        while (target_p != instance_p->m_emitter_p)
        {
            instance_p = instance_p->next;
            if (instance_p == m_emitters.first() || instance_p == nullptr)
            {
                return;
            }
        }
        vfx_effect_template::expression_function function_p = vfx_effect_template::get_expression_function(expression_r.m_expression_function_name_crc);
        if (function_p == nullptr)
        {
            return;
        }
        function_p(instance_p, expression_r.m_params.to_pointer(), expression_r.m_num_expression_params);
        instance_p->m_update_parametric = true;
    }
}

// 0x00CB8210 (:485): killed emitters whose particles are all gone are released; then expressions, filters (moved to
// their parent) and emitters (top-level ones moved to their parent) update.
void vfx_effect::update(float delta_time)
{
    vfx_particle_system_emitter_instance* kill_p = m_emitter_kill_list;
    while (kill_p != nullptr)
    {
        vfx_particle_system_emitter_instance* next_p = kill_p->emit_node_next != m_emitter_kill_list ? kill_p->emit_node_next : nullptr;
        const bool has_particles = kill_p->m_emitter_p->is_iterative ? 0 < kill_p->m_particle_iterative_list.get_elem_count() : 0 < kill_p->m_particle_parametric_list.get_elem_count();
        if (!has_particles)
        {
            vfx_effect_emit_node_remove(m_emitter_kill_list, kill_p);
            release_emitter(kill_p);
        }
        kill_p = next_p;
    }

    update_expressions();

    vfx_particle_system_filter_instance* filter_instance_p = m_filters.first();
    while (filter_instance_p != nullptr)
    {
        filter_instance_p->m_anim_time = m_elapsed_time;
        vfx_effect_get_parent_transform(this, filter_instance_p->m_parent_renderable_instance_p, filter_instance_p->m_parent_world_rotation, filter_instance_p->m_parent_world_position);
        filter_instance_p->update(delta_time);
        filter_instance_p = filter_instance_p->next;
        if (filter_instance_p == m_filters.first())
        {
            break;
        }
    }

    vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
    num_particles = 0;
    while (instance_p != nullptr)
    {
        if (!instance_p->m_is_emitted_node)
        {
            vfx_effect_get_parent_transform(this, instance_p->m_parent_renderable_instance_p, instance_p->m_parent_transform.m_rotation, instance_p->m_parent_transform.m_translation);
        }
        num_particles += instance_p->update(delta_time, m_elapsed_time);
        instance_p = instance_p->next;
        if (instance_p == m_emitters.first())
        {
            break;
        }
    }
    m_is_particle_system_active = 0 < num_particles;
}

// 0x00CA3D80 (:569)
void vfx_effect::enable_emission(bool enabled)
{
    vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
    while (instance_p != nullptr)
    {
        instance_p->emission_enabled = enabled;
        instance_p = instance_p->next;
        if (instance_p == m_emitters.first())
        {
            break;
        }
    }
}

// 0x00C9EE70 (:579)
void vfx_effect::enable_coronas(bool enabled)
{
    if (enabled)
    {
        m_render_flags |= RENDER_FLAGS_CORONAS_ENABLED;
        return;
    }
    m_render_flags &= ~RENDER_FLAGS_CORONAS_ENABLED;
}

// 0x00CB8510 (:588): a birth spawns the node emitter's child emitters at the particle (inserted right after the node
// emitter, each one priority below the previous, so they end up in reverse order) and runs their first update; a
// death kills them. Original bug kept: a node emitter missing from the effect's list is dereferenced.
void vfx_effect::emit_node_emitters_spawn(vfx_particle_iterative* particle_p, vfx_particle_system_emitter_instance* this_p, bool birth)
{
    if (birth)
    {
        vfx_particle_system_emitter* emitter_p = this_p->m_emitter_p;
        particle_p->m_emitted_emitter_list = nullptr;
        vfx_particle_system_emitter_instance* after_p = m_emitters.first();
        while (after_p != nullptr && after_p != this_p)
        {
            after_p = after_p->next;
            if (after_p == m_emitters.first())
            {
                after_p = nullptr;
                break;
            }
        }
        unsigned int priority = after_p->m_sort_priority;
        const base_array<et_sized_native_pointer<vfx_particle_system_emitter> >& children = emitter_p->get_emit_node_emitter_list();
        for (int i = 0; i < children.size(); i++)
        {
            priority--;
            vfx_particle_system_emitter* child_p = children[i].to_pointer();
            if (child_p == nullptr)
            {
                continue;
            }
            vfx_particle_system_emitter_instance* child_instance_p = create_emitter(child_p, (int)priority);
            if (child_instance_p == nullptr)
            {
                continue;
            }
            // create_emitter appended it; move it after the node emitter.
            m_emitters.remove(child_instance_p);
            m_emitters.insert_after(after_p, child_instance_p);
            child_instance_p->m_is_emitted_node = true;
            child_instance_p->m_parent_transform.m_rotation = m_rotation;
            child_instance_p->m_parent_transform.m_translation = particle_p->position;
            child_instance_p->update(VFX_EFFECT_EMIT_NODE_FIRST_DELTA, 0.0f);
            vfx_effect_emit_node_append(particle_p->m_emitted_emitter_list, child_instance_p);
        }
    }
    else
    {
        vfx_particle_system_emitter_instance* child_instance_p = particle_p->m_emitted_emitter_list;
        while (child_instance_p != nullptr)
        {
            vfx_particle_system_emitter_instance* next_p = child_instance_p->emit_node_next != particle_p->m_emitted_emitter_list ? child_instance_p->emit_node_next : nullptr;
            vfx_effect_emit_node_remove(particle_p->m_emitted_emitter_list, child_instance_p);
            kill_emitter(child_instance_p);
            child_instance_p = next_p;
        }
    }
}

// 0x00CB8780 (:638)
void vfx_effect::emit_node_emitters_spawn_cb(vfx_particle_iterative* particle_p, vfx_particle_system_emitter_instance* this_p, void* data_p, bool birth)
{
    static_cast<vfx_effect*>(data_p)->emit_node_emitters_spawn(particle_p, this_p, birth);
}

// 0x00CA3DB0 (:644): the child emitters follow their particles.
void vfx_effect::emit_node_emitters_update(vfx_particle_system_emitter_instance* this_p)
{
    vfx_particle_iterative* particle_p = this_p->m_particle_iterative_list.first();
    while (particle_p != nullptr)
    {
        vfx_particle_system_emitter_instance* child_instance_p = particle_p->m_emitted_emitter_list;
        while (child_instance_p != nullptr)
        {
            child_instance_p->m_parent_transform.m_rotation = m_rotation;
            child_instance_p->m_parent_transform.m_translation = particle_p->position;
            child_instance_p->m_emit_node_parent_p = particle_p;
            child_instance_p = child_instance_p->emit_node_next;
            if (child_instance_p == particle_p->m_emitted_emitter_list)
            {
                break;
            }
        }
        particle_p = particle_p->next;
        if (particle_p == this_p->m_particle_iterative_list.first())
        {
            break;
        }
    }
}

// 0x00CA3E50 (:658)
void vfx_effect::emit_node_emitters_update_cb(vfx_particle_system_emitter_instance* this_p, void* data_p)
{
    static_cast<vfx_effect*>(data_p)->emit_node_emitters_update(this_p);
}

// 0x00CB1F10 (:664)
void vfx_effect::update_render(float delta_time, bool is_visible, rl_renderer* pRenderer)
{
    if (!is_visible)
    {
        return;
    }
    update_expressions();
    update_dynamic_lights();
    if (m_render_flags == RENDER_FLAGS_NONE)
    {
        return;
    }
    float percent = m_elapsed_time / m_effect_template_p->m_vfx_data_p->duration;
    if (percent - 0.0f < 0.0f)
    {
        percent = 0.0f;
    }
    if (0.0f <= percent - 1.0f)
    {
        percent = 1.0f;
    }

    // The particles group: bounds of the effect, centred on the effect's box.
    rl_renderable_particles_instance_group* group_p = m_renderable_particles_instance_group_p;
    group_p->set_world_aabb(m_world_aabb);
    group_p->set_world_bounding_sphere(m_world_bounding_sphere);
    float eps = 0.0f;
    if (Vfx_sort_hack)
    {
        eps = m_eps + 0.0f;
    }
    vector center;
    center.x = (m_world_aabb.max.x + m_world_aabb.min.x) * 0.5f + eps;
    center.y = (m_world_aabb.max.y + m_world_aabb.min.y) * 0.5f + eps;
    center.z = (m_world_aabb.max.z + m_world_aabb.min.z) * 0.5f + eps;
    group_p->set_position(center);
    group_p->set_bounding_volume_dirty(false);
    if (!use_alpha_fade)
    {
        group_p->set_opacity(1.0f);
    }
    else
    {
        group_p->set_opacity(alpha_fade);
    }
    group_p->m_effect_material_tint = m_effect_material_tint;
    group_p->update_partition_system();

    // Emitters: render state, and the parametric constants once the expressions changed them.
    vfx_particle_system_emitter_instance* instance_p = m_emitters.first();
    while (instance_p != nullptr)
    {
        vfx_particle_system_emitter* emitter_p = instance_p->m_emitter_p;
        if (emitter_p->m_particle_type != rl_particle_base::RENDER_TYPE_NODES)
        {
            rl_renderable_particles_instance* renderable_instance_p = instance_p->m_renderable_particles_instance_p;
            if (emitter_p->local_space_emission)
            {
                renderable_instance_p->m_local_emission_position = instance_p->m_parent_transform.m_translation;
                renderable_instance_p->m_local_emission_rotation = instance_p->m_parent_transform.m_rotation;
            }
            if (instance_p->m_has_alternate_view)
            {
                renderable_instance_p->m_alternate_view = instance_p->m_alternate_view;
            }
            renderable_instance_p->m_tail_spawn_count = instance_p->tail_spawn_count;
            renderable_instance_p->m_current_elapsed_time = instance_p->current_elapsed_time;
            renderable_instance_p->m_texture_anim_rate = instance_p->m_texture_anim_rate;
            rl_renderable_particles_instance::parametric_constant_data* constants_p = renderable_instance_p->m_parametric_constant_data_p;
            if (constants_p != nullptr)
            {
                constants_p->m_texture_anim_rate = instance_p->m_texture_anim_rate;
            }
            renderable_instance_p->m_is_visible = m_is_visible && instance_p->m_is_visible;
            renderable_instance_p->m_has_alternate_view = instance_p->m_has_alternate_view;
            if (!emitter_p->is_iterative && constants_p != nullptr)
            {
                constants_p->m_time = instance_p->current_elapsed_time;
                if (instance_p->m_update_parametric)
                {
                    constants_p->m_texture_anim_random_enabled = renderable_instance_p->m_renderable_particles_p->m_texture_anim_random_frames ? 1.0f : 0.0f;
                    const float opacity_scale = emitter_p->m_opacity_scale;
                    constants_p->m_color_start = rl_color_float(vector(emitter_p->color_start.red, emitter_p->color_start.green, emitter_p->color_start.blue), emitter_p->color_start.alpha * opacity_scale);
                    constants_p->m_color_mid_A = rl_color_float(vector(emitter_p->color_mid_A.red, emitter_p->color_mid_A.green, emitter_p->color_mid_A.blue), emitter_p->color_mid_A.alpha * opacity_scale);
                    constants_p->m_color_mid_B = rl_color_float(vector(emitter_p->color_mid_B.red, emitter_p->color_mid_B.green, emitter_p->color_mid_B.blue), emitter_p->color_mid_B.alpha * opacity_scale);
                    constants_p->m_color_end = rl_color_float(vector(emitter_p->color_end.red, emitter_p->color_end.green, emitter_p->color_end.blue), emitter_p->color_end.alpha * opacity_scale);

                    // Parametric particles have no filters: the global wind is folded into the acceleration (one
                    // random direction around the wind per update; uses the global generator, VFX.md 2.5).
                    vector acceleration_start = emitter_p->acceleration_start;
                    vector acceleration_end = emitter_p->acceleration_end;
                    const float weight_start = emitter_p->m_weight_start - 0.0f;
                    const float weight_end = emitter_p->m_weight_end - 0.0f;
                    if (VFX_EFFECT_WEIGHT_EPSILON <= weight_start || VFX_EFFECT_WEIGHT_EPSILON <= -weight_start || VFX_EFFECT_WEIGHT_EPSILON <= weight_end || VFX_EFFECT_WEIGHT_EPSILON <= -weight_end)
                    {
                        vfx_particle_system_filter_instance* wind_p = vfx_system::get_global_wind_instance();
                        const vector wind_direction = wind_p->m_world_rotation.fvec.copy_normalize();
                        const float force = wind_p->m_force;
                        vector direction = wind_direction;
                        direction.rand_around_dir(wind_direction, 1.0f - wind_p->m_force_noise, 1.0f);
                        const float scaled_weight_start = emitter_p->m_weight_start * emitter_p->m_weight_scale;
                        const float scaled_weight_end = emitter_p->m_weight_end * emitter_p->m_weight_scale;
                        acceleration_start.x = direction.x * force * scaled_weight_start + acceleration_start.x;
                        acceleration_start.y = direction.y * force * scaled_weight_start + acceleration_start.y;
                        acceleration_start.z = direction.z * force * scaled_weight_start + acceleration_start.z;
                        acceleration_end.x = direction.x * force * scaled_weight_end + acceleration_end.x;
                        acceleration_end.y = direction.y * force * scaled_weight_end + acceleration_end.y;
                        acceleration_end.z = direction.z * force * scaled_weight_end + acceleration_end.z;
                    }
                    constants_p->m_acceleration_start = acceleration_start;
                    constants_p->m_acceleration_end = acceleration_end;
                    constants_p->m_color_saturation_start = emitter_p->color_saturation_start;
                    constants_p->m_color_saturation_mid_A = emitter_p->color_saturation_mid_A;
                    constants_p->m_color_saturation_mid_B = emitter_p->color_saturation_mid_B;
                    constants_p->m_color_saturation_end = emitter_p->color_saturation_end;
                    constants_p->m_color_mid_A_offset = emitter_p->color_mid_A_offset;
                    constants_p->m_color_mid_B_offset = emitter_p->color_mid_B_offset;
                    constants_p->m_is_local_space = emitter_p->local_space_emission ? VFX_EFFECT_LOCAL_SPACE_TRUE : 0.0f;
                    constants_p->m_density = emitter_p->density;
                    constants_p->m_density_variance = emitter_p->density_variance;
                    constants_p->m_rotation_start = instance_p->get_angular_speed_start(0.0f);
                    constants_p->m_rotation_end = instance_p->get_angular_speed_end(0.0f);
                    constants_p->m_size_start = instance_p->get_size_start(0.0f);
                    constants_p->m_size_mid_A = instance_p->get_size_mid_A(0.0f);
                    constants_p->m_size_mid_B = instance_p->get_size_mid_B(0.0f);
                    constants_p->m_size_end = instance_p->get_size_end(0.0f);
                    float size_mid_A_offset;
                    vfx_interpolate_varkey<vfx_float_key, float>(size_mid_A_offset, emitter_p->num_size_mida_offset_keys, emitter_p->size_mida_offset_keys, 0.0f);
                    constants_p->m_size_mid_A_offset = size_mid_A_offset;
                    float size_mid_B_offset;
                    vfx_interpolate_varkey<vfx_float_key, float>(size_mid_B_offset, emitter_p->num_size_midb_offset_keys, emitter_p->size_midb_offset_keys, 0.0f);
                    constants_p->m_size_mid_B_offset = size_mid_B_offset;
                    if (!emitter_p->use_length)
                    {
                        constants_p->m_length_start = instance_p->get_size_start(0.0f);
                        constants_p->m_length_mid_A = instance_p->get_size_mid_A(0.0f);
                        constants_p->m_length_mid_B = instance_p->get_size_mid_B(0.0f);
                        constants_p->m_length_end = instance_p->get_size_end(0.0f);
                        constants_p->m_length_mid_A_offset = emitter_p->get_size_mid_A_offset(0.0f);
                        constants_p->m_length_mid_B_offset = emitter_p->get_size_mid_B_offset(0.0f);
                    }
                    else
                    {
                        constants_p->m_length_start = instance_p->get_length_start(0.0f);
                        constants_p->m_length_mid_A = instance_p->get_length_mid_A(0.0f);
                        constants_p->m_length_mid_B = instance_p->get_length_mid_B(0.0f);
                        constants_p->m_length_end = instance_p->get_length_end(0.0f);
                        constants_p->m_length_mid_A_offset = emitter_p->get_length_mid_A_offset(0.0f);
                        constants_p->m_length_mid_B_offset = emitter_p->get_length_mid_B_offset(0.0f);
                    }
                    constants_p->m_additive_key_start = emitter_p->additive_key_start;
                    constants_p->m_additive_key_mid_A = emitter_p->additive_key_mid_A;
                    constants_p->m_additive_key_mid_B = emitter_p->additive_key_mid_B;
                    constants_p->m_additive_key_end = emitter_p->additive_key_end;
                    constants_p->m_acceleration_start_variance = emitter_p->acceleration_start_variance;
                    constants_p->m_acceleration_end_variance = emitter_p->acceleration_end_variance;
                    constants_p->m_rotation_start_variance = instance_p->m_emitter_p->angular_speed_start_variance;
                    constants_p->m_rotation_end_variance = instance_p->m_emitter_p->angular_speed_end_variance;
                    constants_p->m_size_start_variance = instance_p->get_size_start_variance(0.0f);
                    constants_p->m_size_mid_A_variance = instance_p->get_size_mid_A_variance(0.0f);
                    constants_p->m_size_mid_B_variance = instance_p->get_size_mid_B_variance(0.0f);
                    constants_p->m_size_end_variance = instance_p->get_size_end_variance(0.0f);
                    if (!emitter_p->use_length)
                    {
                        constants_p->m_length_start_variance = instance_p->get_size_start_variance(0.0f);
                        constants_p->m_length_mid_A_variance = instance_p->get_size_mid_A_variance(0.0f);
                        constants_p->m_length_mid_B_variance = instance_p->get_size_mid_B_variance(0.0f);
                        constants_p->m_length_end_variance = instance_p->get_size_end_variance(0.0f);
                    }
                    else
                    {
                        constants_p->m_length_start_variance = instance_p->get_length_start_variance(0.0f);
                        constants_p->m_length_mid_A_variance = instance_p->get_length_mid_A_variance(0.0f);
                        constants_p->m_length_mid_B_variance = instance_p->get_length_mid_B_variance(0.0f);
                        constants_p->m_length_end_variance = instance_p->get_length_end_variance(0.0f);
                    }
                    instance_p->m_update_parametric = false;
                }
            }
            instance_p->copy_particles(renderable_instance_p);
        }
        instance_p = instance_p->next;
        if (instance_p == m_emitters.first())
        {
            break;
        }
    }

    // Meshes: keyed transform under the effect, facing (towards the camera) or rod (spinning about its up axis to face
    // the camera), scale, opacity.
    rl_renderable_instance* mesh_instance_p = m_renderable_mesh_insts.first();
    int mesh_index = 0;
    while (mesh_instance_p != nullptr)
    {
        vfx_effect_template::mesh& mesh_r = m_effect_template_p->m_vfx_data_p->meshes[mesh_index];
        vector position;
        vfx_interpolate_vector(position, percent, mesh_r.num_position_keys, mesh_r.position_keys.to_pointer());
        quaternion orientation;
        vfx_interpolate_quaternion(orientation, percent, mesh_r.num_rotation_keys, mesh_r.rotation_keys.to_pointer());
        matrix rotation;
        orientation.extract_matrix(rotation);
        vector scale;
        vfx_interpolate_vector(scale, percent, mesh_r.num_scale_keys, mesh_r.scale_keys.to_pointer());
        position.unrotate(m_rotation);
        position.x = m_position.x + position.x;
        position.y = position.y + m_position.y;
        position.z = position.z + m_position.z;
        rotation = rotation * m_rotation;
        const unsigned int type = mesh_r.type;
        if ((type & vfx_effect_template::MESH_TYPE_FACING) != 0)
        {
            const vector& view_position = vfx_system::get_view_position();
            vector to_view(view_position.x - position.x, view_position.y - position.y, view_position.z - position.z);
            to_view.normalize_safe();
            rotation.fvec = to_view;
            rotation.rvec.x = rotation.uvec.y * to_view.z - rotation.uvec.z * to_view.y;
            rotation.rvec.y = rotation.uvec.z * to_view.x - to_view.z * rotation.uvec.x;
            rotation.rvec.z = to_view.y * rotation.uvec.x - rotation.uvec.y * to_view.x;
            rotation.rvec.normalize_safe();
            rotation.uvec.x = rotation.rvec.z * rotation.fvec.y - rotation.rvec.y * rotation.fvec.z;
            rotation.uvec.y = rotation.fvec.z * rotation.rvec.x - rotation.rvec.z * rotation.fvec.x;
            rotation.uvec.z = rotation.rvec.y * rotation.fvec.x - rotation.fvec.y * rotation.rvec.x;
        }
        else if ((type & vfx_effect_template::MESH_TYPE_ROD) != 0)
        {
            const vector& view_position = vfx_system::get_view_position();
            vector to_view(view_position.x - position.x, view_position.y - position.y, view_position.z - position.z);
            to_view.normalize();
            const float dot = to_view.y * rotation.uvec.y + to_view.x * rotation.uvec.x + to_view.z * rotation.uvec.z;
            if (dot < VFX_EFFECT_ROD_DOT_LIMIT && -VFX_EFFECT_ROD_DOT_LIMIT < dot)
            {
                vector right;
                right.x = to_view.y * rotation.uvec.z - to_view.z * rotation.uvec.y;
                right.y = to_view.z * rotation.uvec.x - rotation.uvec.z * to_view.x;
                right.z = rotation.uvec.y * to_view.x - to_view.y * rotation.uvec.x;
                rotation.rvec = right;
                rotation.fvec.x = right.y * rotation.uvec.z - right.z * rotation.uvec.y;
                rotation.fvec.y = right.z * rotation.uvec.x - rotation.uvec.z * right.x;
                rotation.fvec.z = rotation.uvec.y * right.x - right.y * rotation.uvec.x;
                rotation.orthogonalize();
            }
        }
        float opacity = vfx_interpolate_float(percent, mesh_r.num_opacity_keys, mesh_r.opacity_keys.to_pointer());
        if (use_alpha_fade)
        {
            opacity = alpha_fade * opacity;
        }
        matrix scale_matrix(IDENTITY_MATRIX);
        scale_matrix.rvec.x = scale.x;
        scale_matrix.uvec.y = scale.y;
        scale_matrix.fvec.z = scale.z;
        rotation = rotation * scale_matrix;
        mesh_instance_p->set_opacity(opacity);
        mesh_instance_p->set_is_visible(m_is_visible);
        mesh_instance_p->set_position(position);
        mesh_instance_p->set_rotation(rotation);
        mesh_instance_p->update_partition_system();
        mesh_instance_p = mesh_instance_p->next;
        if (mesh_instance_p == m_renderable_mesh_insts.first())
        {
            break;
        }
        mesh_index++;
    }

    // Debris meshes: a rigid body from the mesh's registered shape on first use (VFX.md 1.9), then the filter forces.
    vfx_renderable_filter_instance* mesh_filter_p = m_mesh_filters.first();
    while (mesh_filter_p != nullptr)
    {
        const int effect_index = mesh_filter_p->m_effect_index;
        const rl_handle shape_handle = m_effect_template_p->get_shape_handle(effect_index);
        if (shape_handle.get() != VFX_EFFECT_INVALID_HANDLE)
        {
            if (mesh_filter_p->m_rigid_body_handle.get() == VFX_EFFECT_INVALID_HANDLE)
            {
                const vfx_effect_template::mesh& mesh_r = m_effect_template_p->m_vfx_data_p->meshes[effect_index];
                vfx_physics_rigid_body_create_data create_data;
                create_data.m_position = mesh_filter_p->m_renderable_instance_p->get_position();
                create_data.m_rotation = mesh_filter_p->m_renderable_instance_p->get_rotation();
                create_data.m_material_index = mesh_r.shape_material_index;
                create_data.m_volume = mesh_r.shape_volume;
                create_data.m_center_of_mass = mesh_r.shape_center_of_mass;
                create_data.m_inertia_tensor = mesh_r.shape_inertia_tensor;
                create_data.m_shape_handle = shape_handle;
                mesh_filter_p->m_rigid_body_handle = vfx_system::get_rigid_body_create_cb()(&create_data);
                mesh_filter_p->m_center_of_mass = create_data.m_center_of_mass;
            }
            vfx_effect_get_parent_transform(this, mesh_filter_p->m_parent_renderable_instance_p, mesh_filter_p->m_parent_world_rotation, mesh_filter_p->m_parent_world_position);
            mesh_filter_p->m_anim_time = m_elapsed_time;
            mesh_filter_p->update(delta_time);
        }
        mesh_filter_p = mesh_filter_p->next;
        if (mesh_filter_p == m_mesh_filters.first())
        {
            break;
        }
    }

    // Coronas: occlusion results collected, placed at their keyed transform under the effect; colors are keyed by the
    // effect's progress, the other values by its elapsed time. Disabled coronas get zero radii.
    rl_corona_instance* corona_p = m_renderable_particles_instance_group_p->m_coronas.first();
    int corona_index = -1;
    while (corona_p != nullptr)
    {
        corona_index++;
        const vfx_effect_template::corona& corona_r = m_effect_template_p->m_vfx_data_p->coronas[corona_index];
        if (pRenderer != nullptr)
        {
            update_corona(*pRenderer, corona_p);
        }
        const matrix43 parent(m_position, m_rotation);
        const float progress = m_elapsed_time / m_effect_template_p->m_vfx_data_p->duration;
        vector local_position;
        vfx_interpolate_vector(local_position, progress, corona_r.num_position_keys, corona_r.position_keys.to_pointer());
        quaternion local_orientation;
        vfx_interpolate_quaternion(local_orientation, progress, corona_r.num_rotation_keys, corona_r.rotation_keys.to_pointer());
        matrix local_rotation;
        local_orientation.extract_matrix(local_rotation);
        corona_p->m_rotation = local_rotation * parent.m_rotation;
        corona_p->m_position.x = parent.m_rotation.rvec.x * local_position.x + local_position.y * parent.m_rotation.uvec.x + parent.m_rotation.fvec.x * local_position.z + parent.m_translation.x;
        corona_p->m_position.y = parent.m_rotation.rvec.y * local_position.x + local_position.y * parent.m_rotation.uvec.y + parent.m_rotation.fvec.y * local_position.z + parent.m_translation.y;
        corona_p->m_position.z = parent.m_rotation.rvec.z * local_position.x + local_position.y * parent.m_rotation.uvec.z + parent.m_rotation.fvec.z * local_position.z + parent.m_translation.z;
        corona_p->m_type = corona_r.type;
        corona_p->m_radius_start = vfx_interpolate_float_varkey(corona_r.num_radius_start_keys, corona_r.radius_start_keys.to_pointer(), m_elapsed_time);
        corona_p->m_radius_end = vfx_interpolate_float_varkey(corona_r.num_radius_end_keys, corona_r.radius_end_keys.to_pointer(), m_elapsed_time);
        corona_p->m_width_scale = vfx_interpolate_float_varkey(corona_r.num_width_scale_keys, corona_r.width_scale_keys.to_pointer(), m_elapsed_time);
        corona_p->m_visibility_radius = vfx_interpolate_float_varkey(corona_r.num_visibility_radius_keys, corona_r.visibility_radius_keys.to_pointer(), m_elapsed_time);
        const vector4 color_start = vfx_interpolate_vector4(progress, corona_r.num_color_start_keys, corona_r.color_start_keys.to_pointer());
        corona_p->m_color_start = rl_color_float(color_start);
        const vector4 color_end = vfx_interpolate_vector4(progress, corona_r.num_color_end_keys, corona_r.color_end_keys.to_pointer());
        corona_p->m_color_end = rl_color_float(color_end);
        corona_p->m_saturation = vfx_interpolate_float_varkey(corona_r.num_saturation_keys, corona_r.saturation_keys.to_pointer(), m_elapsed_time);
        corona_p->m_rotation_scale = vfx_interpolate_float_varkey(corona_r.num_rotation_scale_keys, corona_r.rotation_scale_keys.to_pointer(), m_elapsed_time);
        corona_p->m_intensity_scale = vfx_interpolate_float_varkey(corona_r.num_intensity_scale_keys, corona_r.intensity_scale_keys.to_pointer(), m_elapsed_time);
        corona_p->m_intensity_exponential_decay = vfx_interpolate_float_varkey(corona_r.num_intensity_exponential_decay_keys, corona_r.intensity_exponential_decay_keys.to_pointer(), m_elapsed_time);
        corona_p->m_opacity_exponential_decay = vfx_interpolate_float_varkey(corona_r.num_opacity_exponential_decay_keys, corona_r.opacity_exponential_decay_keys.to_pointer(), m_elapsed_time);
        corona_p->m_radius_distance_start = vfx_interpolate_float_varkey(corona_r.num_radius_distance_start_keys, corona_r.radius_distance_start_keys.to_pointer(), m_elapsed_time);
        corona_p->m_radius_distance_end = vfx_interpolate_float_varkey(corona_r.num_radius_distance_end_keys, corona_r.radius_distance_end_keys.to_pointer(), m_elapsed_time);
        corona_p->m_color_distance_start = vfx_interpolate_float_varkey(corona_r.num_color_distance_start_keys, corona_r.color_distance_start_keys.to_pointer(), m_elapsed_time);
        corona_p->m_color_distance_end = vfx_interpolate_float_varkey(corona_r.num_color_distance_end_keys, corona_r.color_distance_end_keys.to_pointer(), m_elapsed_time);
        corona_p->m_view_fade_scale = vfx_interpolate_float_varkey(corona_r.num_view_fade_scale_keys, corona_r.view_fade_scale_keys.to_pointer(), m_elapsed_time);
        corona_p->m_view_fade_decay = vfx_interpolate_float_varkey(corona_r.num_view_fade_decay_keys, corona_r.view_fade_decay_keys.to_pointer(), m_elapsed_time);
        corona_p->m_view_fade_intensity_enabled = corona_r.view_fade_intensity_enabled;
        corona_p->m_view_fade_opacity_enabled = corona_r.view_fade_opacity_enabled;
        corona_p->m_view_fade_radius_enabled = corona_r.view_fade_radius_enabled;
        corona_p->m_opacity_visibility_decay = vfx_interpolate_float_varkey(corona_r.num_opacity_visibility_decay_keys, corona_r.opacity_visibility_decay_keys.to_pointer(), m_elapsed_time);
        corona_p->m_radius_visibility_decay = vfx_interpolate_float_varkey(corona_r.num_radius_visibility_decay_keys, corona_r.radius_visibility_decay_keys.to_pointer(), m_elapsed_time);
        corona_p->m_rotation_offset = vfx_interpolate_float_varkey(corona_r.num_rotation_offset_keys, corona_r.rotation_offset_keys.to_pointer(), m_elapsed_time);
        corona_p->m_position_offset = vfx_interpolate_float_varkey(corona_r.num_position_offset_keys, corona_r.position_offset_keys.to_pointer(), m_elapsed_time);
        corona_p->m_texture_handle = corona_r.material.textures.to_pointer()->tex_ref;
        if ((m_render_flags & RENDER_FLAGS_CORONAS_ENABLED) == 0)
        {
            corona_p->m_radius_start = 0.0f;
            corona_p->m_radius_end = 0.0f;
        }
        corona_p = corona_p->next;
        if (corona_p == m_renderable_particles_instance_group_p->m_coronas.first())
        {
            break;
        }
    }
}

// 0x00CA3E60 (:1121): lights follow their keys under the effect; lights dimmer than 0.001 are hidden.
void vfx_effect::update_dynamic_lights()
{
    rl_light* light_p = m_lights.first();
    if (light_p == nullptr)
    {
        return;
    }
    int light_index = 0;
    do
    {
        const vector effect_position = m_position;
        const matrix effect_rotation(m_rotation);
        vfx_effect_template::vfx_data* data_p = m_effect_template_p->m_vfx_data_p;
        vfx_effect_template::light& light_r = data_p->lights[light_index];
        float percent = m_elapsed_time / data_p->duration;
        if (percent - 0.0f < 0.0f)
        {
            percent = 0.0f;
        }
        if (0.0f <= percent - 1.0f)
        {
            percent = 1.0f;
        }
        const float intensity = vfx_interpolate_float(percent, light_r.num_intensity_keys, light_r.intensity_keys.to_pointer());
        if (VFX_EFFECT_LIGHT_MIN_INTENSITY <= intensity)
        {
            light_p->set_is_visible(true);
            vector position;
            vfx_interpolate_vector(position, percent, light_r.num_position_keys, light_r.position_keys.to_pointer());
            position.unrotate(effect_rotation);
            position.x = position.x + effect_position.x;
            position.y = position.y + effect_position.y;
            position.z = position.z + effect_position.z;
            vector fvec;
            vfx_interpolate_vector(fvec, percent, light_r.num_fvec_keys, light_r.fvec_keys.to_pointer());
            fvec.unrotate(effect_rotation);
            vector color;
            vfx_interpolate_vector(color, percent, light_r.num_clr_keys, light_r.clr_keys.to_pointer());
            const float radius = light_r.atten_end;
            light_p->set_local_bounding_sphere(rl_sphere(ZERO_VECTOR, radius));
            const float clip = radius * -1.0f;
            light_p->set_clip_min(vector(clip, clip, clip));
            light_p->set_clip_max(vector(radius, radius, radius));
            const rl_color_float light_color(vector(color.x * intensity, color.y * intensity, color.z * intensity), 1.0f);
            if (light_r.type == rl_light::LIGHT_TYPE_POINT)
            {
                light_p->set_light_type(rl_light::LIGHT_TYPE_POINT);
                light_p->set_position(position);
                light_p->set_rotation(IDENTITY_MATRIX);
                light_p->set_color(light_color);
                light_p->set_attenuation_start(light_r.atten_start);
                light_p->set_attenuation_end(light_r.atten_end);
            }
            else
            {
                light_p->set_light_type(rl_light::LIGHT_TYPE_SPOT_LIGHT);
                light_p->set_position(position);
                fvec.normalize();
                matrix rotation;
                rotation.make_quick(fvec);
                light_p->set_rotation(rotation);
                light_p->set_color(light_color);
                light_p->set_attenuation_start(light_r.atten_start);
                light_p->set_attenuation_end(light_r.atten_end);
                light_p->set_hotspot_falloff_size(light_r.hotspot_falloff_size);
                light_p->set_hotspot_size(light_r.hotspot_size);
            }
            light_p->update_partition_system();
        }
        else
        {
            light_p->set_is_visible(false);
        }
        light_p = light_p->next;
        light_index++;
    } while (light_p != m_lights.first() && light_p != nullptr);
}

// 0x00741F60 (.h:57)
void vfx_effect::increment_time(float elapsed)
{
    m_elapsed_time = elapsed + m_elapsed_time;
}

// 0x00C9D2A0 (.h:61)
void vfx_effect::set_handle(int insthandle)
{
    m_instance_handle = insthandle;
}

// 0x00C9D2B0 (.h:62)
int vfx_effect::get_handle() const
{
    return m_instance_handle;
}

// 0x00C9D2C0 (.h:63)
void vfx_effect::set_template(vfx_effect_template* template_p)
{
    m_effect_template_p = template_p;
}

// 0x005D1850 (.h:64)
const vfx_effect_template* vfx_effect::get_template() const
{
    return m_effect_template_p;
}

// 0x00741F90 (.h:65): the partition system is updated on the next update_render.
void vfx_effect::set_position(const vector& position)
{
    m_position = position;
    m_render_flags |= RENDER_FLAGS_UPDATE_PARTITION_SYSTEM;
}

// 0x00B23950 (.h:66)
const vector& vfx_effect::get_position() const
{
    return m_position;
}

// 0x00741FB0 (.h:67)
void vfx_effect::set_rotation(const matrix& rotation)
{
    m_rotation = rotation;
    m_render_flags |= RENDER_FLAGS_UPDATE_PARTITION_SYSTEM;
}

// 0x00C9D2D0 (.h:68)
const matrix& vfx_effect::get_rotation() const
{
    return m_rotation;
}

// 0x00742040 (.h:165)
void vfx_effect::set_world_aabb(const rl_aabb& world_aabb)
{
    m_world_aabb = world_aabb;
    m_render_flags |= RENDER_FLAGS_UPDATE_PARTITION_SYSTEM;
}

// 0x00742070 (.h:172)
const rl_sphere& vfx_effect::get_world_bounding_sphere() const
{
    return m_world_bounding_sphere;
}

// 0x00742080 (.h:177)
void vfx_effect::set_world_bounding_sphere(const rl_sphere& world_bounding_sphere)
{
    m_world_bounding_sphere = world_bounding_sphere;
    m_render_flags |= RENDER_FLAGS_UPDATE_PARTITION_SYSTEM;
}

// 0x00741FF0 (.h:73)
void vfx_effect::set_elapsed_time(float elapsed)
{
    m_elapsed_time = elapsed;
}

// 0x00742000 (.h:74)
float vfx_effect::get_elapsed_time() const
{
    return m_elapsed_time;
}

// 0x00742010 (.h:75)
bool vfx_effect::get_is_particle_system_active() const
{
    return m_is_particle_system_active;
}

// 0x00B23960 (.h:77)
void vfx_effect::set_use_alpha_fade(bool enable)
{
    use_alpha_fade = enable;
}

// 0x00742020 (.h:79)
void vfx_effect::set_alpha_fade(float fade_amount)
{
    alpha_fade = fade_amount;
}

// 0x005D1860 (.h:90)
const linked_list_p<vfx_particle_system_emitter_instance>& vfx_effect::get_emitters() const
{
    return m_emitters;
}

// 0x005D1870 (.h:91)
const linked_list_p<vfx_particle_system_filter_instance>& vfx_effect::get_filters() const
{
    return m_filters;
}

// 0x006C7D40 (.h:93)
const linked_list_p<rl_renderable_instance>& vfx_effect::get_mesh_instances() const
{
    return m_renderable_mesh_insts;
}

// 0x00742030 (.h:95)
rl_renderable_particles_instance_group* vfx_effect::get_renderable_particles_instance_group() const
{
    return m_renderable_particles_instance_group_p;
}

// 0x007420A0 (.h:195)
int vfx_effect::get_expression_count() const
{
    return m_effect_template_p->m_vfx_data_p->num_expressions;
}

// 0x007420B0 (.h:200): original quirk kept: always 1 (every expression has one variable, its first parameter).
int vfx_effect::get_variable_count(int expression_index) const
{
    (void)expression_index;
    return 1;
}

// 0x007476E0 (.h:206): the variable of the expression's first parameter (variable_index is not used).
unsigned int vfx_effect::get_variable_handle(int expression_index, int variable_index) const
{
    (void)variable_index;
    return m_effect_template_p->m_vfx_data_p->expressions[expression_index].m_params.to_pointer()->m_variable_crc;
}

// 0x00C9EEA0 (:1207)
const rl_color_float& vfx_effect::get_effect_material_tint() const
{
    return m_effect_material_tint;
}

// 0x00C9EEB0 (:1212)
void vfx_effect::set_effect_material_tint(const rl_color_float& effect_material_tint)
{
    m_effect_material_tint = effect_material_tint;
}

// 0x00CBA700 (:1220): the same body as vfx_effect::create (inlined there in rfg.exe).
vfx_effect* vfx_effect_create(vfx_effect_template* effect_template_p)
{
    return vfx_effect::create(effect_template_p);
}
