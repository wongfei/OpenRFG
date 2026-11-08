// Original: volition/render_lib/code/rl_destroyable_model_instance.cpp
//
// rl_destroyable_model_instance: the render side of a destructible object. All instances of one object share an
// rl_destroyable_model_instance_shared_base: one array of base sub instances (from the split memory manager) laid out
// as [unclaimed][instance A][instance B]..., each instance owning a contiguous range (m_sub_instances,
// m_num_sub_instances). Splitting moves sub instances to the end of the donor's range and gives that range to a new
// instance; unclaiming / releasing moves a range to the end of the unclaimed region, claiming moves it from there to
// the end of the claimer's range. m_sub_instance_index_map maps an original sub instance index to its position in its
// owner's range, m_sub_instance_original_index_map maps a base array position back to the original index. Each
// instance keeps its active sub instances first (m_num_renderable_sub_instances). The kd tree over the base sub
// instance boxes is shared; its leaves' data is (instance id << 16 | position), unclaimed ones 0xFFFF << 16.
// Mesh recompiles (rl_mesh_recompile.cpp) replace the sub instance drawing with one compiled mesh when enabled.

#include "volition/render_lib/code/rl_destroyable_model_instance.h"

#include "volition/render_lib/code/rl_clone_mesh_instance.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_fp_kdtree.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_mesh_recompile.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector4.h"
#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/rendergeometry.hpp"

#include <malloc.h>

#include <intrin.h>
#include <stdlib.h>
#include <string.h>

// <rl_destroyable_model_instance_shared_base::factory_init> 0x0050CA20 (:33)
// <rl_destroyable_model_instance_shared_base::release> 0x0050CB20 (:33)
// <rl_destroyable_model_instance_shared_base::create> 0x00523E80 (:33)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_destroyable_model_instance_shared_base, 0x207F2A39, 1000)
// <rl_destroyable_model_instance::factory_init> 0x0050CBA0 (:34)
// <rl_destroyable_model_instance::create> 0x00549AF0 (:34)
RL_FACTORY_CLASS_CORE(rl_destroyable_model_instance, 0x7A6C541D, 1000)

// m_flags bit 0: the instance was batchable before its compiled mesh made it unbatched.
const unsigned int rl_destroyable_model_instance::kWasBatchable = 1; // 0x012BEDD4

static split_memmgr* Rl_destroyable_instance_memmgr = nullptr; // 0x01B7BC70
static void (*Rl_destroyable_instance_alloc_stats_cb)(int) = nullptr; // 0x01B7BC74
// The shared base whose original -> base position map is in Orig_to_base_index_cache (cleared whenever positions move).
static rl_destroyable_model_instance_shared_base* Current_cached_base = nullptr; // 0x01B7BC78
// The index map split_base_array_interal sorts by (int16_compare).
static unsigned short* Sub_instance_index_map = nullptr; // 0x01B7BC7C
// Magic number: the cache covers shared bases of up to 4096 sub instances.
static const int ORIG_TO_BASE_CACHE_SIZE = 0x1000;
static unsigned short Orig_to_base_index_cache[ORIG_TO_BASE_CACHE_SIZE]; // 0x01B6CAF8

// Sub instance moves go through scratch memory in chunks of this many.
static const int SUB_INSTANCE_MOVE_CHUNK = 0x1000;
// Unclaimed leaves and the "no leaf / no instance" index.
static const unsigned int UNCLAIMED_LEAF_DATA = 0xFFFF0000;
static const unsigned short INVALID_INDEX = 0xFFFF;

// Bytes per base sub instance from the split memory manager: rl_sub_instance (0x20), kd tree leaf (0x10) and the
// index and original index maps (2 + 2), plus a kd tree node (0x10) and the leaf map (2) when the tree is built at run
// time.
static int shared_base_alloc_size(int num_sub_instances, bool dynamic_kdtree)
{
    return dynamic_kdtree ? num_sub_instances * 0x46 : num_sub_instances * 0x34;
}

// The position of a sub instance in the base array.
static int base_position(const rl_destroyable_model_instance_shared_base* shared_data, const rl_sub_instance* sub_inst)
{
    return (int)(sub_inst - shared_data->m_base_sub_instances);
}

// ---- post_split_info

// 0x004BDDA0 (:76)
bool post_split_info::get_moved()
{
    return (flags & 1) != 0;
}

// 0x004BDDB0 (:77)
bool post_split_info::get_split()
{
    return (flags >> 1 & 1) != 0;
}

// 0x004BDDC0 (:78)
void post_split_info::set_moved(bool moved)
{
    if (moved)
    {
        flags |= 1;
        return;
    }
    flags &= 0xFFFE;
}

// 0x004BDDE0 (:79)
void post_split_info::set_split(bool split)
{
    if (split)
    {
        flags |= 2;
        return;
    }
    flags &= 0xFFFD;
}

// ---- rl_destroyable_model_instance_shared_base

// 0x004BDD30 (.h:226)
rl_destroyable_model_instance_shared_base::rl_destroyable_model_instance_shared_base()
{
    m_sub_instance_index_map = nullptr;
    m_sub_instance_original_index_map = nullptr;
    m_sub_instance_leaf_index_map = nullptr;
    m_base_sub_instances = nullptr;
    m_ref_count = 0;
    m_instance_list = nullptr;
    m_kdtree_nodes = nullptr;
    m_kdtree_leaves = nullptr;
    m_kdtree = nullptr;
    m_total_instances_added = 0;
}

// 0x004E4140 (.h:242)
// Frees the arrays once no instance refers to the base any more.
rl_destroyable_model_instance_shared_base::~rl_destroyable_model_instance_shared_base()
{
    if (m_ref_count != 0)
    {
        return;
    }
    Rl_destroyable_instance_memmgr->free(m_base_sub_instances);
    if (Rl_destroyable_instance_alloc_stats_cb != nullptr)
    {
        Rl_destroyable_instance_alloc_stats_cb(-shared_base_alloc_size(m_num_base_sub_instances, m_base_kdtree == nullptr));
    }
    if (m_kdtree != nullptr)
    {
        m_kdtree->release();
    }
    m_base_sub_instances = nullptr;
    m_sub_instance_index_map = nullptr;
    m_sub_instance_original_index_map = nullptr;
    m_sub_instance_leaf_index_map = nullptr;
    m_kdtree_nodes = nullptr;
    m_kdtree_leaves = nullptr;
    m_num_base_sub_instances = 0;
    m_kdtree = nullptr;
    m_base_kdtree = nullptr;
    if (Current_cached_base == this)
    {
        Current_cached_base = nullptr;
    }
}

// 0x007C0B10 (.h:52)
void rl_destroyable_model_instance_shared_base::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x004BDE00 (:90)
void rl_destroyable_model_instance_shared_base::increment_ref_count()
{
    m_ref_count++;
}

// 0x004BDE10 (:96)
void rl_destroyable_model_instance_shared_base::decrement_ref_count()
{
    m_ref_count--;
    if (m_ref_count < 1)
    {
        release();
    }
}

// 0x004BDE50 (:449)
// Appends the instance to the ring; returns its id (the running count of added instances).
unsigned int rl_destroyable_model_instance_shared_base::add_instance(rl_destroyable_model_instance* new_inst)
{
    if (new_inst == nullptr)
    {
        return INVALID_INDEX;
    }
    if (m_instance_list == nullptr)
    {
        new_inst->next = new_inst;
        new_inst->prev = new_inst;
        m_instance_list = new_inst;
    }
    else
    {
        new_inst->prev = m_instance_list->prev;
        new_inst->next = m_instance_list;
        m_instance_list->prev->next = new_inst;
        m_instance_list->prev = new_inst;
    }
    increment_ref_count();
    m_total_instances_added++;
    return m_total_instances_added;
}

