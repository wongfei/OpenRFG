// Original: volition/render_lib/code/rl_scene_entity.cpp
//
// rl_scene_entity: an rl_3d_entity placed in a scene. Moving it or changing its local volume marks the world bounding
// volume dirty (recomputed on demand by compute_bounding_volume) and, with RL_auto_partition_update, re-inserts it into
// its partition system. Hidden entities (FLAGS_NOT_VISIBLE) are not in the partition system. The header-inline members
// of rl_scene_entity.h are defined here as well.

#include "volition/render_lib/code/rl_scene_entity.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/math/bbox.h"

#include <math.h>

// The per-category debug colors (debug visualizations only; not initialized in the binary's .data).
rl_color_float rl_scene_entity::ms_debug_colors[256]; // 0x01515230

// 0x004F5C60 (:24)
// A unit sphere and unit box at the origin for both volumes, not visible-hidden, in the active scene.
rl_scene_entity::rl_scene_entity()
{
    m_lod_off_distance = 0.0f;
    m_flags = 0;
    m_pass_flags = 0;
    m_scene_p = nullptr;
    m_last_visibility_pass = 0;
    m_partition_system_p = nullptr;
    m_gpu_vis_index = -1;
    m_world_bvolume.m_sphere.m_position = ZERO_VECTOR;
    m_world_bvolume.m_sphere.m_radius = 1.0f;
    m_world_bvolume.m_half_vec.x = 1.0f;
    m_world_bvolume.m_half_vec.y = 1.0f;
    m_world_bvolume.m_half_vec.z = 1.0f;
    m_local_bvolume.m_sphere.m_position = ZERO_VECTOR;
    m_local_bvolume.m_sphere.m_radius = 1.0f;
    m_local_bvolume.m_half_vec.x = 1.0f;
    m_local_bvolume.m_half_vec.y = 1.0f;
    m_local_bvolume.m_half_vec.z = 1.0f;
    m_flags = 0;
    m_partition_sector_entry.m_sector_p = nullptr;
    m_category_id = 0;
    m_scene_p = Active_scene_p;
    m_partition_sector_entry.m_entity_p = this;
    m_visibility_list_index = 0xFFFF;
}

// 0x0052DDC0 (:46)
// Frees the entity's GPU visibility query (rl_gpu_vis_free, inlined).
rl_scene_entity::~rl_scene_entity()
{
    if (m_gpu_vis_index != -1)
    {
        rl_gpu_vis_free(m_gpu_vis_index);
        m_gpu_vis_index = -1;
    }
}

// 0x004F5DA0 (.h:147)
bool rl_scene_entity::process_internal_visibility(const rl_frustum& frustum, const linked_list_p<rl_occluder>& occluders)
{
    return true;
}

// 0x004F5DB0 (.h:150)
void rl_scene_entity::on_visible_for_frame(rl_renderer&)
{
}

// 0x004B8700 (.h:153)
unsigned int rl_scene_entity::get_flags() const
{
    return m_flags;
}

// Marks the world volume dirty (and clears FLAGS_GPU_OCCLUSION_CACHED, inlined set_bounding_volume_dirty(true)) and
// re-inserts a visible entity into its partition system when automatic partition updates are on. Inlined in
// set_position, set_rotation, set_local_aabb and set_local_bounding_sphere.
static void moved(rl_scene_entity* entity, unsigned short& flags, rl_partition_system* partition_system_p)
{
    const unsigned short old_flags = flags;
    flags = (old_flags & ~rl_scene_entity::FLAGS_GPU_OCCLUSION_CACHED) | rl_scene_entity::FLAGS_WORLD_BOUNDING_VOLUME_DIRTY;
    if (RL_auto_partition_update() && partition_system_p != nullptr && (old_flags & rl_scene_entity::FLAGS_NOT_VISIBLE) == 0)
    {
        partition_system_p->insert(entity);
    }
}

// 0x004DD140 (.h:239)
void rl_scene_entity::set_local_aabb(const rl_aabb& local_aabb)
{
    m_local_bvolume.set(local_aabb);
    moved(this, m_flags, m_partition_system_p);
}

