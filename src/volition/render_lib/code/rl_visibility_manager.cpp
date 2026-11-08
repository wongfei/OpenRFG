// Original: volition/render_lib/code/rl_visibility_manager.cpp
//
// rl_visibility_manager: the per-frame visibility of a scene. create_visibility_passes builds up to 31 passes (the
// base pass from the view frustum, the CLB shadow cascades, the distortion, outline and x-ray passes, one or two per
// visible secondary light), each with a culling frustum and/or sphere and culling flags. The partition tree is then
// walked once for all passes at the same time: a pass bit set in an entity's m_vis_pass_flags means the entity is
// visible in that pass. Visible entities go into the visibility list of their m_visibility_list_index (set by the
// scene renderer) and get on_visible_for_frame. Occluders (rl_occluder) and GPU occlusion queries can remove entities
// from passes. Lights, low LOD and landmark LOD entities have their own partition systems and walks.
//
// Pass flag bits (visibility_pass_info::vis_pass_flags): 0x01 needs the dependent pass, 0x02 culling frustum,
// 0x04 culling sphere, 0x08 render pass flags filter (ri_pass_culling_flags), 0x10 scene entity flags filter
// (se_culling_flags), 0x20 no occlusion, 0x40 GPU visibility. The Debug drawing is stripped in the retail build (only
// its bounding volume updates are left).

#include "volition/render_lib/code/rl_visibility_manager.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/render_lib/code/rl_occluder.h"
#include "volition/render_lib/code/rl_partition_sector.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "keen/math/axisalignedrectangle.hpp"

#include <algorithm>
#include <math.h>

// <rl_visibility_manager::release> 0x004D4800 (:39)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_visibility_manager)

// The retail values of the visibility switches (debug console variables in development builds).
bool Rl_visibility_threaded = true; // 0x0150A62C
bool Rl_collect_visiblity_stats = false; // 0x01B7BD4D
bool Rl_lod_off_enabled = false; // 0x01B7BD4E
bool Rl_occlusion_enable = true; // 0x0150A62B
bool RL_vis_use_frustum = false; // 0x01B7BD51
bool Rl_shadows_enabled = true; // 0x014FA47F
bool g_dropShadowEnabled = false; // 0x01B7BD2C
// The visibility debug view (stripped in retail; only the bounding volume updates of the list walk remain).
bool g_visibilityDebugRender = false; // 0x01B7BD43
bool g_visibilityDebugRenderFollowCam = true; // 0x0150A5FF
int g_visibilityDebugRenderSize = 10; // 0x0150A61C
float g_visibilityDebugRenderZoom = 2.5f; // 0x0150A620
int g_visibilityDebugPassMask = 1; // 0x0150A624
bool g_visibilityDebugRenderList0 = true; // 0x0150A628
bool g_visibilityDebugRenderList1 = false; // 0x01B7BD48
bool g_visibilityDebugRenderList2 = true; // 0x0150A629
bool g_visibilityDebugRenderList3 = true; // 0x0150A62A
bool g_visibilityDebugRenderList4 = false; // 0x01B7BD49
bool g_visibilityDebugRenderList5 = false; // 0x01B7BD4A
bool g_visibilityDebugRenderList6 = false; // 0x01B7BD4B

extern bool RL_gpu_vis_enabled; // 0x014FA137 (rl_gpu_visibility.cpp)
extern bool RL_gpu_vis_terrain_enabled; // 0x01B7BD4F (rl_gpu_visibility.cpp)
extern bool RL_enable_secondary_lights; // 0x014FA46F (rl_scene_renderer.cpp)

float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio); // 0x004DFB20 (rl_camera.cpp)

rl_visibility_manager::Debug::Context rl_visibility_manager::Debug::ms_context; // 0x01C226AC
vector2 rl_visibility_manager::Debug::ms_screenOffset; // 0x01C20BC4
vector rl_visibility_manager::Debug::ms_cameraOffset; // 0x01C27390
float rl_visibility_manager::Debug::ms_worldToCamera; // 0x01B6AAF0
float rl_visibility_manager::Debug::ms_cameraToScreen; // 0x01B6EAF8

// The entity's render category is enabled in the filter (a 64-bit shift).
static bool category_enabled(uint64_t filter, rl_scene_entity* entity)
{
    return ((1ull << entity->get_category_id()) & filter) != 0;
}

// The occluder use mask: one bit per actual occluder.
static unsigned int all_occluders_mask(const pool_list<rl_occluder*>& occluders)
{
    unsigned int mask = 0;
    for (int i = 0; i < occluders.size(); i++)
    {
        mask |= 1u << (i & 0x1F);
    }
    return mask;
}

// Adds to a list of a fixed capacity (a critical failure when it is full).
template<typename T>
static void list_add(pool_list<T>* list, T value)
{
    list->add(value);
}

// The population count the statistics use.
static unsigned int count_bits(unsigned int v)
{
    unsigned int count = 0;
    for (; v != 0; v &= v - 1)
    {
        count++;
    }
    return count;
}

// 0x00504FD0 (compiler-generated)
rl_visibility_manager::visibility_pass_info::visibility_pass_info()
{
}

// 0x004D6220 (compiler-generated)
rl_visibility_manager::visibility_pass_info::~visibility_pass_info()
{
}

// 0x004D4BA0 (compiler-generated)
rl_visibility_manager::Debug::Context::Context()
{
}

// 0x004D4820 (:63)
void rl_visibility_manager::Debug::clipToRect(keen::AxisAlignedRectangle& innerRectangle, const keen::AxisAlignedRectangle& outerRectangle)
{
    float x = innerRectangle.x;
    float right = x + innerRectangle.width;
    float y = innerRectangle.y;
    float bottom = innerRectangle.height + y;
    const float outer_right = outerRectangle.width + outerRectangle.x;
    const float outer_bottom = outerRectangle.height + outerRectangle.y;
    if (x - outerRectangle.x < 0.0f)
    {
        x = outerRectangle.x;
    }
    innerRectangle.x = x;
    if (y - outerRectangle.y < 0.0f)
    {
        y = outerRectangle.y;
    }
    innerRectangle.y = y;
    if (0.0f <= right - outer_right)
    {
        right = outer_right;
    }
    innerRectangle.width = right - x;
    if (0.0f <= bottom - outer_bottom)
    {
        bottom = outer_bottom;
    }
    innerRectangle.height = bottom - y;
}

// 0x004D4940 (:78)
// Magic numbers: the debug map covers the 16384 m world at the zoom; 8192 is the world's half size.
void rl_visibility_manager::Debug::begin(const Context& context)
{
    ms_context = context;
    const float view_size = (8192.0f / g_visibilityDebugRenderZoom) * 2.0f;
    ms_worldToCamera = 16384.0f / view_size;
    ms_cameraToScreen = (float)(1 << (g_visibilityDebugRenderSize & 0x1F)) / view_size;
    if (g_visibilityDebugRenderFollowCam)
    {
        ms_cameraOffset = context.mainCameraPosition;
    }
    else
    {
        ms_cameraOffset = vector(0.0f, 0.0f, 0.0f);
    }
    const float screen_size = ms_cameraToScreen * view_size;
    ms_screenOffset.x = screen_size * 0.5f + 0.0f;
    ms_screenOffset.y = screen_size * 0.5f + 0.0f;
}

// 0x004D4AB0 (:121)
int rl_visibility_manager::Debug::debugTransform(float value, float worldToCamera, float cameraToScreen, float screenOffset, float cameraOffset)
{
    return (int)((value - cameraOffset) * worldToCamera * cameraToScreen + screenOffset);
}