// 0x004BDED0 (:463)
// Unlinks the instance; the base releases itself with its last instance.
void rl_destroyable_model_instance_shared_base::remove_instance(rl_destroyable_model_instance* inst)
{
    if (inst == nullptr)
    {
        return;
    }
    if (m_instance_list == inst && inst->next == inst)
    {
        inst->next = nullptr;
        inst->prev = nullptr;
        m_instance_list = nullptr;
    }
    else
    {
        if (m_instance_list == inst)
        {
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
    decrement_ref_count();
}

// 0x0050CCA0 (:475)
// One block from the split memory manager: base sub instances, kd tree leaves, (run time kd tree nodes,) index map,
// original index map (, run time leaf map). A prebuilt tree (base_kdtree) provides its leaves (copied), nodes and leaf
// map.
bool rl_destroyable_model_instance_shared_base::init(rl_fp_kdtree_base_data* base_kdtree, int num_sub_instances)
{
    const int size = shared_base_alloc_size(num_sub_instances, base_kdtree == nullptr);
    m_num_base_sub_instances = num_sub_instances;
    m_ref_count = 0;
    m_base_kdtree = nullptr;
    unsigned char* block = (unsigned char*)Rl_destroyable_instance_memmgr->alloc(size, "rl_destroyable_model_instance_shared_base::init", 0x10);
    if (Rl_destroyable_instance_alloc_stats_cb != nullptr)
    {
        Rl_destroyable_instance_alloc_stats_cb(size);
    }
    m_base_sub_instances = (rl_sub_instance*)block;
    m_kdtree_leaves = (rl_fp_kdtree_leaf*)(block + num_sub_instances * sizeof(rl_sub_instance));
    unsigned char* next = (unsigned char*)(m_kdtree_leaves + num_sub_instances);
    if (base_kdtree == nullptr)
    {
        m_kdtree_nodes = (rl_fp_kdtree_node*)next;
        next = (unsigned char*)(m_kdtree_nodes + num_sub_instances);
    }
    else
    {
        memcpy(m_kdtree_leaves, base_kdtree->leaves.to_pointer(), num_sub_instances * sizeof(rl_fp_kdtree_leaf));
        m_kdtree_nodes = base_kdtree->nodes.to_pointer();
    }
    m_sub_instance_index_map = (unsigned short*)next;
    unsigned short* original_index_map = m_sub_instance_index_map + num_sub_instances;
    m_sub_instance_original_index_map = original_index_map;
    if (base_kdtree == nullptr)
    {
        m_sub_instance_leaf_index_map = original_index_map + num_sub_instances;
    }
    else
    {
        m_sub_instance_leaf_index_map = base_kdtree->leaf_map.to_pointer();
    }
    m_base_kdtree = base_kdtree;
    return m_base_sub_instances != nullptr && m_sub_instance_index_map != nullptr && original_index_map != nullptr && m_sub_instance_leaf_index_map != nullptr && m_kdtree_leaves != nullptr && m_kdtree_nodes != nullptr;
}

// 0x0053A090 (:562)
// Without a prebuilt tree: one leaf per base sub instance with a valid sub mesh (12.4 fixed point box, unclaimed),
// the tree built over them and the leaf map filled. With one: the tree over the prebuilt nodes and the copied leaves.
// rl_fp_kdtree::init (both forms) is inlined.
void rl_destroyable_model_instance_shared_base::build_rbb(const rl_mesh_data_runtime* mesh_data)
{
    if (m_kdtree != nullptr)
    {
        m_kdtree->release();
    }
    if (m_base_kdtree != nullptr)
    {
        m_kdtree = rl_fp_kdtree::create();
        m_kdtree->init(m_base_kdtree, m_kdtree_leaves);
        return;
    }
    int num_leaves = 0;
    for (int i = 0; i < m_num_base_sub_instances; i++)
    {
        const rl_sub_instance& sub_inst = m_base_sub_instances[i];
        matrix rotation;
        rotation.set(sub_inst.m_transform.pitch, sub_inst.m_transform.bank, sub_inst.m_transform.heading);
        if (sub_inst.m_submesh_id < (unsigned int)mesh_data->num_sub_meshes)
        {
            const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_inst.m_submesh_id];
            vector box_min;
            vector box_max;
            bbox_instance(box_min, box_max, rotation, sub_inst.m_transform.pos, submesh.bmin, submesh.bmax);
            rl_fp_kdtree_leaf& leaf = m_kdtree_leaves[num_leaves];
            leaf.data = (unsigned int)i | UNCLAIMED_LEAF_DATA;
            leaf.aabb.set_min(box_min);
            leaf.aabb.set_max(box_max);
            num_leaves++;
        }
    }
    m_kdtree = rl_fp_kdtree::create();
    m_kdtree->init(m_kdtree_nodes, m_num_base_sub_instances, m_kdtree_leaves, num_leaves);
    memset(m_sub_instance_leaf_index_map, 0xFF, m_num_base_sub_instances * sizeof(unsigned short));
    for (int i = 0; i < num_leaves; i++)
    {
        m_sub_instance_leaf_index_map[m_kdtree_leaves[i].data & 0xFFFF] = (unsigned short)i;
    }
}

// 0x004BDF50 (:615)
// The unclaimed leaves point at their position in the unclaimed region.
void rl_destroyable_model_instance_shared_base::update_unclaimed_rbb_inst_ids()
{
    for (unsigned int i = 0; i < (unsigned int)m_num_unclaimed; i++)
    {
        const unsigned short leaf_index = m_sub_instance_leaf_index_map[m_sub_instance_original_index_map[i]];
        if (leaf_index != INVALID_INDEX)
        {
            m_kdtree_leaves[leaf_index].data = i | UNCLAIMED_LEAF_DATA;
        }
    }
}

// 0x004BE0E0 (:1304)
// The base array position of an original sub instance index. Bases of up to 4096 sub instances fill
// Orig_to_base_index_cache once (until positions move); bigger ones are searched linearly.
int rl_destroyable_model_instance_shared_base::find_base_sub_instance_index(int orig_sub_inst_index)
{
    if (orig_sub_inst_index < 0 || orig_sub_inst_index >= m_num_base_sub_instances)
    {
        return -1;
    }
    if (Current_cached_base != this)
    {
        const bool cacheable = m_num_base_sub_instances <= ORIG_TO_BASE_CACHE_SIZE;
        if (cacheable)
        {
            Current_cached_base = nullptr;
            memset(Orig_to_base_index_cache, 0xFF, sizeof(Orig_to_base_index_cache));
        }
        for (int i = 0; i < m_num_base_sub_instances; i++)
        {
            if (cacheable)
            {
                if ((int)m_sub_instance_original_index_map[i] < m_num_base_sub_instances)
                {
                    Orig_to_base_index_cache[m_sub_instance_original_index_map[i]] = (unsigned short)i;
                }
            }
            else if (m_sub_instance_original_index_map[i] == (unsigned short)orig_sub_inst_index)
            {
                return i;
            }
        }
        if (!cacheable)
        {
            return -1;
        }
    }
    Current_cached_base = this;
    if (Orig_to_base_index_cache[orig_sub_inst_index] == INVALID_INDEX)
    {
        return -1;
    }
    return Orig_to_base_index_cache[orig_sub_inst_index];
}

// 0x004BE1A0 (:1340)
void rl_destroyable_model_instance_shared_base::maybe_reset_validation_cache()
{
    if (Current_cached_base == this)
    {
        Current_cached_base = nullptr;
    }
}

// ---- split_base_array_interal

// 0x004BDE30 (:282)
// Sorts original sub instance indices by their current position.
int int16_compare(const void* arg1, const void* arg2)
{
    return (int)Sub_instance_index_map[*(const short*)arg1] - (int)Sub_instance_index_map[*(const short*)arg2];
}

// 0x004E46A0 (:302)
// Moves the sub instances indices_to_split to the end of the old array (swapping each with the last remaining
// non-split one) and makes that tail the new array; the index maps follow. Decals of the old list follow their sub
// instance: moved ones get its new position, split ones move to the new list.
// Notes (decompiled behaviour, kept as is): parent_inst_id is unused.
void split_base_array_interal(split_base_array_params& new_params, split_base_array_params& old_params, rl_destroyable_model_instance_shared_base* shared_data, short* indices_to_split, int num_indices, unsigned int parent_inst_id)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    Sub_instance_index_map = shared_data->m_sub_instance_index_map;
    unsigned short* original_index_map = shared_data->m_sub_instance_original_index_map;
    rl_sub_instance* old_array = *old_params.sub_insts_array;
    const int base_offset = base_position(shared_data, old_array);
    qsort(indices_to_split, num_indices, sizeof(short), int16_compare);
    const int old_size = *old_params.array_size;
    post_split_info* remap = (post_split_info*)pool->alloc(old_size * sizeof(post_split_info), 0);
    memset(remap, 0, old_size * sizeof(post_split_info));
    *old_params.array_size -= num_indices;
    const int new_start = *old_params.array_size;
    int tail = old_size;
    int top = num_indices - 1;
    int i = 0;
    for (; i < num_indices; i++)
    {
        // The highest tail slot that does not already hold one of the (sorted) indices to split.
        for (;;)
        {
            tail--;
            if (top < 0 || tail < 0 || Sub_instance_index_map[indices_to_split[top]] != (unsigned int)tail)
            {
                break;
            }
            top--;
        }
        const unsigned short position = Sub_instance_index_map[indices_to_split[i]];
        if (tail <= (int)position)
        {
            // The rest is already in the tail.
            for (; i < num_indices; i++)
            {
                const unsigned short rest_position = Sub_instance_index_map[indices_to_split[i]];
                remap[rest_position].flags |= 3;
                remap[rest_position].new_index = rest_position - (unsigned short)new_start;
            }
            break;
        }
        remap[tail].flags |= 1;
        remap[tail].new_index = position;
        remap[position].flags |= 3;
        remap[position].new_index = (unsigned short)tail - (unsigned short)new_start;
        Sub_instance_index_map[original_index_map[base_offset + tail]] = position;
        const rl_sub_instance swap = old_array[position];
        old_array[position] = old_array[tail];
        old_array[tail] = swap;
        const unsigned short swap_index = original_index_map[base_offset + position];
        original_index_map[base_offset + position] = original_index_map[base_offset + tail];
        original_index_map[base_offset + tail] = swap_index;
    }
    *new_params.sub_insts_array = old_array + new_start;
    *new_params.array_size = num_indices;
    for (int n = 0; n < num_indices; n++)
    {
        Sub_instance_index_map[original_index_map[base_offset + new_start + n]] = (unsigned short)n;
    }
    if (old_params.decal_list != nullptr && new_params.decal_list != nullptr)
    {
        rl_decal* decal = *old_params.decal_list;
        while (decal != nullptr)
        {
            rl_decal* next = decal->next != *old_params.decal_list ? decal->next : nullptr;
            const unsigned int handle = decal->get_generic_ri_handle();
            if ((remap[handle].flags & 1) != 0)
            {
                decal->set_generic_ri_handle(remap[handle].new_index);
            }
            if ((remap[handle].flags >> 1 & 1) != 0)
            {
                if (*old_params.decal_list == decal && decal->next == decal)
                {
                    decal->next = nullptr;
                    decal->prev = nullptr;
                    *old_params.decal_list = nullptr;
                }
                else
                {
                    if (*old_params.decal_list == decal)
                    {
                        *old_params.decal_list = decal->next;
                    }
                    if (decal->prev != nullptr)
                    {
                        decal->prev->next = decal->next;
                    }
                    if (decal->next != nullptr)
                    {
                        decal->next->prev = decal->prev;
                    }
                    decal->next = nullptr;
                    decal->prev = nullptr;
                }
                rl_decal*& new_list = *new_params.decal_list;
                if (new_list == nullptr)
                {
                    decal->next = decal;
                    decal->prev = decal;
                    new_list = decal;
                }
                else
                {
                    decal->prev = new_list->prev;
                    decal->next = new_list;
                    new_list->prev->next = decal;
                    new_list->prev = decal;
                }
            }
            decal = next;
        }
    }
    Sub_instance_index_map = nullptr;
}

