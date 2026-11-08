// Original: volition/rfg/code/code_effects/glass.cpp
//
// Breakable glass (VFX.md FX8). A glass piece of a destroyable mover shattering (rfg_dyn_shardify_setup) becomes a
// glass object: its pane (the sub mesh's box seen along its thin axis), its world transform, the impact point and
// force. Objects queue for the shatter (glass_obj_do_frame takes one per frame): create_shatter_effect cuts the pane
// into shards (rings of circle points around the impact, clipped to the pane and triangulated), physics_init gives
// each shard a velocity and spin (neighbours may merge into one piece), create_mesh builds the shards as skinned
// meshes (18 vertices per shard, one bone per shard, 64 shards per mesh) from the Vertex_pool, and the update render
// task moves the bones along a damped ballistic path until GLASS_SHARD_LIFETIME. Vertex buffers are freed a few
// frames late (Free_buffers) since the GPU may still read them. glass.xtbl tints shatter effects by texture.
// The arithmetic follows rfg.exe (float expressions in double, rounded at stores) where it was checked.

#include "volition/rfg/code/code_effects/glass.h"

#include "keen/base/memoryallocator.hpp"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/render_lib/code/rl_vertex_format_defines.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/polyclip.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/triangulate.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

// ---- file-local types (rfg.pdb)
// size 0x8
struct free_buffer
{
    unsigned char* data; // 0x00
    char delay; // 0x04
};
static_assert(sizeof(free_buffer) == 0x8, "free_buffer");

// size 0x14
struct glass_tint
{
    glass_tint(); // 0x006E36E0

    int texture_handle; // 0x00
    rl_color_float tint; // 0x04
};
static_assert(sizeof(glass_tint) == 0x14, "glass_tint");

// size 0x20, vertex format RLVF_PIXLIT_1UV_NMAP_CA (layout from rfg.pdb)
struct rl_mesh_pixlit_nmap_ca_vertex
{
    vector pos; // 0x00
    rl_compressed_vector normal; // 0x0C
    rl_compressed_vector tangent; // 0x10
    unsigned char vweights[4]; // 0x14
    unsigned char vindices[4]; // 0x18
    unsigned int uvs; // 0x1C
};
static_assert(sizeof(rl_mesh_pixlit_nmap_ca_vertex) == 0x20, "rl_mesh_pixlit_nmap_ca_vertex");

// ---- globals (tweakables)
bool Glass_enabled = true; // 0x01648F80
bool Glass_debug; // 0x01DEE4B6
bool Glass_debug_full; // 0x01DEE4B5
bool Glass_merge = true; // 0x01648D2B
int Glass_merge_rand = 0xFFFF; // 0x01648F7C
float Glass_impact_force_dir_percent = 0.2f; // 0x01648F50
float Glass_impact_force_scale = 100.0f; // 0x01648F54
float Glass_impact_force_min = 1.3f; // 0x01648F58
float Glass_impact_force_max = 100.0f; // 0x01648F5C
float Glass_rotation_min = 1.0f; // 0x01648F60
float Glass_rotation_max = 100.0f; // 0x01648F64
float Glass_gravity = 8.0f; // 0x01648F68
float Glass_damping = 150.0f; // 0x01648F6C
float Glass_rotational_damping = 1.0f; // 0x01648F70
float Glass_rotation_scale = 100.0f; // 0x01648F74
float Glass_exp = 2.2f; // 0x01648F78

// ---- file-static globals
static const float GLASS_SHARD_LIFETIME = 5.0f; // 0x012D0CA8
static bool Glass_lod_enable = true; // 0x01648F81
static rl_dev_vertex_format GLASS_VERTEX_FORMAT_ID = RLVF_PIXLIT_1UV_NMAP_CA; // 0x01648F84 (34)
static int Glass_index_buffer_size = -1; // 0x01648F88
static short Base_indices[24] = {0, 1, 2, 5, 4, 3, 7, 8, 6, 9, 8, 7, 10, 12, 11, 11, 12, 13, 14, 16, 15, 16, 17, 15}; // 0x01648F8C
// The shard whose vertices are copied into Test_shard (debugging); -2: none.
static int Debug_shard_index = -2; // 0x01648FBC
static int Glass_lod_circles = 5; // 0x01648FC0
static int Glass_lod_chance = 50; // 0x01648FC4
static int Glass_lod_capacity = 2; // 0x01648FC8
static farray<free_buffer,300> Free_buffers; // 0x01649018
static farray<rl_mesh_pixlit_nmap_ca_vertex,18> Test_shard; // 0x01649988
// The damping variance of a glass shard (always multiplied by 0 in rfg.exe).
static float density_variance_percent = 0.35f; // 0x0164ABD4
// The radius added to every shatter circle (circle 0, of radius 0, is this ring around the impact).
static float radius_min = 0.05f; // 0x0164ABD8
static unsigned char* Vertex_pool_buf; // 0x01DEE4A0
static glass_obj* Glass_temp; // 0x01DEE4A4
static short* Glass_index_buffer; // 0x01DEE4AC
static int Num_glass_tints; // 0x01DEE4B0
static bool Glass_system_init; // 0x01DEE4B4
static linked_list_p<glass_obj> Glass_objects_to_process; // 0x01DEE4D4
// Glass_shard_pool is placed here (game_save_generic.cpp, which also names it, is not compiled).
static object_pool<glass_shard,750,1> Glass_shard_pool; // 0x01DF3E20
static object_pool<glass_obj,20,1> Glass_obj_pool; // 0x01E10A60
static page_mempool Vertex_pool; // 0x01E14030
static linked_list_p<glass_obj> Glass_objects; // 0x01E14308
static game_render_task Update_render_task_glass; // 0x01E21F04
static glass_tint Glass_tints[50]; // 0x01E21F18

const int glass_obj::SHARDS_MAX = 320; // 0x012C70D8
const int glass_obj::MAX_SHARDS_PER_INSTANCE = 64; // 0x012C70DC
const int glass_obj::MAX_INSTANCES = 5; // 0x012C70E0

// Magic numbers: rfg.exe's float literals used in double arithmetic.
static const double DEG_TO_RAD = (double)0.017453292f;
static const float GLASS_PANE_ROTATION = -1.57077f; // the pane's thin axis turned to face forward

// ---- constructors

// 0x006E36E0
glass_tint::glass_tint()
{
}

// 0x006ECCC0 (:124)
glass_circle::glass_circle()
{
    m_radius = 0.0f;
}

// 0x006E5440
glass_circle::~glass_circle()
{
}

// 0x006EF570
glass_poly::glass_poly()
{
}

// 0x006EB6F0
glass_poly::~glass_poly()
{
}

// 0x006F27D0
glass_shard::glass_shard()
{
}

// 0x006E5E40
glass_pane::glass_pane()
{
}

// 0x006EF530
// The sub meshes' render block offsets start at -1 (their constructor).
glass_obj::glass_obj()
{
    m_shards.init();
}

// ---- internal functions

// 0x006E36F0 (:167)
// Empty in rfg.exe.
void unused_dcf_glass_impact()
{
}

// 0x006E3700 (:216)
static unsigned char* alloc_vertex_data(int size)
{
    if (!Vertex_pool.can_alloc((unsigned int)size, 0))
    {
        return nullptr;
    }
    return (unsigned char*)Vertex_pool.alloc((unsigned int)size, 0);
}

// 0x006ECD10 (:224)
// The buffer is released 4 frames later (process_free_buffers), when the GPU no longer reads it.
static void free_vertex_data(unsigned char* data, int size)
{
    free_buffer buffer;
    buffer.data = data;
    // Magic number: frames before the release.
    buffer.delay = 4;
    Free_buffers.add(buffer);
}

// 0x006E3730 (:232)
static unsigned char* alloc_gpu_data(int size)
{
    keen::MemoryAllocator* allocator = rfg_get_gpu_scratch_allocator();
    return (unsigned char*)allocator->allocate((unsigned int)size, 0x10, 0, nullptr);
}

// 0x006E3750 (:239)
static void free_gpu_data(unsigned char* data)
{
    rfg_get_gpu_scratch_allocator()->free(data);
}

