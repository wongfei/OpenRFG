// Original: volition/render_lib/code/rl_renderable_instance.cpp
//
// rl_renderable_instance: a drawable scene entity (mesh, compound and destroyable instances, particles): its renderable,
// tint, opacity, LOD level, sort settings, decals and on-death callbacks. A new instance adds itself to the active scene
// in visibility list 1 and casts shadows (FLAGS_CASTS_SHADOWS | FLAGS_CASTS_DROP_SHADOWS) by default. The
// header-inline members of rl_renderable_instance.h (with rl_compressed_transform and rl_deferred_batch_info) are
// defined here as well.

#include "volition/render_lib/code/rl_renderable_instance.h"

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_planar_decal.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/vlib/code/math/matrix.h"

#include <stdlib.h>

// The render pass flags of the four outline layers.
static const unsigned short Outline_pass_flags = rl_renderable_instance::PASS_FLAGS_OUTLINE_0 | rl_renderable_instance::PASS_FLAGS_OUTLINE_1 | rl_renderable_instance::PASS_FLAGS_OUTLINE_2 | rl_renderable_instance::PASS_FLAGS_OUTLINE_3;

// 0x005433B0 (:23)
rl_renderable_instance::rl_renderable_instance()
{
    m_instance_parameters = vector4(0.0f, 0.0f, 0.0f, 0.0f);
    m_tint = vector(1.0f, 1.0f, 1.0f);
    m_renderable_p = nullptr;
    m_opacity = 1.0f;
    m_lod_level = 0;
    m_sort_method = SORT_METHOD_NONE;
    m_conditional_index = -1;
    m_sort_priority = 0;
    m_xray_opacity = 0xFF;
    m_xray_material_handle = 0xFF;
    get_scene()->add_renderable_instance(this);
    m_decal_list = nullptr;
    set_visibility_list_index(1);
    m_flags |= FLAGS_CASTS_SHADOWS | FLAGS_CASTS_DROP_SHADOWS;
    set_category_id(2);
    m_batching_complexity = 1;
}

// 0x00543750 (:35)
// Leaves the scene, releases the decals and calls the on-death callbacks.
rl_renderable_instance::~rl_renderable_instance()
{
    if (get_scene() != nullptr)
    {
        get_scene()->remove_renderable_instance(this);
    }
    rl_decal* decal_p = m_decal_list;
    while (decal_p != nullptr)
    {
        rl_decal* next_decal = decal_p->next;
        rl_decal* head = m_decal_list;
        unlink_decal(decal_p);
        decal_p->set_owner(nullptr);
        rl_decal_unregister(decal_p);
        decal_p->release();
        decal_p = next_decal != head ? next_decal : nullptr;
    }
    m_on_death.do_callbacks();
}

// Not in the binary: the decal list unlink the original inlines (linked_list_p::remove).
void rl_renderable_instance::unlink_decal(rl_decal* decal_p)
{
    if (m_decal_list == decal_p)
    {
        if (decal_p->next == decal_p)
        {
            decal_p->next = nullptr;
            decal_p->prev = nullptr;
            m_decal_list = nullptr;
            return;
        }
        m_decal_list = decal_p->next;
    }
    if (decal_p->prev != nullptr)
    {
        decal_p->prev->next = decal_p->next;
    }
    if (decal_p->next != nullptr)
    {
        decal_p->next->prev = decal_p->prev;
    }
    decal_p->next = nullptr;
    decal_p->prev = nullptr;
}

// Not in the binary: the decal list append the original inlines (linked_list_p::append).
void rl_renderable_instance::append_decal(rl_decal* decal_p)
{
    if (m_decal_list == nullptr)
    {
        decal_p->next = decal_p;
        decal_p->prev = decal_p;
        m_decal_list = decal_p;
        return;
    }
    decal_p->prev = m_decal_list->prev;
    decal_p->next = m_decal_list;
    m_decal_list->prev->next = decal_p;
    m_decal_list->prev = decal_p;
}

// 0x004C4910 (:57)
// Inserts into or removes from the partition system and updates FLAGS_NOT_VISIBLE.
void rl_renderable_instance::visible_update(bool is_visible)
{
    rl_partition_system* partition_system_p = get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (!is_visible)
        {
            if (get_is_visible())
            {
                partition_system_p->remove(this);
            }
        }
        else if (!get_is_visible())
        {
            partition_system_p->insert(this);
        }
    }
    rl_scene_entity::set_is_visible(is_visible);
}