// 0x00508E80 (.h:249)
rl_aabb rl_scene_entity::get_local_aabb() const
{
    rl_aabb aabb;
    m_local_bvolume.get_aabb(aabb);
    return aabb;
}

// 0x00B1FB00 (.h:254)
void rl_scene_entity::get_local_aabb(rl_aabb& aabb) const
{
    m_local_bvolume.get_aabb(aabb);
}

// 0x004DD2F0 (.h:259)
void rl_scene_entity::set_local_bounding_sphere(const rl_sphere& local_bounding_sphere)
{
    m_local_bvolume.set(local_bounding_sphere);
    moved(this, m_flags, m_partition_system_p);
}

// 0x007F2090 (.h:269)
rl_sphere rl_scene_entity::get_local_bounding_sphere() const
{
    return m_local_bvolume.m_sphere;
}

// 0x004DD370 (.h:279)
// The world volume is the box of the local box transformed by the world matrix (bbox_instance); clears the dirty flag.
void rl_scene_entity::compute_bounding_volume() const
{
    rl_scene_entity* self = const_cast<rl_scene_entity*>(this);
    vector local_min;
    vector local_max;
    local_min.x = m_local_bvolume.m_sphere.m_position.x - m_local_bvolume.m_half_vec.x;
    local_min.y = m_local_bvolume.m_sphere.m_position.y - m_local_bvolume.m_half_vec.y;
    local_min.z = m_local_bvolume.m_sphere.m_position.z - m_local_bvolume.m_half_vec.z;
    local_max.x = m_local_bvolume.m_half_vec.x + m_local_bvolume.m_sphere.m_position.x;
    local_max.y = m_local_bvolume.m_half_vec.y + m_local_bvolume.m_sphere.m_position.y;
    local_max.z = m_local_bvolume.m_half_vec.z + m_local_bvolume.m_sphere.m_position.z;
    vector world_min;
    vector world_max;
    const matrix43& world = get_world_transform();
    bbox_instance(world_min, world_max, world.m_rotation, world.m_translation, local_min, local_max);
    rl_bvolume& volume = self->m_world_bvolume;
    volume.m_half_vec.x = (world_max.x - world_min.x) * 0.5f;
    volume.m_half_vec.y = (world_max.y - world_min.y) * 0.5f;
    volume.m_half_vec.z = (world_max.z - world_min.z) * 0.5f;
    volume.m_sphere.m_position.x = world_min.x + volume.m_half_vec.x;
    volume.m_sphere.m_position.y = volume.m_half_vec.y + world_min.y;
    volume.m_sphere.m_position.z = volume.m_half_vec.z + world_min.z;
    volume.m_sphere.m_radius = sqrtf(volume.m_half_vec.y * volume.m_half_vec.y + volume.m_half_vec.x * volume.m_half_vec.x + volume.m_half_vec.z * volume.m_half_vec.z);
    self->m_flags &= ~FLAGS_WORLD_BOUNDING_VOLUME_DIRTY;
}

// 0x004DD5C0 (.h:293)
const rl_bvolume& rl_scene_entity::get_world_bvolume() const
{
    if ((m_flags & FLAGS_WORLD_BOUNDING_VOLUME_DIRTY) != 0)
    {
        compute_bounding_volume();
    }
    return m_world_bvolume;
}

// 0x004DD5E0 (.h:303)
rl_sphere rl_scene_entity::get_world_sphere() const
{
    if ((m_flags & FLAGS_WORLD_BOUNDING_VOLUME_DIRTY) != 0)
    {
        compute_bounding_volume();
    }
    return m_world_bvolume.m_sphere;
}

// 0x00C9E1C0 (.h:321)
// Notes (decompiled behaviour, kept as is): the dirty flag is not touched.
void rl_scene_entity::set_world_bounding_sphere(const rl_sphere& world_sphere)
{
    m_world_bvolume.set(world_sphere);
}