// 0x006E3770 (:403)
// A point of the pane (x, y in the pane, z = 0) around the impact point to world space: the offset (x, y, 0) is
// added to the impact point and transformed by xform.
static vector convert_circle_point2d_point3d(const vector2& point2d, const vector& impact_point, const matrix43& xform)
{
    const float px = impact_point.x + point2d.x + point2d.y * 0.0f;
    const float py = impact_point.y + point2d.x * 0.0f + point2d.y;
    const float pz = impact_point.z + point2d.x * 0.0f + point2d.y * 0.0f;
    // The rotated offset is summed before the translation (rfg.exe's order, rounded per step).
    const matrix& m = xform.m_rotation;
    return vector(m.rvec.x * px + m.uvec.x * py + m.fvec.x * pz + xform.m_translation.x,
                  m.rvec.y * px + m.uvec.y * py + m.fvec.y * pz + xform.m_translation.y,
                  m.rvec.z * px + m.uvec.z * py + m.fvec.z * pz + xform.m_translation.z);
}

// 0x006E39C0 (:415)
// As above for a vector's x and y.
static vector convert_circle_point2d_point3d(const vector& point2d, const vector& impact_point, const matrix43& xform)
{
    return convert_circle_point2d_point3d(vector2(point2d.x, point2d.y), impact_point, xform);
}

// 0x006E3C10 (:419)
static vector convert_to_vector3(const vector2& vec2)
{
    return vector(vec2.x, vec2.y, 0.0f);
}

// 0x006E3C30 (:424)
// Strictly inside the pane (p0 is the top left, p3 the bottom right).
static bool point_in_pane(const glass_pane& pane, const vector2& point)
{
    return pane.m_p0.x < point.x && point.x < pane.m_p3.x && point.y < pane.m_p0.y && pane.m_p3.y < point.y;
}

// 0x006E8820 (:434)
// All three points strictly inside the pane.
static bool poly_in_pane(const glass_pane& pane, const glass_poly& poly)
{
    for (int i = 0; i < 3; i++)
    {
        const vector& p = poly.m_points[i];
        if (!(pane.m_p0.x < p.x && p.x < pane.m_p3.x && p.y < pane.m_p0.y && pane.m_p3.y < p.y))
        {
            return false;
        }
    }
    return true;
}

// 0x006E88A0 (:445)
// The triangle's part of the pane (polyclip_2d of the pane quad by the triangle) into inter; false (nothing clipped)
// when the triangle lies inside the pane.
static bool clip_2d_shard(const glass_pane& pane, const glass_poly& poly, vector2* inter, int& num_verts)
{
    vector2 triangle[3];
    for (int i = 0; i < 3; i++)
    {
        triangle[i].set(poly.m_points[i].x, poly.m_points[i].y);
    }
    vector2 quad[4] = {pane.m_p0, pane.m_p2, pane.m_p3, pane.m_p1};
    if (poly_in_pane(pane, poly))
    {
        return false;
    }
    vector2* which = nullptr;
    num_verts = (int)polyclip_2d(3, triangle, 4, quad, inter, which);
    return true;
}

// 0x006E3C90 (:479)
static bool pane_point_encompassed(vector2 pane_point, vector2 point, float radius)
{
    const float dx = pane_point.x - point.x;
    const float dy = pane_point.y - point.y;
    return dy * dy + dx * dx < radius * radius;
}

// 0x006E3D00 (:488)
// Every pane corner within the circle.
bool glass_pane::encompassed(vector2 point, float radius)
{
    return pane_point_encompassed(m_p0, point, radius) && pane_point_encompassed(m_p1, point, radius) && pane_point_encompassed(m_p2, point, radius) && pane_point_encompassed(m_p3, point, radius);
}

// 0x006E8A10 (:499)
// The normal and tangent of a side quad (4 vertices) from its edges 0-1 and 0-2, flipped with negate.
static void create_side_normals(rl_mesh_pixlit_nmap_ca_vertex* side, bool negate)
{
    const float dx = side[2].pos.x - side[0].pos.x;
    const float dy = side[2].pos.y - side[0].pos.y;
    const float dz = side[2].pos.z - side[0].pos.z;
    vector tangent(side[1].pos.x - side[0].pos.x, side[1].pos.y - side[0].pos.y, side[1].pos.z - side[0].pos.z);
    vector normal(dz * tangent.y - tangent.z * dy, tangent.z * dx - tangent.x * dz, dy * tangent.x - dx * tangent.y);
    if (negate)
    {
        tangent.set(tangent.x * -1.0f, tangent.y * -1.0f, tangent.z * -1.0f);
        normal.set(normal.x * -1.0f, normal.y * -1.0f, normal.z * -1.0f);
    }
    normal.normalize();
    tangent.normalize();
    for (int i = 0; i < 4; i++)
    {
        side[i].normal.set(normal.x, normal.y, normal.z, 0.0f);
        side[i].tangent.set(tangent.x, tangent.y, tangent.z, 0.0f);
    }
}

// 0x006E3EB0 (:522)
static void vector3_min(vector& result, const vector& a, const vector& b)
{
    result.x = 0.0f <= b.x - a.x ? a.x : b.x;
    result.y = 0.0f <= b.y - a.y ? a.y : b.y;
    result.z = 0.0f <= b.z - a.z ? a.z : b.z;
}

// 0x006E3F30 (:529)
static void vector3_max(vector& result, const vector& a, const vector& b)
{
    result.x = b.x - a.x < 0.0f ? a.x : b.x;
    result.y = b.y - a.y < 0.0f ? a.y : b.y;
    result.z = 0.0f <= b.z - a.z ? b.z : a.z;
}

// Not in the binary: min/max of a vertex position into the running box (create_mesh inlines it).
static void glass_bounds_add(vector& bmin, vector& bmax, const vector& p)
{
    if (0.0f <= bmin.x - p.x)
    {
        bmin.x = p.x;
    }
    if (0.0f <= bmin.y - p.y)
    {
        bmin.y = p.y;
    }
    if (0.0f <= bmin.z - p.z)
    {
        bmin.z = p.z;
    }
    if (bmax.x - p.x < 0.0f)
    {
        bmax.x = p.x;
    }
    if (bmax.y - p.y < 0.0f)
    {
        bmax.y = p.y;
    }
    if (bmax.z - p.z < 0.0f)
    {
        bmax.z = p.z;
    }
}

// Not in the binary: a vertex copied into Test_shard when its shard is Debug_shard_index (create_mesh inlines it).
static void glass_debug_copy(int shard_index, int& test_index, const rl_mesh_pixlit_nmap_ca_vertex& vertex)
{
    if (Debug_shard_index == shard_index && test_index < 18)
    {
        Test_shard[test_index++] = vertex;
    }
}