// 0x005349A0 (:126)
// Stripped drawing: only the bounding volumes of the entities in the debug pass mask are updated
// (render_debug.md 4.3).
void rl_visibility_manager::Debug::drawVisibilityLists(base_array<pool_list<rl_scene_entity*>*>& lists)
{
    const bool enabled[7] = { g_visibilityDebugRenderList0, g_visibilityDebugRenderList1, g_visibilityDebugRenderList2, g_visibilityDebugRenderList3, g_visibilityDebugRenderList4, g_visibilityDebugRenderList5, g_visibilityDebugRenderList6 };
    for (int i = 0; i < 7; i++)
    {
        if (enabled[i])
        {
            drawVisibilityList(*lists[i], 0);
        }
    }
}

// 0x0051C8A0 (:151)
void rl_visibility_manager::Debug::drawVisibilityList(pool_list<rl_scene_entity*>& list, unsigned int color)
{
    const int mask = g_visibilityDebugPassMask;
    for (int i = 0; i < list.size(); i++)
    {
        rl_scene_entity* entity = list[i];
        if ((entity->get_vis_pass_flags() & mask) != 0)
        {
            entity->get_world_bvolume();
        }
    }
}

// 0x0051C8E0 (:182)
// Stripped (empty loops).
void rl_visibility_manager::Debug::drawVisibleSectors(base_array<visible_sector_entry>& sectors)
{
}

// 0x0051C910 (:224)
// Stripped drawing: only the bounding volumes are updated.
void rl_visibility_manager::Debug::drawOccluders(base_array<rl_occluder*>& occluders)
{
    for (int i = 0; i < occluders.size(); i++)
    {
        occluders[i]->get_world_bvolume();
    }
}

// 0x004D4B00 (:242)
void rl_visibility_manager::Debug::drawFrustum(const vector* corners, unsigned int frustumColor)
{
}

// 0x004D4B10 (:333)
void rl_visibility_manager::Debug::drawGenericFrustum(const rl_frustum& frustum, unsigned int frustumColor)
{
}

// 0x004D4B30 (:371)
// Stripped: only the corner points are computed.
void rl_visibility_manager::Debug::drawViewFrustum(const rl_view_frustum& frustum, unsigned int frustumColor)
{
    vector corners[9];
    for (int index = 0; index < 9; index++)
    {
        corners[index] = frustum.get_point(index);
    }
}

// 0x004D4B90 (:401)
void rl_visibility_manager::Debug::end()
{
}

// 0x00534B70 (:453)
rl_visibility_manager::rl_visibility_manager()
{
    m_merged_occluder_p = nullptr;
    for (int i = 0; i < VISIBILITY_LIST_COUNT; i++)
    {
        m_visibility_lists.elements()[i] = nullptr;
    }
    m_visible_light_list = nullptr;
}

// 0x0051C940 (:464)
rl_visibility_manager::~rl_visibility_manager()
{
}

// 0x00534D60 (:469)
// Magic number: up to 1000 potential and actual occluders.
void rl_visibility_manager::init()
{
    m_potential_occluders.init_dynamic(1000);
    m_actual_occluders.init_dynamic(1000);
}

// 0x0051C9D0 (:477)
bool rl_visibility_manager::bounding_volume_is_visible_in_frustum(const rl_bvolume& bvolume) const
{
    return !m_view_frustum.is_occluded(bvolume);
}

// 0x0051C9F0 (:491)
bool rl_visibility_manager::entity_is_visible_in_frustum(rl_scene_entity* scene_entity_p) const
{
    return !m_view_frustum.is_occluded(scene_entity_p->get_world_bvolume());
}

// 0x0051CA20 (:552)
// The passes of passes_to_test in which the volume passes all of the pass's tests (dependent pass visible, entity
// flags, render pass flags, culling sphere, culling frustum).
unsigned int rl_visibility_manager::bounding_volume_is_visible_for_passes(const rl_bvolume& bvolume, unsigned int se_flags, unsigned int ri_pass_flags, unsigned int passes_to_test, unsigned int already_visible_passes) const
{
    unsigned int visible = 0;
    for (int i = 0; i < m_visibility_passes.size(); i++)
    {
        const visibility_pass_info& pass = m_visibility_passes[i];
        if ((pass.pass_index_shifted & passes_to_test) == 0)
        {
            continue;
        }
        const unsigned int flags = pass.vis_pass_flags;
        if ((flags & 0x01) != 0 && ((visible | already_visible_passes) & (1u << (pass.dependent_pass_index & 0x1F))) == 0)
        {
            continue;
        }
        if ((flags & 0x10) != 0 && (pass.se_culling_flags & se_flags) == 0)
        {
            continue;
        }
        if ((flags & 0x08) != 0 && (pass.ri_pass_culling_flags & ri_pass_flags) == 0)
        {
            continue;
        }
        if ((flags & 0x04) != 0 && !ix_sphere_sphere(pass.bounding_sphere.m_position, bvolume.m_sphere.m_position, pass.bounding_sphere.m_radius, bvolume.m_sphere.m_radius))
        {
            continue;
        }
        if ((flags & 0x02) != 0 && pass.culling_frustum.is_occluded(bvolume))
        {
            continue;
        }
        visible |= pass.pass_index_shifted;
    }
    return visible;
}

// 0x0051CB00 (:619)
unsigned int rl_visibility_manager::sector_is_visible_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test, unsigned int already_visible_passes) const
{
    return bounding_volume_is_visible_for_passes(sector_p->get_world_bvolume(), 0xFFFFFFFF, 0xFFFFFFFF, passes_to_test, already_visible_passes);
}

// 0x00501B30 (:631)
// The passes in which an occluder hides the whole sector (or its dependent pass is occluded); occluders that do not
// even partially cover the sector are dropped from occluder_use_flags for the sector's entities and children.
unsigned int rl_visibility_manager::sector_is_occluded_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test, unsigned int& occluder_use_flags) const
{
    unsigned int occluded = 0;
    for (int i = 0; i < m_visibility_passes.size(); i++)
    {
        const visibility_pass_info& pass = m_visibility_passes[i];
        const unsigned int pass_bit = pass.pass_index_shifted;
        if ((passes_to_test & pass_bit) == 0 || (pass.vis_pass_flags & 0x20) != 0)
        {
            continue;
        }
        if ((pass.vis_pass_flags & 0x01) == 0)
        {
            for (int o = 0; o < m_actual_occluders.size(); o++)
            {
                rl_occluder* occluder = m_actual_occluders[o];
                const unsigned int occluder_bit = 1u << (o & 0x1F);
                if ((occluder_use_flags & occluder_bit) != 0)
                {
                    if (occluder->is_occluded(sector_p))
                    {
                        occluded |= pass_bit;
                        break;
                    }
                    if (!occluder->is_partially_occluded(sector_p))
                    {
                        occluder_use_flags &= ~occluder_bit;
                    }
                }
            }
        }
        else if ((occluded & (1u << (pass.dependent_pass_index & 0x1F))) != 0)
        {
            occluded |= pass_bit;
        }
    }
    return occluded;
}

