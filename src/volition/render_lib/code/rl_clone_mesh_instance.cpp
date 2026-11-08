// Original: volition/render_lib/code/rl_clone_mesh_instance.cpp
//
// Clone meshes (lod.md L2): many copies of one mesh drawn with instancing. An rl_clone_mesh_pool owns an array of
// clone elements (32 bytes each) handed out from its end downwards; each rl_clone_mesh_instance takes a block of
// them and keeps them in the pool's ring of instances. An instance draws its clones relative to its own position.
// Elements are either instances (a fixed point orientation, colour, position and random value; the undergrowth's) or
// spline knots (a Catmull-Rom style spline with a tension; clones placed along each segment).
//
// LOD: a clone mesh has a table of LOD descriptions (fade in and fade out distance ranges and a sub mesh each). Every
// frame the instance finds the distance range from the camera to its world box (nearest and farthest box corner) and
// draws each LOD whose ranges overlap it, so two neighbouring LODs cross-fade where their ranges meet. The fade
// ranges go to the shader as instance parameters (scale and offset per range, calculate_instance_params); the
// instance's own fade range (the undergrowth layer's) caps the fade out.
//
// Only the unbatched render path draws: add_to_batch, render_for_deferred and process_internal_lod are empty in the
// binary, and so is set_wind_info (the undergrowth's wind control vectors go nowhere). The draw is keen's
// writeDrawInstancedVertexBuffersCommand inlined: the mesh's vertex buffer as stream 0, then the frame's instance
// buffer with 0x44 bytes per clone (a 3x4 transform with the instance's offset from the render offset, the four
// instance parameters and an 8-bit RGBA colour).

#include "volition/render_lib/code/rl_clone_mesh_instance.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include "keen/base/trace.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// <rl_clone_mesh_pool::factory_init> 0x00509FB0 (:203)
// <rl_clone_mesh_pool::create> 0x005216D0 (:203)
RL_FACTORY_CLASS_CORE(rl_clone_mesh_pool, 0x11235813, 4)
// <rl_clone_mesh_instance::can_create> 0x004E2FD0 (:331)
// <rl_clone_mesh_instance::factory_init> 0x0050A0B0 (:331)
// <rl_clone_mesh_instance::create> 0x00548FC0 (:331)
RL_FACTORY_CLASS_CORE(rl_clone_mesh_instance, 0x01491625, 1024)

// Every clone mesh draws its highest LOD over the instance's whole fade range (off in retail; a debug switch).
bool RL_clone_force_high_lod = false; // 0x01B7BC51
// Clone mesh drawing (on in retail; a debug switch).
bool g_cloneMeshRenderingEnabled = true; // 0x014FA08B

// The LOD table of a clone mesh until set_lod_descs: one LOD, fully faded in from 0 m, fading out at 5000 m.
// Magic numbers: the retail .data values.
static rl_clone_mesh_lod_desc Default_lod_desc = {{0.0f, 0.0f}, {5000.0f, 5000.0f}, 0, 0}; // 0x014FA104

// Magic number: the size of a clone's instance data (rl_clone_mesh_instance::render).
static const unsigned int CLONE_INSTANCE_SIZE = 0x44;
// Magic number: at most this many render blocks of a LOD's sub mesh are drawn.
static const int MAX_CLONE_RENDER_BLOCKS = 4;
// LOD description flag: the clones face the camera (turned about the up axis), keeping their scale.
static const unsigned int CLONE_LOD_FLAG_BILLBOARD = 1;

// ---- rl_clone_mesh_pool

// 0x004BBAD0 (.h:337)
rl_clone_mesh_pool::rl_clone_mesh_pool()
{
    m_instance_list = nullptr;
    m_dyn_alloc = false;
}

// 0x004BBAF0 (.h:343)
// Notes (decompiled behaviour, kept as is): the array allocated with new[] is freed with the scalar operator delete
// (equivalent for the trivially destructible elements).
rl_clone_mesh_pool::~rl_clone_mesh_pool()
{
    if (m_dyn_alloc && m_clones_array != nullptr)
    {
        delete[] m_clones_array;
    }
}

// 0x005379C0 (:203)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_clone_mesh_pool::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_clone_mesh_pool();
}

// 0x004E2F70 (:219)
// The clone array from the pool (16-byte aligned), or from the heap without one.
void rl_clone_mesh_pool::init(int max_num_instances, mempool_base* pool)
{
    if (pool != nullptr)
    {
        // Magic number: the clone array is 16-byte aligned.
        m_clones_array = (rl_clone_mesh_element*)pool->alloc(max_num_instances * sizeof(rl_clone_mesh_element), 16);
        m_max_clone_instances = max_num_instances;
        m_available_clones = max_num_instances;
        return;
    }
    m_clones_array = new rl_clone_mesh_element[max_num_instances];
    m_max_clone_instances = max_num_instances;
    m_available_clones = max_num_instances;
    m_dyn_alloc = true;
}