// 0x00508F70 (.h:327)
rl_aabb rl_scene_entity::get_world_aabb() const
{
    if ((m_flags & FLAGS_WORLD_BOUNDING_VOLUME_DIRTY) != 0)
    {
        compute_bounding_volume();
    }
    rl_aabb aabb;
    m_world_bvolume.get_aabb(aabb);
    return aabb;
}

// 0x007451A0 (.h:336)
void rl_scene_entity::get_world_aabb(rl_aabb& aabb) const
{
    if ((m_flags & FLAGS_WORLD_BOUNDING_VOLUME_DIRTY) != 0)
    {
        compute_bounding_volume();
    }
    m_world_bvolume.get_aabb(aabb);
}

// 0x004B8730 (.h:345)
// Notes (decompiled behaviour, kept as is): the dirty flag is not touched, so a later move recomputes the volume from
// the local box.
void rl_scene_entity::set_world_aabb(const rl_aabb& world_aabb)
{
    m_world_bvolume.set(world_aabb);
}

// 0x004DD610 (.h:351)
void rl_scene_entity::set_bounding_volume_dirty(bool dirty)
{
    if (dirty)
    {
        m_flags = (m_flags & ~FLAGS_GPU_OCCLUSION_CACHED) | FLAGS_WORLD_BOUNDING_VOLUME_DIRTY;
    }
    else
    {
        m_flags &= ~FLAGS_WORLD_BOUNDING_VOLUME_DIRTY;
    }
}

// 0x004DD640 (.h:362)
void rl_scene_entity::set_position(const vector& position)
{
    rl_3d_entity::set_position(position);
    moved(this, m_flags, m_partition_system_p);
}

// 0x004DD690 (.h:372)
void rl_scene_entity::set_rotation(const matrix& rotation)
{
    rl_3d_entity::set_rotation(rotation);
    moved(this, m_flags, m_partition_system_p);
}

// 0x004B8890 (.h:382)
void rl_scene_entity::update_partition_system()
{
    if (m_partition_system_p != nullptr && (m_flags & FLAGS_NOT_VISIBLE) == 0)
    {
        m_partition_system_p->insert(this);
    }
}

// 0x004B88B0 (.h:390)
const vector& rl_scene_entity::get_position() const
{
    return rl_3d_entity::get_position();
}

// 0x004B88C0 (.h:395)
const matrix& rl_scene_entity::get_rotation() const
{
    return rl_3d_entity::get_rotation();
}

// 0x004B88D0 (.h:401)
void rl_scene_entity::set_last_visibility_pass(unsigned int last_visibility_pass)
{
    m_last_visibility_pass = (unsigned char)last_visibility_pass;
}

// 0x004B88E0 (.h:412)
rl_scene* rl_scene_entity::get_scene() const
{
    return m_scene_p;
}

// 0x004B88F0 (.h:417)
void rl_scene_entity::scene_removed()
{
    m_scene_p = nullptr;
}

// 0x004B8900 (.h:422)
void rl_scene_entity::scene_added(rl_scene* scene_p)
{
    m_scene_p = scene_p;
}

// 0x004B8910 (.h:427)
bool rl_scene_entity::get_is_visible() const
{
    return (m_flags & FLAGS_NOT_VISIBLE) == 0;
}

// 0x004B8920 (.h:432)
void rl_scene_entity::set_is_visible(bool is_visible)
{
    if (!is_visible)
    {
        m_flags |= FLAGS_NOT_VISIBLE;
    }
    else
    {
        m_flags &= ~FLAGS_NOT_VISIBLE;
    }
}

// 0x006E1380 (.h:441)
// The flag stores "not occluded".
void rl_scene_entity::set_gpu_occluded(bool is_gpu_occluded)
{
    if (is_gpu_occluded)
    {
        m_flags &= ~FLAGS_NOT_GPU_OCCLUDED;
    }
    else
    {
        m_flags |= FLAGS_NOT_GPU_OCCLUDED;
    }
}