// 0x00515810 (:64)
// Ages the decals.
void rl_renderable_instance::on_visible_for_frame(rl_renderer&)
{
    rl_decal_process_decals(&m_decal_list);
}

// 0x004F1690 (:74)
// The opacity, LOD level, visibility, flags and sort settings of another instance.
void rl_renderable_instance::copy_parameters(const rl_renderable_instance* param_source)
{
    m_opacity = param_source->m_opacity;
    m_lod_level = param_source->m_lod_level;
    const bool source_visible = param_source->get_is_visible();
    if (source_visible != get_is_visible())
    {
        visible_update(source_visible);
    }
    m_flags = param_source->m_flags;
    m_sort_method = param_source->m_sort_method;
    m_sort_priority = param_source->m_sort_priority;
}

// 0x00543860 (:85)
// Creates a planar decal on the instance (or defers it while render_lib is locked); metrics warning 0x10 when it fails.
unsigned int rl_renderable_instance::add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle)
{
    const unsigned int handle = reserved_handle;
    if (rl_decal_maybe_defer_creation(reserved_handle, decal_params, this))
    {
        return reserved_handle;
    }
    if (decal_params != nullptr)
    {
        if (decal_params->m_type == RLDT_PLANAR)
        {
            rl_decal* decal_p = rl_planar_decal_create(decal_params, get_position(), get_rotation());
            if (decal_p != nullptr)
            {
                append_decal(decal_p);
                decal_p->set_owner(this);
                return rl_decal_register(decal_p, handle);
            }
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    // Magic number: metrics warning 0x10, a decal could not be created.
    rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x10);
    return 0xFFFFFFFF;
}

// 0x004C4980 (:105)
// Moves the decals (all, or those of one generic handle) to another instance.
void rl_renderable_instance::transfer_decals(rl_renderable_instance* new_owner, unsigned int decal_handle)
{
    if (new_owner == nullptr)
    {
        return;
    }
    rl_decal* decal_p = m_decal_list;
    while (decal_p != nullptr)
    {
        rl_decal* next_decal = decal_p->next;
        rl_decal* following = next_decal != m_decal_list ? next_decal : nullptr;
        if (decal_handle == 0xFFFFFFFF || decal_p->get_generic_ri_handle() == decal_handle)
        {
            unlink_decal(decal_p);
            decal_p->set_owner(new_owner);
            new_owner->append_decal(decal_p);
        }
        decal_p = following;
    }
}

// 0x004C4A70 (:122)
void rl_renderable_instance::remove_decal(rl_decal* decal_p)
{
    unlink_decal(decal_p);
    decal_p->set_owner(nullptr);
}

// 0x004C4AD0 (:129)
void rl_renderable_instance::remove_all_decals()
{
    rl_decal* decal_p = m_decal_list;
    while (decal_p != nullptr)
    {
        rl_decal* next_decal = decal_p->next;
        rl_decal* head = m_decal_list;
        remove_decal(decal_p);
        decal_p = next_decal != head ? next_decal : nullptr;
    }
}

// 0x004C4B10 (:137)
void rl_renderable_instance::render_decals(rl_renderer&, int, rl_decal**)
{
}

// 0x004C4B20 (:141)
// By material (get_material, vtbl+0x08), then by decal handle (m_handle at +0x90).
static int decal_sort_compare(const void* left, const void* right)
{
    rl_decal* lhs = *(rl_decal* const*)left;
    rl_decal* rhs = *(rl_decal* const*)right;
    const unsigned int rhs_material = rhs->get_material();
    const unsigned int lhs_material = lhs->get_material();
    if (lhs_material != rhs_material)
    {
        return (int)(lhs->get_material() - rhs->get_material());
    }
    return (int)(lhs->get_decal_handle() - rhs->get_decal_handle());
}

// 0x004C4B80 (:155)
void rl_renderable_instance::sort_decals(rl_decal** decal_list_p, int num_decals)
{
    qsort(decal_list_p, num_decals, sizeof(rl_decal*), decal_sort_compare);
}

// 0x004C4BA0 (:163)
// Up to decal_list_size decals, sorted.
int rl_renderable_instance::build_sorted_decal_list(rl_decal** decal_list_p, int decal_list_size)
{
    rl_decal* decal_p = m_decal_list;
    int count = 0;
    if (decal_p != nullptr)
    {
        while (count != decal_list_size)
        {
            decal_list_p[count] = decal_p;
            decal_p = decal_p->next;
            count++;
            if (decal_p == m_decal_list || decal_p == nullptr)
            {
                break;
            }
        }
        if (count > 0)
        {
            qsort(decal_list_p, count, sizeof(rl_decal*), decal_sort_compare);
        }
    }
    return count;
}

// 0x004BA500 (.h:68)
void rl_compressed_transform::set(const matrix43& transform)
{
    transform.m_rotation.extract_angles(pitch, bank, heading);
    pos = transform.m_translation;
}

// 0x004BA530 (.h:73)
void rl_compressed_transform::get(matrix43& transform) const
{
    transform.m_rotation.set(pitch, bank, heading);
    transform.m_translation = pos;
}

// 0x004BCFA0 (compiler-generated)
rl_compressed_transform::rl_compressed_transform()
{
}

// 0x00543480 (.h:113)
void rl_renderable_instance::render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags)
{
    render(renderer, camera, flags);
}