// 0x004BC3E0 (:233)
void rl_clone_mesh_pool::init(int max_num_instances, rl_clone_mesh_element* instance_array)
{
    m_clones_array = instance_array;
    m_max_clone_instances = max_num_instances;
    m_available_clones = max_num_instances;
}

// 0x004BC400 (:250)
// The clones are taken from the end of the free part: [m_available_clones, m_max_clone_instances) is in use.
rl_clone_mesh_element* rl_clone_mesh_pool::alloc_clones(int num_clones)
{
    if (m_available_clones < num_clones)
    {
        return nullptr;
    }
    m_available_clones -= num_clones;
    return m_clones_array + m_available_clones;
}

// 0x004BC420 (:266)
// The used clones below the freed block move up over it, and the instances whose clones moved follow them.
void rl_clone_mesh_pool::free_clones(rl_clone_mesh_element* clones, int num_clones)
{
    if (num_clones <= 0)
    {
        return;
    }
    rl_clone_mesh_element* first_used = m_clones_array + m_available_clones;
    memmove(first_used + num_clones, first_used, (clones - first_used) * sizeof(rl_clone_mesh_element));
    m_available_clones += num_clones;
    rl_clone_mesh_instance* inst = m_instance_list;
    do
    {
        if (inst == nullptr)
        {
            return;
        }
        if (inst->m_elements < clones && inst->m_elements != nullptr)
        {
            inst->m_elements += num_clones;
        }
        inst = inst->next;
    } while (inst != m_instance_list);
}

// 0x0091BE20 (.h:205)
bool rl_clone_mesh_pool::can_alloc(int num_clones)
{
    return num_clones <= m_available_clones;
}

// 0x004BC490 (:307)
// Appended to the pool's ring of instances.
void rl_clone_mesh_pool::add_instance(rl_clone_mesh_instance* new_inst)
{
    if (new_inst == nullptr)
    {
        return;
    }
    if (m_instance_list == nullptr)
    {
        new_inst->next = new_inst;
        new_inst->prev = new_inst;
        m_instance_list = new_inst;
        return;
    }
    new_inst->prev = m_instance_list->prev;
    new_inst->next = m_instance_list;
    m_instance_list->prev->next = new_inst;
    m_instance_list->prev = new_inst;
}

// 0x004BC4F0 (:316)
void rl_clone_mesh_pool::remove_instance(rl_clone_mesh_instance* inst)
{
    if (inst == nullptr)
    {
        return;
    }
    if (m_instance_list == inst)
    {
        if (inst->next == inst)
        {
            inst->next = nullptr;
            inst->prev = nullptr;
            m_instance_list = nullptr;
            return;
        }
        m_instance_list = inst->next;
    }
    if (inst->prev != nullptr)
    {
        inst->prev->next = inst->next;
    }
    if (inst->next != nullptr)
    {
        inst->next->prev = inst->prev;
    }
    inst->next = nullptr;
    inst->prev = nullptr;
}

// ---- splitting and deleting clones

// 0x004BC2D0
static int clone_index_compare(const void* arg1, const void* arg2)
{
    return *(const int*)arg1 - *(const int*)arg2;
}

// 0x004BC2E0
// Moves the listed clones (indices sorted in place) to the end of the source array by swapping them with the last
// clones that are not listed, then hands that tail to the destination.
static void split_clone_array_internal(split_clone_array_params* src_params, split_clone_array_params* dst_params, int* clone_indices, int num_clones)
{
    qsort(clone_indices, num_clones, sizeof(int), clone_index_compare);
    rl_clone_mesh_element* const elements = *src_params->clone_array;
    int last = *src_params->array_size;
    int hi = num_clones - 1;
    for (int lo = 0; lo < num_clones; lo++)
    {
        // The last clone not listed (listed clones already at the end are skipped).
        last--;
        while (hi >= 0 && last >= 0 && clone_indices[hi] == last)
        {
            hi--;
            last--;
        }
        const int index = clone_indices[lo];
        if (last <= index)
        {
            break;
        }
        // The 32 bytes are swapped as raw memory (the element's union is not copyable in C++).
        unsigned char swap[sizeof(rl_clone_mesh_element)];
        memcpy(swap, &elements[index], sizeof(rl_clone_mesh_element));
        memcpy(&elements[index], &elements[last], sizeof(rl_clone_mesh_element));
        memcpy(&elements[last], swap, sizeof(rl_clone_mesh_element));
    }
    *src_params->array_size -= num_clones;
    *dst_params->array_size = num_clones;
    *dst_params->clone_array = *src_params->clone_array + *src_params->array_size;
}