// 0x00501C50 (:678)
// The passes (without filters) whose culling sphere and frustum contain the whole sector.
unsigned int rl_visibility_manager::sector_is_fully_visible_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test) const
{
    unsigned int fully_visible = 0;
    for (int i = 0; i < m_visibility_passes.size(); i++)
    {
        const visibility_pass_info& pass = m_visibility_passes[i];
        if ((pass.pass_index_shifted & passes_to_test) == 0 || (pass.vis_pass_flags & 0x18) != 0)
        {
            continue;
        }
        const rl_bvolume& volume = sector_p->get_world_bvolume();
        rl_aabb box;
        volume.get_aabb(box);
        if ((pass.vis_pass_flags & 0x04) != 0)
        {
            const float dist_sq = vec_dist_squared(volume.m_sphere.m_position, pass.bounding_sphere.m_position);
            const float radius = pass.bounding_sphere.m_radius;
            const float sector_radius = volume.m_sphere.m_radius;
            if (radius < sector_radius || (radius - sector_radius) * (radius - sector_radius) < dist_sq || !ix_box_completely_in_sphere(pass.bounding_sphere.m_position, radius, box.min, box.max))
            {
                continue;
            }
        }
        if ((pass.vis_pass_flags & 0x02) == 0 || pass.culling_frustum.is_completely_inside(box))
        {
            fully_visible |= pass.pass_index_shifted;
        }
    }
    return fully_visible;
}

// 0x0051CB20 (:757)
unsigned int rl_visibility_manager::entity_is_visible_for_passes(rl_scene_entity* scene_entity_p, unsigned int passes_to_test, unsigned int already_visible_passes) const
{
    const unsigned int se_flags = scene_entity_p->get_flags();
    const unsigned int ri_flags = scene_entity_p->get_render_pass_flags();
    return bounding_volume_is_visible_for_passes(scene_entity_p->get_world_bvolume(), se_flags, ri_flags, passes_to_test, already_visible_passes);
}

// 0x0051CB60 (:773)
// The passes in which the entity is occluded: by an occluder, or (GPU visibility passes, for entities that are not
// GPU occluded-exempt, terrain or in the x-ray pass) by its occlusion query result.
unsigned int rl_visibility_manager::entity_is_occluded_for_passes(rl_renderer& renderer, rl_camera& camera, rl_scene_entity* scene_entity_p, unsigned int passes_to_test, unsigned int occluder_use_flags) const
{
    unsigned int occluded = 0;
    for (int i = 0; i < m_visibility_passes.size(); i++)
    {
        const visibility_pass_info& pass = m_visibility_passes[i];
        const unsigned int pass_bit = pass.pass_index_shifted;
        const unsigned int flags = pass.vis_pass_flags;
        if ((passes_to_test & pass_bit) == 0 || (flags & 0x20) != 0)
        {
            continue;
        }
        if ((flags & 0x01) != 0)
        {
            if ((occluded & (1u << (pass.dependent_pass_index & 0x1F))) != 0)
            {
                occluded |= pass_bit;
            }
            continue;
        }
        // Magic number: render pass flag 0x100 is the x-ray pass.
        const bool xray = m_xray_pass_vis_index != 0xFF && (scene_entity_p->get_render_pass_flags() & 0x100) != 0;
        const bool terrain = !RL_gpu_vis_terrain_enabled && scene_entity_p->get_is_terrain();
        bool use_occluders = true;
        if ((flags & 0x40) != 0 && (scene_entity_p->get_flags() & rl_scene_entity::FLAGS_NOT_GPU_OCCLUDED) == 0 && !terrain && !xray)
        {
            int gpu_vis_index = scene_entity_p->get_gpu_vis_index();
            if (gpu_vis_index == -1)
            {
                rl_gpu_vis_alloc(renderer, camera, scene_entity_p);
                gpu_vis_index = scene_entity_p->get_gpu_vis_index();
            }
            if (gpu_vis_index != -1)
            {
                const rl_gpu_vis_state state = rl_gpu_vis_get_state(renderer, gpu_vis_index);
                if (state == RL_GPU_VIS_STATE_OCCLUDED)
                {
                    occluded |= pass_bit;
                    use_occluders = false;
                }
                else if (state != RL_GPU_VIS_STATE_NOT_READY)
                {
                    use_occluders = false;
                }
            }
        }
        if (use_occluders)
        {
            for (int o = 0; o < m_actual_occluders.size(); o++)
            {
                if ((occluder_use_flags & (1u << (o & 0x1F))) != 0 && m_actual_occluders[o]->is_occluded(scene_entity_p))
                {
                    occluded |= pass_bit;
                    break;
                }
            }
        }
    }
    return occluded;
}

// 0x0051CD40 (:854)
// The single-threaded walk (Rl_visibility_threaded off): tests the sector, then its entities, adds the visible ones
// to their lists, and recurses into the children with the passes that still need testing.
// Notes (decompiled behaviour, kept as is): unlike the threaded path this neither calls on_visible_for_frame nor
// applies the LOD-off distance.
void rl_visibility_manager::perform_sector_visibility(rl_renderer& renderer, rl_camera& camera, rl_partition_sector* sector_p, unsigned int pass_fully_visible_flags, unsigned int pass_use_flags, unsigned int parent_occluder_use_flags)
{
    const unsigned int not_fully_visible = ~pass_fully_visible_flags;
    const unsigned int sector_visible = bounding_volume_is_visible_for_passes(sector_p->get_world_bvolume(), 0xFFFFFFFF, 0xFFFFFFFF, not_fully_visible & pass_use_flags, pass_fully_visible_flags) | pass_fully_visible_flags;
    if (sector_visible == 0)
    {
        return;
    }
    unsigned int sector_occluded = 0;
    if (parent_occluder_use_flags != 0)
    {
        sector_occluded = sector_is_occluded_for_passes(sector_p, sector_visible, parent_occluder_use_flags);
    }
    const unsigned int not_occluded = ~sector_occluded;
    if (not_occluded == 0 && !Rl_collect_visiblity_stats)
    {
        return;
    }
    const int visibility_pass = m_last_visibility_pass;
    rl_partition_sector_entry* head = sector_p->get_entries().first();
    for (rl_partition_sector_entry* entry = head; entry != nullptr;)
    {
        rl_scene_entity* entity = entry->m_entity_p;
        unsigned int entity_visible = 0;
        if (category_enabled(m_render_category_filter, entity) && m_visibility_lists[entity->get_visibility_list_index()] != nullptr)
        {
            unsigned int to_test = not_fully_visible & sector_visible;
            if (!Rl_collect_visiblity_stats)
            {
                to_test &= not_occluded;
            }
            entity_visible = entity_is_visible_for_passes(entity, to_test, pass_fully_visible_flags) | pass_fully_visible_flags;
        }
        unsigned int entity_occluded = sector_occluded;
        if (((not_occluded & entity_visible) != 0 && parent_occluder_use_flags != 0) || RL_gpu_vis_enabled)
        {
            entity_occluded = entity_is_occluded_for_passes(renderer, camera, entity, not_occluded & entity_visible, parent_occluder_use_flags) | sector_occluded;
        }
        const unsigned int final_flags = ~entity_occluded & entity_visible;
        if (final_flags != 0)
        {
            list_add(m_visibility_lists[entity->get_visibility_list_index()], entity);
            entity->set_last_visibility_pass(visibility_pass);
            entity->set_vis_pass_flags(final_flags);
            if ((m_shadow_min_max_flags & final_flags) != 0)
            {
                const rl_bvolume& volume = entity->get_world_bvolume();
                const float min_y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
                if (0.0f <= m_shadow_min_y - min_y)
                {
                    m_shadow_min_y = min_y;
                }
                const float max_y = volume.m_half_vec.y + volume.m_sphere.m_position.y;
                if (m_shadow_max_y - max_y < 0.0f)
                {
                    m_shadow_max_y = max_y;
                }
            }
        }
        if (Rl_collect_visiblity_stats && entity_visible != 0)
        {
            m_num_entities_in_frustum += count_bits(entity_visible);
            if (entity_occluded != 0)
            {
                m_num_entities_occluded += count_bits(entity_occluded);
            }
        }
        entry = entry->next;
        if (entry == head)
        {
            break;
        }
    }
    if (sector_p->get_child_count() > 0)
    {
        pass_fully_visible_flags |= sector_is_fully_visible_for_passes(sector_p, not_fully_visible & sector_visible);
    }
    const unsigned int child_use_flags = Rl_collect_visiblity_stats ? sector_visible : (not_occluded & sector_visible);
    for (int i = 0; i < sector_p->get_child_count(); i++)
    {
        if (sector_p->get_child(i) != nullptr)
        {
            perform_sector_visibility(renderer, camera, sector_p->get_child(i), pass_fully_visible_flags, child_use_flags, parent_occluder_use_flags);
        }
    }
}