// ---- rl_destroyable_model_instance

// 0x0053A010 (:34)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_destroyable_model_instance::release()
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
    this->~rl_destroyable_model_instance();
}

// 0x007C0B20 (.h:113)
void rl_destroyable_model_instance::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x00723860 (.h:113)
int rl_destroyable_model_instance::get_objects_used()
{
    return ms_class_factory_info.m_objects_max - ms_object_pool.size();
}

// 0x0071B5D0 (.h:113)
int rl_destroyable_model_instance::get_objects_free()
{
    return ms_object_pool.size();
}

// 0x005499A0 (.h:321)
rl_destroyable_model_instance::rl_destroyable_model_instance()
{
    m_shared_data = nullptr;
    prev = nullptr;
    next = nullptr;
    m_recompile_data_p = nullptr;
    m_recompile_parameters_p = nullptr;
    m_flags = 0;
}

// 0x00549A20 (.h:336)
// Invalidates a pending recompile (with the recompile thread paused), frees the compiled data and gives the sub
// instances back to the unclaimed region. The compound destructor then finds no arrays of its own.
rl_destroyable_model_instance::~rl_destroyable_model_instance()
{
    if (m_recompile_parameters_p != nullptr)
    {
        rl_mesh_recompile_pause();
        _InterlockedExchange((volatile long*)&m_recompile_parameters_p->m_valid, 0);
        rl_mesh_recompile_unpause();
        m_recompile_parameters_p = nullptr;
    }
    rl_delete_mesh_recompile_data(m_recompile_data_p);
    if (m_shared_data != nullptr)
    {
        release_sub_instances_internal(m_sub_instances, m_num_sub_instances);
        rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
        m_sub_instances = nullptr;
        m_num_sub_instances = 0;
        m_num_renderable_sub_instances = 0;
        shared_data->remove_instance(this);
    }
    m_shared_data = nullptr;
    m_sub_instances = nullptr;
    m_kdtree_nodes = nullptr;
    m_kdtree_leaves = nullptr;
    m_kdtree = nullptr;
}

// 0x0053A2E0 (:1443)
// The compiled mesh is dropped (the instance gets back its batchable flag from before the recompile) and a new
// recompile is queued (inlined in every sub instance change).
void rl_destroyable_model_instance::invalidate_compiled_data_cache(bool initing)
{
    rl_mesh_recompile_data* data_p = m_recompile_data_p;
    if (data_p != nullptr)
    {
        set_is_batchable((m_flags & kWasBatchable) != 0);
    }
    rl_delete_mesh_recompile_data(data_p);
    m_recompile_data_p = nullptr;
    rl_start_recompile_mesh_instance(this, initing);
}

// 0x004E41F0 (:108)
// Moves sub_insts (the end of this instance's range, or all of it) to the end of the unclaimed region: the ranges in
// between shift up (in chunks of 4096 through scratch memory) and the instances before this one follow.
// Notes (decompiled behaviour, kept as is): the leaf update tests the leaf map at the new position before looking up
// the leaf of the original index.
void rl_destroyable_model_instance::release_sub_instances_internal(rl_sub_instance* sub_insts, int num_sub_instances)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
    if (sub_insts != shared_data->m_base_sub_instances + shared_data->m_num_unclaimed)
    {
        int remaining = num_sub_instances;
        for (int done = 0; done < num_sub_instances; done += SUB_INSTANCE_MOVE_CHUNK)
        {
            mempool_mark_scope chunk_scope(pool);
            const int count = remaining > SUB_INSTANCE_MOVE_CHUNK ? SUB_INSTANCE_MOVE_CHUNK : remaining;
            void* temp = pool->alloc(sizeof(rl_sub_instance) * count, 0);
            unsigned short* original_indices = shared_data->m_sub_instance_original_index_map + base_position(shared_data, sub_insts);
            memcpy(temp, sub_insts, count * sizeof(rl_sub_instance));
            rl_sub_instance* dest = shared_data->m_base_sub_instances + shared_data->m_num_unclaimed + done;
            memmove(dest + count, dest, ((unsigned char*)sub_insts - (unsigned char*)dest) & ~(sizeof(rl_sub_instance) - 1));
            memcpy(dest, temp, count * sizeof(rl_sub_instance));
            memcpy(temp, original_indices, count * sizeof(unsigned short));
            unsigned short* dest_indices = shared_data->m_sub_instance_original_index_map + shared_data->m_num_unclaimed + done;
            memmove(dest_indices + count, dest_indices, (original_indices - dest_indices) * sizeof(unsigned short));
            memcpy(dest_indices, temp, count * sizeof(unsigned short));
            sub_insts += SUB_INSTANCE_MOVE_CHUNK;
            remaining -= SUB_INSTANCE_MOVE_CHUNK;
        }
    }
    for (int i = shared_data->m_num_unclaimed; i < shared_data->m_num_unclaimed + num_sub_instances; i++)
    {
        const unsigned short original_index = shared_data->m_sub_instance_original_index_map[i];
        shared_data->m_sub_instance_index_map[original_index] = (unsigned short)i;
        if (shared_data->m_sub_instance_leaf_index_map[i] != INVALID_INDEX)
        {
            const unsigned short leaf_index = shared_data->m_sub_instance_leaf_index_map[original_index];
            if (leaf_index != INVALID_INDEX && (int)leaf_index < shared_data->m_num_base_sub_instances)
            {
                shared_data->m_kdtree_leaves[leaf_index].data = (unsigned int)i | UNCLAIMED_LEAF_DATA;
            }
        }
    }
    shared_data->m_num_unclaimed += num_sub_instances;
    m_num_sub_instances -= num_sub_instances;
    m_sub_instances += num_sub_instances;
    rl_destroyable_model_instance* inst = shared_data->m_instance_list;
    while (inst != nullptr)
    {
        if (inst->m_sub_instances < m_sub_instances)
        {
            inst->m_sub_instances += num_sub_instances;
        }
        inst = inst->next;
        if (inst == m_shared_data->m_instance_list)
        {
            break;
        }
    }
    shared_data->maybe_reset_validation_cache();
}