// ---- rl_clone_mesh_instance

// 0x00548F00 (.h:353)
// Not batchable.
rl_clone_mesh_instance::rl_clone_mesh_instance()
{
    m_elements = nullptr;
    m_parent_pool = nullptr;
    m_mat_map = nullptr;
    m_num_lods = 0;
    m_num_elements = 0;
    m_flags &= ~FLAGS_IS_BATCHABLE;
    m_last_processed_frame = 0;
}

// 0x00548F70 (.h:364)
// The clones go back to the pool. Notes (decompiled behaviour, kept as is): an instance without clones stays in the
// pool's ring.
rl_clone_mesh_instance::~rl_clone_mesh_instance()
{
    if (m_parent_pool != nullptr && m_elements != nullptr)
    {
        m_parent_pool->free_clones(m_elements, m_num_elements);
        m_elements = nullptr;
        m_parent_pool->remove_instance(this);
    }
}

// 0x00537A40 (:331)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_clone_mesh_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_clone_mesh_instance();
}

// 0x0050A1B0 (:344)
// The instance takes num_clone_elements clones from the pool and copies the elements in; its local box holds the
// mesh's first sub mesh box at every clone. It starts at the origin with no rotation and one LOD (Default_lod_desc).
// Fails without a pool, elements, mesh or material map, or when the pool is out of clones.
bool rl_clone_mesh_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_clone_mesh_pool* parent_pool, rl_clone_mesh_element* clone_element_p, int num_clone_elements,
                                  rl_clone_mesh_element_type element_type, float spline_tension, int num_clones_per_segment)
{
    if (parent_pool == nullptr || clone_element_p == nullptr || mesh_p == nullptr || mat_map_p == nullptr)
    {
        return false;
    }
    if (mesh_p->get_mesh_data()->vertex_buffer.vertex_format != RLVF_CLONE_CLR)
    {
        keen::debug::nativePrintArgs("clone mesh instance inited with invalid vertex format (%d) !", mesh_p->get_mesh_data()->vertex_buffer.vertex_format);
    }
    parent_pool->add_instance(this);
    m_num_clones_per_segment = num_clones_per_segment;
    m_spline_tension = spline_tension;
    vector bmin(FLT_MAX, FLT_MAX, FLT_MAX);
    vector bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_parent_pool = parent_pool;
    m_renderable_p = mesh_p;
    m_mat_map = mat_map_p;
    m_num_lods = 1;
    m_lod_descs = &Default_lod_desc;
    m_element_type = element_type;
    vector sub_min;
    vector sub_max;
    mesh_p->get_subpiece_local_bb(0, sub_min, sub_max);
    m_num_elements = 0;
    m_elements = parent_pool->alloc_clones(num_clone_elements);
    if (m_elements == nullptr)
    {
        parent_pool->remove_instance(this);
        return false;
    }
    memcpy(m_elements, clone_element_p, num_clone_elements * sizeof(rl_clone_mesh_element));
    m_num_elements = num_clone_elements;
    const int num_clones = get_num_clones();
    for (int i = 0; i < num_clones; i++)
    {
        rl_clone_mesh_info info;
        get_clone(i, info);
        vector inst_min;
        vector inst_max;
        bbox_instance(inst_min, inst_max, info.orient, info.pos, sub_min, sub_max);
        bbox_expand(bmin, bmax, inst_min, inst_max);
    }
    set_position(ZERO_VECTOR);
    set_rotation(IDENTITY_MATRIX);
    set_local_aabb(rl_aabb(bmin, bmax));
    update_partition_system();
    return true;
}

// 0x0050A600 (:408)
// An instance of the sibling's mesh over split-off clones: the sibling's pool, mesh, material map, LODs, local box,
// position and rotation. Notes (decompiled behaviour, kept as is): the fade range and the wind settings are not
// copied, and the split clones' box is the sibling's whole box.
void rl_clone_mesh_instance::init(rl_clone_mesh_instance* sibling, rl_clone_mesh_element* split_clones, int num_split_clones, rl_clone_mesh_element_type element_type, int num_clones_per_segment)
{
    sibling->m_parent_pool->add_instance(this);
    m_parent_pool = sibling->m_parent_pool;
    m_renderable_p = sibling->m_renderable_p;
    m_mat_map = sibling->m_mat_map;
    m_num_lods = sibling->m_num_lods;
    m_lod_descs = sibling->m_lod_descs;
    m_elements = split_clones;
    m_num_elements = num_split_clones;
    m_element_type = element_type;
    m_num_clones_per_segment = num_clones_per_segment;
    set_local_aabb(sibling->get_local_aabb());
    set_position(sibling->get_position());
    set_rotation(sibling->get_rotation());
    update_partition_system();
}