// 0x0051D220 (:966)
// The threaded walk's first step: collects the visible sectors that have entries (with their pass flags) into
// m_visible_sectors; the entities are tested afterwards.
void rl_visibility_manager::perform_sector_visibility_threaded(rl_partition_sector* sector_p, unsigned int pass_fully_visible_flags, unsigned int pass_use_flags, unsigned int parent_occluder_use_flags)
{
    const unsigned int not_fully_visible = ~pass_fully_visible_flags;
    const unsigned int sector_visible = bounding_volume_is_visible_for_passes(sector_p->get_world_bvolume(), 0xFFFFFFFF, 0xFFFFFFFF, not_fully_visible & pass_use_flags, pass_fully_visible_flags) | pass_fully_visible_flags;
    if (sector_visible == 0)
    {
        return;
    }
    unsigned int sector_occluded = 0;
    if (parent_occluder_use_flags != 0)
    {
        sector_occluded = sector_is_occluded_for_passes(sector_p, sector_visible, parent_occluder_use_flags);
    }
    if (~sector_occluded == 0 && !Rl_collect_visiblity_stats)
    {
        return;
    }
    unsigned int child_fully_visible = pass_fully_visible_flags;
    if (sector_p->get_child_count() > 0)
    {
        child_fully_visible = sector_is_fully_visible_for_passes(sector_p, not_fully_visible & sector_visible) | pass_fully_visible_flags;
    }
    const unsigned int child_use_flags = Rl_collect_visiblity_stats ? sector_visible : (~sector_occluded & sector_visible);
    if (sector_p->get_entries().get_elem_count() > 0)
    {
        visible_sector_entry entry;
        entry.sector_p = sector_p;
        entry.pass_fully_visible_flags = child_fully_visible;
        entry.occluder_use_flags = parent_occluder_use_flags;
        entry.pass_visibility_flags = sector_visible;
        entry.pass_occlusion_flags = sector_occluded;
        m_visible_sectors.add(entry);
    }
    for (int i = 0; i < sector_p->get_child_count(); i++)
    {
        if (sector_p->get_child(i) != nullptr)
        {
            perform_sector_visibility_threaded(sector_p->get_child(i), child_fully_visible, child_use_flags, parent_occluder_use_flags);
        }
    }
}

// 0x00561370 (:1033)
// The frame's visibility: occluders, visible lights, low LOD and landmark LOD entities, then the passes and the walk of
// the main partition system. With Rl_visibility_threaded (retail) the visible sectors are collected first, then each
// entity is tested (and LOD-offed), gets its pass flags and on_visible_for_frame, and finally the entities with pass
// flags are added to their visibility lists.
void rl_visibility_manager::perform_visibility_pass(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p)
{
    m_render_category_filter = scene_p->get_render_category_filter();
    // Magic numbers: the shadow height range starts empty at +-1000 m.
    m_shadow_max_y = -1000.0f;
    m_num_entities_in_frustum = 0;
    m_num_entities_occluded = 0;
    m_num_entities_lod_offed = 0;
    m_shadow_min_y = 1000.0f;
    const unsigned int visibility_pass = scene_p->get_last_visibility_pass() + 1;
    m_last_visibility_pass = visibility_pass;
    scene_p->set_last_visibility_pass(visibility_pass);
    select_occluders(scene_p);
    determine_visible_lights(renderer, camera, scene_p);
    determine_visible_low_lod(renderer, camera, scene_p);
    determine_visible_landmark_lod(renderer, camera, scene_p);
    create_visibility_passes(scene_p, camera);
    rl_partition_sector* root = scene_p->get_partition_system()->get_root_sector();
    const unsigned int occluder_mask = all_occluders_mask(m_actual_occluders);
    if (!Rl_visibility_threaded)
    {
        if (root != nullptr)
        {
            perform_sector_visibility(renderer, camera, root, 0, 0xFFFFFFFF, occluder_mask);
        }
    }
    else
    {
        m_visible_sectors.resize(0);
        if (root != nullptr)
        {
            perform_sector_visibility_threaded(root, 0, 0xFFFFFFFF, occluder_mask);
        }
        for (int s = 0; s < m_visible_sectors.size(); s++)
        {
            const visible_sector_entry& sector = m_visible_sectors[s];
            rl_partition_sector_entry* head = sector.sector_p->get_entries().first();
            for (rl_partition_sector_entry* entry = head; entry != nullptr;)
            {
                rl_scene_entity* entity = entry->m_entity_p;
                unsigned int visible = 0;
                unsigned int occluded = 0;
                test_collected_entity(renderer, camera, sector, entity, visible, occluded);
                // The LOD-off distance (the distance in front of the near plane).
                const vector& position = entity->get_position();
                const vector4& near_plane = m_view_frustum.get_plane_info_array()[rl_view_frustum::NEAR_PLANE].m_plane;
                const bool lod_off = Rl_lod_off_enabled && m_lod_off_enabled && 1.0f <= entity->get_lod_off_distance() &&
                                     entity->get_lod_off_distance() < -(position.y * near_plane.y + position.x * near_plane.x + position.z * near_plane.z + near_plane.w);
                if ((~occluded & visible) == 0 || lod_off)
                {
                    entity->set_vis_pass_flags(0);
                }
                else
                {
                    entity->set_vis_pass_flags(~occluded & visible);
                    entity->set_last_visibility_pass(m_last_visibility_pass);
                    entity->on_visible_for_frame(renderer);
                    // Notes (decompiled behaviour, kept as is): the shadow height range is tested against the
                    // entity's render pass flags here (the single-threaded walk tests its pass flags).
                    if ((m_shadow_min_max_flags & entity->get_render_pass_flags()) != 0)
                    {
                        const rl_bvolume& volume = entity->get_world_bvolume();
                        const float min_y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
                        if (0.0f <= m_shadow_min_y - min_y)
                        {
                            m_shadow_min_y = min_y;
                        }
                        const float max_y = volume.m_half_vec.y + volume.m_sphere.m_position.y;
                        if (m_shadow_max_y - max_y < 0.0f)
                        {
                            m_shadow_max_y = max_y;
                        }
                    }
                }
                if (Rl_collect_visiblity_stats && visible != 0)
                {
                    m_num_entities_in_frustum += count_bits(visible);
                    if (occluded != 0)
                    {
                        m_num_entities_occluded += count_bits(occluded);
                    }
                    if (lod_off)
                    {
                        m_num_entities_lod_offed++;
                    }
                }
                entry = entry->next;
                if (entry == head)
                {
                    break;
                }
            }
        }
        for (int s = 0; s < m_visible_sectors.size(); s++)
        {
            rl_partition_sector_entry* head = m_visible_sectors[s].sector_p->get_entries().first();
            for (rl_partition_sector_entry* entry = head; entry != nullptr;)
            {
                rl_scene_entity* entity = entry->m_entity_p;
                if (entity->get_vis_pass_flags() != 0)
                {
                    pool_list<rl_scene_entity*>* list = m_visibility_lists[entity->get_visibility_list_index()];
                    if (list->size() < list->capacity())
                    {
                        list->add(entity);
                    }
                    else
                    {
                        // Magic number: metrics warning 0x1000, a visibility list overflowed.
                        rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x1000);
                    }
                }
                entry = entry->next;
                if (entry == head)
                {
                    break;
                }
            }
        }
    }
    if (g_visibilityDebugRender)
    {
        Debug::Context context;
        context.pPass = nullptr;
        context.mainCameraPosition = m_viewer_p->get_position();
        context.pDefaultViewFrustum = &m_view_frustum;
        context.pLowLodViewFrustum = &m_low_lod_view_frustum;
        context.pLandmarkViewFrustum = &m_landmark_lod_view_frustum;
        Debug::begin(context);
        Debug::drawVisibilityLists(m_visibility_lists);
        Debug::drawOccluders(m_actual_occluders);
    }
}