// 0x004E44A0 (:202)
// Moves sub_insts (the end of the unclaimed region) to the end of this instance's range: the ranges in between shift
// down and the instances before this one follow.
void rl_destroyable_model_instance::aquire_sub_instances_internal(rl_sub_instance* sub_insts, int num_sub_instances)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
    void* temp = pool->alloc(sizeof(rl_sub_instance) * num_sub_instances, 0);
    unsigned short* original_index_map = shared_data->m_sub_instance_original_index_map;
    const int source_position = base_position(shared_data, sub_insts);
    // The sub instances from the end of the unclaimed region through the end of this instance's range.
    const int num_between = base_position(shared_data, m_sub_instances) - shared_data->m_num_unclaimed + m_num_sub_instances;
    memcpy(temp, sub_insts, num_sub_instances * sizeof(rl_sub_instance));
    rl_sub_instance* unclaimed_end = shared_data->m_base_sub_instances + shared_data->m_num_unclaimed;
    memmove(unclaimed_end - num_sub_instances, unclaimed_end, num_between * sizeof(rl_sub_instance));
    memcpy(unclaimed_end - num_sub_instances + num_between, temp, num_sub_instances * sizeof(rl_sub_instance));
    memcpy(temp, original_index_map + source_position, num_sub_instances * sizeof(unsigned short));
    unsigned short* unclaimed_end_indices = shared_data->m_sub_instance_original_index_map + shared_data->m_num_unclaimed;
    memmove(unclaimed_end_indices - num_sub_instances, unclaimed_end_indices, num_between * sizeof(unsigned short));
    memcpy(unclaimed_end_indices - num_sub_instances + num_between, temp, num_sub_instances * sizeof(unsigned short));
    shared_data->m_num_unclaimed -= num_sub_instances;
    m_num_sub_instances += num_sub_instances;
    rl_destroyable_model_instance* inst = shared_data->m_instance_list;
    while (inst != nullptr)
    {
        if (inst->m_sub_instances < m_sub_instances)
        {
            inst->m_sub_instances -= num_sub_instances;
        }
        inst = inst->next;
        if (inst == m_shared_data->m_instance_list)
        {
            break;
        }
    }
    m_sub_instances -= num_sub_instances;
    const int offset = base_position(m_shared_data, m_sub_instances);
    for (int i = m_num_sub_instances - num_sub_instances; i < m_num_sub_instances; i++)
    {
        m_shared_data->m_sub_instance_index_map[m_shared_data->m_sub_instance_original_index_map[offset + i]] = (unsigned short)i;
    }
    m_shared_data->maybe_reset_validation_cache();
}

// 0x00549B50 (:639)
// A new shared base from the sub instance infos (all initially unclaimed by index; positions = original indices),
// its kd tree, and this instance owning all of them (sub_inst_indices null) or only sub_inst_indices. The instance
// sits at the origin; then the renderable partition, leaf ids and a first recompile.
bool rl_destroyable_model_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_fp_kdtree_base_data* base_kdtree, rl_sub_instance_info* sub_inst_info_p, int num_sub_inst_infos, short* sub_inst_indices, int num_sub_insts)
{
    if (m_shared_data != nullptr)
    {
        m_shared_data->remove_instance(this);
    }
    m_shared_data = rl_destroyable_model_instance_shared_base::create();
    if (m_shared_data == nullptr)
    {
        return false;
    }
    if (!m_shared_data->init(base_kdtree, num_sub_inst_infos))
    {
        m_shared_data->release();
        m_shared_data = nullptr;
        return false;
    }
    m_inst_id = m_shared_data->add_instance(this);
    for (int i = 0; i < num_sub_inst_infos; i++)
    {
        const rl_sub_instance_info& info = sub_inst_info_p[i];
        rl_sub_instance& sub_inst = m_shared_data->m_base_sub_instances[i];
        sub_inst.m_transform.set(info.m_transform);
        sub_inst.m_instance_index = INVALID_INDEX;
        sub_inst.m_is_active = info.m_is_active;
        sub_inst.m_is_visible = info.m_is_active;
        sub_inst.m_submesh_id = (unsigned short)info.m_submesh_id;
        sub_inst.m_disable_deferred = info.m_disable_deferred;
        m_shared_data->m_sub_instance_index_map[i] = (unsigned short)i;
        m_shared_data->m_sub_instance_original_index_map[i] = (unsigned short)i;
    }
    m_shared_data->build_rbb(mesh_p->get_mesh_data());
    rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
    if (sub_inst_indices == nullptr)
    {
        m_sub_instances = shared_data->m_base_sub_instances;
        m_num_sub_instances = num_sub_inst_infos;
        m_num_renderable_sub_instances = num_sub_inst_infos;
        m_kdtree_nodes = shared_data->m_kdtree_nodes;
        m_kdtree_leaves = shared_data->m_kdtree_leaves;
        shared_data->m_num_unclaimed = 0;
    }
    else
    {
        shared_data->m_num_unclaimed = shared_data->m_num_base_sub_instances;
        split_base_array_params new_params = { &m_num_sub_instances, &m_sub_instances, nullptr };
        split_base_array_params old_params = { &shared_data->m_num_unclaimed, &shared_data->m_base_sub_instances, nullptr };
        split_base_array_interal(new_params, old_params, shared_data, sub_inst_indices, num_sub_insts, INVALID_INDEX);
        m_shared_data->update_unclaimed_rbb_inst_ids();
        m_shared_data->maybe_reset_validation_cache();
    }
    m_renderable_p = (rl_renderable*)mesh_p;
    m_mat_map = mat_map_p;
    rl_compound_mesh_instance::set_position(ZERO_VECTOR);
    rl_compound_mesh_instance::set_rotation(IDENTITY_MATRIX);
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    update_partition_system();
    invalidate_compiled_data_cache(true);
    return true;
}

// 0x00549E50 (:735)
// Shares the sibling's base and owns the unclaimed sub instances (all of them, or only sub_inst_indices), at the
// origin; then the renderable partition, leaf ids and a recompile.
void rl_destroyable_model_instance::init(rl_destroyable_model_instance* sibling, short* sub_inst_indices, int num_sub_insts)
{
    if (m_shared_data != nullptr)
    {
        m_shared_data->remove_instance(this);
    }
    m_shared_data = sibling->m_shared_data;
    m_inst_id = m_shared_data->add_instance(this);
    rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
    if (sub_inst_indices == nullptr)
    {
        m_sub_instances = shared_data->m_base_sub_instances;
        m_kdtree_nodes = shared_data->m_kdtree_nodes;
        m_kdtree_leaves = shared_data->m_kdtree_leaves;
        m_num_sub_instances = shared_data->m_num_unclaimed;
        m_num_renderable_sub_instances = shared_data->m_num_unclaimed;
        shared_data->m_num_unclaimed = 0;
    }
    else
    {
        split_base_array_params new_params = { &m_num_sub_instances, &m_sub_instances, nullptr };
        split_base_array_params old_params = { &shared_data->m_num_unclaimed, &shared_data->m_base_sub_instances, nullptr };
        split_base_array_interal(new_params, old_params, shared_data, sub_inst_indices, num_sub_insts, INVALID_INDEX);
        m_shared_data->update_unclaimed_rbb_inst_ids();
        m_shared_data->maybe_reset_validation_cache();
    }
    m_renderable_p = sibling->get_renderable();
    m_mat_map = sibling->get_material_map();
    rl_compound_mesh_instance::set_position(ZERO_VECTOR);
    rl_compound_mesh_instance::set_rotation(IDENTITY_MATRIX);
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    update_partition_system();
    invalidate_compiled_data_cache(true);
}

// 0x0054A090 (:801)
// The split-off instance: shares the base and owns sub_insts (already in place) and the decal list.
void rl_destroyable_model_instance::init(rl_renderable* mesh_p, const rl_material_map* mat_map_p, rl_destroyable_model_instance_shared_base* shared_base, rl_sub_instance* sub_insts, int num_sub_instances, rl_decal* decal_list)
{
    if (m_shared_data != nullptr)
    {
        m_shared_data->remove_instance(this);
    }
    m_shared_data = shared_base;
    m_inst_id = shared_base->add_instance(this);
    m_sub_instances = sub_insts;
    m_kdtree_nodes = m_shared_data->m_kdtree_nodes;
    m_num_sub_instances = num_sub_instances;
    m_num_renderable_sub_instances = num_sub_instances;
    m_renderable_p = mesh_p;
    m_kdtree_leaves = m_shared_data->m_kdtree_leaves;
    m_mat_map = mat_map_p;
    m_decal_list = decal_list;
    rl_decal* decal = decal_list;
    while (decal != nullptr)
    {
        decal->set_owner(this);
        decal = decal->next;
        if (decal == m_decal_list)
        {
            break;
        }
    }
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    invalidate_compiled_data_cache(true);
}

