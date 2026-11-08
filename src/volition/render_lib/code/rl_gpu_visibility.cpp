// Original: volition/render_lib/code/rl_gpu_visibility.cpp
//
// GPU visibility: one occlusion query box per scene entity of the base pass. The visibility pass polls the results of
// earlier frames (rl_gpu_vis_get_state, through entity_is_occluded_for_passes) and the scene renderer issues this
// frame's queries against the depth pre-pass (rl_gpu_vis_render). Results are therefore at least one frame old.

#include "volition/render_lib/code/rl_gpu_visibility.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_fog_volume.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/occlusionquery.hpp"

#include <math.h>
#include <string.h>

// The query box vertices (the fog volume's layout).
typedef rl_fog_volume::BoxVertex BoxVertex;

static BoxVertex* Box_positions = nullptr; // 0x01B7BCB0
static unsigned short* Box_indices = nullptr; // 0x01B7BCB4
static rl_mesh* Box_mesh_p = nullptr; // 0x01B7BCB8
static rl_mesh_data* Mesh_data = nullptr; // 0x01B69AFC
static rl_submesh_data* Submesh = nullptr; // 0x01B76BF8
static rl_render_block* Renderblock = nullptr; // 0x01B6EAFC
static unsigned int Vis_query_shader_handle; // 0x01B7BC08
static rl_shader_const_ref World_xform_shader_const; // 0x01514B64
static rl_shader_const_ref Scale_shader_const; // 0x01514B68
static rl_shader_const_ref Offset_shader_const; // 0x01514B6C
static rl_shader_const_ref Camera_velocity_shader_const; // 0x01514B70
static keen::OcclusionQuery s_bracketQuery; // 0x01B76BFC

// The query queue: one entry per queried entity (rl_scene_entity::m_gpu_vis_index is its index).
static object_pool<vis_queue_entry, 2048, 1> Vis_queue; // 0x01C461D0
// Debug: the entry polled by the debug view (only ever cleared).
static vis_queue_entry* Entry_debug = nullptr; // 0x01B7BCC0
static bool Found_debug_entry = false; // 0x01B7BCA5
static int Frame_count = 0; // 0x01B7BCAC
// The eye position of the previous frame and the camera movement since then.
static vector Eye_pos_last; // 0x01C20BCC
static vector Camera_velocity; // 0x01B7BD64
static float Camera_velocity_mag = 0.0f; // 0x01B7BCA8
// Not in the binary: the counts of rl_gpu_vis_take_census.
static rl_gpu_vis_census Census;

// Occlusion queries for scene entities (the visibility manager queues entities when on).
bool RL_gpu_vis_enabled = true; // 0x014FA137
// Terrain entities are queried too.
bool RL_gpu_vis_terrain_enabled = false; // 0x01B7BD4F
// Debug: draw the query boxes with depth writes and color (state flag kDepthOnly off, zbuf mode FULL).
bool RL_gpu_vis_debug = false; // 0x01B7BC86
// Debug: rl_gpu_vis_render_debug draws the query boxes (it has no callers).
bool RL_gpu_vis_debug_render = false; // 0x01B7BC87
// Unused.
bool RL_gpu_vis_quarter = false; // 0x01B7BC85
// Unused.
int RL_gpu_vis_delay_for_query = 0; // 0x01B7BCA0
// Grow the query boxes along the camera velocity.
bool RL_gpu_vis_cam_enabled = false; // 0x01B7BCA4
// A result above this many pixels is visible.
int RL_gpu_vis_threshold = 0; // 0x01B7BCBC
// The query box grows by this fraction of the entity's local box.
float RL_gpu_vis_bbox_scale = 0.1f; // 0x014FA268
// Frames an entity stays NOT_READY after its last visible result before it reads as occluded, plus a random 0..rand.
int RL_gpu_vis_delay = 3; // 0x014FA26C
int RL_gpu_vis_delay_rand = 2; // 0x014FA270
// The eye check box grows by the near clip plus this (metres).
float RL_gpu_vis_near_fudge = 1.0f; // 0x014FA274
// A camera move of more than this in one frame (metres) resets every query.
float RL_gpu_vis_velocity_threshold = 3.0f; // 0x014FA278
// The camera velocity term of the query box (RL_gpu_vis_cam_enabled).
float RL_gpu_vis_cam_mag_max = 1.0f; // 0x014FA27C
float RL_gpu_vis_cam_mag_scale = 1.0f; // 0x014FA280
float RL_gpu_vis_cam_scale_percent = 0.3f; // 0x014FA284
// The query boxes grow by this (metres).
vector RL_gpu_vis_bbox_offset(0.1f, 0.1f, 0.1f); // 0x01514B58