// Not in the binary: the entity test of the collected-sector walk (inlined in perform_visibility_pass): the visible
// passes (when the entity's category and visibility list are enabled) and the occluded passes.
void rl_visibility_manager::test_collected_entity(rl_renderer& renderer, rl_camera& camera, const visible_sector_entry& sector, rl_scene_entity* entity, unsigned int& visible, unsigned int& occluded) const
{
    visible = 0;
    if (category_enabled(m_render_category_filter, entity) && m_visibility_lists[entity->get_visibility_list_index()] != nullptr)
    {
        unsigned int to_test = sector.pass_visibility_flags & ~sector.pass_fully_visible_flags;
        if (!Rl_collect_visiblity_stats)
        {
            to_test &= ~sector.pass_occlusion_flags;
        }
        visible = entity_is_visible_for_passes(entity, to_test, sector.pass_fully_visible_flags) | sector.pass_fully_visible_flags;
    }
    occluded = sector.pass_occlusion_flags;
    if (((visible & ~occluded) != 0 && sector.occluder_use_flags != 0) || RL_gpu_vis_enabled)
    {
        occluded = entity_is_occluded_for_passes(renderer, camera, entity, ~sector.pass_occlusion_flags & visible, sector.occluder_use_flags) | sector.pass_occlusion_flags;
    }
}

// 0x004D4BB0 (:1324)
// Front to back by camera distance, with a relative tolerance of 0.0001 (at least 0.0001 absolute).
bool ftb_occluder_sort_predicate(const rl_occluder* lhs, const rl_occluder* rhs)
{
    const float rhs_dist = rhs->get_camera_distance_squared();
    const float lhs_dist = lhs->get_camera_distance_squared();
    float scale = fabsf(lhs_dist);
    if (scale - fabsf(rhs_dist) < 0.0f)
    {
        scale = fabsf(rhs_dist);
    }
    if (0.0f <= 1.0f - scale)
    {
        scale = 1.0f;
    }
    return lhs_dist < rhs_dist - scale * 0.0001f;
}

// 0x0055DD50 (:1345)
// The occluders in the view frustum that are useful from the viewer, sorted front to back; an occluder hidden by an
// earlier one is dropped.
void rl_visibility_manager::select_occluders(rl_scene* scene_p)
{
    m_potential_occluders.resize(0);
    m_actual_occluders.resize(0);
    if (m_merged_occluder_p != nullptr)
    {
        m_merged_occluder_p->release();
        m_merged_occluder_p = nullptr;
    }
    if (!Rl_occlusion_enable)
    {
        return;
    }
    const pool_list<rl_occluder*>& occluders = scene_p->get_occluder_list();
    for (int i = 0; i < occluders.size(); i++)
    {
        rl_occluder* occluder = occluders[i];
        occluder->update(m_viewer_p->get_position());
        if (!m_view_frustum.is_occluded(occluder->get_world_bvolume()) && occluder->is_useful(m_viewer_p))
        {
            m_potential_occluders.add(occluder);
            occluder->set_occluded(false);
        }
    }
    std::sort(m_potential_occluders.elements(), m_potential_occluders.elements() + m_potential_occluders.size(), ftb_occluder_sort_predicate);
    for (int i = 0; i < m_potential_occluders.size(); i++)
    {
        rl_occluder* occluder = m_potential_occluders[i];
        for (int j = i; j < m_potential_occluders.size(); j++)
        {
            rl_occluder* other = m_potential_occluders[j];
            if (i != j && !other->get_occluded())
            {
                other->set_occluded(occluder->is_occluded(other));
            }
        }
    }
    if (m_merged_occluder_p != nullptr)
    {
        m_actual_occluders.add(m_merged_occluder_p);
    }
    for (int i = 0; i < m_potential_occluders.size(); i++)
    {
        if (!m_potential_occluders[i]->get_occluded())
        {
            m_actual_occluders.add(m_potential_occluders[i]);
        }
    }
}

// Sets up the single visibility pass of a separate walk (lights, landmark LOD): the view frustum given as the culling
// frustum.
static void setup_single_frustum_pass(farray<rl_visibility_manager::visibility_pass_info, 31>& passes, const rl_view_frustum& frustum)
{
    passes.resize(0);
    rl_visibility_manager::visibility_pass_info* pass = passes.add_in_place();
    pass->vis_pass_flags = 2;
    const unsigned char index = (unsigned char)(passes.size() - 1);
    pass->pass_index = index;
    pass->pass_index_shifted = 1u << (index & 0x1F);
    rl_frustum culling(frustum);
    pass->culling_frustum = culling;
}

// 0x0051D340 (:1451)
// With secondary lights enabled, the lights of the light partition system that are visible in the view frustum (one
// pass) and not occluded in it.
void rl_visibility_manager::determine_visible_lights(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p)
{
    if (m_visible_light_list == nullptr)
    {
        return;
    }
    m_visible_light_list->resize(0);
    if (!RL_enable_secondary_lights)
    {
        return;
    }
    rl_partition_sector* root = scene_p->get_light_partition_system()->get_root_sector();
    const unsigned int occluder_mask = all_occluders_mask(m_actual_occluders);
    setup_single_frustum_pass(m_visibility_passes, m_view_frustum);
    m_visible_sectors.resize(0);
    if (root != nullptr)
    {
        perform_sector_visibility_threaded(root, 0, 1, occluder_mask);
    }
    for (int s = 0; s < m_visible_sectors.size(); s++)
    {
        const visible_sector_entry& sector = m_visible_sectors[s];
        rl_partition_sector_entry* head = sector.sector_p->get_entries().first();
        for (rl_partition_sector_entry* entry = head; entry != nullptr;)
        {
            rl_light* light = (rl_light*)entry->m_entity_p;
            unsigned int visible = 0;
            if (category_enabled(m_render_category_filter, light) && m_visibility_lists[light->get_visibility_list_index()] != nullptr)
            {
                unsigned int to_test = sector.pass_visibility_flags & ~sector.pass_fully_visible_flags;
                if (!Rl_collect_visiblity_stats)
                {
                    to_test &= ~sector.pass_occlusion_flags;
                }
                visible = entity_is_visible_for_passes(light, to_test, sector.pass_fully_visible_flags) | sector.pass_fully_visible_flags;
            }
            unsigned int occluded = sector.pass_occlusion_flags;
            if (visible != 0)
            {
                if (~occluded != 0 && sector.occluder_use_flags != 0)
                {
                    occluded = entity_is_occluded_for_passes(renderer, camera, light, ~occluded & visible, sector.occluder_use_flags) | sector.pass_occlusion_flags;
                }
                if (occluded != 0xFFFFFFFF)
                {
                    m_visible_light_list->add(light);
                    light->set_last_visibility_pass(m_last_visibility_pass);
                }
            }
            if (Rl_collect_visiblity_stats && visible != 0)
            {
                m_num_entities_in_frustum += count_bits(visible);
                if (occluded != 0)
                {
                    m_num_entities_occluded += count_bits(occluded);
                }
            }
            entry = entry->next;
            if (entry == head)
            {
                break;
            }
        }
    }
}