// 0x004BC570 (:429)
// Empty: clone meshes are not batched.
void rl_clone_mesh_instance::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
}

// 0x004BC580 (:446)
// Empty.
void rl_clone_mesh_instance::render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags)
{
}

// 0x004E2DA0
// The squared distances from pt to the nearest and the farthest corner of the box (the corners most along and most
// against the direction from the box's centre to pt); near is 0 inside the box. LTCG: bvolume in ESI, pt in EDI.
static void get_distance_range(float& dist_to_near_sq, float& dist_to_far_sq, const rl_bvolume& bvolume, const vector& pt)
{
    vector dir(pt.x - bvolume.m_sphere.m_position.x, pt.y - bvolume.m_sphere.m_position.y, pt.z - bvolume.m_sphere.m_position.z);
    dir.normalize_safe();
    int neg_index;
    int pos_index;
    bbox_find_np_vertices(neg_index, pos_index, dir);
    const vector bmin(bvolume.m_sphere.m_position.x - bvolume.m_half_vec.x, bvolume.m_sphere.m_position.y - bvolume.m_half_vec.y, bvolume.m_sphere.m_position.z - bvolume.m_half_vec.z);
    const vector bmax(bvolume.m_half_vec.x + bvolume.m_sphere.m_position.x, bvolume.m_half_vec.y + bvolume.m_sphere.m_position.y, bvolume.m_half_vec.z + bvolume.m_sphere.m_position.z);
    vector near_vertex;
    vector far_vertex;
    bbox_find_ith_vertex(near_vertex, pos_index, bmin, bmax);
    bbox_find_ith_vertex(far_vertex, neg_index, bmin, bmax);
    dist_to_near_sq = ix_point_in_box(pt, bmin, bmax) ? 0.0f : vec_dist_squared(near_vertex, pt);
    dist_to_far_sq = vec_dist_squared(far_vertex, pt);
}

// 0x0050A990 (:464)
// The LODs to draw: every LOD whose fade out ends at or beyond the box's nearest corner and whose fade in starts at
// or before its farthest corner. One LOD (or RL_clone_force_high_lod, or no active camera) draws LOD 0 only. Beyond
// the last LOD's fade out the range is the last LOD; an empty range becomes LOD 0.
// Notes (decompiled behaviour, kept as is): the last LOD's fade out is squared in double, the others' are rounded to
// float first; the "beyond" result is overwritten by the loop's start value anyway.
void rl_clone_mesh_instance::determine_lod_range(int& min_lod, int& max_lod, rl_renderer& renderer)
{
    min_lod = 0;
    max_lod = 0;
    if (m_num_lods <= 1 || RL_clone_force_high_lod)
    {
        return;
    }
    const rl_camera* camera = renderer.get_active_camera();
    if (camera == nullptr)
    {
        return;
    }
    float dist_to_near_sq;
    float dist_to_far_sq;
    get_distance_range(dist_to_near_sq, dist_to_far_sq, get_world_bvolume(), camera->get_position());
    const double last_fade_out_end = (double)m_lod_descs[m_num_lods - 1].fade_out_range.end;
    if ((double)dist_to_near_sq >= last_fade_out_end * last_fade_out_end)
    {
        max_lod = m_num_lods - 1;
        min_lod = m_num_lods - 1;
    }
    min_lod = m_num_lods - 1;
    for (int i = 0; i < m_num_lods; i++)
    {
        const double fade_out_end = (double)m_lod_descs[i].fade_out_range.end;
        const double fade_in_start = (double)m_lod_descs[i].fade_in_range.start;
        if ((double)(float)(fade_out_end * fade_out_end) >= (double)dist_to_near_sq && (double)dist_to_far_sq >= (double)(float)(fade_in_start * fade_in_start))
        {
            min_lod = min_lod < i ? min_lod : i;
            max_lod = max_lod > i ? max_lod : i;
        }
    }
    if (min_lod > max_lod)
    {
        min_lod = 0;
        max_lod = 0;
    }
}