// 0x0053B1F0 (:111)
// The same unit box mesh as rl_fog_volume::system_init ("VisSystem"), the rl_vis_query shader constants and the
// bracket occlusion query.
// Notes (decompiled behaviour, kept as is): the mesh draws 37 indices of the 36 in the index buffer, the uvs are
// uninitialized, and the mesh data objects are never freed.
void rl_gpu_vis_system_init(keen::GraphicsSystem* pGraphicsSystem)
{
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    const unsigned short indices[36] = {0, 1, 2, 2, 1, 3, 1, 5, 3, 3, 5, 7, 2, 4, 0, 6, 4, 2, 6, 5, 4, 7, 5, 6, 0, 4, 5, 0, 5, 1, 7, 6, 2, 3, 7, 2};
    Box_positions = (BoxVertex*)allocator_p->allocate(8 * sizeof(BoxVertex), 16, 0, nullptr);
    const float corners[8][3] = {{-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}};
    for (int i = 0; i < 8; i++)
    {
        Box_positions[i].pos.x = corners[i][0];
        Box_positions[i].pos.y = corners[i][1];
        Box_positions[i].pos.z = corners[i][2];
        Box_positions[i].pos.w = 1.0f;
    }
    Box_indices = (unsigned short*)allocator_p->allocate(sizeof(indices), 16, 0, nullptr);
    for (int i = 0; i < 36; i++)
    {
        Box_indices[i] = indices[i];
    }
    Box_mesh_p = rl_mesh::create();
    Mesh_data = new rl_mesh_data();
    Submesh = new rl_submesh_data();
    Renderblock = new rl_render_block;
    Mesh_data->index_buffer.index_size = 2;
    Mesh_data->index_buffer.prim_type = 1;
    Mesh_data->index_buffer.num_blocks = 1;
    // Magic number: 37 indices (one more than the box has).
    Mesh_data->index_buffer.num_indices = 0x25;
    Mesh_data->vertex_buffer.vert_stride_0 = sizeof(BoxVertex);
    Mesh_data->vertex_buffer.vertex_format = RLVF_COMPOSITOR;
    Mesh_data->vertex_buffer.num_uv_channels = 0;
    Mesh_data->vertex_buffer.vert_stride_1 = 0;
    Mesh_data->num_sub_meshes = 1;
    Mesh_data->vertex_buffer.num_verts = 8;
    Submesh->num_render_blocks = 1;
    Submesh->bmin.set(-0.5f, -0.5f, -0.5f);
    Submesh->bmax.set(0.5f, 0.5f, 0.5f);
    Submesh->offset_transform = ZERO_VECTOR;
    Submesh->render_blocks.from_pointer(Renderblock);
    Renderblock->maximum_index = 8;
    Renderblock->num_indices = 0x25;
    Renderblock->material_map_idx = 0;
    Renderblock->minimum_index = 0;
    Renderblock->start_index = 0;
    Mesh_data->sub_meshes.from_pointer(Submesh);
    rl_mesh_data_runtime data;
    data.num_sub_meshes = Mesh_data->num_sub_meshes;
    data.sub_meshes = Mesh_data->sub_meshes.to_pointer();
    data.vertex_buffer.num_verts = Mesh_data->vertex_buffer.num_verts;
    data.vertex_buffer.vert_stride_0 = Mesh_data->vertex_buffer.vert_stride_0;
    data.vertex_buffer.vertex_format = Mesh_data->vertex_buffer.vertex_format;
    data.vertex_buffer.num_uv_channels = Mesh_data->vertex_buffer.num_uv_channels;
    data.vertex_buffer.vert_stride_1 = Mesh_data->vertex_buffer.vert_stride_1;
    data.vertex_buffer.verts = (unsigned char*)Box_positions;
    data.index_buffer.num_indices = Mesh_data->index_buffer.num_indices;
    data.index_buffer.index_size = Mesh_data->index_buffer.index_size;
    data.index_buffer.prim_type = Mesh_data->index_buffer.prim_type;
    data.index_buffer.num_blocks = Mesh_data->index_buffer.num_blocks;
    data.index_buffer.indices = (unsigned char*)Box_indices;
    Box_mesh_p->init(&data, "VisSystem", false);

    rl_shader_library* library_p = rl_shader_library::get_instance();
    Vis_query_shader_handle = library_p->lookup("rl_vis_query");
    World_xform_shader_const.set_ref(library_p->get_shader_const_index(Vis_query_shader_handle, crc_stri("World_xform", 0)));
    Scale_shader_const.set_ref(library_p->get_shader_const_index(Vis_query_shader_handle, crc_stri("Scale", 0)));
    Offset_shader_const.set_ref(library_p->get_shader_const_index(Vis_query_shader_handle, crc_stri("Offset", 0)));
    Camera_velocity_shader_const.set_ref(library_p->get_shader_const_index(Vis_query_shader_handle, crc_stri("Camera_velocity", 0)));
    keen::graphics::createOcclusionQuery(pGraphicsSystem, &s_bracketQuery, allocator_p, "bracketQuery");
}