// 0x006F28B0 (:540)
// The shards as skinned meshes: 18 vertices each in one vertex stream from the Vertex_pool (front face, back face
// offset by the pane thickness along -fvec, three side quads), relative to the world transform's position, bone index
// = shard index % 64; the shared Glass_index_buffer. Each mesh takes 64 shards (one bone each). Also sets each shard's
// centroid and rotation axis (merged pairs share their mean). Without a mesh, skin instance or bones the build stops
// there (the vertex stream is freed when nothing was built).
void glass_obj::create_mesh()
{
    const int num_shards = m_shards.get_elem_count();
    if (num_shards == 0)
    {
        m_num_instances = 0;
        return;
    }
    // Magic number: 18 vertices of 0x20 bytes per shard.
    const int alloc_size = num_shards * 0x240;
    m_vertex_alloc_size = alloc_size;
    unsigned char* verts = alloc_vertex_data(alloc_size);
    m_vertex_stream_p = verts;
    if (verts == nullptr)
    {
        m_num_instances = 0;
        return;
    }
    vector bmin(FLT_MAX, FLT_MAX, FLT_MAX);
    vector bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    rl_mesh_pixlit_nmap_ca_vertex* v = (rl_mesh_pixlit_nmap_ca_vertex*)verts;
    int shard_index = -1;
    glass_shard* shard = m_shards.first();
    while (shard != nullptr)
    {
        shard_index++;
        int test_index = 0;
        // Magic number: one bone per shard, 64 per mesh.
        const unsigned char bone = (unsigned char)(shard_index % 64);
        // The front face, wound by the poly's winding.
        for (int k = 0; k < 3; k++)
        {
            const int src = shard->m_poly.m_winding ? k : 2 - k;
            const vector& p = shard->m_poly.m_points[src];
            v[k].pos.set(p.x - m_world_xform.m_translation.x, p.y - m_world_xform.m_translation.y, p.z - m_world_xform.m_translation.z);
            v[k].normal.set(m_world_xform.m_rotation.fvec.x, m_world_xform.m_rotation.fvec.y, m_world_xform.m_rotation.fvec.z, 0.0f);
            v[k].tangent.set(m_world_xform.m_rotation.rvec.x, m_world_xform.m_rotation.rvec.y, m_world_xform.m_rotation.rvec.z, 0.0f);
            v[k].vweights[0] = 0xFF;
            v[k].vweights[1] = 0;
            v[k].vweights[2] = 0;
            v[k].vweights[3] = 0;
            v[k].vindices[0] = bone;
            v[k].vindices[1] = bone;
            v[k].vindices[2] = bone;
            v[k].vindices[3] = bone;
            glass_bounds_add(bmin, bmax, v[k].pos);
            // Magic number: the uvs as 1/1024 fixed point (u in the high half; v sign-extends over it, as rfg.exe).
            const vector2& uv = shard->m_poly.m_uvs[src];
            v[k].uvs = (unsigned int)((int)(short)(int)((double)uv.x * 1024.0) << 16 | (int)(short)(int)((double)uv.y * 1024.0));
            glass_debug_copy(shard_index, test_index, v[k]);
        }
        const vector centroid((float)(((double)v[2].pos.x + (double)v[1].pos.x + (double)v[0].pos.x) / 3.0), (float)(((double)v[2].pos.y + (double)v[0].pos.y + (double)v[1].pos.y) / 3.0),
                              (float)(((double)v[2].pos.z + (double)v[0].pos.z + (double)v[1].pos.z) / 3.0));
        shard->m_centroid = centroid;
        shard->m_rotation_axis = centroid.copy_normalize();
        if (shard != m_shards.first() && shard->m_merged)
        {
            glass_shard* prev_shard = shard->prev;
            const vector mean((float)(((double)prev_shard->m_centroid.x + (double)shard->m_centroid.x) * 0.5), (float)(((double)prev_shard->m_centroid.y + (double)shard->m_centroid.y) * 0.5),
                              (float)(((double)prev_shard->m_centroid.z + (double)shard->m_centroid.z) * 0.5));
            shard->m_centroid = mean;
            shard->m_rotation_axis = mean.copy_normalize();
            prev_shard->m_centroid = shard->m_centroid;
            prev_shard->m_rotation_axis = shard->m_rotation_axis;
        }
        // The back face: the front pushed back by the thickness along -fvec, normal and tangent negated.
        const vector back_normal(m_world_xform.m_rotation.fvec.x * -1.0f, m_world_xform.m_rotation.fvec.y * -1.0f, m_world_xform.m_rotation.fvec.z * -1.0f);
        const vector back_tangent(m_world_xform.m_rotation.rvec.x * -1.0f, m_world_xform.m_rotation.rvec.y * -1.0f, m_world_xform.m_rotation.rvec.z * -1.0f);
        for (int k = 0; k < 3; k++)
        {
            v[3 + k] = v[k];
            v[3 + k].normal.set(back_normal.x, back_normal.y, back_normal.z, 0.0f);
            v[3 + k].tangent.set(back_tangent.x, back_tangent.y, back_tangent.z, 0.0f);
            const float thickness = m_pane.m_thickness;
            v[3 + k].pos.set(back_normal.x * thickness + v[k].pos.x, v[k].pos.y + back_normal.y * thickness, back_normal.z * thickness + v[k].pos.z);
            glass_bounds_add(bmin, bmax, v[3 + k].pos);
            glass_debug_copy(shard_index, test_index, v[3 + k]);
        }
        // The three sides (front edge vertex pairs with their back copies), ordered by the winding.
        const int w0 = shard->m_poly.m_winding ? 3 : 0;
        const int w1 = shard->m_poly.m_winding ? 2 : 1;
        const int w2 = shard->m_poly.m_winding ? 1 : 2;
        const int w3 = shard->m_poly.m_winding ? 0 : 3;
        rl_mesh_pixlit_nmap_ca_vertex* side = &v[6];
        side[w0] = v[0];
        side[w1] = v[3];
        side[w2] = v[1];
        side[w3] = v[4];
        create_side_normals(side, false);
        for (int k = 0; k < 4; k++)
        {
            glass_debug_copy(shard_index, test_index, side[k]);
        }
        side = &v[10];
        side[w0] = v[0];
        side[w1] = v[3];
        side[w2] = v[2];
        side[w3] = v[5];
        create_side_normals(side, true);
        for (int k = 0; k < 4; k++)
        {
            glass_debug_copy(shard_index, test_index, side[k]);
        }
        side = &v[14];
        side[w0] = v[2];
        side[w1] = v[5];
        side[w2] = v[1];
        side[w3] = v[4];
        create_side_normals(side, true);
        for (int k = 0; k < 4; k++)
        {
            glass_debug_copy(shard_index, test_index, side[k]);
        }
        shard = shard->next;
        v += 18;
        if (shard == m_shards.first())
        {
            break;
        }
    }
    // Magic number: 1/64, the shards per mesh.
    m_num_instances = (int)ceil((double)((float)m_shards.get_elem_count() * 0.015625f));
    int remaining = m_shards.get_elem_count();
    int left_for_last = m_shards.get_elem_count();
    for (int i = 0; i < m_num_instances; i++)
    {
        m_mesh_p[i] = rl_mesh::create();
        if (m_mesh_p[i] == nullptr)
        {
            m_num_instances = i;
            if (i == 0)
            {
                free_vertex_data(verts, alloc_size);
            }
            return;
        }
        const int num_bones = remaining > 0x3F ? 0x40 : remaining;
        remaining -= num_bones;
        rl_skin_instance* inst = nullptr;
        if (rl_skin_instance::can_allocate_bones(num_bones))
        {
            inst = rl_skin_instance::create();
            m_mesh_instance_p[i] = inst;
        }
        if (inst == nullptr)
        {
            m_mesh_p[i]->release();
            m_num_instances = i;
            if (i == 0)
            {
                free_vertex_data(verts, alloc_size);
            }
            return;
        }
        inst->allocate_bones(num_bones);
        rl_mesh_data_runtime& data = m_mesh_data[i];
        rl_submesh_data& submesh = m_submesh[i];
        data.sub_meshes = &submesh;
        data.num_sub_meshes = 1;
        submesh.num_render_blocks = 1;
        submesh.render_blocks.from_pointer(&m_render_block[i]);
        submesh.offset_transform = ZERO_VECTOR;
        submesh.bmax = bmax;
        submesh.bmin = bmin;
        rl_render_block& block = m_render_block[i];
        block.material_map_idx = m_material_map_index;
        // Magic numbers: 64 shards of 24 indices per mesh.
        block.start_index = (unsigned int)(i * 0x600);
        block.num_indices = (unsigned int)((i == m_num_instances - 1 ? left_for_last : 0x40) * 0x18);
        block.minimum_index = 0;
        block.maximum_index = 0;
        data.vertex_buffer.num_verts = m_shards.get_elem_count() * 0x12;
        data.vertex_buffer.verts = verts;
        data.vertex_buffer.vertex_format = (unsigned char)GLASS_VERTEX_FORMAT_ID;
        data.vertex_buffer.vert_stride_0 = 0x20;
        data.vertex_buffer.num_uv_channels = 1;
        data.vertex_buffer.vert_stride_1 = 0;
        data.index_buffer.num_indices = (unsigned int)Glass_index_buffer_size;
        data.index_buffer.indices = (unsigned char*)Glass_index_buffer;
        data.index_buffer.index_size = 2;
        data.index_buffer.prim_type = 1;
        data.index_buffer.num_blocks = 1;
        m_mesh_p[i]->init(&data, nullptr, false);
        inst->init(m_mesh_p[i], m_material_map_p, 0);
        // Magic numbers: rl_scene_entity flags (0x2000 before the box; 0x24 set, 0x80 cleared after; meanings
        // inferred).
        inst->m_flags |= 0x2000;
        inst->calculate_local_aabb();
        inst->rl_scene_entity::set_position(m_world_xform.m_translation);
        inst->rl_scene_entity::set_rotation(IDENTITY_MATRIX);
        inst->m_flags = (unsigned short)((inst->m_flags & 0xFF7F) | 0x24);
        game_render_scene_update(inst);
        left_for_last -= 0x40;
    }
}