// 0x0054A200 (:851)
// New transforms and active flags (original indices); a changed active flag re-partitions.
// Notes (decompiled behaviour, kept as is): m_disable_deferred is not updated.
void rl_destroyable_model_instance::update_instances(rl_sub_instance_update* updates_p, int num_updates)
{
    bool active_changed = false;
    for (int i = 0; i < num_updates; i++)
    {
        const rl_sub_instance_update& update = updates_p[i];
        rl_sub_instance& sub_inst = m_sub_instances[m_shared_data->m_sub_instance_index_map[update.m_base_index]];
        update.m_info.m_transform.m_rotation.extract_angles(sub_inst.m_transform.pitch, sub_inst.m_transform.bank, sub_inst.m_transform.heading);
        sub_inst.m_transform.pos = update.m_info.m_transform.m_translation;
        if (sub_inst.m_is_active != update.m_info.m_is_active)
        {
            active_changed = true;
        }
        sub_inst.m_is_active = update.m_info.m_is_active;
        sub_inst.m_is_visible = update.m_info.m_is_active;
    }
    if (active_changed)
    {
        partition_renderable_sub_instances();
        update_rbb_inst_ids();
    }
    invalidate_compiled_data_cache(false);
}

// 0x004BDFA0 (:885)
void rl_destroyable_model_instance::get_sub_instance_info(rl_sub_instance_info& sub_info, int base_index) const
{
    const rl_sub_instance& sub_inst = m_sub_instances[m_shared_data->m_sub_instance_index_map[base_index]];
    sub_info.m_transform.m_rotation.set(sub_inst.m_transform.pitch, sub_inst.m_transform.bank, sub_inst.m_transform.heading);
    sub_info.m_transform.m_translation = sub_inst.m_transform.pos;
    sub_info.m_is_active = sub_inst.m_is_visible;
    sub_info.m_submesh_id = sub_inst.m_submesh_id;
    sub_info.m_disable_deferred = sub_inst.m_disable_deferred;
}

// 0x0054A300 (:905)
// Sets the active (and visible) flag of the given original indices; re-partitions unless skip_renderable_update.
void rl_destroyable_model_instance::set_sub_instance_is_active(short* sub_inst_indices, int num_sub_insts, bool is_active, bool skip_renderable_update)
{
    for (int i = 0; i < num_sub_insts; i++)
    {
        const short index = sub_inst_indices[i];
        if (index >= 0 && index < m_shared_data->m_num_base_sub_instances)
        {
            rl_sub_instance& sub_inst = m_sub_instances[m_shared_data->m_sub_instance_index_map[index]];
            sub_inst.m_is_active = is_active;
            sub_inst.m_is_visible = is_active;
        }
    }
    if (!skip_renderable_update)
    {
        partition_renderable_sub_instances();
        update_rbb_inst_ids();
        m_shared_data->maybe_reset_validation_cache();
    }
    invalidate_compiled_data_cache(false);
}

// 0x0054A3E0 (:938)
// Gives the sub instances back to the unclaimed region; their decals are released.
void rl_destroyable_model_instance::unclaim_sub_instances(short* sub_inst_indices, int num_sub_instances)
{
    if (num_sub_instances == 0)
    {
        return;
    }
    int num_split = 0;
    rl_sub_instance* split_sub_insts = nullptr;
    rl_decal* split_decals = nullptr;
    split_base_array_params new_params = { &num_split, &split_sub_insts, &split_decals };
    split_base_array_params old_params = { &m_num_sub_instances, &m_sub_instances, &m_decal_list };
    split_base_array_interal(new_params, old_params, m_shared_data, sub_inst_indices, num_sub_instances, m_inst_id);
    rl_decal* decal = split_decals;
    while (decal != nullptr)
    {
        rl_decal* next_decal = decal->next;
        const bool more = next_decal != split_decals;
        if (split_decals == decal && next_decal == decal)
        {
            decal->next = nullptr;
            decal->prev = nullptr;
            split_decals = nullptr;
        }
        else
        {
            if (split_decals == decal)
            {
                split_decals = next_decal;
            }
            if (decal->prev != nullptr)
            {
                decal->prev->next = decal->next;
            }
            if (decal->next != nullptr)
            {
                decal->next->prev = decal->prev;
            }
            decal->next = nullptr;
            decal->prev = nullptr;
        }
        decal->set_owner(nullptr);
        rl_decal_unregister(decal);
        decal->release();
        decal = more ? next_decal : nullptr;
    }
    // The split range is still this instance's (at its end) until it is released.
    m_num_sub_instances += num_split;
    release_sub_instances_internal(split_sub_insts, num_split);
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    invalidate_compiled_data_cache(false);
}

// 0x0054A540 (:990)
// Takes the sub instances from the unclaimed region.
void rl_destroyable_model_instance::claim_sub_instances(short* sub_inst_indices, int num_sub_instances)
{
    if (num_sub_instances == 0)
    {
        return;
    }
    int num_split = 0;
    rl_sub_instance* split_sub_insts = nullptr;
    rl_destroyable_model_instance_shared_base* shared_data = m_shared_data;
    split_base_array_params new_params = { &num_split, &split_sub_insts, nullptr };
    split_base_array_params old_params = { &shared_data->m_num_unclaimed, &shared_data->m_base_sub_instances, nullptr };
    split_base_array_interal(new_params, old_params, shared_data, sub_inst_indices, num_sub_instances, INVALID_INDEX);
    // The split range is still unclaimed (at its end) until it is acquired.
    m_shared_data->m_num_unclaimed += num_split;
    aquire_sub_instances_internal(split_sub_insts, num_split);
    m_shared_data->update_unclaimed_rbb_inst_ids();
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    invalidate_compiled_data_cache(false);
}

// 0x0054A630 (:1039)
// A new instance with this one's transform: the enable / disable lists are applied, then the split sub instances
// (and their decals) move to the end of this range and become the new instance's.
rl_destroyable_model_instance* rl_destroyable_model_instance::split(const rl_split_info& split_info)
{
    rl_destroyable_model_instance* new_inst = create();
    if (new_inst == nullptr)
    {
        return nullptr;
    }
    new_inst->rl_compound_mesh_instance::set_position(get_position());
    new_inst->rl_compound_mesh_instance::set_rotation(get_rotation());
    set_sub_instance_is_active(split_info.m_sub_insts_to_enable, split_info.m_num_sub_insts_to_enable, true, true);
    set_sub_instance_is_active(split_info.m_sub_insts_to_disable, split_info.m_num_sub_insts_to_disable, false, true);
    int num_split = 0;
    rl_sub_instance* split_sub_insts = nullptr;
    rl_decal* split_decals = nullptr;
    split_base_array_params new_params = { &num_split, &split_sub_insts, &split_decals };
    split_base_array_params old_params = { &m_num_sub_instances, &m_sub_instances, &m_decal_list };
    split_base_array_interal(new_params, old_params, m_shared_data, split_info.m_sub_insts_to_split, split_info.m_num_sub_insts_to_split, m_inst_id);
    // The split decals go to the new instance (EDX at 0x0054A791; the Ghidra decompile shows a null list here).
    new_inst->init(m_renderable_p, m_mat_map, m_shared_data, split_sub_insts, num_split, split_decals);
    new_inst->copy_parameters(this);
    partition_renderable_sub_instances();
    update_rbb_inst_ids();
    update_partition_system();
    m_shared_data->maybe_reset_validation_cache();
    invalidate_compiled_data_cache(false);
    return new_inst;
}

// 0x004BE020 (:1097)
// This instance's leaves point at their position in its range.
void rl_destroyable_model_instance::update_rbb_inst_ids()
{
    const unsigned short* original_indices = m_shared_data->m_sub_instance_original_index_map + base_position(m_shared_data, m_sub_instances);
    for (unsigned int i = 0; i < (unsigned int)m_num_sub_instances; i++)
    {
        const unsigned short leaf_index = m_shared_data->m_sub_instance_leaf_index_map[original_indices[i]];
        if (leaf_index != INVALID_INDEX)
        {
            m_shared_data->m_kdtree_leaves[leaf_index].data = m_inst_id << 16 | (i & 0xFFFF);
        }
    }
}