// 0x004E7D50 (:225)
// Notes (decompiled behaviour, kept as is): the bracket query is not destroyed.
void rl_gpu_vis_system_shutdown()
{
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    if (Box_positions != nullptr)
    {
        allocator_p->free(Box_positions);
        Box_positions = nullptr;
    }
    if (Box_indices != nullptr)
    {
        allocator_p->free(Box_indices);
        Box_indices = nullptr;
    }
    if (Box_mesh_p != nullptr)
    {
        Box_mesh_p->release();
        Box_mesh_p = nullptr;
    }
}

// Not in the binary: the camera velocity term of the query box (inlined twice in calc_vis_aabb). 0 unless
// RL_gpu_vis_cam_enabled; the ratio is computed in double (the remap the decompile shows, from 0 to the percent).
static float gpu_vis_cam_scale()
{
    if (!RL_gpu_vis_cam_enabled)
    {
        return 0.0f;
    }
    float mag = RL_gpu_vis_cam_mag_scale * Camera_velocity_mag;
    if (0.0f <= mag - RL_gpu_vis_cam_mag_max)
    {
        mag = RL_gpu_vis_cam_mag_max;
    }
    const float t = (float)(1.0 - ((double)RL_gpu_vis_cam_mag_max - (double)mag) / (double)RL_gpu_vis_cam_mag_max);
    return (float)((double)t * ((double)RL_gpu_vis_cam_scale_percent - 0.0) + 0.0);
}

// 0x0050F420 (:259)
// LTCG: entry_p in EAX, camera on the stack.
// The query box in the entity's local space: m_offset is the local box centre, m_scale its size grown by
// RL_gpu_vis_bbox_scale and RL_gpu_vis_bbox_offset (the unit box mesh is scaled and offset by them). The eye check box
// is grown further by the near clip plus RL_gpu_vis_near_fudge, so a camera close to the box counts as inside.
// Marks the entity's query box as up to date.
static void calc_vis_aabb(rl_camera& camera, vis_queue_entry* entry_p)
{
    rl_scene_entity* entity_p = entry_p->m_entity;
    const rl_aabb local = entity_p->get_local_aabb();
    const float min_x = local.min.x;
    const float min_y = local.min.y;
    const float min_z = local.min.z;
    const float max_x = local.max.x;
    const float max_y = local.max.y;
    const float max_z = local.max.z;
    const float near_dist = camera.get_near_clip() + RL_gpu_vis_near_fudge;
    const float grow = (gpu_vis_cam_scale() + RL_gpu_vis_bbox_scale) * 0.5f;
    const float size_x = max_x - min_x;
    const float size_y = max_y - min_y;
    const float size_z = max_z - min_z;
    const float grow_x = size_x * grow;
    const float grow_y = size_y * grow;
    const float grow_z = size_z * grow;
    const float eye_x = near_dist + RL_gpu_vis_bbox_offset.x;
    const float eye_y = near_dist + RL_gpu_vis_bbox_offset.y;
    const float eye_z = near_dist + RL_gpu_vis_bbox_offset.z;
    // The order of the additions follows the disassembly: (box +- grow) +- (near + offset).
    entry_p->m_eye_check_aabb.min.x = (min_x - grow_x) - eye_x;
    entry_p->m_eye_check_aabb.min.y = (min_y - grow_y) - eye_y;
    entry_p->m_eye_check_aabb.min.z = (min_z - grow_z) - eye_z;
    entry_p->m_eye_check_aabb.max.x = (max_x + grow_x) + eye_x;
    entry_p->m_eye_check_aabb.max.y = (max_y + grow_y) + eye_y;
    entry_p->m_eye_check_aabb.max.z = (max_z + grow_z) + eye_z;
    entry_p->m_scale.x = size_x;
    entry_p->m_scale.y = size_y;
    entry_p->m_scale.z = size_z;
    const float scale = gpu_vis_cam_scale() + RL_gpu_vis_bbox_scale;
    entry_p->m_scale.x = entry_p->m_scale.x + (entry_p->m_scale.x * scale + RL_gpu_vis_bbox_offset.x);
    entry_p->m_scale.y = entry_p->m_scale.y + (entry_p->m_scale.y * scale + RL_gpu_vis_bbox_offset.y);
    entry_p->m_scale.z = entry_p->m_scale.z + (entry_p->m_scale.z * scale + RL_gpu_vis_bbox_offset.z);
    entry_p->m_offset.x = (max_x + min_x) * 0.5f;
    entry_p->m_offset.y = (max_y + min_y) * 0.5f;
    entry_p->m_offset.z = (max_z + min_z) * 0.5f;
    entity_p->set_is_gpu_occlusion_cached(true);
}