// 0x006E8C80 (:815)
// Two shards share a side: at least two corners match within 0.0001 relative (absolute below 1).
bool shards_share_sides(glass_shard* shard_0_p, glass_shard* shard_1_p)
{
    int matches = 0;
    for (int i = 0; i < 3; i++)
    {
        const vector& a = shard_0_p->m_poly.m_points[i];
        for (int j = 0; j < 3; j++)
        {
            const vector& b = shard_1_p->m_poly.m_points[j];
            const float comps_a[3] = {a.x, a.y, a.z};
            const float comps_b[3] = {b.x, b.y, b.z};
            bool same = true;
            for (int c = 0; c < 3 && same; c++)
            {
                float scale = fabsf(comps_a[c]) - fabsf(comps_b[c]) < 0.0f ? fabsf(comps_b[c]) : fabsf(comps_a[c]);
                if (0.0f <= 1.0f - scale)
                {
                    scale = 1.0f;
                }
                // Magic number: the relative tolerance (a double constant).
                const double tolerance = (double)scale * 0.0001;
                same = (double)comps_b[c] - tolerance <= (double)comps_a[c] && (double)comps_a[c] <= tolerance + (double)comps_b[c];
            }
            if (same && ++matches > 1)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x006E8EF0 (:833)
// The impact time, then per shard: a random variance in [-1, 1], a velocity blending the impact force direction
// towards the direction from the impact point by the shard's distance (with the variance), scaled by the clamped
// force and an inverse square falloff, its speed clamped to the force limits, and a spin from the speed. With
// Glass_merge a shard sharing a side with an unmerged previous shard may (random against Glass_merge_rand) merge with
// it, moving as one.
void glass_obj::physics_init()
{
    m_impact_time = (float)(unsigned int)Frametime_total_milliseconds;
    vector force(m_impact_force.x / Glass_impact_force_scale, m_impact_force.y / Glass_impact_force_scale, m_impact_force.z / Glass_impact_force_scale);
    // The squared lengths are summed in double and rounded once (rfg.exe; hkdiff glass_create_shatter_effect).
    float magnitude = sqrtf((float)((double)force.x * force.x + (double)force.y * force.y + (double)force.z * force.z));
    if (magnitude - Glass_impact_force_min < 0.0f)
    {
        magnitude = Glass_impact_force_min;
    }
    if (0.0f <= magnitude - Glass_impact_force_max)
    {
        magnitude = Glass_impact_force_max;
    }
    const vector dir = force.copy_normalize_safe();
    glass_shard* shard = m_shards.first();
    while (shard != nullptr)
    {
        bool merge = Glass_merge && shard != m_shards.first() && (Glass_merge_rand & vrand()) != 0;
        if (merge)
        {
            glass_shard* prev_shard = shard->prev;
            merge = !prev_shard->m_merged && shards_share_sides(shard, prev_shard);
        }
        if (merge)
        {
            glass_shard* prev_shard = shard->prev;
            shard->m_merged = true;
            shard->m_rand_variance = prev_shard->m_rand_variance;
            shard->m_velocity = prev_shard->m_velocity;
            shard->m_rotation = prev_shard->m_rotation;
        }
        else
        {
            shard->m_merged = false;
            const float variance = fl_rand_range(-1.0f, 1.0f);
            shard->m_rand_variance = variance;
            const vector* p = &shard->m_poly.m_points[0];
            // The centroid in float steps.
            vector to((p[0].x + p[1].x + p[2].x) / 3.0f - m_impact_point.x, (p[0].y + p[1].y + p[2].y) / 3.0f - m_impact_point.y, (p[0].z + p[1].z + p[2].z) / 3.0f - m_impact_point.z);
            const float dist = to.mag_and_normalize_safe();
            float falloff = (float)(1.0 / ((double)dist * dist));
            if (falloff - 0.0f < 0.0f)
            {
                falloff = 0.0f;
            }
            if (!(falloff - Glass_impact_force_max < 0.0f))
            {
                falloff = Glass_impact_force_max;
            }
            // Magic number: the variance spreads the blend by 15% (0.15 as a float constant widened to double).
            float blend = (float)((double)(dist / magnitude) * (double)0.15f * (double)variance + (double)(dist / magnitude));
            if (blend - 0.0f < 0.0f)
            {
                blend = 0.0f;
            }
            if (0.0f <= blend - 1.0f)
            {
                blend = 1.0f;
            }
            shard->m_velocity.set((to.x - dir.x) * blend + dir.x, (to.y - dir.y) * blend + dir.y, (to.z - dir.z) * blend + dir.z);
            shard->m_velocity.set(shard->m_velocity.x * magnitude * falloff, shard->m_velocity.y * magnitude * falloff, shard->m_velocity.z * magnitude * falloff);
            const vector& vel = shard->m_velocity;
            float speed = sqrtf((float)((double)vel.x * vel.x + (double)vel.y * vel.y + (double)vel.z * vel.z));
            if (speed - Glass_impact_force_min < 0.0f)
            {
                speed = Glass_impact_force_min;
            }
            if (0.0f <= speed - Glass_impact_force_max)
            {
                speed = Glass_impact_force_max;
            }
            const vector unit = shard->m_velocity.copy_normalize_safe();
            shard->m_velocity.set(unit.x * speed, unit.y * speed, unit.z * speed);
            if (speed - Glass_rotation_min < 0.0f)
            {
                speed = Glass_rotation_min;
            }
            if (0.0f <= speed - Glass_rotation_max)
            {
                speed = Glass_rotation_max;
            }
            // Magic numbers: the spin varies by up to 15% with the variance (0.15 as a float constant widened to double).
            shard->m_rotation = (float)(((double)variance * 0.5 + 0.5) * (double)speed * (double)0.15f + (double)speed);
        }
        shard = shard->next;
        if (shard == m_shards.first())
        {
            break;
        }
    }
}

// 0x006E3FB0 (:891)
// The pane uv of a point (0 at p0, 1 at p3).
static vector2 compute_glass_uvs(const glass_pane& pane, const vector& point_2d)
{
    return vector2((point_2d.x - pane.m_p0.x) / (pane.m_p3.x - pane.m_p0.x), (point_2d.y - pane.m_p0.y) / (pane.m_p3.y - pane.m_p0.y));
}

// 0x006E9630 (:904)
// Any point strictly inside the pane.
static bool poly_inside_pane(const glass_poly& poly, const glass_pane& pane)
{
    for (int i = 0; i < 3; i++)
    {
        if (point_in_pane(pane, vector2(poly.m_points[i].x, poly.m_points[i].y)))
        {
            return true;
        }
    }
    return false;
}

// 0x006E96B0 (:916)
// Degenerate: two corners equal.
static bool poly_should_ignore(const glass_poly& poly)
{
    const vector* p = &poly.m_points[0];
    const bool d01 = p[0].x == p[1].x && p[0].y == p[1].y && p[0].z == p[1].z;
    const bool d02 = p[0].x == p[2].x && p[0].y == p[2].y && p[0].z == p[2].z;
    const bool d21 = p[2].x == p[1].x && p[2].y == p[1].y && p[2].z == p[1].z;
    return d01 || d02 || d21;
}

// 0x006ED2B0 (:931)
// Whether a shard of ring circle_i is dropped: always within the first Glass_lod_circles rings (the hole around the
// impact); past them, once more than 1/Glass_lod_capacity of the shard pool is in use, with Glass_lod_chance percent.
// Nothing is dropped with the lod off.
static bool poly_lod(const glass_poly& poly, int circle_i)
{
    if (!Glass_lod_enable)
    {
        return false;
    }
    if (circle_i < Glass_lod_circles)
    {
        return true;
    }
    const unsigned int capacity = Glass_shard_pool.get_capacity();
    const unsigned int used = capacity - (unsigned int)Glass_shard_pool.get_free();
    if (capacity / (unsigned int)Glass_lod_capacity < used)
    {
        // Magic number: percent.
        return !((unsigned int)Glass_lod_chance < vrand() % 100);
    }
    return false;
}

// 0x006F6530 (:966)
// One queued glass object shatters per frame.
void glass_obj_do_frame()
{
    glass_obj* glass = Glass_objects_to_process.first();
    if (glass == nullptr)
    {
        return;
    }
    Glass_objects_to_process.remove(glass);
    glass->create_shatter_effect();
}

// 0x006E97B0 (:980)
void glass_obj::queue_shatter_effect(const vector& impact_point, const vector& impact_force)
{
    m_impact_point = impact_point;
    m_impact_force = impact_force;
    Glass_objects_to_process.append(this);
}

// Not in the binary: a ring shard's corner, a circle point around the impact (local pane space).
static vector glass_ring_point(const vector2& circle_point, float lx, float ly, float lz)
{
    return vector(circle_point.x + lx, circle_point.y + ly, (float)((double)lz + 0.0));
}

// Not in the binary: a shard corner's pane uv, then the corner (x, y of the pane) to world space (create_shatter_effect
// inlines this for the clipped and the unclipped shards).
static void glass_shard_corner_to_world(glass_shard* shard, int k, const vector2& point, const glass_pane& pane, const matrix43& xform)
{
    shard->m_poly.m_uvs[k] = compute_glass_uvs(pane, vector(point.x, point.y, 0.0f));
    shard->m_poly.m_points[k] = convert_circle_point2d_point3d(point, ZERO_VECTOR, xform);
}

// Not in the binary: create_shatter_effect up to the mesh build (the hkdiff glass test runs it alone).
static void glass_obj_shatter_shards(glass_obj& glass)
{
    // Magic numbers: the circle and shard counts of rfg.exe.
    const int CIRCLES_MAX = 50;
    const int CIRCLE_POINTS = 10;
    // Not SHARDS_MAX (320): the shard list is capped at 300.
    const int SHARDS_LIMIT = 300;

    // The impact point in pane space.
    const matrix43 inv = glass.m_world_xform.invert_orthogonal();
    float lx = inv.m_rotation.rvec.x * glass.m_impact_point.x + inv.m_rotation.uvec.x * glass.m_impact_point.y + inv.m_rotation.fvec.x * glass.m_impact_point.z + inv.m_translation.x;
    float ly = inv.m_rotation.rvec.y * glass.m_impact_point.x + inv.m_rotation.uvec.y * glass.m_impact_point.y + inv.m_rotation.fvec.y * glass.m_impact_point.z + inv.m_translation.y;
    float lz = inv.m_rotation.rvec.z * glass.m_impact_point.x + inv.m_rotation.uvec.z * glass.m_impact_point.y + inv.m_rotation.fvec.z * glass.m_impact_point.z + inv.m_translation.z;

    // On the pane's plane within (thickness + 1) relative to max(1, |lz|) (an inlined float compare against 0).
    // Notes (original behaviour, kept as is): the tolerance is never below |lz| for a non-negative thickness, so the
    // clamp never runs.
    const float tolerance_base = (float)((double)glass.m_pane.m_thickness + 1.0);
    float scale = (float)fabs((double)lz);
    if ((float)((double)scale - 0.0) < 0.0f)
    {
        scale = 0.0f;
    }
    if (!((float)(1.0 - (double)scale) < 0.0f))
    {
        scale = 1.0f;
    }
    const float tolerance = (float)((double)tolerance_base * (double)scale);
    if ((double)lz < 0.0 - (double)tolerance || !((double)lz <= (double)tolerance + 0.0))
    {
        // Magic number: half the thickness (a double constant).
        lz = (float)((double)glass.m_pane.m_thickness * 0.5);
        if (lx < glass.m_pane.m_p0.x)
        {
            lx = glass.m_pane.m_p0.x;
        }
        if (glass.m_pane.m_p3.x < lx)
        {
            lx = glass.m_pane.m_p3.x;
        }
        if (ly < glass.m_pane.m_p3.y)
        {
            ly = glass.m_pane.m_p3.y;
        }
        if (glass.m_pane.m_p0.y < ly)
        {
            ly = glass.m_pane.m_p0.y;
        }
    }

    // The circles, until one holds the whole pane.
    farray<glass_circle,50> circles;
    circles.resize(CIRCLES_MAX);
    int num_circles = 0;
    float t = 0.0f;
    bool done = false;
    while (num_circles < CIRCLES_MAX && !done)
    {
        glass_circle& circle = circles[num_circles];
        const float radius = (float)pow((double)t, (double)Glass_exp);
        circle.m_radius = radius;
        // Magic numbers: the circle step 0.1 (a double constant) varied by +-0.025.
        t = (float)((double)fl_rand_range(-0.025f, 0.025f) + (double)0.1f + (double)t);
        circle.m_points.resize(CIRCLE_POINTS);
        // Magic number: 0.9 of the radius (0.1 as a double constant).
        const double radius_09 = (double)radius - (double)radius * (double)0.1f;
        float angle = 0.0f;
        for (int k = 0; k < CIRCLE_POINTS; k++)
        {
            // Magic numbers: +-0.0700 pi (12.6 degrees) jitter, 0.2 pi (36 degree) steps.
            const float point_angle = (float)((double)fl_rand_range(-0.219911486f, 0.219911486f) + (double)angle);
            const float point_radius = (float)((double)radius_min + radius_09);
            circle.m_points[k].x = (float)sin((double)point_angle) * point_radius;
            circle.m_points[k].y = (float)cos((double)point_angle) * point_radius;
            angle = (float)((double)angle + (double)0.628318548f);
        }
        // The corners p0, p1, p2, p3 all inside the circle (the squares in double, rounded once).
        const double radius_sq = (double)radius * (double)radius;
        const vector2* corners[4] = {&glass.m_pane.m_p0, &glass.m_pane.m_p1, &glass.m_pane.m_p2, &glass.m_pane.m_p3};
        done = true;
        for (int c = 0; c < 4 && done; c++)
        {
            const float dx = corners[c]->x - lx;
            const float dy = corners[c]->y - ly;
            done = (double)(float)((double)dx * (double)dx + (double)dy * (double)dy) < radius_sq;
        }
        num_circles++;
    }

    // Two shards per point between circle i and i + 1, split along a random diagonal.
    glass.m_shards.init();
    const int last_circle = num_circles - 1;
    for (int i = 1; i < last_circle; i++)
    {
        const farray<vector2,10>& inner = circles[i].m_points;
        const farray<vector2,10>& outer = circles[i + 1].m_points;
        int num_points = inner.size();
        if (num_points > SHARDS_LIMIT)
        {
            num_points = SHARDS_LIMIT;
        }
        bool pool_empty = false;
        for (int j = 0; j < num_points && !pool_empty; j++)
        {
            const bool flip = (vrand() & 1) != 0;
            const int j_next = (j + 1) % num_points;
            for (int half = 0; half < 2; half++)
            {
                glass_shard* shard = Glass_shard_pool.alloc(0);
                if (shard == nullptr)
                {
                    pool_empty = true;
                    break;
                }
                glass.m_shards.append(shard);
                glass_poly& poly = shard->m_poly;
                const vector2* p0;
                const vector2* p1;
                const vector2* p2;
                if (half == 0)
                {
                    p0 = &inner[j];
                    p1 = &outer[j];
                    p2 = flip ? &inner[j_next] : &outer[j_next];
                }
                else if (flip)
                {
                    p0 = &outer[j];
                    p1 = &outer[j_next];
                    p2 = &inner[j_next];
                }
                else
                {
                    p0 = &inner[j];
                    p1 = &outer[j_next];
                    p2 = &inner[j_next];
                }
                poly.m_points[0] = glass_ring_point(*p0, lx, ly, lz);
                poly.m_points[1] = glass_ring_point(*p1, lx, ly, lz);
                poly.m_points[2] = glass_ring_point(*p2, lx, ly, lz);
                if (!poly_inside_pane(poly, glass.m_pane) || poly_should_ignore(poly) || poly_lod(poly, i))
                {
                    glass.m_shards.remove(shard);
                    Glass_shard_pool.free(shard);
                }
            }
        }
    }

    // Shards crossing the pane edge are cut to the pane and triangulated into new shards (collected apart, appended at
    // the end); all get uvs and world positions.
    linked_list_p<glass_shard> clipped;
    clipped.init();
    // Sized generously (rfg.exe's frame holds 15 indices, 5 triangles; a cut triangle has up to 7 corners).
    vector2 inter[32];
    int indices[96];
    vector polygon[32];
    glass_shard* shard = glass.m_shards.first();
    while (shard != nullptr)
    {
        glass_shard* next = shard->next != glass.m_shards.first() ? shard->next : nullptr;
        int num_verts = -1;
        if (!clip_2d_shard(glass.m_pane, shard->m_poly, inter, num_verts))
        {
            // Inside the pane: kept.
            for (int k = 0; k < 3; k++)
            {
                const vector2 point(shard->m_poly.m_points[k].x, shard->m_poly.m_points[k].y);
                glass_shard_corner_to_world(shard, k, point, glass.m_pane, glass.m_world_xform);
            }
            shard = next;
            continue;
        }
        glass.m_shards.remove(shard);
        Glass_shard_pool.free(shard);
        if (num_verts < 3)
        {
            shard = next;
            continue;
        }
        // The cut polygon closed with its first corner again.
        inter[num_verts] = inter[0];
        const int num_poly = num_verts + 1;
        for (int k = 0; k < num_poly; k++)
        {
            polygon[k].set(inter[k].x, inter[k].y, 0.0f);
        }
        const int num_tris = triangulate_concave_polygon(indices, polygon, num_poly);
        const int* index = indices;
        for (int tri = 0; tri < num_tris; tri++)
        {
            glass_shard* piece = Glass_shard_pool.alloc(0);
            if (piece == nullptr)
            {
                break;
            }
            clipped.append(piece);
            for (int k = 0; k < 3; k++)
            {
                glass_shard_corner_to_world(piece, k, inter[*index++], glass.m_pane, glass.m_world_xform);
            }
            if (poly_should_ignore(piece->m_poly))
            {
                clipped.remove(piece);
                Glass_shard_pool.free(piece);
            }
        }
        shard = next;
    }

    // At most 300 shards: the last ones of each list go.
    while (glass.m_shards.get_elem_count() > SHARDS_LIMIT)
    {
        glass_shard* last = glass.m_shards.first() != nullptr ? glass.m_shards.first()->prev : nullptr;
        glass.m_shards.remove(last);
        Glass_shard_pool.free(last);
    }
    while (glass.m_shards.get_elem_count() + clipped.get_elem_count() > SHARDS_LIMIT)
    {
        glass_shard* last = clipped.first() != nullptr ? clipped.first()->prev : nullptr;
        clipped.remove(last);
        Glass_shard_pool.free(last);
    }
    glass.m_shards.concat(clipped);

    // The winding: whether the corners turn counter-clockwise seen along the pane's forward axis.
    const vector& fvec = glass.m_world_xform.m_rotation.fvec;
    for (shard = glass.m_shards.first(); shard != nullptr;)
    {
        const vector* p = &shard->m_poly.m_points[0];
        const float e1x = p[1].x - p[0].x;
        const float e1y = p[1].y - p[0].y;
        const float e1z = p[1].z - p[0].z;
        const float e2x = p[2].x - p[0].x;
        const float e2y = p[2].y - p[0].y;
        const float e2z = p[2].z - p[0].z;
        // The cross product and the dot product in double, rounded once each.
        const float cx = (float)((double)e2z * (double)e1y - (double)e2y * (double)e1z);
        const float cy = (float)((double)e1z * (double)e2x - (double)e2z * (double)e1x);
        const float cz = (float)((double)e2y * (double)e1x - (double)e1y * (double)e2x);
        const float dot = (float)((double)cx * (double)fvec.x + (double)cy * (double)fvec.y + (double)cz * (double)fvec.z);
        shard->m_poly.m_winding = dot > 0.0f;
        shard = shard->next;
        if (shard == glass.m_shards.first())
        {
            break;
        }
    }

    glass.physics_init();

    // One mesh and one skin instance per 64 shards: shards past the free ones go.
    // Magic number: 1/64 (MAX_SHARDS_PER_INSTANCE, a double constant).
    const float meshes_scaled = (float)((double)(float)glass.m_shards.get_elem_count() * 0.015625);
    const int meshes_needed = (int)(float)ceil((double)meshes_scaled);
    int meshes_free = rl_mesh::get_objects_free();
    if (rl_skin_instance::get_objects_free() < meshes_free)
    {
        meshes_free = rl_skin_instance::get_objects_free();
    }
    if (meshes_free < meshes_needed)
    {
        const int shards_kept = meshes_free * glass_obj::MAX_SHARDS_PER_INSTANCE;
        int index = -1;
        for (shard = glass.m_shards.first(); shard != nullptr;)
        {
            glass_shard* next = shard->next != glass.m_shards.first() ? shard->next : nullptr;
            index++;
            if (index >= shards_kept)
            {
                glass.m_shards.remove(shard);
                Glass_shard_pool.free(shard);
            }
            shard = next;
        }
    }
}

// 0x006F3EB0 (:990)
// The impact point in pane space (clamped onto the pane when off its plane), then circles around it: radius
// t^Glass_exp with t stepping 0.1 (+-0.025), 10 points each at 36 degree steps (+-12.6 degrees), until a circle holds
// every pane corner. Each pair of neighbouring circles (from circle 1) becomes a ring of two triangles per point, split
// along a random diagonal; triangles with no corner in the pane, degenerate ones and the LOD-dropped ones go. Then each
// shard is clipped to the pane (the cut polygon triangulated into new shards), given uvs and world positions, the list
// is capped at 300 shards, the winding is recorded, physics_init runs, shards without a free mesh go and the meshes are
// built. Ghidra's decompiler times out on this function; it was ported from the disassembly.
void glass_obj::create_shatter_effect()
{
    glass_obj_shatter_shards(*this);
    create_mesh();
    Glass_objects.append(this);
    if (m_num_instances <= 0)
    {
        glass_obj_release(this);
    }
}

// 0x006E4040 (:1361)
// The integral of the damping density over time: (density^time - 1) / ln(density); time itself without damping
// (density 1 or more). Negative densities count as 0.
static float calculate_damping(float density, float time)
{
    if (!(density < 1.0f))
    {
        return time;
    }
    const float d = (float)((double)density - 0.0) < 0.0f ? 0.0f : density;
    const double powered = (double)(float)pow((double)d, (double)time) - 1.0;
    return (float)(powered / (double)(float)log((double)d));
}

// 0x006F07C0 (:1378)
// Moves the shards (as bones) along their damped ballistic paths: position = velocity times the linear damping
// integral plus half the gravity times t squared, rotation about the shard's axis by its spin times the rotational
// damping integral; the meshes' boxes follow. Released after GLASS_SHARD_LIFETIME.
void glass_obj::update_render()
{
    const double now_ms = (double)(unsigned int)Frametime_total_milliseconds;
    // Magic number: milliseconds to seconds (a double constant).
    const float t = (float)(now_ms * 0.001 - (double)m_impact_time * 0.001);
    if (!(t < GLASS_SHARD_LIFETIME))
    {
        glass_obj_release(this);
        if (this == Glass_temp)
        {
            Glass_temp = nullptr;
        }
        return;
    }
    vector bmin(FLT_MAX, FLT_MAX, FLT_MAX);
    vector bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    const float t2 = (float)((double)t * (double)t);
    // Magic number: the jerk terms run over the 5 s life ((a1 - a0) t^2 (t / 5) / 6, with a1 = a0 here: zero).
    const float gravity = -Glass_gravity;
    const float fall = (float)((double)(float)((double)gravity * 0.5) * (double)t2);
    const vector offset(0.0f, fall, 0.0f);
    const float damping = (float)((double)density_variance_percent * (double)Glass_damping * 0.0 + (double)Glass_damping);
    const float linear = calculate_damping((float)((double)damping * 0.001), t);
    const float rotational = calculate_damping((float)((double)Glass_rotational_damping * (double)damping * 0.001), t);
    matrix43 bones[64];
    int num_bones = 0;
    int instance = 0;
    int shard_count = 0;
    glass_shard* shard = m_shards.first();
    while (shard != nullptr)
    {
        glass_shard* next_shard = shard->next != m_shards.first() ? shard->next : nullptr;
        const vector pos((float)((double)(float)((double)(float)((double)shard->m_velocity.x * linear) + offset.x) + ZERO_VECTOR.x),
                         (float)((double)(float)((double)(float)((double)shard->m_velocity.y * linear) + offset.y) + ZERO_VECTOR.y),
                         (float)((double)(float)((double)(float)((double)shard->m_velocity.z * linear) + offset.z) + ZERO_VECTOR.z));
        shard_count++;
        matrix rotation;
        rotation.set(shard->m_rotation_axis, (float)((double)(float)((double)shard->m_rotation * rotational) * (double)Glass_rotation_scale * DEG_TO_RAD));
        vector translation = pos;
        if (Debug_shard_index >= 0 && shard_count != Debug_shard_index)
        {
            translation.set(FLT_MAX, FLT_MAX, FLT_MAX);
        }
        glass_bounds_add(bmin, bmax, pos);
        bones[num_bones].m_rotation = rotation;
        bones[num_bones].m_translation = translation;
        num_bones++;
        if (num_bones == 0x40)
        {
            m_mesh_instance_p[instance]->set_bones_matricies(bones, 0x40);
            num_bones = 0;
            instance++;
        }
        shard = next_shard;
    }
    if (m_num_instances > 0 && num_bones > 0)
    {
        m_mesh_instance_p[m_num_instances - 1]->set_bones_matricies(bones, num_bones);
    }
    const float dx = m_pane.m_p0.x - m_pane.m_p3.x;
    const float dy = m_pane.m_p0.y - m_pane.m_p3.y;
    const float diagonal = (float)sqrt((double)(float)((double)dy * dy + (double)dx * dx));
    bmin.set(bmin.x - diagonal, bmin.y - diagonal, bmin.z - diagonal);
    const vector box_max(bmax.x + diagonal, bmax.y + diagonal, bmax.z + diagonal);
    for (int i = 0; i < m_num_instances; i++)
    {
        rl_aabb box;
        box.min = bmin;
        box.max = box_max;
        m_mesh_instance_p[i]->set_local_aabb(box);
        game_render_scene_update(m_mesh_instance_p[i]);
    }
}

// 0x006E40C0 (:1479)
static bool glass_get_tint_internal(int texture_handle, rl_color_float& ret)
{
    for (int i = 0; i < Num_glass_tints; i++)
    {
        if (Glass_tints[i].texture_handle == texture_handle)
        {
            ret = Glass_tints[i].tint;
            return true;
        }
    }
    return false;
}

// 0x006ED300 (:1492)
// The glass.xtbl tint of the first texture (up to 17 per material) of the sub mesh's render blocks that has one;
// white otherwise.
void glass_get_tint(rl_compound_mesh_instance* reference_p, int submesh_index, rl_color_float& ret)
{
    ret.red = 1.0f;
    ret.green = 1.0f;
    ret.blue = 1.0f;
    ret.alpha = 1.0f;
    rl_mesh* mesh = (rl_mesh*)reference_p->get_renderable();
    const rl_mesh_data_runtime* data = mesh->get_mesh_data();
    const rl_material_map* map = reference_p->get_material_map();
    if (map == nullptr)
    {
        return;
    }
    const rl_submesh_data& submesh = data->sub_meshes[submesh_index];
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        const unsigned int mat_h = map->get_material(submesh.render_blocks.to_pointer()[b].material_map_idx);
        rl_material* material = (rl_material*)rl_material_library_get_material(mat_h);
        int texture = 0;
        int tmu = 0;
        int tries = 16;
        bool ok = material->get_texture_data(texture, 0);
        while (ok)
        {
            tmu++;
            if (glass_get_tint_internal(texture, ret))
            {
                return;
            }
            if (tries < 0)
            {
                break;
            }
            ok = material->get_texture_data(texture, tmu);
            tries--;
        }
    }
}

// 0x006E9850 (:1522)
// Counts the pending frees down; the first one due is released (one per frame).
static void process_free_buffers()
{
    for (int i = 0; i < Free_buffers.size(); i++)
    {
        const char delay = Free_buffers[i].delay;
        Free_buffers[i].delay = delay - 1;
        if (delay > 0)
        {
            continue;
        }
        Vertex_pool.release_bytes(Free_buffers[i].data);
        Free_buffers.remove_unsorted(i);
        return;
    }
}

// 0x006F1110 (:1533)
static void glass_update_render_all(void*, rl_renderer*)
{
    process_free_buffers();
    glass_obj* glass = Glass_objects.first();
    while (glass != nullptr)
    {
        glass_obj* next = glass->next != Glass_objects.first() ? glass->next : nullptr;
        glass->update_render();
        glass = next;
    }
}

// 0x006E4110 (:1546)
// The vertex pool (250 KB in 29 KB pages) and the shared index buffer: Base_indices repeated for 300 shards.
static void create_buffers()
{
    // Magic numbers: the vertex pool size and page size.
    Vertex_pool_buf = alloc_gpu_data(0x3E800);
    Vertex_pool.init(Vertex_pool_buf, 0x3E800, 0x7400, "glass page mempool", 0x10, 0);
    // Magic number: 300 shards of 24 indices.
    Glass_index_buffer_size = 0x1C20;
    Glass_index_buffer = (short*)alloc_gpu_data(0x3840);
    short vertex_base = 0;
    for (int s = 0; s < 0x1C20; s += 0x18)
    {
        for (int k = 0; k < 24; k++)
        {
            Glass_index_buffer[s + k] = (short)(Base_indices[k] + vertex_base);
        }
        vertex_base += 0x12;
    }
}

// 0x006E41C0 (:1573)
static void free_buffers()
{
    free_gpu_data(Vertex_pool_buf);
    free_gpu_data((unsigned char*)Glass_index_buffer);
}

// ---- public functions

// 0x006ECD70 (:246)
// A glass object for the mover's sub mesh (a single render block): the pane from the sub mesh's box (x and z; y is
// the thickness), the world transform (the sub instance turned to face forward, then the mover), the impact force
// blended towards the pane's facing (at least 40 along it without one) and queued for the shatter.
glass_obj* glass_obj_create(rfg_mover* mover_p, int submesh_index, const matrix43& subinst_xform, const vector& impact_point, const vector& impact_force)
{
    if (!Glass_enabled)
    {
        return nullptr;
    }
    // Notes (decompiled behaviour, kept as is): the mover's data is read without checking it is loaded.
    rl_renderable_instance* model = (rl_renderable_instance*)mover_p->rdd.get_data()->model_instance_p;
    rl_mesh* mesh = (rl_mesh*)model->get_renderable();
    const rl_mesh_data_runtime* data = mesh->get_mesh_data();
    const rl_submesh_data& submesh = data->sub_meshes[submesh_index];
    if (submesh.num_render_blocks != 1)
    {
        return nullptr;
    }
    glass_obj* glass = Glass_obj_pool.alloc(0);
    if (glass == nullptr)
    {
        return nullptr;
    }
    glass->mover_p = nullptr;
    const vector bmin = submesh.bmin;
    const vector bmax = submesh.bmax;
    matrix43 xform = subinst_xform;
    xform.m_rotation.rotate_around_local_x(GLASS_PANE_ROTATION);
    glass->m_pane.m_p0.set(bmin.x, bmax.z);
    glass->m_pane.m_p1.set(bmax.x, bmax.z);
    glass->m_pane.m_p2.set(bmin.x, bmin.z);
    glass->m_pane.m_p3.set(bmax.x, bmin.z);
    glass->m_pane.m_thickness = bmax.y - bmin.y;
    const matrix43 mover_xform(mover_p->pos, mover_p->orient);
    glass->m_world_xform = xform * mover_xform;
    float magnitude = sqrtf(impact_force.y * impact_force.y + impact_force.x * impact_force.x + impact_force.z * impact_force.z);
    vector dir;
    // Magic numbers: forces under 0.01 push 40 along the pane's facing.
    if (0.01f <= magnitude)
    {
        dir = impact_force.copy_normalize();
    }
    else
    {
        dir = glass->m_world_xform.m_rotation.fvec;
        magnitude = 40.0f;
    }
    const float keep = 1.0f - Glass_impact_force_dir_percent;
    const vector& fvec = glass->m_world_xform.m_rotation.fvec;
    vector force(fvec.x * keep + dir.x * Glass_impact_force_dir_percent, fvec.y * keep + dir.y * Glass_impact_force_dir_percent, fvec.z * keep + dir.z * Glass_impact_force_dir_percent);
    if (force.y * dir.y + force.x * dir.x + force.z * dir.z < 0.0f)
    {
        force.set(force.x * -1.0f, force.y * -1.0f, force.z * -1.0f);
    }
    force.set(force.x * magnitude, force.y * magnitude, force.z * magnitude);
    glass->m_material_map_p = model->get_material_map();
    glass->m_material_map_index = submesh.render_blocks.to_pointer()[0].material_map_idx;
    glass->m_vertex_stream_p = nullptr;
    glass->queue_shatter_effect(impact_point, force);
    return glass;
}

// 0x006F0510 (:332)
// Out of its list (live or queued; nothing when in neither), the vertex stream freed (late), the meshes and
// instances released, the shards freed, off its mover, back to the pool.
void glass_obj_release(glass_obj* glass_obj_p)
{
    bool found = false;
    for (glass_obj* glass = Glass_objects.first(); glass != nullptr;)
    {
        if (glass == glass_obj_p)
        {
            Glass_objects.remove(glass_obj_p);
            found = true;
            break;
        }
        glass = glass->next;
        if (glass == Glass_objects.first())
        {
            break;
        }
    }
    if (!found)
    {
        glass_obj* glass = Glass_objects_to_process.first();
        if (glass == nullptr)
        {
            return;
        }
        while (glass != glass_obj_p)
        {
            glass = glass->next;
            if (glass == Glass_objects_to_process.first() || glass == nullptr)
            {
                return;
            }
        }
        Glass_objects_to_process.remove(glass_obj_p);
    }
    if (glass_obj_p->m_num_instances > 0)
    {
        free_vertex_data((unsigned char*)glass_obj_p->m_vertex_stream_p, glass_obj_p->m_vertex_alloc_size);
        glass_obj_p->m_vertex_stream_p = nullptr;
        for (int i = 0; i < glass_obj_p->m_num_instances; i++)
        {
            if (glass_obj_p->m_mesh_p[i] != nullptr)
            {
                glass_obj_p->m_mesh_p[i]->release();
                glass_obj_p->m_mesh_p[i] = nullptr;
            }
            if (glass_obj_p->m_mesh_instance_p[i] != nullptr)
            {
                glass_obj_p->m_mesh_instance_p[i]->release();
                glass_obj_p->m_mesh_instance_p[i] = nullptr;
            }
        }
    }
    if (glass_obj_p->m_vertex_stream_p != nullptr)
    {
        free_vertex_data((unsigned char*)glass_obj_p->m_vertex_stream_p, glass_obj_p->m_vertex_alloc_size);
        glass_obj_p->m_vertex_stream_p = nullptr;
    }
    glass_shard* shard = glass_obj_p->m_shards.first();
    while (shard != nullptr)
    {
        glass_shard* next = shard->next;
        glass_shard* head = glass_obj_p->m_shards.first();
        glass_obj_p->m_shards.remove(shard);
        Glass_shard_pool.free(shard);
        shard = next != head ? next : nullptr;
    }
    glass_obj_p->m_shards.init();
    if (glass_obj_p->mover_p != nullptr)
    {
        glass_obj_p->mover_p->remove_glass_object(glass_obj_p);
        glass_obj_p->mover_p = nullptr;
    }
    Glass_obj_pool.free(glass_obj_p);
}

// 0x006E41F0 (:1580)
// glass.xtbl: per Glass entry (up to 50) the bitmap and its tint (R, G, B bytes, white by default; sRGB to linear).
void parse_glass_table()
{
    xml_element* table = xml_table_parse("glass.xtbl");
    Num_glass_tints = 0;
    for (xml_element* glass = xml_table_find_opt(table, "Glass"); glass != nullptr && Num_glass_tints < 50; Num_glass_tints++)
    {
        char bitmap_name[64];
        xml_table_get_req_string(bitmap_name, 0x40, glass, "Bitmap_name");
        Glass_tints[Num_glass_tints].texture_handle = bm_load(bitmap_name, false);
        unsigned char r = 0xFF;
        unsigned char g = 0xFF;
        unsigned char b = 0xFF;
        xml_element* color = xml_find(glass, "Color");
        if (color != nullptr)
        {
            xml_element* re = xml_find(color, "R");
            xml_element* ge = xml_find(color, "G");
            xml_element* be = xml_find(color, "B");
            if (re != nullptr)
            {
                r = (unsigned char)atoi(re->text);
            }
            if (ge != nullptr)
            {
                g = (unsigned char)atoi(ge->text);
            }
            if (be != nullptr)
            {
                b = (unsigned char)atoi(be->text);
            }
        }
        rl_color_float srgb;
        // Magic number: bytes to [0, 1] (a double constant).
        srgb.red = (float)((double)(float)r / 255.0);
        srgb.green = (float)((double)(float)g / 255.0);
        srgb.blue = (float)((double)(float)b / 255.0);
        srgb.alpha = 1.0f;
        Glass_tints[Num_glass_tints].tint = srgb.from_sRGB_to_linear();
        glass = xml_table_find_next(table, glass, "Glass");
    }
    xml_table_close(table);
}

// 0x006E4400 (:1644)
// Unreferenced in rfg.exe.
void unused_dcf_reload_glass_table()
{
    parse_glass_table();
}

// 0x006F1150 (:1650)
void glass_system_init()
{
    if (Glass_system_init)
    {
        return;
    }
    Glass_system_init = true;
    Free_buffers.remove_all();
    Update_render_task_glass.m_callback = glass_update_render_all;
    game_render_update_render_task_register(&Update_render_task_glass);
    create_buffers();
    parse_glass_table();
}

// 0x006E4410 (:1692)
void glass_system_shutdown()
{
    if (!Glass_system_init)
    {
        return;
    }
    Glass_system_init = false;
    free_buffers();
}

// 0x006E4450 (:1708)
// Empty in rfg.exe.
void glass_level_init_pre()
{
}

// 0x006F11B0 (:1712)
// Every glass object (live and queued) is released and every pending vertex buffer freed at once.
void glass_level_shutdown()
{
    if (Glass_temp != nullptr)
    {
        glass_obj_release(Glass_temp);
        Glass_temp = nullptr;
    }
    for (glass_obj* glass = Glass_objects.first(); glass != nullptr;)
    {
        glass_obj* next = glass->next != Glass_objects.first() ? glass->next : nullptr;
        glass_obj_release(glass);
        glass = next;
    }
    for (glass_obj* glass = Glass_objects_to_process.first(); glass != nullptr;)
    {
        glass_obj* next = glass->next != Glass_objects_to_process.first() ? glass->next : nullptr;
        glass_obj_release(glass);
        glass = next;
    }
    Glass_objects.init();
    Glass_objects_to_process.init();
    for (int i = 0; i < Free_buffers.size(); i++)
    {
        Vertex_pool.release_bytes(Free_buffers[i].data);
    }
    Free_buffers.remove_all();
}