// 0x004E49D0 (:1116)
// Moves the active sub instances to the front of the range (two cursors swapping; the maps and the decals' sub
// instance handles follow) and counts them; the count is also the batching complexity.
void rl_destroyable_model_instance::partition_renderable_sub_instances()
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    int count = 0;
    if (m_num_sub_instances == 0)
    {
        m_num_renderable_sub_instances = 0;
    }
    else
    {
        unsigned short* index_map = m_shared_data->m_sub_instance_index_map;
        unsigned short* original_indices = m_shared_data->m_sub_instance_original_index_map + base_position(m_shared_data, m_sub_instances);
        unsigned short* remap = (unsigned short*)pool->alloc(m_num_sub_instances * sizeof(unsigned short), 0);
        memset(remap, 0xFF, m_num_sub_instances * sizeof(unsigned short));
        int hi = m_num_sub_instances - 1;
        int lo = 0;
        if (hi > 0)
        {
            for (;;)
            {
                while (m_sub_instances[lo].m_is_active && lo < hi)
                {
                    lo++;
                }
                bool done = false;
                while (!m_sub_instances[hi].m_is_active)
                {
                    if (hi <= lo)
                    {
                        done = true;
                        break;
                    }
                    hi--;
                }
                if (done || hi <= lo)
                {
                    break;
                }
                index_map[original_indices[lo]] = (unsigned short)hi;
                index_map[original_indices[hi]] = (unsigned short)lo;
                const rl_sub_instance swap = m_sub_instances[lo];
                m_sub_instances[lo] = m_sub_instances[hi];
                m_sub_instances[hi] = swap;
                const unsigned short swap_index = original_indices[lo];
                original_indices[lo] = original_indices[hi];
                original_indices[hi] = swap_index;
                remap[hi] = (unsigned short)lo;
                remap[lo] = (unsigned short)hi;
            }
        }
        rl_decal* decal = m_decal_list;
        while (decal != nullptr)
        {
            const unsigned short new_handle = remap[decal->get_generic_ri_handle()];
            if (new_handle != INVALID_INDEX)
            {
                decal->set_generic_ri_handle(new_handle);
            }
            decal = decal->next;
            if (decal == m_decal_list)
            {
                break;
            }
        }
        count = hi;
        if (m_sub_instances[hi].m_is_active)
        {
            count++;
        }
        m_num_renderable_sub_instances = count;
    }
    m_batching_complexity = (short)count;
}

// 0x0050CD90 (:1197)
// The sub instances of this instance whose leaves meet the decal's box (in the instance's frame), filtered by the
// decal's filter callback (on the original index); a single sub instance is always hit.
int rl_destroyable_model_instance::find_decal_sub_insts(int* out_indices, int max_out_indices, const rl_decal_parameters* decal_params)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    if (m_num_sub_instances == 1)
    {
        *out_indices = 0;
        return 1;
    }
    vector local_pos(decal_params->m_pos.x - get_position().x, decal_params->m_pos.y - get_position().y, decal_params->m_pos.z - get_position().z);
    const matrix local_orient = decal_params->m_transform * get_rotation().copy_transpose();
    local_pos.rotate(get_rotation());
    const unsigned short* original_indices = m_shared_data->m_sub_instance_original_index_map + base_position(m_shared_data, m_sub_instances);
    rl_aabb decal_aabb;
    rl_decal_get_local_aabb(decal_aabb, decal_params);
    rl_aabb query_aabb;
    bbox_instance(query_aabb.min, query_aabb.max, local_orient, local_pos, decal_aabb.min, decal_aabb.max);
    rl_fp_kdtree::query_output output;
    output.m_leaves = (rl_fp_kdtree_leaf*)pool->alloc(m_shared_data->m_num_base_sub_instances * sizeof(rl_fp_kdtree_leaf), 0);
    output.m_max_leaves = m_shared_data->m_num_base_sub_instances;
    output.m_num_leaves = 0;
    m_shared_data->m_kdtree->query_aabb(output, query_aabb);
    int count = 0;
    for (int i = 0; i < output.m_num_leaves; i++)
    {
        const unsigned int data = output.m_leaves[i].data;
        const unsigned short position = (unsigned short)(data & 0xFFFF);
        if (count >= max_out_indices)
        {
            break;
        }
        if ((data >> 16) != m_inst_id)
        {
            continue;
        }
        if (decal_params->m_filter != 0xFFFFFFFF && decal_params->m_filter_cb != nullptr && decal_params->m_filter_cb(decal_params->m_filter_data, original_indices[position]) != decal_params->m_filter)
        {
            continue;
        }
        out_indices[count++] = position;
    }
    return count;
}

// 0x0050CFA0 (:1257)
// Starts the fade of the decals on a sub instance (VFX.md FX7): every decal under the handle of each of its decals,
// on any owner (rl_decal_fade_out with no generic instance filter, inlined).
void rl_destroyable_model_instance::fade_decals_on_sub_instance(int sub_instance_index, int fadetime_ms)
{
    const unsigned short position = m_shared_data->m_sub_instance_index_map[sub_instance_index];
    rl_decal* decal = m_decal_list;
    while (decal != nullptr)
    {
        if (decal->get_generic_ri_handle() == position && decal->get_decal_handle() != 0xFFFFFFFF)
        {
            rl_decal_fade_out(decal->get_decal_handle(), fadetime_ms, 0xFFFFFFFF);
        }
        decal = decal->next;
        if (decal == m_decal_list)
        {
            break;
        }
    }
}

// 0x004BE0A0 (:1271)
unsigned int rl_destroyable_model_instance::get_decal_ri_handle(int sub_instance_index)
{
    return m_shared_data->m_sub_instance_index_map[sub_instance_index];
}

// 0x004BE0C0 (:1279)
// 0x1C bytes plus 0x34 per sub-instance, and 0x12 more per sub-instance for a kd tree built at run time.
int rl_destroyable_model_instance::calculate_memory_cost(int num_sub_instances, bool dynamic_kdtree)
{
    int cost = num_sub_instances * 0x34 + 0x1C;
    if (dynamic_kdtree)
    {
        cost += num_sub_instances * 0x12;
    }
    return cost;
}

// 0x004BE1C0 (:1355)
// The original sub instance is unclaimed.
bool rl_destroyable_model_instance::can_claim_sub_instance(int sub_inst_index)
{
    const int position = m_shared_data->find_base_sub_instance_index(sub_inst_index);
    if (position == -1)
    {
        return false;
    }
    return position >= 0 && position < m_shared_data->m_num_unclaimed;
}