// 0x00524B30 (:285)
// LTCG: renderer in ESI, entry_p in EDI.
// Draws the entry's query box: the unit box placed by the entity's world matrix, scaled by m_scale around m_offset.
// Notes (decompiled behaviour, kept as is): Scale and Offset are written as four floats from three-float vectors:
// Scale.w is the 4 bytes after the entry (the next entry's m_pixels_drawn) and Offset.w is m_scale.x. With
// RL_gpu_vis_cam_enabled, Camera_velocity.w is likewise the global after it; this port writes 0 there instead.
static void rl_gpu_vis_render_mesh(rl_renderer& renderer, vis_queue_entry* entry_p)
{
    // Magic numbers: matrix43 is 12 floats, the other constants 4.
    renderer.set_vertex_shader_constant(World_xform_shader_const.get_ref(), &entry_p->m_entity->get_world_transform().m_rotation.rvec.x, 12);
    renderer.set_vertex_shader_constant(Scale_shader_const.get_ref(), &entry_p->m_scale.x, 4);
    renderer.set_vertex_shader_constant(Offset_shader_const.get_ref(), &entry_p->m_offset.x, 4);
    if (RL_gpu_vis_cam_enabled)
    {
        const float camera_velocity[4] = {Camera_velocity.x, Camera_velocity.y, Camera_velocity.z, 0.0f};
        renderer.set_vertex_shader_constant(Camera_velocity_shader_const.get_ref(), camera_velocity, 4);
    }
    renderer.render(Box_mesh_p, 0, 0);
}

// 0x00524C00 (:304)
// LTCG: both arguments on the stack; passes them to rl_gpu_vis_render_mesh in registers.
// Issues one more occlusion query for the entry.
// Notes (decompiled behaviour, kept as is): the test is m_query_index + 1 < 3, so at most two of the three query
// slots are ever in flight. A query that cannot be allocated (the manager is out of queries) is simply skipped.
void rl_gpu_vis_render_mesh_query(rl_renderer& renderer, vis_queue_entry* entry_p)
{
    if (entry_p->m_query_index + 1 < 3)
    {
        rl_render_lib* render_lib_p = rl_render_lib::get_instance();
        const rl_stamped_handle handle = render_lib_p->occlusion_query_issue_begin(renderer);
        if (handle.is_valid())
        {
            rl_gpu_vis_render_mesh(renderer, entry_p);
            render_lib_p->occlusion_query_issue_end(handle, renderer);
            entry_p->m_query_handle[entry_p->m_query_index] = handle;
            entry_p->m_query_index++;
            Census.queries_issued++;
        }
    }
}