// 0x004BC590 (:514)
// The LOD's fade in range and its fade out range capped by the instance's fade range (the instance's own range with
// RL_clone_force_high_lod), as scale and offset pairs for the shader: params = (1 / (in_end - in_start),
// -in_start / (in_end - in_start), 1 / (out_end - out_start), -out_start / (out_end - out_start)). An empty range is
// widened by 0.0001.
void rl_clone_mesh_instance::calculate_instance_params(int lod_index)
{
    const rl_clone_mesh_lod_desc& lod_desc = m_lod_descs[lod_index];
    const float fade_in_start = lod_desc.fade_in_range.start;
    float fade_in_end = lod_desc.fade_in_range.end;
    float fade_out_start;
    float fade_out_end;
    if (RL_clone_force_high_lod)
    {
        fade_out_start = m_fade_range.start;
        fade_out_end = m_fade_range.end;
    }
    else
    {
        fade_out_start = lod_desc.fade_out_range.start - m_fade_range.start < 0.0f ? lod_desc.fade_out_range.start : m_fade_range.start;
        fade_out_end = lod_desc.fade_out_range.end - m_fade_range.end < 0.0f ? lod_desc.fade_out_range.end : m_fade_range.end;
    }
    // Magic number: the widening of an empty range, the float 0.0001 added in double.
    const double epsilon = (double)0.0001f;
    if (fade_in_start == fade_in_end)
    {
        fade_in_end = (float)((double)fade_in_end + epsilon);
    }
    if (fade_out_start == fade_out_end)
    {
        fade_out_start = (float)((double)fade_out_start - epsilon);
    }
    const float fade_in_scale = (float)(1.0 / ((double)fade_in_end - (double)fade_in_start));
    const float fade_out_scale = (float)(1.0 / ((double)fade_out_end - (double)fade_out_start));
    m_instance_parameters.x = fade_in_scale;
    m_instance_parameters.y = (float)((double)fade_in_start * -1.0 * (double)fade_in_scale);
    m_instance_parameters.z = fade_out_scale;
    m_instance_parameters.w = (float)((double)fade_out_start * -1.0 * (double)fade_out_scale);
}

// 0x004BC720 (:553)
// The listed clones go back to the pool.
void rl_clone_mesh_instance::delete_clones(int* clone_indices, int num_clones)
{
    split_clone_array_params src;
    src.array_size = &m_num_elements;
    src.clone_array = &m_elements;
    int deleted_size = 0;
    rl_clone_mesh_element* deleted = nullptr;
    split_clone_array_params dst;
    dst.array_size = &deleted_size;
    dst.clone_array = &deleted;
    split_clone_array_internal(&src, &dst, clone_indices, num_clones);
    m_parent_pool->free_clones(deleted, deleted_size);
}

// 0x00549020 (:570)
// A new instance of the same mesh over the listed clones (which leave this instance), or none when the factory is
// empty.
rl_clone_mesh_instance* rl_clone_mesh_instance::split_clones(int* clone_indices, int num_clones)
{
    rl_clone_mesh_instance* split_inst = create();
    if (split_inst == nullptr)
    {
        return nullptr;
    }
    split_clone_array_params src;
    src.array_size = &m_num_elements;
    src.clone_array = &m_elements;
    int split_size = 0;
    rl_clone_mesh_element* split = nullptr;
    split_clone_array_params dst;
    dst.array_size = &split_size;
    dst.clone_array = &split;
    split_clone_array_internal(&src, &dst, clone_indices, num_clones);
    split_inst->init(this, split, split_size, m_element_type, m_num_clones_per_segment);
    return split_inst;
}

// 0x004BC790 (:594)
void rl_clone_mesh_instance::set_material_map(const rl_material_map* mat_p)
{
    if (mat_p != nullptr)
    {
        m_mat_map = mat_p;
    }
}

// 0x004E2FE0 (:602)
// A spline of n knots has n - 3 segments.
int rl_clone_mesh_instance::get_num_clones()
{
    if (m_element_type == RLCMET_INSTANCES)
    {
        return m_num_elements;
    }
    if (m_element_type == RLCMET_SPLINE)
    {
        const int num_clones = (m_num_elements - 3) * m_num_clones_per_segment;
        return num_clones < 1 ? 0 : num_clones;
    }
    return 0;
}