// 0x004BE200 (:1365)
// The original sub instance is in this instance's range.
bool rl_destroyable_model_instance::owns_sub_instance(int sub_inst_index)
{
    const unsigned int position = (unsigned int)m_shared_data->find_base_sub_instance_index(sub_inst_index);
    if (position == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int start = (unsigned int)base_position(m_shared_data, m_sub_instances);
    return start <= position && position < m_num_sub_instances + start;
}

// 0x004BE250 (:1377)
unsigned int rl_destroyable_model_instance::get_base_uid()
{
    return (unsigned int)m_shared_data;
}

// 0x0055EF70 (:1383)
// Debug drawing of the sub instance boxes, only with RL_show_kdtree (off in retail); not ported.
void rl_destroyable_model_instance::render_debug()
{
}

// One byte of a packed unit vector back to [-1, 1] (float steps; the 1/255 is a float constant widened).
static float rl_recompile_unpack(unsigned char b)
{
    return (float)b * 0.003921569f * 2.0f - 1.0f;
}

// [-1, 1] to a byte, rounded (float steps, then truncated).
static unsigned char rl_recompile_pack(float v)
{
    return (unsigned char)(int)((v + 1.0f) * 0.5f * 255.0f + 0.5f);
}

// The rotation part of render_transform applied to v (float steps in the order rfg.exe sums them).
static vector rl_recompile_rotate(const matrix43& m, const vector& v)
{
    vector out;
    out.x = (m.m_rotation.rvec.x * v.x + m.m_rotation.uvec.x * v.y) + m.m_rotation.fvec.x * v.z;
    out.y = (m.m_rotation.rvec.y * v.x + m.m_rotation.uvec.y * v.y) + m.m_rotation.fvec.y * v.z;
    out.z = (m.m_rotation.rvec.z * v.x + m.m_rotation.uvec.z * v.y) + m.m_rotation.fvec.z * v.z;
    return out;
}

// 0x004BE260 (:1454)
// The first time an old vertex is used it gets the next new index: copied to the new buffer with its position
// transformed and its normal (and tangent with its w) rotated, renormalized and packed again (the normal's w is 0).
// The arithmetic is float (each double step in rfg.exe rounds to float at once).
static void process_vertex(unsigned int& new_idx, unsigned int& next_vert, const rl_vertex_buffer_data_runtime& vb_data, const unsigned int vert_stride, unsigned int old_idx, unsigned char* new_verts_p, const matrix43& render_transform, const bool has_tangent)
{
    if (new_idx != 0xFFFFFFFF)
    {
        return;
    }
    new_idx = next_vert;
    next_vert++;
    unsigned char* dst = new_verts_p + new_idx * vert_stride;
    const unsigned char* src = vb_data.verts + old_idx * vert_stride;
    memcpy(dst, src, vert_stride);
    const vector& pos = *(const vector*)src;
    const vector rotated = rl_recompile_rotate(render_transform, pos);
    float* dst_pos = (float*)dst;
    dst_pos[0] = render_transform.m_translation.x + rotated.x;
    dst_pos[1] = render_transform.m_translation.y + rotated.y;
    dst_pos[2] = render_transform.m_translation.z + rotated.z;
    vector normal(rl_recompile_unpack(src[0xC]), rl_recompile_unpack(src[0xD]), rl_recompile_unpack(src[0xE]));
    normal = rl_recompile_rotate(render_transform, normal);
    normal.normalize();
    dst[0xC] = rl_recompile_pack(normal.x);
    dst[0xD] = rl_recompile_pack(normal.y);
    dst[0xE] = rl_recompile_pack(normal.z);
    dst[0xF] = 0;
    if (!has_tangent)
    {
        return;
    }
    const float tangent_w = rl_recompile_unpack(src[0x13]);
    vector tangent(rl_recompile_unpack(src[0x10]), rl_recompile_unpack(src[0x11]), rl_recompile_unpack(src[0x12]));
    tangent = rl_recompile_rotate(render_transform, tangent);
    tangent.normalize();
    const vector4 t(tangent, tangent_w);
    dst[0x10] = rl_recompile_pack(t.x);
    dst[0x11] = rl_recompile_pack(t.y);
    dst[0x12] = rl_recompile_pack(t.z);
    dst[0x13] = rl_recompile_pack(t.w);
}

// 0x0050D070 (:1527)
// The recompile thread's work: every visible, opaque sub instance's geometry merged into one vertex and index buffer
// in world space (relative to the instance), one render block per material. Vertices are shared within a sub instance
// only. A pause of the recompile system (Rl_recompile_mesh_pause_count) is waited out at every step; an instance
// invalidated meanwhile (params_p->m_valid 0) gives up. Null when there is nothing to compile.
rl_mesh_recompile_data* rl_destroyable_model_instance::compile_cache_data(rl_mesh_recompile_parameters* params_p)
{
    const rl_material_map* mat_map = m_mat_map;
    if (mat_map == nullptr || !mat_map->m_isSet)
    {
        return nullptr;
    }
    const unsigned int num_materials = mat_map->m_data.num_materials;
    if ((int)num_materials < 1 || m_renderable_p == nullptr)
    {
        return nullptr;
    }
    keen::MemoryAllocator* allocator = Init_params.m_pBufferAllocator;
    rl_mesh* mesh = (rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* data = mesh->m_data_set ? &mesh->m_data : nullptr;
    const rl_dev_vertex_format vertex_format = (rl_dev_vertex_format)data->vertex_buffer.vertex_format;
    if ((unsigned int)(vertex_format - RLVF_PIXLIT_0UV) > 15) // only the 16 pixel-lit formats
    {
        return nullptr;
    }
    const unsigned int vert_stride = data->vertex_buffer.vert_stride_0;
    const unsigned int index_size = data->index_buffer.index_size;
    // original: the per-vertex table is sized by the index count
    const unsigned int table_size = data->index_buffer.num_indices;
    const rl_dev_primitive_topology prim_type = data->index_buffer.prim_type == 1 ? RL_PRIMITIVE_TOPOLOGY_TRIANGLELIST : RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    const bool has_tangent = vertex_format == RLVF_PIXLIT_1UV_NMAP || vertex_format == RLVF_PIXLIT_2UV_NMAP || vertex_format == RLVF_PIXLIT_3UV_NMAP || vertex_format == RLVF_PIXLIT_4UV_NMAP;
    unsigned int* counts = (unsigned int*)_alloca(num_materials * sizeof(unsigned int));
    memset(counts, 0, num_materials * sizeof(unsigned int));
    unsigned int* table = new unsigned int[table_size];
    memset(table, 0, table_size * sizeof(unsigned int));

    // Pass 1: the index count per material and the vertex count (vertices marked in table, per sub instance).
    unsigned int total_verts = 0;
    for (int s = 0; s < m_num_renderable_sub_instances; s++)
    {
        const rl_sub_instance& sub = m_sub_instances[s];
        if (!sub.m_is_visible || (int)sub.m_submesh_id >= data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = ((const rl_submesh_data*)data->sub_meshes)[sub.m_submesh_id];
        const rl_render_block* blocks = (const rl_render_block*)submesh.render_blocks;
        bool transparent = false;
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (Rl_recompile_mesh_pause_count != 0)
            {
                rl_mesh_recompile_block(); // inlined
                if (params_p->m_valid == 0)
                {
                    delete[] table;
                    return nullptr;
                }
            }
            if (rl_material_library_material_is_transparent(mat_map->m_data.materials[blocks[b].material_map_idx]))
            {
                transparent = true;
                break;
            }
        }
        if (transparent)
        {
            continue;
        }
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (Rl_recompile_mesh_pause_count != 0)
            {
                rl_mesh_recompile_block();
                if (params_p->m_valid == 0)
                {
                    delete[] table;
                    return nullptr;
                }
            }
            const rl_render_block& block = blocks[b];
            const unsigned char* indices = data->index_buffer.indices + index_size * block.start_index;
            unsigned int& count = counts[block.material_map_idx];
            if (prim_type == RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP && count != 0)
            {
                count += (count & 1) + 2; // the degenerate joint (and parity fix) between strips
            }
            count += block.num_indices;
            for (unsigned int i = 0; i < block.num_indices; i++)
            {
                const unsigned int old_idx = index_size == 2 ? ((const unsigned short*)indices)[i] : ((const unsigned int*)indices)[i];
                table[old_idx] = 1;
            }
        }
        for (unsigned int i = 0; i < table_size; i++)
        {
            total_verts += table[i];
        }
        memset(table, 0, table_size * sizeof(unsigned int));
    }
    unsigned int total_indices = 0;
    for (unsigned int m = 0; m < num_materials; m++)
    {
        total_indices += counts[m];
    }
    if (total_indices == 0)
    {
        delete[] table;
        return nullptr;
    }
    const unsigned int index_stride = total_verts < 0x10000 ? 2 : 4;
    unsigned char* vertex_data = (unsigned char*)allocator->allocate(total_verts * vert_stride, 16, 0, nullptr);
    if (vertex_data == nullptr)
    {
        delete[] table;
        return nullptr;
    }
    unsigned char* index_data = (unsigned char*)allocator->allocate(index_stride * total_indices, 16, 0, nullptr);
    if (index_data == nullptr)
    {
        allocator->free(vertex_data);
        delete[] table;
        return nullptr;
    }

    // Pass 2: the vertices and indices, each material's indices in its own range.
    unsigned char** block_start = (unsigned char**)_alloca(num_materials * sizeof(unsigned char*));
    unsigned int* filled = (unsigned int*)_alloca(num_materials * sizeof(unsigned int));
    memset(filled, 0, num_materials * sizeof(unsigned int));
    memset(table, 0xFF, table_size * sizeof(unsigned int)); // no new index yet
    block_start[0] = index_data;
    for (unsigned int m = 1; m < num_materials; m++)
    {
        block_start[m] = block_start[m - 1] + counts[m - 1] * index_stride;
    }
    unsigned int next_vert = 0;
    for (int s = 0; s < m_num_renderable_sub_instances; s++)
    {
        const rl_sub_instance& sub = m_sub_instances[s];
        if (!sub.m_is_visible || (int)sub.m_submesh_id >= data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = ((const rl_submesh_data*)data->sub_meshes)[sub.m_submesh_id];
        const rl_render_block* blocks = (const rl_render_block*)submesh.render_blocks;
        bool transparent = false;
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (Rl_recompile_mesh_pause_count != 0)
            {
                rl_mesh_recompile_block();
                if (params_p->m_valid == 0)
                {
                    allocator->free(vertex_data);
                    allocator->free(index_data);
                    delete[] table;
                    return nullptr;
                }
            }
            if (rl_material_library_material_is_transparent(mat_map->m_data.materials[blocks[b].material_map_idx])) // inlined
            {
                transparent = true;
                break;
            }
        }
        if (transparent)
        {
            continue;
        }
        // the sub instance's transform with the submesh offset: rotation * offset + position
        const matrix43 offset(submesh.offset_transform, IDENTITY_MATRIX);
        matrix orient;
        orient.set(sub.m_transform.pitch, sub.m_transform.bank, sub.m_transform.heading);
        const vector& o = offset.m_translation;
        vector translation;
        translation.x = orient.rvec.x * o.x + o.y * orient.uvec.x + orient.fvec.x * o.z + sub.m_transform.pos.x;
        translation.y = orient.rvec.y * o.x + o.y * orient.uvec.y + orient.fvec.y * o.z + sub.m_transform.pos.y;
        translation.z = orient.rvec.z * o.x + o.y * orient.uvec.z + orient.fvec.z * o.z + sub.m_transform.pos.z;
        const matrix43 render_transform(translation, offset.m_rotation * orient);
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            const rl_render_block& block = blocks[b];
            const unsigned int material = block.material_map_idx;
            const unsigned char* indices = data->index_buffer.indices + index_size * block.start_index;
            for (unsigned int i = 0; i < block.num_indices; i++)
            {
                if (Rl_recompile_mesh_pause_count != 0)
                {
                    rl_mesh_recompile_block();
                    if (params_p->m_valid == 0)
                    {
                        allocator->free(vertex_data);
                        allocator->free(index_data);
                        delete[] table;
                        return nullptr;
                    }
                }
                const unsigned int old_idx = index_size == 2 ? ((const unsigned short*)indices)[i] : ((const unsigned int*)indices)[i];
                unsigned int& new_idx = table[old_idx];
                process_vertex(new_idx, next_vert, data->vertex_buffer, vert_stride, old_idx, vertex_data, render_transform, has_tangent);
                unsigned int& fill = filled[material];
                if (index_stride == 2)
                {
                    unsigned short* dst = (unsigned short*)block_start[material];
                    if (i == 0 && fill != 0 && prim_type == RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
                    {
                        // join the strips: the previous last index and this first one repeated (and once more for
                        // an odd length)
                        dst[fill] = dst[fill - 1];
                        dst[fill + 1] = (unsigned short)new_idx;
                        fill += 2;
                        if ((fill & 1) != 0)
                        {
                            dst[fill] = (unsigned short)new_idx;
                            fill++;
                        }
                    }
                    dst[fill] = (unsigned short)new_idx;
                    fill++;
                }
                else
                {
                    unsigned int* dst = (unsigned int*)block_start[material];
                    if (i == 0 && fill != 0 && prim_type == RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
                    {
                        dst[fill] = dst[fill - 1];
                        dst[fill + 1] = new_idx;
                        fill += 2;
                        if ((fill & 1) != 0)
                        {
                            dst[fill] = new_idx;
                            fill++;
                        }
                    }
                    dst[fill] = new_idx;
                    fill++;
                }
            }
        }
        memset(table, 0xFF, table_size * sizeof(unsigned int));
    }

    rl_mesh_recompile_data* recompile = new rl_mesh_recompile_data();
    recompile->num_verts = total_verts;
    recompile->prim_type = prim_type;
    recompile->vertex_format = vertex_format;
    recompile->vertex_stride = (unsigned short)vert_stride;
    recompile->index_stride = (unsigned short)index_stride;
    recompile->num_render_blocks = 0;
    for (unsigned int m = 0; m < num_materials; m++)
    {
        if (counts[m] != 0)
        {
            recompile->num_render_blocks++;
        }
    }
    keen::RenderGeometryDescription description;
    description.pVertexFormat = rl_render_lib::get_instance()->m_vertex_formats_p[vertex_format];
    description.vertexCount = total_verts;
    description.indexCount = total_indices;
    description.primitiveType = prim_type != RL_PRIMITIVE_TOPOLOGY_TRIANGLELIST ? keen::PrimitiveType_TriangleStrip : keen::PrimitiveType_TriangleList;
    description.indexFormat = index_stride != 2 ? keen::IndexFormat_Uint32 : keen::IndexFormat_Uint16;
    description.cpuAccessMode = 0;
    description.streamIndex = 0;
    keen::RenderGeometryInitializationData init_data;
    init_data.pVertexData = vertex_data;
    init_data.pIndexData = index_data;
    init_data.pSkinningData = nullptr;
    init_data.pMorphingData = nullptr;
    init_data.pPlatformData = nullptr;
    keen::graphics::initializeRenderGeometry(&recompile->geometry, Init_params.m_pKeenGraphicsSystem, description, init_data, "rl_destroyable_model_instance");
    recompile->index_data_p = index_data;
    recompile->vertex_data_p = vertex_data;
    recompile->render_blocks_p = new rl_render_block[recompile->num_render_blocks];
    int block_index = 0;
    for (unsigned int m = 0; m < num_materials; m++)
    {
        if (counts[m] == 0)
        {
            continue;
        }
        rl_render_block& block = recompile->render_blocks_p[block_index++];
        block.num_indices = counts[m];
        block.material_map_idx = (unsigned short)m;
        block.start_index = (unsigned int)(block_start[m] - index_data) / index_stride;
        block.minimum_index = 0;
        block.maximum_index = 0;
    }
    delete[] table;
    return recompile;
}

// 0x004BEAC0 (:1908)
// Installs (or drops) the compiled mesh. The batchable flag from before the first recompile is kept in
// kWasBatchable; a compiled mesh is drawn unbatched.
rl_mesh_recompile_data* rl_destroyable_model_instance::set_compiled_cache_data(rl_mesh_recompile_data* data_p, rl_mesh_recompile_parameters* params_p)
{
    rl_mesh_recompile_data* old_data_p = m_recompile_data_p;
    if (old_data_p == nullptr)
    {
        if (get_is_batchable())
        {
            m_flags |= kWasBatchable;
        }
        else
        {
            m_flags &= ~kWasBatchable;
        }
    }
    m_recompile_data_p = data_p;
    if (data_p != nullptr)
    {
        set_is_batchable(false);
        m_recompile_parameters_p = nullptr;
    }
    return old_data_p;
}

// 0x0053A330 (:1933)
// The compiled mesh when there is one (one draw per render block, first pass of its material), else the sub
// instances.
void rl_destroyable_model_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    rl_mesh_recompile_data* geometry_p = m_recompile_data_p;
    if (!Rl_recompile_mesh_enable || geometry_p == nullptr)
    {
        rl_compound_mesh_instance::render(renderer, camera, flags);
        return;
    }
    prerender(renderer);
    for (unsigned int b = 0; b < geometry_p->num_render_blocks; b++)
    {
        const rl_render_block& block = geometry_p->render_blocks_p[b];
        renderer.set_world_transform(get_world_transform());
        const unsigned int mat_h = m_mat_map->get_material(block.material_map_idx);
        renderer.set_shader_type(shader_type_standard);
        renderer.set_shader_configuration(0);
        rl_material_library_set_render_material(renderer, mat_h, 0);
        renderer.set_vertex_format(geometry_p->vertex_format);
        renderer.draw_geometry(&geometry_p->geometry, geometry_p->vertex_stride, block.num_indices, block.start_index);
    }
    if ((flags & 2) == 0)
    {
        rl_material_library_clear_render_material(renderer);
        rl_decal* decals[256];
        const int num_decals = build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            render_decals(renderer, num_decals, decals);
        }
    }
}