// Not in the binary: the entity loop of the low LOD and landmark LOD walks (inlined in both): visible entities go to
// their list with their pass flags.
void rl_visibility_manager::add_collected_entities(rl_renderer& renderer, rl_camera& camera)
{
    for (int s = 0; s < m_visible_sectors.size(); s++)
    {
        const visible_sector_entry& sector = m_visible_sectors[s];
        rl_partition_sector_entry* head = sector.sector_p->get_entries().first();
        for (rl_partition_sector_entry* entry = head; entry != nullptr;)
        {
            rl_scene_entity* entity = entry->m_entity_p;
            unsigned int visible = 0;
            if (category_enabled(m_render_category_filter, entity) && m_visibility_lists[entity->get_visibility_list_index()] != nullptr)
            {
                unsigned int to_test = sector.pass_visibility_flags & ~sector.pass_fully_visible_flags;
                if (!Rl_collect_visiblity_stats)
                {
                    to_test &= ~sector.pass_occlusion_flags;
                }
                visible = entity_is_visible_for_passes(entity, to_test, sector.pass_fully_visible_flags) | sector.pass_fully_visible_flags;
            }
            unsigned int occluded = sector.pass_occlusion_flags;
            if ((visible & ~occluded) != 0 && sector.occluder_use_flags != 0)
            {
                occluded = entity_is_occluded_for_passes(renderer, camera, entity, ~occluded & visible, sector.occluder_use_flags) | sector.pass_occlusion_flags;
            }
            const unsigned int final_flags = ~occluded & visible;
            if (final_flags != 0)
            {
                m_visibility_lists[entity->get_visibility_list_index()]->add(entity);
                entity->set_vis_pass_flags(final_flags);
                entity->set_last_visibility_pass(m_last_visibility_pass);
            }
            if (Rl_collect_visiblity_stats && visible != 0)
            {
                m_num_entities_in_frustum += count_bits(visible);
                if (occluded != 0)
                {
                    m_num_entities_occluded += count_bits(occluded);
                }
            }
            entry = entry->next;
            if (entry == head)
            {
                break;
            }
        }
    }
}

// 0x0051D8F0 (:1540)
// The landmark LOD entities visible in the landmark view frustum (one pass).
void rl_visibility_manager::determine_visible_landmark_lod(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p)
{
    rl_partition_sector* root = scene_p->get_landmark_lod_partition_system()->get_root_sector();
    const unsigned int occluder_mask = all_occluders_mask(m_actual_occluders);
    setup_single_frustum_pass(m_visibility_passes, m_landmark_lod_view_frustum);
    m_visible_sectors.resize(0);
    if (root != nullptr)
    {
        perform_sector_visibility_threaded(root, 0, 1, occluder_mask);
    }
    add_collected_entities(renderer, camera);
}

// 0x005473A0 (:1620)
// The low LOD entities: the full pass set with the base pass culled by the low LOD view frustum, walked for the base
// pass and the shadow passes.
void rl_visibility_manager::determine_visible_low_lod(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p)
{
    create_visibility_passes(scene_p, camera);
    rl_frustum culling(m_low_lod_view_frustum);
    m_visibility_passes[m_base_pass_vis_index].culling_frustum = culling;
    rl_partition_sector* root = scene_p->get_low_lod_partition_system()->get_root_sector();
    const unsigned int occluder_mask = all_occluders_mask(m_actual_occluders);
    m_visible_sectors.resize(0);
    if (root != nullptr)
    {
        perform_sector_visibility_threaded(root, 0, (1u << (m_base_pass_vis_index & 0x1F)) | m_shadow_vis_flags, occluder_mask);
    }
    add_collected_entities(renderer, camera);
}