// 0x00524C80 (:330)
// Debug: draws every query box with depth test and additive alpha (RL_gpu_vis_debug_render; no callers).
void rl_gpu_vis_render_debug(rl_renderer& renderer, rl_camera& camera)
{
    if (!RL_gpu_vis_debug_render)
    {
        return;
    }
    const rl_primitive_state old_state = renderer.m_state_p->get_macro_state();
    rl_primitive_state state = old_state;
    state.set_alpha_mode(RLPAM_ADDITIVE);
    state.set_zbuf_mode(RLPZM_READ);
    renderer.set_macro_state(state, false);
    renderer.m_state_p->set_shader_type(shader_type_generic);
    renderer.m_state_p->set_shader_configuration(0);
    renderer.set_active_shader(Vis_query_shader_handle, rl_shader_technique_base, 0);
    {
        csspinlock_scope lock(Vis_queue.thread_access);
        for (vis_queue_entry* entry_p = Vis_queue.get_first(0); entry_p != nullptr; entry_p = Vis_queue.get_next(entry_p))
        {
            if (entry_p->m_entity != nullptr)
            {
                rl_gpu_vis_render_mesh(renderer, entry_p);
            }
        }
    }
    renderer.set_macro_state(old_state, false);
    renderer.set_world_transform(IDENTITY_MATRIX43);
}

// Not in the binary: releases one in-flight query (inlined everywhere a query handle is dropped).
static void gpu_vis_release_query(rl_stamped_handle& handle)
{
    rl_render_lib::get_instance()->occlusion_query_release_handle(handle);
    handle.invalidate();
}

// 0x0050FAA0 (:375)
// Polls the entity's queries (oldest first; the first ready result replaces m_pixels_drawn). Any pixels: VISIBLE (or
// NOT_READY while no result has arrived yet) and the delay restarts; no pixels: NOT_READY until the delay has counted
// down, then OCCLUDED. A camera inside the query box (flag bit 0, set by rl_gpu_vis_render) drops the queries and
// reads as VISIBLE.
// Notes (decompiled behaviour, kept as is): every poll that sees pixels (including the initial "no result yet" count)
// draws from the global generator (Vlib_rand_instance) through int_rand_range, so the simulation's random sequence
// depends on GPU timing. The viewer saves and restores the generator around its render frame. The entry is read
// without the queue lock.
rl_gpu_vis_state rl_gpu_vis_get_state(rl_renderer& renderer, int handle)
{
    vis_queue_entry* entry_p = Vis_queue.get_entry(handle);
    entry_p->m_flags |= 2;
    if (entry_p == Entry_debug)
    {
        Found_debug_entry = true;
    }
    if ((entry_p->m_flags & 1) != 0)
    {
        entry_p->m_delay = (char)RL_gpu_vis_delay;
        for (int i = 0; i < 3; i++)
        {
            if (entry_p->m_query_handle[i].is_valid())
            {
                gpu_vis_release_query(entry_p->m_query_handle[i]);
            }
        }
        entry_p->m_query_index = 0;
        Census.visible++;
        // The decompile returns the final loop counter (1) + 1.
        return RL_GPU_VIS_STATE_VISIBLE;
    }
    const int num_queries = entry_p->m_query_index;
    for (int i = 0; i < num_queries; i++)
    {
        if (!entry_p->m_query_handle[i].is_valid())
        {
            continue;
        }
        const int pixels = rl_render_lib::get_instance()->occlusion_query_get_pixels_drawn(entry_p->m_query_handle[i], renderer, false);
        if (pixels != -1)
        {
            entry_p->m_pixels_drawn = pixels;
            gpu_vis_release_query(entry_p->m_query_handle[i]);
            for (int j = i; j < 3; j++)
            {
                if (j + 1 == 3)
                {
                    entry_p->m_query_handle[j].invalidate();
                }
                else
                {
                    entry_p->m_query_handle[j] = entry_p->m_query_handle[j + 1];
                }
            }
            // Notes (decompiled behaviour, kept as is): the decrement is an unsigned char += 0xFF.
            entry_p->m_query_index += 0xFF;
            break;
        }
    }
    if (RL_gpu_vis_threshold < entry_p->m_pixels_drawn)
    {
        entry_p->m_delay = (char)RL_gpu_vis_delay;
        if (RL_gpu_vis_delay != 0)
        {
            entry_p->m_delay += (char)int_rand_range(0, RL_gpu_vis_delay_rand);
        }
        // Magic number: 1,000,000,000 is the initial "no result yet" pixel count (rl_gpu_vis_alloc).
        const rl_gpu_vis_state state = entry_p->m_pixels_drawn != 1000000000 ? RL_GPU_VIS_STATE_VISIBLE : RL_GPU_VIS_STATE_NOT_READY;
        (state == RL_GPU_VIS_STATE_VISIBLE ? Census.visible : Census.not_ready)++;
        return state;
    }
    const char delay = entry_p->m_delay;
    entry_p->m_delay = delay - 1;
    if (delay > 0)
    {
        Census.not_ready++;
        return RL_GPU_VIS_STATE_NOT_READY;
    }
    entry_p->m_delay = 0;
    Census.occluded++;
    return RL_GPU_VIS_STATE_OCCLUDED;
}