// 0x004B8940 (.h:450)
bool rl_scene_entity::get_gpu_occluded() const
{
    return (m_flags & FLAGS_NOT_GPU_OCCLUDED) == 0;
}

// 0x005D0B70 (.h:459)
void rl_scene_entity::set_is_terrain(bool is_terrain)
{
    if (is_terrain)
    {
        m_flags |= FLAGS_TERRAIN;
    }
    else
    {
        m_flags &= ~FLAGS_TERRAIN;
    }
}

// 0x004B8950 (.h:468)
bool rl_scene_entity::get_is_terrain() const
{
    return (m_flags & FLAGS_TERRAIN) != 0;
}

// 0x004B8960 (.h:477)
void rl_scene_entity::set_is_gpu_occlusion_cached(bool is_gpu_occlusion_cached)
{
    if (is_gpu_occlusion_cached)
    {
        m_flags |= FLAGS_GPU_OCCLUSION_CACHED;
    }
    else
    {
        m_flags &= ~FLAGS_GPU_OCCLUSION_CACHED;
    }
}

// 0x004B8980 (.h:486)
bool rl_scene_entity::get_is_gpu_occlusion_cached() const
{
    return (m_flags & FLAGS_GPU_OCCLUSION_CACHED) != 0;
}

// 0x004B8990 (.h:495)
float rl_scene_entity::get_camera_distance_squared() const
{
    return m_camera_distance_squared;
}

// 0x004B89A0 (.h:500)
void rl_scene_entity::set_camera_distance_squared(float camera_distance_squared)
{
    m_camera_distance_squared = camera_distance_squared;
}

// 0x004B89C0 (.h:506)
const rl_color_float& rl_scene_entity::get_debug_color() const
{
    return ms_debug_colors[m_category_id];
}

// 0x004B89D0 (.h:517)
int rl_scene_entity::get_visibility_list_index() const
{
    return m_visibility_list_index;
}

// 0x004B89E0 (.h:522)
void rl_scene_entity::set_visibility_list_index(int visibility_list_index)
{
    m_visibility_list_index = (unsigned short)visibility_list_index;
}

// 0x004B89F0 (.h:527)
unsigned int rl_scene_entity::get_vis_pass_flags() const
{
    return m_vis_pass_flags;
}

// 0x004B8A00 (.h:532)
void rl_scene_entity::set_vis_pass_flags(unsigned int vis_pass_flags)
{
    m_vis_pass_flags = vis_pass_flags;
}

// 0x004B8A10 (.h:538)
unsigned short rl_scene_entity::get_render_pass_flags() const
{
    return m_pass_flags;
}

// 0x004B8A20 (.h:548)
float rl_scene_entity::get_lod_off_distance() const
{
    return m_lod_off_distance;
}

// 0x00710DF0 (.h:553)
void rl_scene_entity::set_lod_off_distance(float lod_off_distance)
{
    m_lod_off_distance = lod_off_distance;
}

// 0x004B8A30 (.h:558)
int rl_scene_entity::get_gpu_vis_index() const
{
    return m_gpu_vis_index;
}

// 0x004B8A40 (.h:563)
void rl_scene_entity::set_gpu_vis_index(int gpu_vis_index)
{
    m_gpu_vis_index = gpu_vis_index;
}

// 0x004B8A50 (.h:568)
rl_partition_system* rl_scene_entity::get_partition_system() const
{
    return m_partition_system_p;
}

// 0x004B8A60 (.h:573)
void rl_scene_entity::set_partition_system(rl_partition_system* partition_system_p)
{
    m_partition_system_p = partition_system_p;
}

// 0x004B8A70 (.h:579)
rl_partition_sector_entry* rl_scene_entity::get_partition_sector_entry()
{
    return &m_partition_sector_entry;
}

// 0x004B8A80 (.h:584)
void rl_scene_entity::set_category_id(unsigned char id)
{
    m_category_id = id;
}

// 0x004B8A90 (.h:589)
unsigned char rl_scene_entity::get_category_id()
{
    return m_category_id;
}