// 0x00534DE0 (:1716)
// The visibility passes of the frame, in order: base (view frustum, GPU visibility when enabled), the CLB shadow
// cascades near/mid/far (+ terrain extended) or a full shadow pass, the drop shadow, distortion (render pass flag 1,
// depends on the base pass), the outline layers (render pass flags 0x10 << layer), x-ray (when enabled), and a
// sphere pass (plus a frustum and a shadow pass for spot lights) per visible secondary light while there are passes
// left.
void rl_visibility_manager::create_visibility_passes(rl_scene* scene_p, rl_camera& camera)
{
    m_visibility_passes.resize(0);
    m_non_shadow_vis_flags = 0;
    m_shadow_vis_flags = 0;
    m_shadow_min_max_flags = 0;

    visibility_pass_info* base = m_visibility_passes.add_in_place();
    base->vis_pass_flags = RL_gpu_vis_enabled ? 0x42 : 0x02;
    const unsigned short base_index = (unsigned short)(m_visibility_passes.size() - 1);
    base->pass_index = base_index;
    base->pass_index_shifted = 1u << (base_index & 0x1F);
    rl_frustum view_culling(m_view_frustum);
    base->culling_frustum = view_culling;
    m_base_pass_vis_index = (unsigned char)base->pass_index;
    m_non_shadow_vis_flags |= 1u << (base->pass_index & 0x1F);
    if (RL_vis_use_frustum)
    {
        // The sphere around the viewer camera's frustum.
        base->vis_pass_flags |= 4;
        const rl_camera* viewer = (const rl_camera*)m_viewer_p;
        const float near_clip = viewer->get_near_clip();
        const float range = viewer->get_far_clip() - near_clip;
        const float fov = viewer->get_fov();
        float horizontal_fov;
        if (viewer->get_type() == rl_camera::RLCT_PERSPECTIVE)
        {
            horizontal_fov = calculate_horizontal_fov_perspective(fov, false);
        }
        else
        {
            horizontal_fov = rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * fov;
        }
        const double half_range = (double)range * 0.5;
        const float middle = (float)((double)near_clip + half_range);
        // Magic number: 0.017453 (degrees to radians).
        const float x = (float)tan((double)(horizontal_fov * 0.017453f * 0.5f)) * range;
        const float y = (float)tan((double)(fov * 0.017453f * 0.5f)) * range;
        const float dy = 0.0f - y;
        base->bounding_sphere.m_radius = sqrtf((0.0f - x) * (0.0f - x) + dy * dy + (middle - range) * (middle - range));
        // Notes (decompiled behaviour, kept as is): the near clip distance is added to every coordinate of the centre
        // instead of along the forward vector.
        const vector& position = m_viewer_p->get_position();
        const vector& fvec = m_viewer_p->get_rotation().fvec;
        const float half = (float)half_range;
        base->bounding_sphere.m_position = vector(half * fvec.x + near_clip + position.x, position.y + fvec.y * half + near_clip, position.z + fvec.z * half + near_clip);
    }

    rl_light* largest_light = scene_p->get_largest_light();
    if (!Rl_shadows_enabled || rl_shadow_system::m_shadow_mode != RLSM_CLB)
    {
        m_clb_base_vis_index = 0xFF;
    }
    else
    {
        // Magic numbers: scene entity flags 0x40 (casts shadows), 0x400 (terrain casting shadows).
        const unsigned int caster_flags = rl_shadow_system::m_terrain_casts_shadows ? 0x440 : 0x40;
        const rl_shadow_map_range ranges[3] = { SMR_NEAR, SMR_MID, SMR_FAR };
        visibility_pass_info* near_pass = nullptr;
        for (int r = 0; r < 3; r++)
        {
            visibility_pass_info* pass = m_visibility_passes.add_in_place();
            pass->se_culling_flags = caster_flags;
            pass->vis_pass_flags = 0x32;
            const unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
            pass->pass_index = index;
            pass->pass_index_shifted = 1u << (index & 0x1F);
            rl_shadow_system::get_instance()->build_shadow_culling_frustum(pass->culling_frustum, &camera, largest_light, ranges[r]);
            m_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
            if (r == 0)
            {
                m_shadow_min_max_flags |= 1u << (pass->pass_index & 0x1F);
                near_pass = pass;
            }
        }
        if (rl_shadow_system::m_terrain_casts_shadows && rl_shadow_system::m_terrain_casts_shadows_extended)
        {
            visibility_pass_info* pass = m_visibility_passes.add_in_place();
            pass->vis_pass_flags = 0x32;
            pass->se_culling_flags = 0x400;
            const unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
            pass->pass_index = index;
            pass->pass_index_shifted = 1u << (index & 0x1F);
            rl_shadow_system::get_instance()->build_shadow_culling_frustum(pass->culling_frustum, &camera, largest_light, SMR_TERRAIN_EXTENDED);
            m_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
        }
        m_clb_base_vis_index = (unsigned char)near_pass->pass_index;
    }
    if (!Rl_shadows_enabled || rl_shadow_system::m_shadow_mode == RLSM_CLB)
    {
        m_base_shadow_vis_index = 0xFF;
    }
    else
    {
        // Notes (decompiled behaviour, kept as is): m_base_shadow_vis_index is not set in this branch.
        visibility_pass_info* pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 0x32;
        pass->se_culling_flags = 0x40;
        const unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
        pass->pass_index = index;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        rl_shadow_system::get_instance()->build_shadow_culling_frustum(pass->culling_frustum, &camera, largest_light, SMR_FULL);
        m_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
    }
    if (!g_dropShadowEnabled || rl_shadow_system::m_drop_shadow_light == nullptr)
    {
        m_drop_shadow_vis_index = 0xFF;
    }
    else
    {
        visibility_pass_info* pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 0x32;
        const unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
        pass->pass_index = index;
        // Magic number: scene entity flag 0x100 (casts drop shadows).
        pass->se_culling_flags = 0x100;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        rl_shadow_system::get_instance()->build_drop_shadow_culling_frustum(pass->culling_frustum, &camera, rl_shadow_system::m_drop_shadow_light);
        m_drop_shadow_vis_index = (unsigned char)pass->pass_index;
        m_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
    }

    // Distortion: render pass flag 1, visible where the base pass is.
    visibility_pass_info* distortion = m_visibility_passes.add_in_place();
    distortion->vis_pass_flags = 9;
    const unsigned short distortion_index = (unsigned short)(m_visibility_passes.size() - 1);
    distortion->pass_index = distortion_index;
    distortion->ri_pass_culling_flags = 1;
    distortion->pass_index_shifted = 1u << (distortion_index & 0x1F);
    distortion->dependent_pass_index = 0;
    m_distortion_vis_index = (unsigned char)distortion_index;
    m_non_shadow_vis_flags |= 1u << (distortion->pass_index & 0x1F);

    rl_outline_state outline_state;
    scene_p->get_outline_state(outline_state);
    for (int layer = 0; layer < 4; layer++)
    {
        if (outline_state.layer[layer] == RLOL_DISABLED)
        {
            m_outline_vis_index[layer] = 0xFF;
            continue;
        }
        visibility_pass_info* pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 9;
        const unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
        pass->pass_index = index;
        pass->dependent_pass_index = 0;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        // Magic number: render pass flags 0x10 << layer, the outline layers.
        pass->ri_pass_culling_flags = 0x10u << (layer & 0x1F);
        m_outline_vis_index[layer] = (unsigned char)pass->pass_index;
        m_non_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
    }

    rl_xray_render_state xray;
    scene_p->get_xray_render_state(xray);
    if (xray.m_xray_effect_enabled)
    {
        const float radius = xray.m_xray_effect_fade_dist + xray.m_xray_effect_fade_start;
        visibility_pass_info* pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 0xD;
        unsigned short index = (unsigned short)(m_visibility_passes.size() - 1);
        pass->pass_index = index;
        // Magic number: render pass flag 0x100, the x-ray pass.
        pass->ri_pass_culling_flags = 0x100;
        pass->dependent_pass_index = 0;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        pass->bounding_sphere.m_position = m_viewer_p->get_position();
        pass->bounding_sphere.m_radius = radius;
        m_xray_pass_vis_index = (unsigned char)index;
        m_non_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
        pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 0x15;
        index = (unsigned short)(m_visibility_passes.size() - 1);
        pass->pass_index = index;
        pass->dependent_pass_index = 0;
        // Magic number: scene entity flag 8, terrain.
        pass->se_culling_flags = 8;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        pass->bounding_sphere.m_position = m_viewer_p->get_position();
        pass->bounding_sphere.m_radius = radius;
        m_xray_terrain_vis_index = (unsigned char)index;
        m_non_shadow_vis_flags |= 1u << (pass->pass_index & 0x1F);
    }
    else
    {
        m_xray_pass_vis_index = 0xFF;
        m_xray_terrain_vis_index = 0xFF;
    }

    for (int i = 0; i < m_visible_light_list->size(); i++)
    {
        rl_light* light = (*m_visible_light_list)[i];
        // Magic number: 31 passes at most (the pass bits of a 32-bit mask).
        if (m_visibility_passes.size() >= 0x1F)
        {
            light->set_vis_pass_indices(0xFFFF, 0xFFFF);
            continue;
        }
        visibility_pass_info* pass = m_visibility_passes.add_in_place();
        pass->vis_pass_flags = 5;
        pass->bounding_sphere.m_position = light->get_position();
        pass->bounding_sphere.m_radius = light->get_attenuation_end();
        // Notes (decompiled behaviour, kept as is): the light passes take the pass count after the add as their index
        // (one past their slot), unlike the other passes.
        const unsigned short index = (unsigned short)m_visibility_passes.size();
        pass->pass_index = index;
        pass->pass_index_shifted = 1u << (index & 0x1F);
        pass->dependent_pass_index = 0;
        m_non_shadow_vis_flags |= 1u << (index & 0x1F);
        const unsigned int render_index = pass->pass_index;
        unsigned int shadow_index = 0xFFFFFFFF;
        if (light->get_light_type() == rl_light::LIGHT_TYPE_SPOT_LIGHT)
        {
            // Magic number: 57.29583 (radians to degrees); the frustum is twice the falloff angle wide.
            rl_view_frustum spot_frustum;
            vector fvec;
            light->get_spotlight_fvec(fvec);
            const float fov = light->get_hotspot_falloff_size() * 2.0f * 57.29583f;
            matrix orient;
            orient.make_quick(fvec);
            spot_frustum.init(light->get_position(), orient, fov, fov, 0.0f, light->get_attenuation_end());
            pass->vis_pass_flags |= 2;
            rl_frustum spot_culling(spot_frustum);
            pass->culling_frustum = spot_culling;
            if (m_visibility_passes.size() < 0x1F)
            {
                visibility_pass_info* shadow_pass = m_visibility_passes.add_in_place();
                shadow_pass->vis_pass_flags = 0x36;
                shadow_pass->se_culling_flags = 0x40;
                shadow_pass->bounding_sphere.m_position = light->get_position();
                shadow_pass->bounding_sphere.m_radius = light->get_attenuation_end();
                const unsigned short shadow_pass_index = (unsigned short)m_visibility_passes.size();
                shadow_pass->pass_index = shadow_pass_index;
                shadow_pass->pass_index_shifted = 1u << (shadow_pass_index & 0x1F);
                rl_frustum shadow_culling(spot_frustum);
                shadow_pass->culling_frustum = shadow_culling;
                shadow_index = shadow_pass->pass_index;
            }
        }
        light->set_vis_pass_indices(render_index, shadow_index);
    }
}