// 0x0050FCA0 (:480)
// Takes a queue entry for the entity (when the queue is full the index stays -1 and the entity uses the CPU
// occluders) and computes its query box.
void rl_gpu_vis_alloc(rl_renderer& renderer, rl_camera& camera, rl_scene_entity* entity)
{
    if (!Vis_queue.can_alloc())
    {
        entity->set_gpu_vis_index(-1);
        return;
    }
    vis_queue_entry* entry_p = Vis_queue.alloc(0);
    // Polled this frame (bit 1), camera not inside (bit 0).
    entry_p->m_flags = (entry_p->m_flags & ~1u) | 2;
    entry_p->m_entity = entity;
    entity->set_gpu_vis_index(Vis_queue.get_index(entry_p));
    // Magic number: 1,000,000,000 pixels means no result yet.
    entry_p->m_pixels_drawn = 1000000000;
    for (int i = 0; i < 3; i++)
    {
        entry_p->m_query_handle[i].invalidate();
    }
    entry_p->m_query_index = 0;
    entry_p->m_delay = 0;
    calc_vis_aabb(camera, entry_p);
}

// 0x00524E00 (:517)
// LTCG: entry_p in EAX.
// Returns the entry to the queue and releases its queries.
// Notes (decompiled behaviour, kept as is): the entry is freed before its queries are released, and m_entity is
// left set.
static void rl_gpu_vis_query_free(vis_queue_entry* entry_p)
{
    Vis_queue.free(entry_p);
    for (int i = 0; i < 3; i++)
    {
        if (entry_p->m_query_handle[i].is_valid())
        {
            gpu_vis_release_query(entry_p->m_query_handle[i]);
        }
    }
}

// 0x00524E70 (:533)
void rl_gpu_vis_free(int handle)
{
    rl_gpu_vis_query_free(Vis_queue.get_entry(handle));
}

// 0x004C1ED0 (:540)
void rl_gpu_vis_set_enabled(bool enable)
{
    RL_gpu_vis_enabled = enable;
}

// 0x004C1EE0 (:545)
bool rl_gpu_vis_is_enabled()
{
    return RL_gpu_vis_enabled;
}

// 0x00524EA0 (:550)
// Issues this frame's queries against the depth pre-pass, inside the bracket query, with the rl_vis_query shader and
// no color writes (depth test only; depth writes and color too with RL_gpu_vis_debug). For each entry the query box
// is refreshed when stale; a camera inside the eye check box marks the entry (bit 0) instead of querying it.
void rl_gpu_vis_render(rl_renderer& renderer, rl_camera& camera)
{
    if (!RL_gpu_vis_enabled)
    {
        return;
    }
    ID3D11DeviceContext* context_p = renderer.getCommandWriter()->getBuffer()->pContext;
    context_p->Begin(s_bracketQuery.pQuery);
    if (Entry_debug != nullptr)
    {
        Entry_debug = nullptr;
    }
    const vector eye = camera.get_position();
    renderer.m_state_p->set_shader_type(shader_type_generic);
    renderer.m_state_p->set_shader_configuration(0);
    renderer.m_state_p->set_depth_only(!RL_gpu_vis_debug);
    renderer.set_active_shader(Vis_query_shader_handle, rl_shader_technique_base, 0);
    const rl_primitive_state old_state = renderer.m_state_p->get_macro_state();
    rl_primitive_state state = old_state;
    // The original builds the state as bits: (old & 0xFFF8FE3F) | (zbuf << 6) | 0x8000.
    state.set_zbuf_mode(RL_gpu_vis_debug ? RLPZM_FULL : RLPZM_READ);
    state.set_color_write_mode(RLPCW_NONE);
    state.set_scissor_enabled(false);
    renderer.set_macro_state(state, false);
    {
        csspinlock_scope lock(Vis_queue.thread_access);
        for (vis_queue_entry* entry_p = Vis_queue.get_first(0); entry_p != nullptr; entry_p = Vis_queue.get_next(entry_p))
        {
            rl_scene_entity* entity_p = entry_p->m_entity;
            if (entity_p == nullptr)
            {
                continue;
            }
            if (RL_gpu_vis_cam_enabled || !entity_p->get_is_gpu_occlusion_cached())
            {
                calc_vis_aabb(camera, entry_p);
            }
            if (ix_point_in_box_oriented(eye, entity_p->get_position(), entity_p->get_rotation(), entry_p->m_eye_check_aabb.min, entry_p->m_eye_check_aabb.max))
            {
                entry_p->m_flags |= 1;
                Census.camera_inside++;
            }
            else
            {
                rl_gpu_vis_render_mesh_query(renderer, entry_p);
                entry_p->m_flags &= ~1u;
            }
        }
    }
    renderer.set_macro_state(old_state, false);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    renderer.m_state_p->set_depth_only(false);
    context_p = renderer.getCommandWriter()->getBuffer()->pContext;
    context_p->End(s_bracketQuery.pQuery);
}