// 0x004E3020 (:613)
// An instance clone is unpacked. A spline clone lies on its segment (knots i..i+3) at t = (clone % per segment) /
// per segment with the tension's basis; it faces along the spline's tangent (up as close to world up as it can),
// it is white and its random value 0. The basis and the tangent are evaluated in double as the binary does (the weights rounded to float).
void rl_clone_mesh_instance::get_clone(int clone_index, rl_clone_mesh_info& _info)
{
    if (m_element_type == RLCMET_INSTANCES)
    {
        m_elements[clone_index].get(_info);
        return;
    }
    if (m_element_type != RLCMET_SPLINE)
    {
        return;
    }
    const int segment = clone_index / m_num_clones_per_segment;
    const vector4& p0 = m_elements[segment].knot_pos;
    const vector4& p1 = m_elements[segment + 1].knot_pos;
    const vector4& p2 = m_elements[segment + 2].knot_pos;
    const vector4& p3 = m_elements[segment + 3].knot_pos;
    const float frac = (float)((double)(float)(clone_index % m_num_clones_per_segment) / (double)(float)m_num_clones_per_segment);
    const float t = (float)((double)frac - 0.0) >= 0.0f ? frac : 0.0f;
    const float t2 = (float)((double)t * (double)t);
    const float t3 = (float)((double)t2 * (double)t);
    const float a = (float)((1.0 - (double)m_spline_tension) * 0.5);
    const double a_t = (double)a * (double)t;
    const double a2 = (double)a * 2.0;
    const double c3 = 3.0 - a2;
    // The position weights of knots 0-3.
    const float w0 = (float)(((double)(-a) * (double)t3 + (double)t2 * a2) - a_t);
    const float w1 = (float)(((double)t2 * ((double)a - 3.0) + (double)t3 * (2.0 - (double)a)) + 1.0);
    const float w2 = (float)(((double)t3 * ((double)a - 2.0) + (double)t2 * c3) + a_t);
    const float w3 = (float)((double)a * (double)t3 - (double)a * (double)t2);
    _info.pos.x = (float)((((double)p1.x * (double)w1 + (double)p0.x * (double)w0) + (double)p2.x * (double)w2) + (double)p3.x * (double)w3);
    _info.pos.y = (float)((((double)p1.y * (double)w1 + (double)p0.y * (double)w0) + (double)p2.y * (double)w2) + (double)p3.y * (double)w3);
    _info.pos.z = (float)((((double)p1.z * (double)w1 + (double)p0.z * (double)w0) + (double)p2.z * (double)w2) + (double)p3.z * (double)w3);
    // The weights' derivatives.
    const float d0 = (float)(((double)a * -3.0 * (double)t2 + (double)a * 4.0 * (double)t) - (double)a);
    const float d1 = (float)(((double)a - 3.0) * 2.0 * (double)t + (2.0 - (double)a) * 3.0 * (double)t2);
    const float d2 = (float)((((double)a - 2.0) * 3.0 * (double)t2 + c3 * 2.0 * (double)t) + (double)a);
    const float d3 = (float)((double)a * 3.0 * (double)t2 - (double)t * a2);
    vector tangent;
    tangent.x = (float)((((double)d1 * (double)p1.x + (double)d0 * (double)p0.x) + (double)d2 * (double)p2.x) + (double)d3 * (double)p3.x);
    tangent.y = (float)((((double)d1 * (double)p1.y + (double)d0 * (double)p0.y) + (double)d2 * (double)p2.y) + (double)d3 * (double)p3.y);
    tangent.z = (float)((((double)d1 * (double)p1.z + (double)d0 * (double)p0.z) + (double)d2 * (double)p2.z) + (double)d3 * (double)p3.z);
    tangent.normalize_safe();
    _info.orient.make_uvec(tangent, UP_VECTOR);
    _info.color = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    _info.rand_value = 0.0f;
}

// 0x004BC7C0 (:668)
// Empty.
void rl_clone_mesh_instance::process_internal_lod(rl_renderer& renderer, int lod_index)
{
}

// 0x0050AAF0 (:1028)
// The LOD range once per frame.
void rl_clone_mesh_instance::process_internal(rl_renderer& renderer)
{
    const rl_render_lib* render_lib = rl_render_lib::get_instance();
    if (render_lib->get_current_frame_count() != m_last_processed_frame)
    {
        determine_lod_range(m_min_lod, m_max_lod, renderer);
        m_last_processed_frame = render_lib->get_current_frame_count();
    }
}