// 0x005434A0 (.h:114)
void rl_renderable_instance::render_debug()
{
}

// 0x005434B0 (.h:122)
void rl_renderable_instance::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
}

// 0x005434C0 (.h:123)
int rl_renderable_instance::get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const
{
    return -1;
}

// 0x005434D0 (.h:124)
int rl_renderable_instance::get_instance_stream_index(unsigned int sub_id) const
{
    return -1;
}

// 0x005434E0 (.h:125)
void rl_renderable_instance::get_index_range(int& index_min, int& index_max, unsigned int sub_id) const
{
}

// 0x005434F0 (.h:129)
void rl_renderable_instance::add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
}

// 0x004BA580 (.h:146)
const vector& rl_renderable_instance::get_tint() const
{
    return m_tint;
}

// 0x00543500 (.h:224)
void rl_renderable_instance::set_material_map(const rl_material_map* mat_p)
{
}

// 0x00543510 (.h:225)
const rl_material_map* rl_renderable_instance::get_material_map()
{
    return nullptr;
}

// 0x006E13F0 (.h:228)
void rl_renderable_instance::add_on_death_cb(rl_callback_widget* widget_p)
{
    m_on_death.add_widget(widget_p);
}

// 0x006E1400 (.h:229)
void rl_renderable_instance::remove_on_death_cb(rl_callback_widget* widget_p)
{
    m_on_death.remove_widget(widget_p);
}

// 0x004BA590 (.h:245)
void rl_renderable_instance::set_batching_complexity(int batching_complexity)
{
    m_batching_complexity = (short)batching_complexity;
}

int rl_renderable_instance::get_batching_complexity() const
{
    return m_batching_complexity;
}

int rl_renderable_instance::get_conditional_index()
{
    return m_conditional_index;
}

void rl_renderable_instance::set_conditional_index(int conditional_index)
{
    m_conditional_index = conditional_index;
}

// 0x004BA5A0 (.h:287)
// A slot from an atomic counter (the deferred list is filled from several threads); false when it is full.
bool rl_deferred_batch_info::add_def_id(unsigned int def_id, float dist_to_camera_sq, bool opacity_fade)
{
    const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_items, 1);
    if (m_max_items < index + 1)
    {
        return false;
    }
    rl_deferred_batch_item& item = m_elem[index];
    item.def_id = def_id;
    item.dist_sq = dist_to_camera_sq;
    item.opacity_fade = opacity_fade;
    return true;
}

// 0x004BA5E0 (.h:298)
void rl_deferred_batch_info::init(rl_deferred_batch_item* item_array, unsigned int array_size)
{
    m_elem = item_array;
    m_max_items = array_size;
    m_num_items = 0;
}

// 0x004DF7F0 (.h:299)
// The item count, clamped to the capacity (the counter keeps counting past it).
unsigned int rl_deferred_batch_info::size()
{
    const unsigned int count = m_num_items;
    return m_max_items < count ? m_max_items : count;
}

// 0x004BA600 (.h:301)
void rl_deferred_batch_info::clear()
{
    m_num_items = 0;
}