// 0x00525110 (:633)
// Frees every entry (a camera jump).
static void rl_gpu_vis_reset()
{
    Census.resets++;
    csspinlock_scope lock(Vis_queue.thread_access);
    vis_queue_entry* entry_p = Vis_queue.get_first(0);
    while (entry_p != nullptr)
    {
        vis_queue_entry* next_p = Vis_queue.get_next(entry_p);
        if (entry_p->m_entity != nullptr)
        {
            entry_p->m_entity->set_gpu_vis_index(-1);
            rl_gpu_vis_query_free(Vis_queue.get_entry(Vis_queue.get_index(entry_p)));
        }
        entry_p = next_p;
    }
}

// 0x005251F0 (:645)
// Frees the entries that were not polled last frame (their entity was not tested: culled, hidden or gone) and clears
// the polled bit of the rest. Then measures the camera movement; a jump of more than RL_gpu_vis_velocity_threshold
// resets the queue.
void rl_gpu_vis_prerender(rl_camera& camera)
{
    Frame_count++;
    {
        csspinlock_scope lock(Vis_queue.thread_access);
        vis_queue_entry* entry_p = Vis_queue.get_first(0);
        while (entry_p != nullptr)
        {
            vis_queue_entry* next_p = Vis_queue.get_next(entry_p);
            if (entry_p->m_entity == nullptr || (entry_p->m_flags & 2) != 0)
            {
                entry_p->m_flags &= ~2u;
            }
            else
            {
                entry_p->m_entity->set_gpu_vis_index(-1);
                rl_gpu_vis_query_free(Vis_queue.get_entry(Vis_queue.get_index(entry_p)));
            }
            entry_p = next_p;
        }
    }
    const vector eye = camera.get_position();
    Camera_velocity.x = eye.x - Eye_pos_last.x;
    Camera_velocity.y = eye.y - Eye_pos_last.y;
    Camera_velocity.z = eye.z - Eye_pos_last.z;
    // The squares are summed in double, rounded, then square-rooted in double.
    Camera_velocity_mag = (float)sqrt((double)(float)(((double)Camera_velocity.x * (double)Camera_velocity.x + (double)Camera_velocity.y * (double)Camera_velocity.y) + (double)Camera_velocity.z * (double)Camera_velocity.z));
    if (RL_gpu_vis_velocity_threshold < Camera_velocity_mag)
    {
        rl_gpu_vis_reset();
    }
    Eye_pos_last = eye;
}

// Not in the binary: see rl_gpu_visibility.h.
void rl_gpu_vis_take_census(rl_gpu_vis_census& census)
{
    census = Census;
    Census = rl_gpu_vis_census();
    csspinlock_scope lock(Vis_queue.thread_access);
    for (const vis_queue_entry* entry_p = Vis_queue.get_first(0); entry_p != nullptr; entry_p = Vis_queue.get_next(entry_p))
    {
        census.entries++;
        census.queries_in_flight += entry_p->m_query_index;
    }
}