// 0x00521740 (:720)
// Every LOD of the frame's range: its instance parameters, the standard shader in configuration 1, then each render
// block (at most MAX_CLONE_RENDER_BLOCKS) with its material as one instanced draw of all clones. The clones go into
// the frame's instance buffer per block (rl_renderer::begin_instancing_batch), relative to the render offset; with
// the LOD's billboard flag each clone is turned to face the camera (its forward axis towards it, up as close to
// world up as it can), keeping its scale. Untinted.
// Notes (decompiled behaviour, kept as is): process_internal is inlined; the clone instance data is rebuilt for every
// render block; only the clone vertex format (RLVF_CLONE_CLR) draws. The draw is
// GraphicsCommandWriter::writeDrawInstancedVertexBuffersCommand inlined, with four geometry streams (only stream 0
// set) and the instance stream; the index buffer data's index count is left unset.
void rl_clone_mesh_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    process_internal(renderer);
    if (rl_render_lib::get_instance()->get_current_frame_count() != m_last_processed_frame || !g_cloneMeshRenderingEnabled)
    {
        return;
    }
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
    const vector& render_offset = renderer.get_render_offset();
    const vector& position = get_position();
    const vector offset(position.x - render_offset.x, position.y - render_offset.y, position.z - render_offset.z);
    for (int lod_index = m_min_lod; lod_index <= m_max_lod; lod_index++)
    {
        const rl_clone_mesh_lod_desc& lod_desc = m_lod_descs[lod_index];
        const rl_submesh_data& submesh = mesh_data->sub_meshes[lod_desc.submesh_index];
        calculate_instance_params(lod_index);
        // The shader reads the parameters as (fade out scale, fade in offset, fade in scale, fade out offset).
        const float instance_params[4] = {m_instance_parameters.z, m_instance_parameters.y, m_instance_parameters.x, m_instance_parameters.w};
        renderer.set_shader_type(shader_type_standard);
        renderer.set_shader_configuration(1);
        const int num_blocks = submesh.num_render_blocks < MAX_CLONE_RENDER_BLOCKS ? submesh.num_render_blocks : MAX_CLONE_RENDER_BLOCKS;
        for (int block_index = 0; block_index < num_blocks; block_index++)
        {
            const rl_render_block& block = submesh.render_blocks.to_pointer()[block_index];
            const unsigned int material_index = block.material_map_idx;
            if (!m_mat_map->m_isSet || m_mat_map->m_data.num_materials <= material_index)
            {
                keen::debug::nativePrintArgs("rl_clone_mesh_instance::render - material index broken for instance %p!\n", this);
                continue;
            }
            rl_material_library_set_render_material(renderer, m_mat_map->m_data.materials[material_index], 0);
            renderer.set_vertex_format((rl_dev_vertex_format)mesh_data->vertex_buffer.vertex_format);
            if (mesh_data->vertex_buffer.vertex_format != RLVF_CLONE_CLR)
            {
                keen::debug::nativePrintArgs("rl_clone_mesh_instance::render - unsupported clone mesh vertex format (%i) instance %p!\n", mesh_data->vertex_buffer.vertex_format, this);
                continue;
            }
            const keen::RenderGeometry& geometry = mesh_p->getRenderGeometry();
            keen::PrimitiveData primitive;
            primitive.primitiveTopology = geometry.primitiveTopology;
            primitive.vertexDrawCount = block.num_indices;
            if (primitive.vertexDrawCount == 0)
            {
                primitive.vertexDrawCount = geometry.pIndexBuffer == nullptr ? geometry.vertexCount : geometry.indexCount;
            }
            // Magic number: the four geometry streams of the inlined draw command.
            keen::VertexBufferData vertex_buffers[4];
            memset(vertex_buffers, 0, sizeof(vertex_buffers));
            vertex_buffers[0].pBuffer = geometry.pVertexBuffer;
            keen::IndexBufferData index_buffer;
            index_buffer.pBuffer = geometry.pIndexBuffer;
            index_buffer.indexCount = 0;
            index_buffer.offset = mesh_data->index_buffer.index_size * block.start_index;
            index_buffer.indexFormat = geometry.indexFormat;
            const int num_clones = get_num_clones();
            keen::GeometryInstancingBatch batch;
            float* instance = (float*)renderer.begin_instancing_batch(&batch, num_clones, CLONE_INSTANCE_SIZE);
            if (instance == nullptr)
            {
                continue;
            }
            for (int clone_index = 0; clone_index < num_clones; clone_index++, instance += CLONE_INSTANCE_SIZE / sizeof(float))
            {
                rl_clone_mesh_info info;
                get_clone(clone_index, info);
                if ((lod_desc.flags & CLONE_LOD_FLAG_BILLBOARD) != 0)
                {
                    const vector& fvec = info.orient.fvec;
                    const float scale = (float)sqrt((double)(float)(((double)fvec.x * (double)fvec.x + (double)fvec.y * (double)fvec.y) + (double)fvec.z * (double)fvec.z));
                    const vector& camera_pos = camera.get_position();
                    vector to_camera((camera_pos.x - position.x) - info.pos.x, (camera_pos.y - position.y) - info.pos.y, (camera_pos.z - position.z) - info.pos.z);
                    to_camera.normalize();
                    info.orient.make_uvec(to_camera, UP_VECTOR);
                    info.orient.rvec.x *= scale;
                    info.orient.rvec.y *= scale;
                    info.orient.rvec.z *= scale;
                    info.orient.uvec.x *= scale;
                    info.orient.uvec.y *= scale;
                    info.orient.uvec.z *= scale;
                    info.orient.fvec.x *= scale;
                    info.orient.fvec.y *= scale;
                    info.orient.fvec.z *= scale;
                }
                // A 3x4 transform by rows (the axes' x, y and z components with the position), the parameters and
                // the colour as bytes (ARGB in a little endian dword), each rounded half up.
                instance[0] = info.orient.rvec.x;
                instance[1] = info.orient.uvec.x;
                instance[2] = info.orient.fvec.x;
                instance[3] = offset.x + info.pos.x;
                instance[4] = info.orient.rvec.y;
                instance[5] = info.orient.uvec.y;
                instance[6] = info.orient.fvec.y;
                instance[7] = offset.y + info.pos.y;
                instance[8] = info.orient.rvec.z;
                instance[9] = info.orient.uvec.z;
                instance[10] = info.orient.fvec.z;
                instance[11] = offset.z + info.pos.z;
                instance[12] = instance_params[0];
                instance[13] = instance_params[1];
                instance[14] = instance_params[2];
                instance[15] = instance_params[3];
                const unsigned int red = (unsigned char)(int)((double)info.color.red * 255.0 + 0.5);
                const unsigned int green = (unsigned char)(int)((double)info.color.green * 255.0 + 0.5);
                const unsigned int blue = (unsigned char)(int)((double)info.color.blue * 255.0 + 0.5);
                const unsigned int alpha = (unsigned char)(int)((double)info.color.alpha * 255.0 + 0.5);
                ((unsigned int*)instance)[16] = (alpha << 24) | (red << 16) | (green << 8) | blue;
            }
            renderer.end_instancing_batch();
            renderer.flush_states();
            renderer.getCommandWriter()->writeDrawInstancedVertexBuffersCommand(vertex_buffers, 4, &index_buffer, &primitive, &batch);
        }
    }
}