// 0x004DF800 (.h:311)
// Below full opacity the instance is transparent.
void rl_renderable_instance::set_opacity(float opacity)
{
    m_opacity = opacity;
    if (opacity < 1.0f)
    {
        m_flags |= FLAGS_IS_TRANSPARENT;
    }
    else
    {
        m_flags &= ~FLAGS_IS_TRANSPARENT;
    }
}

// 0x004BA610 (.h:321)
float rl_renderable_instance::get_opacity()
{
    return m_opacity;
}

// 0x00753D60 (.h:326)
void rl_renderable_instance::set_tint(vector& tint)
{
    m_tint = tint;
}

// 0x00543530 (.h:331)
// The local volume becomes the renderable's (set_local_aabb, inlined).
void rl_renderable_instance::set_renderable(rl_renderable* renderable_p)
{
    m_renderable_p = renderable_p;
    rl_aabb aabb;
    renderable_p->get_local_aabb(aabb);
    set_local_aabb(aabb);
}

// 0x00543520 (.h:340)
rl_renderable* rl_renderable_instance::get_renderable() const
{
    return m_renderable_p;
}

// 0x004BA620 (.h:345)
void rl_renderable_instance::set_instance_params(const vector4& instance_params)
{
    m_instance_parameters = instance_params;
}

// 0x006E1410 (.h:349)
const vector4& rl_renderable_instance::get_instance_params() const
{
    return m_instance_parameters;
}

// 0x004BA640 (.h:354)
void rl_renderable_instance::set_lod_level(int lod_level)
{
    m_lod_level = (short)lod_level;
}

// 0x004BA650 (.h:359)
int rl_renderable_instance::get_lod_level() const
{
    return m_lod_level;
}

// 0x004BA660 (.h:365): get_is_batchable is inline in rl_renderable_instance.h.

// 0x004BA670 (.h:370)
void rl_renderable_instance::set_is_batchable(bool is_batchable)
{
    if (is_batchable)
    {
        m_flags |= FLAGS_IS_BATCHABLE;
    }
    else
    {
        m_flags &= ~FLAGS_IS_BATCHABLE;
    }
}

// 0x004BA690 (.h:379)
bool rl_renderable_instance::get_is_transparent() const
{
    return (m_flags & FLAGS_IS_TRANSPARENT) != 0;
}

// 0x004BA6A0 (.h:384)
void rl_renderable_instance::set_is_transparent(bool is_transparent)
{
    if (is_transparent)
    {
        m_flags |= FLAGS_IS_TRANSPARENT;
    }
    else
    {
        m_flags &= ~FLAGS_IS_TRANSPARENT;
    }
}

bool rl_renderable_instance::get_casts_shadows() const
{
    return (m_flags & FLAGS_CASTS_SHADOWS) != 0;
}

// 0x005D0B90 (.h:398)
void rl_renderable_instance::set_casts_shadows(bool casts_shadows)
{
    if (casts_shadows)
    {
        m_flags |= FLAGS_CASTS_SHADOWS;
    }
    else
    {
        m_flags &= ~FLAGS_CASTS_SHADOWS;
    }
}

bool rl_renderable_instance::get_casts_terrain_shadows() const
{
    return (m_flags & FLAGS_CASTS_TERRAIN_SHADOWS) != 0;
}

// 0x005D0BB0 (.h:412)
void rl_renderable_instance::set_casts_terrain_shadows(bool casts_shadows)
{
    if (casts_shadows)
    {
        m_flags |= FLAGS_CASTS_TERRAIN_SHADOWS;
    }
    else
    {
        m_flags &= ~FLAGS_CASTS_TERRAIN_SHADOWS;
    }
}

bool rl_renderable_instance::get_casts_drop_shadows() const
{
    return (m_flags & FLAGS_CASTS_DROP_SHADOWS) != 0;
}

// 0x005D0BD0 (.h:426)
void rl_renderable_instance::set_casts_drop_shadows(bool casts_drop_shadows)
{
    if (casts_drop_shadows)
    {
        m_flags |= FLAGS_CASTS_DROP_SHADOWS;
    }
    else
    {
        m_flags &= ~FLAGS_CASTS_DROP_SHADOWS;
    }
}

bool rl_renderable_instance::get_casts_transparent_shadows() const
{
    return (m_flags & FLAGS_CAST_TRANSPARENT_SHADOWS) != 0;
}