// 0x00501E60 (:2009)
bool rl_visibility_manager::entity_is_visible_in_occlusion(rl_scene_entity* scene_entity_p, unsigned int occluder_use_flags) const
{
    for (int i = 0; i < m_actual_occluders.size(); i++)
    {
        if ((occluder_use_flags & (1u << (i & 0x1F))) != 0 && m_actual_occluders[i]->is_occluded(scene_entity_p))
        {
            return false;
        }
    }
    return true;
}

// 0x00501EC0 (:2026)
// Occluders that do not even partially cover the sector are dropped from occluder_use_flags.
bool rl_visibility_manager::sector_is_visible_in_occlusion(rl_partition_sector* sector_p, unsigned int& occluder_use_flags) const
{
    for (int i = 0; i < m_actual_occluders.size(); i++)
    {
        rl_occluder* occluder = m_actual_occluders[i];
        const unsigned int occluder_bit = 1u << (i & 0x1F);
        if ((occluder_use_flags & occluder_bit) != 0)
        {
            if (occluder->is_occluded(sector_p))
            {
                return false;
            }
            if (!occluder->is_partially_occluded(sector_p))
            {
                occluder_use_flags &= ~occluder_bit;
            }
        }
    }
    return true;
}

// 0x004BCCA0 (.h:87)
unsigned int rl_visibility_manager::get_base_pass_vis_flags()
{
    return 1u << (m_base_pass_vis_index & 0x1F);
}

// 0x004BCCB0 (.h:88)
unsigned int rl_visibility_manager::get_clb_near_vis_flags()
{
    return 1u << (m_clb_base_vis_index & 0x1F);
}

// 0x004BCCC0 (.h:89)
unsigned int rl_visibility_manager::get_clb_mid_vis_flags()
{
    return 1u << ((m_clb_base_vis_index + 1) & 0x1F);
}

// 0x004BCCD0 (.h:90)
unsigned int rl_visibility_manager::get_clb_far_vis_flags()
{
    return 1u << ((m_clb_base_vis_index + 2) & 0x1F);
}

// 0x004BCCF0 (.h:91)
unsigned int rl_visibility_manager::get_clb_terrain_vis_flags()
{
    return 1u << ((m_clb_base_vis_index + 3) & 0x1F);
}

// 0x004BCD10 (.h:92)
unsigned int rl_visibility_manager::get_drop_shadow_vis_flags()
{
    return 1u << (m_drop_shadow_vis_index & 0x1F);
}

// 0x004BCD20 (.h:93)
unsigned int rl_visibility_manager::get_distortion_vis_flags()
{
    return 1u << (m_distortion_vis_index & 0x1F);
}

// 0x004BCD30 (.h:95)
unsigned int rl_visibility_manager::get_outline_vis_flags(int pass)
{
    return 1u << (m_outline_vis_index[pass] & 0x1F);
}

// 0x004BCD50 (.h:96)
unsigned int rl_visibility_manager::get_xray_pass_vis_flags()
{
    return 1u << (m_xray_pass_vis_index & 0x1F);
}

// 0x004BCD60 (.h:97)
unsigned int rl_visibility_manager::get_xray_terrain_vis_flags()
{
    return 1u << (m_xray_terrain_vis_index & 0x1F);
}

// 0x004BCD70 (.h:99)
unsigned int rl_visibility_manager::get_non_shadow_vis_flags()
{
    return m_non_shadow_vis_flags;
}

// 0x004BCD80 (.h:100)
unsigned int rl_visibility_manager::get_shadow_vis_flags()
{
    return m_shadow_vis_flags;
}

// 0x004BCD90 (.h:102)
float rl_visibility_manager::get_shadow_min_y()
{
    return m_shadow_min_y;
}

// 0x004BCDA0 (.h:103)
float rl_visibility_manager::get_shadow_max_y()
{
    return m_shadow_max_y;
}

// 0x004BCDB0 (.h:220)
void rl_visibility_manager::set_view_frustum(const rl_view_frustum& view_frustum)
{
    memcpy(&m_view_frustum, &view_frustum, sizeof(rl_view_frustum));
}

// 0x004BCDD0 (.h:225)
void rl_visibility_manager::set_low_lod_view_frustum(const rl_view_frustum& view_frustum)
{
    memcpy(&m_low_lod_view_frustum, &view_frustum, sizeof(rl_view_frustum));
}

// 0x004BCDF0 (.h:230)
void rl_visibility_manager::set_landmark_lod_view_frustum(const rl_view_frustum& view_frustum)
{
    memcpy(&m_landmark_lod_view_frustum, &view_frustum, sizeof(rl_view_frustum));
}

// 0x004BCE10 (.h:235)
void rl_visibility_manager::set_viewer(rl_3d_entity* viewer_p)
{
    m_viewer_p = viewer_p;
}

// 0x007C1EB0 (.h:240)
void rl_visibility_manager::set_lod_off_enabled(bool enabled)
{
    m_lod_off_enabled = enabled;
}

// 0x004E3970 (.h:245)
void rl_visibility_manager::set_visibility_list(visibility_lists index, pool_list<rl_scene_entity*>* visibility_list_p)
{
    m_visibility_lists.elements()[index] = visibility_list_p;
}

// 0x004BCE20 (.h:250)
void rl_visibility_manager::set_visible_light_list(pool_list<rl_light*>* visibility_list_p)
{
    m_visible_light_list = visibility_list_p;
}

const pool_list<rl_occluder*>& rl_visibility_manager::get_potential_occluders() const
{
    return m_potential_occluders;
}

const pool_list<rl_occluder*>& rl_visibility_manager::get_actual_occluders() const
{
    return m_actual_occluders;
}

unsigned int rl_visibility_manager::get_num_entities_in_view_frustum() const
{
    return m_num_entities_in_frustum;
}

unsigned int rl_visibility_manager::get_num_entities_occluded() const
{
    return m_num_entities_occluded;
}

unsigned int rl_visibility_manager::get_num_entities_lod_offed() const
{
    return m_num_entities_lod_offed;
}