// 0x004BC7D0 (:1043)
// Empty: the wind is not passed on.
void rl_clone_mesh_instance::set_wind_info(const rl_clone_mesh_wind_info& wind_info)
{
}

// ---- header-inline members

// 0x0084BC60 (.h:239)
void rl_clone_mesh_instance::set_fade_range(const rl_clone_mesh_fade_range& fade_range)
{
    m_fade_range = fade_range;
}

// 0x0084BC80 (.h:240)
void rl_clone_mesh_instance::set_lod_descs(const rl_clone_mesh_lod_desc* lod_descs, int num_lods)
{
    m_lod_descs = lod_descs;
    m_num_lods = num_lods;
}

// 0x00548F60 (.h:260)
void rl_clone_mesh_instance::on_visible_for_frame(rl_renderer& renderer)
{
    rl_decal_process_decals(&m_decal_list);
}

// 0x0091BE30 (.h:263)
void rl_clone_mesh_instance::set_wind_reference_height(float height)
{
    m_wind_reference_height = height;
}

// 0x0091BE50 (.h:264)
void rl_clone_mesh_instance::set_wind_animated(bool is_animated)
{
    m_wind_animated = is_animated;
}

// 0x0091BE60 (.h:267)
const rl_clone_mesh_element* rl_clone_mesh_instance::get_clones(int& num_clones)
{
    num_clones = m_num_elements;
    return m_elements;
}

// 0x00548F50 (.h:273)
const rl_material_map* rl_clone_mesh_instance::get_material_map()
{
    return m_mat_map;
}

// 0x004BBA90 (.h:195), 0x004BBAA0 (.h:195), 0x004BBAB0 (.h:234) and 0x004BBAC0 (.h:234) are the factory macro's
// get_object_pool and get_objects_max.

// Not in the binary (lod.md L2 test census).
void rl_clone_mesh_take_census(const rl_clone_mesh_pool* pool, rl_clone_mesh_census& census)
{
    memset(&census, 0, sizeof(census));
    if (pool == nullptr)
    {
        return;
    }
    census.pool_clones_max = pool->m_max_clone_instances;
    census.pool_clones_used = pool->m_max_clone_instances - pool->m_available_clones;
    const rl_clone_mesh_instance* const first = pool->m_instance_list;
    unsigned int latest_frame = 0;
    const rl_clone_mesh_instance* inst = first;
    while (inst != nullptr)
    {
        census.instances++;
        census.clones += inst->m_num_elements;
        latest_frame = inst->m_last_processed_frame > latest_frame ? inst->m_last_processed_frame : latest_frame;
        inst = inst->next != first ? inst->next : nullptr;
    }
    inst = first;
    while (inst != nullptr && latest_frame != 0)
    {
        if (inst->m_last_processed_frame == latest_frame)
        {
            census.drawn_instances++;
            for (int lod = inst->m_min_lod; lod <= inst->m_max_lod; lod++)
            {
                const int slot = lod < 3 ? lod : 3;
                census.drawn_by_lod[slot]++;
                census.clones_by_lod[slot] += inst->m_num_elements;
            }
        }
        inst = inst->next != first ? inst->next : nullptr;
    }
}