// 0x00A056C0 (.h:440)
void rl_renderable_instance::set_casts_transparent_shadows(bool casts_shadows)
{
    if (casts_shadows)
    {
        m_flags |= FLAGS_CAST_TRANSPARENT_SHADOWS;
    }
    else
    {
        m_flags &= ~FLAGS_CAST_TRANSPARENT_SHADOWS;
    }
}

bool rl_renderable_instance::get_xray_visible() const
{
    return (m_pass_flags & PASS_FLAGS_XRAY_EFFECT) != 0;
}

// 0x00A056E0 (.h:449)
void rl_renderable_instance::set_xray_visible(bool xray_visible)
{
    if (xray_visible)
    {
        m_pass_flags |= PASS_FLAGS_XRAY_EFFECT;
    }
    else
    {
        m_pass_flags &= ~PASS_FLAGS_XRAY_EFFECT;
    }
}

bool rl_renderable_instance::get_distortion_visible() const
{
    return (m_pass_flags & PASS_FLAGS_DISTORTION) != 0;
}

// 0x00A05700 (.h:464)
void rl_renderable_instance::set_distortion_visible(bool distortion_visible)
{
    if (distortion_visible)
    {
        m_pass_flags |= PASS_FLAGS_DISTORTION;
    }
    else
    {
        m_pass_flags &= ~PASS_FLAGS_DISTORTION;
    }
}

// 0x004BA6C0 (.h:488)
bool rl_renderable_instance::get_distortion_only() const
{
    return (m_pass_flags & PASS_FLAGS_DISTORTION_ONLY) != 0;
}

// 0x006C3980 (.h:479)
void rl_renderable_instance::set_distortion_only(bool distortion_only)
{
    if (distortion_only)
    {
        m_pass_flags |= PASS_FLAGS_DISTORTION_ONLY;
    }
    else
    {
        m_pass_flags &= ~PASS_FLAGS_DISTORTION_ONLY;
    }
}

// 0x004BA6D0 (.h:494)
void rl_renderable_instance::get_xray_render_params(unsigned char& mat_handle, unsigned char& opacity) const
{
    mat_handle = m_xray_material_handle;
    opacity = m_xray_opacity;
}

// 0x00A05720 (.h:500)
void rl_renderable_instance::set_xray_render_params(const unsigned char& mat_handle, const unsigned char opacity)
{
    m_xray_material_handle = mat_handle;
    m_xray_opacity = opacity;
}

// 0x004BA6F0 (.h:506)
bool rl_renderable_instance::get_is_additive() const
{
    return (m_pass_flags & PASS_FLAGS_ADDITIVE) != 0;
}

// 0x00C9E200 (.h:511)
void rl_renderable_instance::set_is_additive(bool is_additive)
{
    if (is_additive)
    {
        m_pass_flags |= PASS_FLAGS_ADDITIVE;
    }
    else
    {
        m_pass_flags &= ~PASS_FLAGS_ADDITIVE;
    }
}

void rl_renderable_instance::set_render_pass_flags(unsigned short pass_flags)
{
    m_pass_flags = pass_flags;
}

// 0x004BA700 (.h:521)
rl_renderable_instance::sort_method rl_renderable_instance::get_sort_method() const
{
    return m_sort_method;
}

// 0x006C39A0 (.h:526)
void rl_renderable_instance::set_sort_method(sort_method _sort_method)
{
    m_sort_method = _sort_method;
}

// 0x004BA710 (.h:532)
unsigned short rl_renderable_instance::get_sort_priority() const
{
    return m_sort_priority;
}

// 0x00C9E220 (.h:537)
void rl_renderable_instance::set_sort_priority(unsigned short sort_priority)
{
    m_sort_priority = sort_priority;
}

// 0x004DF830 (.h:542)
void rl_renderable_instance::set_is_visible(bool is_visible)
{
    if (is_visible != get_is_visible())
    {
        visible_update(is_visible);
    }
}

// 0x0071AD10 (.h:553)
// One outline layer (0..3) at a time.
void rl_renderable_instance::set_outline_enabled(int outline_pass)
{
    if ((unsigned int)outline_pass < 4)
    {
        m_pass_flags = (m_pass_flags & ~Outline_pass_flags) | (unsigned short)(PASS_FLAGS_OUTLINE_0 << (outline_pass & 0x1F));
    }
}

// 0x00710E00 (.h:563)
void rl_renderable_instance::set_outline_disabled()
{
    m_pass_flags &= ~Outline_pass_flags;
}