// 0x004BDCD0 (.h:180)
rl_destroyable_model_instance_shared_base* rl_destroyable_model_instance::get_shared_data()
{
    return m_shared_data;
}

// 0x004BDCE0 (.h:183)
void rl_destroyable_model_instance::set_internal_arrays(rl_sub_instance* new_sub_instances)
{
    m_sub_instances = new_sub_instances;
}

// 0x004BDCF0 (.h:184)
void rl_destroyable_model_instance::get_internal_arrays(rl_sub_instance** sub_instances)
{
    *sub_instances = m_sub_instances;
}

// 0x004BDD00 (.h:209)
// Invalidates the pending recompile's parameters.
void rl_destroyable_model_instance::set_recompile_parameters(rl_mesh_recompile_parameters* params_p)
{
    if (m_recompile_parameters_p != nullptr)
    {
        _InterlockedExchange((volatile long*)&m_recompile_parameters_p->m_valid, 0);
    }
    m_recompile_parameters_p = params_p;
}

// 0x004BEB10 (:2030)
void rl_destroyable_model_instance_system_init(split_memmgr* mgr, void (*allocation_stats)(int))
{
    Rl_destroyable_instance_memmgr = mgr;
    Rl_destroyable_instance_alloc_stats_cb = allocation_stats;
}
