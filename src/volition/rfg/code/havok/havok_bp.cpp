// Original: volition/rfg/code/havok/havok_bp.cpp
//
// The game's object broadphase (decompiled). Each broadphase layer (bpo_index) is an hkp3AxisSweep of its own that
// holds a havok_bpo per world object, plus one streaming region object (flag 0x10) that follows the player. Creates,
// updates and deletes are queued per layer and applied by havok_bpo_do_frame, whose broadphase pair changes drive:
// - islands: objects whose boxes overlap (flags 1/2) are merged into one havok_bpi; when a pair separates the island
//   is rebuilt through the temporary Miniphase sweep (rebuild_island) and may split.
// - the streaming state of islands: an object (flag 4) inside the streaming region (flag 0x20) counts toward its
//   island's inside_count; islands move between the INSIDE/OUTSIDE/ENTERING/LEAVING lists accordingly, which the
//   world streams objects in and out by. A force-outside region (flag 0x40) marks objects 0x80 instead.
// Objects and islands live in Object_buf/Island_buf and are linked by 16-bit indices into circular lists (the free
// lists, the per-state island lists, an island's object list and the rebuild list).
// Compiled in rfg_havok_unity.cpp (allow_bpo_updates, Rfg_broadphase and Thread_id are havok.cpp statics).
//
// Notes (decompiled behaviour, kept as is):
// - island_alloc puts new islands in the LEAVING list.
// - The thread checks (havok_bpo_validate_updates, inlined in every entry point) only read the thread id.
// - process_new_pairs and process_deleted_pairs ignore their pair list id.

#include "volition/rfg/code/havok/havok_bp.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"

#include "keen/base/trace.hpp"

#include <malloc.h>
#include <new>

// ---- globals
havok_bpo* Object_buf; // 0x02B82200
havok_bpi* Island_buf; // 0x0276D2B4
unsigned short Island_list[havok_bpi::NUM_ISLAND_STATES]; // 0x02622710, head of each state's island list
int Total_objects; // 0x02622724
unsigned short Object_free_list; // 0x02642728
int Object_free_list_size; // 0x02642724
int Object_free_list_size_low_water; // 0x0262270C
hkp3AxisSweep* Miniphase; // 0x02B82258, rebuild_island's scratch broadphase
static hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> Del_pairs; // 0x0165FF48
static hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> New_pairs; // 0x0165FF54
static int Num_broadphases; // 0x02B82268

// Statistics of the last havok_bpo_do_frame.
static int Objects_deleted; // 0x02663074
static int Object_delete_pairs; // 0x02642F94
static int Objects_updated; // 0x0266306C
static int Object_update_pairs; // 0x0264306C
static int Objects_added; // 0x02666054
static int Object_added_pairs; // 0x0276D2B8

static const unsigned short INDEX_NONE = 0xFFFF;

// havok_bpo::flags
static const unsigned char BPO_FLAG_ISLAND = 0x01; // joins islands with other island objects
static const unsigned char BPO_FLAG_ISLAND_LINK = 0x02; // joins islands with island objects (not with other links)
static const unsigned char BPO_FLAG_STREAMED = 0x04; // counts toward its island's inside_count
static const unsigned char BPO_FLAG_NEVER_FORCED_OUTSIDE = 0x08;
static const unsigned char BPO_FLAG_STREAMING_REGION = 0x10;
static const unsigned char BPO_FLAG_INSIDE_REGION = 0x20;
static const unsigned char BPO_FLAG_FORCE_OUTSIDE_REGION = 0x40;
static const unsigned char BPO_FLAG_FORCED_OUTSIDE = 0x80;

// ---- index lists (circular, linked by 16-bit indices; inlined everywhere in the original)
template<typename T, unsigned short T::*NEXT, unsigned short T::*PREV>
static void index_list_remove(unsigned short& head, T* buf, T* elem)
{
    const unsigned short index = (unsigned short)(elem - buf);
    if (head == index)
    {
        if (elem->*NEXT == index)
        {
            elem->*NEXT = INDEX_NONE;
            elem->*PREV = INDEX_NONE;
            head = INDEX_NONE;
            return;
        }
        head = elem->*NEXT;
    }
    buf[elem->*PREV].*NEXT = elem->*NEXT;
    buf[elem->*NEXT].*PREV = elem->*PREV;
    elem->*NEXT = INDEX_NONE;
    elem->*PREV = INDEX_NONE;
}

// Appends at the tail (before the head).
template<typename T, unsigned short T::*NEXT, unsigned short T::*PREV>
static void index_list_add(unsigned short& head, T* buf, T* elem)
{
    const unsigned short index = (unsigned short)(elem - buf);
    if (head == INDEX_NONE)
    {
        elem->*NEXT = index;
        elem->*PREV = index;
        head = index;
        return;
    }
    elem->*PREV = buf[head].*PREV;
    elem->*NEXT = head;
    buf[buf[head].*PREV].*NEXT = index;
    buf[head].*PREV = index;
}

#define ISLAND_LIST_REMOVE(head, isl) index_list_remove<havok_bpi, &havok_bpi::next, &havok_bpi::prev>(head, Island_buf, isl)
#define ISLAND_LIST_ADD(head, isl) index_list_add<havok_bpi, &havok_bpi::next, &havok_bpi::prev>(head, Island_buf, isl)
#define REBUILD_LIST_REMOVE(head, isl) index_list_remove<havok_bpi, &havok_bpi::rebuild_next, &havok_bpi::rebuild_prev>(head, Island_buf, isl)
#define REBUILD_LIST_ADD(head, isl) index_list_add<havok_bpi, &havok_bpi::rebuild_next, &havok_bpi::rebuild_prev>(head, Island_buf, isl)
#define OBJECT_LIST_REMOVE(head, objp) index_list_remove<havok_bpo, &havok_bpo::next, &havok_bpo::prev>(head, Object_buf, objp)
#define OBJECT_LIST_ADD(head, objp) index_list_add<havok_bpo, &havok_bpo::next, &havok_bpo::prev>(head, Object_buf, objp)

// An hkArray over a base_array's storage, never freed (the original builds them on the stack for the batch calls).
template<typename T, typename U>
struct base_array_view : public hkArray<T, hkContainerHeapAllocator>
{
    base_array_view(base_array<U>& a)
    {
        this->m_data = (T*)a.elements();
        this->m_size = a.size();
        this->m_capacityAndFlags = a.capacity() | hkArrayBase<T>::DONT_DEALLOCATE_FLAG;
    }
};

// 0x0080EE80 (:171)
// The updates belong to one thread (the assert is compiled out).
static void havok_bpo_validate_updates()
{
    if (!allow_bpo_updates)
    {
        get_current_thread_id();
    }
}

// ---- islands and objects
// 0x0080EC10 (:71)
havok_bpi* island_alloc(int bpo_index)
{
    if (Island_list[havok_bpi::FREE] == INDEX_NONE)
    {
        return nullptr;
    }
    havok_bpi* island = &Island_buf[Island_list[havok_bpi::FREE]];
    ISLAND_LIST_REMOVE(Island_list[havok_bpi::FREE], island);
    island->bpo_index = (signed char)bpo_index;
    island->state = havok_bpi::LEAVING;
    ISLAND_LIST_ADD(Island_list[havok_bpi::LEAVING], island);
    return island;
}

// 0x00818AC0 (:87)
void island_free(havok_bpi* isl)
{
    ISLAND_LIST_REMOVE(Island_list[isl->state], isl);
    isl->init();
    ISLAND_LIST_ADD(Island_list[havok_bpi::FREE], isl);
}

// 0x0080ED40 (:97)
void island_change_state(havok_bpi* island, havok_bpi::bpi_state new_state)
{
    if (island->state == new_state)
    {
        return;
    }
    ISLAND_LIST_REMOVE(Island_list[island->state], island);
    ISLAND_LIST_ADD(Island_list[new_state], island);
    island->state = (unsigned char)new_state;
}

// 0x00818BF0 (:116)
havok_bpo* object_alloc(int bpo_index, unsigned char flags, void* user_data, havok_bpi* island)
{
    if (Object_free_list == INDEX_NONE)
    {
        return nullptr;
    }
    havok_bpo* objp = &Object_buf[Object_free_list];
    OBJECT_LIST_REMOVE(Object_free_list, objp);
    Object_free_list_size--;
    objp->init(flags, user_data); // inlined
    objp->bpo_index = (short)bpo_index;
    if (island != nullptr)
    {
        island->add_object(objp);
    }
    return objp;
}

// 0x00818CD0 (:149)
// An island is freed with its last object.
void object_free(havok_bpo* objp)
{
    if (objp->owner != INDEX_NONE)
    {
        havok_bpi* isl = &Island_buf[objp->owner];
        isl->remove_object(objp);
        if (isl->obj_count == 0)
        {
            island_free(isl);
        }
    }
    objp->state = havok_bpo::BPO_NONE;
    objp->bpo_index = -1;
    OBJECT_LIST_ADD(Object_free_list, objp);
    Object_free_list_size++;
}

// 0x00818E20 (:187)
// Every object and island back on its free list.
static void havok_bpo_init_objects_and_islands()
{
    Object_free_list = INDEX_NONE;
    for (int i = 0; i < Total_objects; i++)
    {
        Object_buf[i].init(0, nullptr); // inlined
        OBJECT_LIST_ADD(Object_free_list, &Object_buf[i]);
    }
    Object_free_list_size = Total_objects;
    Object_free_list_size_low_water = Total_objects;
    for (int i = 0; i < havok_bpi::NUM_ISLAND_STATES; i++)
    {
        Island_list[i] = INDEX_NONE;
    }
    // One island per object at most.
    for (int i = 0; i < Total_objects; i++)
    {
        Island_buf[i].init(); // inlined
        ISLAND_LIST_ADD(Island_list[havok_bpi::FREE], &Island_buf[i]);
    }
}

// ---- rfg_object_broadphase
// 0x00819090 (:221)
// The queues hold max_objects deletes and 3/4 of that for creates and updates.
void rfg_object_broadphase::system_init(int max_objects, int bpo_index)
{
    my_bpo_index = bpo_index;
    delete_list.init((havok_bpo**)Permanent_mempool.alloc(max_objects * sizeof(havok_bpo*), 16), max_objects, 0);
    const int queue_size = max_objects * 3 / 4;
    update_list.init((havok_bpo**)Permanent_mempool.alloc(queue_size * sizeof(havok_bpo*), 16), queue_size, 0);
    update_aabb_list.init((hkAabb*)Permanent_mempool.alloc(queue_size * sizeof(hkAabb), 16), queue_size, 0);
    create_list.init((havok_bpo**)Permanent_mempool.alloc(queue_size * sizeof(havok_bpo*), 16), queue_size, 0);
    create_aabb_list.init((hkAabb*)Permanent_mempool.alloc(queue_size * sizeof(hkAabb), 16), queue_size, 0);
}

// A sweep over the broadphase extents with the world broadphase's quantization (offsets and scales).
static hkp3AxisSweep* havok_bpo_create_sweep()
{
    vector bmin;
    vector bmax;
    havok_get_broadphase_extents(bmin, bmax);
    hkVector4f world_min;
    world_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    hkVector4f world_max;
    world_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    hkp3AxisSweep* sweep = new hkp3AxisSweep(world_min, world_max, 0);
    const hkp3AxisSweep* world_sweep = (const hkp3AxisSweep*)Havok_world->m_broadPhase;
    sweep->m_offsetLow = world_sweep->m_offsetLow;
    sweep->m_offsetHigh = world_sweep->m_offsetHigh;
    sweep->m_scale = world_sweep->m_scale;
    sweep->m_offsetLow32bit = world_sweep->m_offsetLow32bit;
    sweep->m_offsetHigh32bit = world_sweep->m_offsetHigh32bit;
    sweep->m_scale32bit = world_sweep->m_scale32bit;
    return sweep;
}

// 0x0083B850 (:264)
void rfg_object_broadphase::level_init_pre()
{
    island_broad_phase = havok_bpo_create_sweep(); // inlined
    havok_bpo_validate_updates();
    streaming_region = Rfg_broadphase[my_bpo_index].bpo_create(ZERO_VECTOR, ZERO_VECTOR, (void*)0xFFFFFFFF, BPO_FLAG_STREAMING_REGION);
}

// 0x008191F0 (:289)
void rfg_object_broadphase::level_shutdown()
{
    if (island_broad_phase != nullptr && island_broad_phase->getReferenceCount() == 1)
    {
        island_broad_phase->removeReference();
        island_broad_phase = nullptr;
        delete_list.remove_all();
    }
}

// 0x00834C50 (:307)
// Applies the queued deletes, updates and creates to the sweep and processes the pairs they add and remove.
void rfg_object_broadphase::do_frame()
{
    const int num_deletes = delete_list.size();
    Objects_deleted += num_deletes;
    if (num_deletes < 8)
    {
        for (int i = 0; i < delete_list.size(); i++)
        {
            island_broad_phase->removeObject(delete_list[i], Del_pairs);
        }
    }
    else
    {
        base_array_view<hkpBroadPhaseHandle*, havok_bpo*> handles(delete_list);
        island_broad_phase->removeObjectBatch(handles, Del_pairs);
    }
    Object_delete_pairs += Del_pairs.getSize();

    Objects_updated += update_list.size();
    island_broad_phase->updateAabbs((hkpBroadPhaseHandle**)update_list.elements(), update_aabb_list.elements(), update_list.size(), New_pairs, Del_pairs);
    hkpTypedBroadPhaseDispatcher::removeDuplicates(New_pairs, Del_pairs);
    process_new_pairs(New_pairs, 1);
    Object_update_pairs += New_pairs.getSize();
    New_pairs.clear();
    for (int i = 0; i < update_list.size(); i++)
    {
        update_list[i]->state = havok_bpo::BPO_IDLE;
        update_list[i]->state_index = -1;
    }
    update_list.remove_all();
    update_aabb_list.remove_all();
    process_deleted_pairs(Del_pairs);
    Del_pairs.clear();
    for (int i = 0; i < delete_list.size(); i++)
    {
        object_free(delete_list[i]);
    }

    const int num_creates = create_list.size();
    Objects_added += num_creates;
    delete_list.remove_all();
    if (num_creates < 6)
    {
        for (int i = 0; i < create_list.size(); i++)
        {
            island_broad_phase->addObject(create_list[i], create_aabb_list[i], New_pairs, false);
        }
    }
    else
    {
        base_array_view<hkpBroadPhaseHandle*, havok_bpo*> handles(create_list);
        base_array_view<hkAabb, hkAabb> aabbs(create_aabb_list);
        island_broad_phase->addObjectBatch(handles, aabbs, New_pairs);
    }
    Object_added_pairs += New_pairs.getSize();
    for (int i = 0; i < create_list.size(); i++)
    {
        create_list[i]->state = havok_bpo::BPO_IDLE;
        create_list[i]->state_index = -1;
    }
    create_list.remove_all();
    create_aabb_list.remove_all();
    process_new_pairs(New_pairs, 2);
    New_pairs.clear();
}

// 0x0083B990 (:432)
// The streaming region spans the whole safe broadphase height.
void rfg_object_broadphase::bpo_region_update(const vector& bmin, const vector& bmax)
{
    const vector region_min(bmin.x, Havok_safe_broadphase.min.y + 0.1f, bmin.z);
    const vector region_max(bmax.x, Havok_safe_broadphase.max.y - 0.1f, bmax.z);
    if (!region_min.is_valid() || !region_max.is_valid())
    {
        return;
    }
    havok_bpo* handle = streaming_region;
    havok_bpo_validate_updates();
    if (handle != nullptr)
    {
        Rfg_broadphase[handle->bpo_index].bpo_update(handle, region_min, region_max); // havok_bpo_update, inlined
    }
}

// 0x00834EF0 (:443)
// Boxes outside the safe broadphase are refused. Island objects (flags 1/2/4) get an island of their own.
havok_bpo* rfg_object_broadphase::bpo_create(const vector& bmin, const vector& bmax, void* user_data, unsigned char flags)
{
    if (!bmin.is_valid() || !bmax.is_valid() || !ix_point_in_box(bmin, Havok_safe_broadphase.min, Havok_safe_broadphase.max) ||
        !ix_point_in_box(bmax, Havok_safe_broadphase.min, Havok_safe_broadphase.max))
    {
        return nullptr;
    }
    havok_bpi* island = nullptr;
    if ((flags & (BPO_FLAG_ISLAND | BPO_FLAG_ISLAND_LINK | BPO_FLAG_STREAMED)) != 0)
    {
        island = island_alloc(my_bpo_index);
        if (island == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return nullptr;
        }
    }
    havok_bpo* objp = object_alloc(my_bpo_index, flags, user_data, island);
    if (objp == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        if (island != nullptr)
        {
            island_free(island);
        }
        return nullptr;
    }
    hkAabb aabb;
    game_to_havok_aabb(aabb, bmin, bmax);
    objp->state = havok_bpo::BPO_CREATE;
    if (update_list.full()) // the update queue, as in the original
    {
        do_frame();
    }
    objp->state_index = (short)create_list.add(objp);
    create_aabb_list.add(aabb);
    return objp;
}

// 0x00835030 (:504)
// A pending create is dropped at once; anything else is queued for deletion.
void rfg_object_broadphase::bpo_delete(havok_bpo* handle)
{
    switch (handle->state)
    {
    case havok_bpo::BPO_CREATE:
    {
        const int index = handle->state_index;
        create_list.remove_unsorted(index);
        if (index < create_list.size())
        {
            create_list[index]->state_index = (short)index;
        }
        create_aabb_list.remove_unsorted(index);
        object_free(handle);
        return;
    }
    case havok_bpo::BPO_DELETE:
        return;
    case havok_bpo::BPO_UPDATE:
    {
        const int index = handle->state_index;
        update_list.remove_unsorted(index);
        if (index < update_list.size())
        {
            update_list[index]->state_index = (short)index;
        }
        update_aabb_list.remove_unsorted(index);
        break;
    }
    case havok_bpo::BPO_IDLE:
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    handle->state = havok_bpo::BPO_DELETE;
    if (delete_list.full())
    {
        do_frame();
    }
    handle->state_index = (short)delete_list.add(handle);
}

// 0x00835140 (:549)
void rfg_object_broadphase::bpo_update(havok_bpo* handle, const vector& bmin, const vector& bmax)
{
    if (handle == nullptr || !bmin.is_valid() || !bmax.is_valid())
    {
        return;
    }
    if (!ix_point_in_box(bmin, Havok_safe_broadphase.min, Havok_safe_broadphase.max) ||
        !ix_point_in_box(bmax, Havok_safe_broadphase.min, Havok_safe_broadphase.max))
    {
        return;
    }
    hkAabb aabb;
    game_to_havok_aabb(aabb, bmin, bmax);
    switch (handle->state)
    {
    case havok_bpo::BPO_CREATE:
        create_aabb_list[handle->state_index] = aabb;
        return;
    case havok_bpo::BPO_DELETE:
        return;
    case havok_bpo::BPO_UPDATE:
        update_aabb_list[handle->state_index] = aabb;
        return;
    case havok_bpo::BPO_IDLE:
        handle->state = havok_bpo::BPO_UPDATE;
        if (update_list.full())
        {
            do_frame();
        }
        handle->state_index = (short)update_list.add(handle);
        update_aabb_list.add(aabb);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x0083BA60 (:601)
// New flags need a new object (and island): the old one is deleted and a new one created with the same box.
havok_bpo* rfg_object_broadphase::bpo_update_flags(havok_bpo* handle, unsigned char flags)
{
    if (handle == nullptr)
    {
        return nullptr;
    }
    if (handle->flags == flags)
    {
        return handle;
    }
    vector bmin;
    vector bmax;
    bpo_get_bbox(handle, bmin, bmax);
    havok_bpo_validate_updates();
    Rfg_broadphase[handle->bpo_index].bpo_delete(handle); // havok_bpo_delete, inlined
    void* user_data = handle->user_data;
    havok_bpo_validate_updates();
    return Rfg_broadphase[my_bpo_index].bpo_create(bmin, bmax, user_data, flags); // havok_bpo_create, inlined
}

// 0x00819220 (:621)
// The queued box, or the sweep's (quantized) box once applied.
void rfg_object_broadphase::bpo_get_bbox(havok_bpo* handle, vector& bmin, vector& bmax)
{
    if (handle == nullptr)
    {
        bmin = ZERO_VECTOR;
        bmax = ZERO_VECTOR;
        return;
    }
    const hkAabb* aabb;
    hkAabb sweep_aabb;
    switch (handle->state)
    {
    case havok_bpo::BPO_CREATE:
        aabb = &create_aabb_list[handle->state_index];
        break;
    case havok_bpo::BPO_DELETE:
    case havok_bpo::BPO_IDLE:
        island_broad_phase->getAabb(handle, sweep_aabb);
        aabb = &sweep_aabb;
        break;
    case havok_bpo::BPO_UPDATE:
        aabb = &update_aabb_list[handle->state_index];
        break;
    default:
        bmin = ZERO_VECTOR;
        bmax = ZERO_VECTOR;
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    havok_to_game_aabb(bmin, bmax, *aabb); // inlined
}

// 0x008193F0 (:665)
// The smaller island joins the larger one; the merged island is entering when either part was inside or entering,
// leaving when both were outside or leaving.
bool rfg_object_broadphase::merge_islands(havok_bpo* obj_a, havok_bpo* obj_b)
{
    if (obj_a->owner == obj_b->owner)
    {
        return false;
    }
    havok_bpi* island = &Island_buf[obj_a->owner];
    havok_bpi* isl = &Island_buf[obj_b->owner];
    if (island->obj_count < isl->obj_count)
    {
        havok_bpi* larger = isl;
        isl = island;
        island = larger;
    }
    island->obj_count += isl->obj_count;
    island->inside_count += isl->inside_count;
    if (isl->obj_list != INDEX_NONE)
    {
        for (havok_bpo* objp = &Object_buf[isl->obj_list];; objp = &Object_buf[objp->next])
        {
            objp->owner = (unsigned short)(island - Island_buf);
            if (objp->next == isl->obj_list)
            {
                break;
            }
        }
    }
    // Splice the object lists.
    if (island->obj_list == INDEX_NONE)
    {
        island->obj_list = isl->obj_list;
    }
    else if (isl->obj_list != INDEX_NONE)
    {
        havok_bpo& head_a = Object_buf[island->obj_list];
        havok_bpo& head_b = Object_buf[isl->obj_list];
        Object_buf[head_a.prev].next = isl->obj_list;
        Object_buf[head_b.prev].next = island->obj_list;
        const unsigned short tail_b = head_b.prev;
        head_b.prev = head_a.prev;
        head_a.prev = tail_b;
    }
    const bool island_in = island->state == havok_bpi::INSIDE || island->state == havok_bpi::ENTERING;
    const bool island_out = island->state == havok_bpi::OUTSIDE || island->state == havok_bpi::LEAVING;
    switch (isl->state)
    {
    case havok_bpi::INSIDE:
        if (island->state != havok_bpi::INSIDE)
        {
            island_change_state(island, havok_bpi::ENTERING);
        }
        break;
    case havok_bpi::ENTERING:
        island_change_state(island, havok_bpi::ENTERING);
        break;
    case havok_bpi::OUTSIDE:
    case havok_bpi::LEAVING:
        if (island_in)
        {
            island_change_state(island, havok_bpi::ENTERING);
        }
        else if (island_out)
        {
            island_change_state(island, havok_bpi::LEAVING);
        }
        break;
    default:
        break;
    }
    island_free(isl);
    return true;
}

// The streaming (or force-outside) region of a pair, or null.
static havok_bpo* pair_region(havok_bpo* obj_a, havok_bpo* obj_b)
{
    if ((obj_a->flags & (BPO_FLAG_STREAMING_REGION | BPO_FLAG_FORCE_OUTSIDE_REGION)) != 0)
    {
        return obj_a;
    }
    return (obj_b->flags & (BPO_FLAG_STREAMING_REGION | BPO_FLAG_FORCE_OUTSIDE_REGION)) != 0 ? obj_b : nullptr;
}

// Whether two objects link their islands (island/island or island/link).
static bool pair_links_islands(const havok_bpo* obj_a, const havok_bpo* obj_b)
{
    return ((obj_a->flags & BPO_FLAG_ISLAND) != 0 && (obj_b->flags & BPO_FLAG_ISLAND) != 0) ||
           ((obj_a->flags & BPO_FLAG_ISLAND) != 0 && (obj_b->flags & BPO_FLAG_ISLAND_LINK) != 0) ||
           ((obj_a->flags & BPO_FLAG_ISLAND_LINK) != 0 && (obj_b->flags & BPO_FLAG_ISLAND) != 0);
}

// 0x008195A0 (:765)
// New overlaps: linked islands merge; an object entering the streaming region counts toward its island (which
// starts entering); an object entering a force-outside region is forced outside.
void rfg_object_broadphase::process_new_pairs(hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& new_pairs, int id)
{
    const int num_pairs = new_pairs.getSize();
    for (int i = 0; i < num_pairs; i++)
    {
        havok_bpo* obj_a = (havok_bpo*)new_pairs[i].m_a;
        havok_bpo* obj_b = (havok_bpo*)new_pairs[i].m_b;
        if (obj_a == nullptr || obj_b == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        havok_bpo* region = pair_region(obj_a, obj_b);
        if (region == nullptr)
        {
            if (pair_links_islands(obj_a, obj_b))
            {
                merge_islands(obj_a, obj_b);
            }
            continue;
        }
        havok_bpo* other = region != obj_a ? obj_a : obj_b;
        const unsigned char flags = other->flags;
        if ((region->flags & BPO_FLAG_STREAMING_REGION) != 0 && (flags & BPO_FLAG_INSIDE_REGION) == 0)
        {
            other->flags = flags | BPO_FLAG_INSIDE_REGION;
            if ((flags & BPO_FLAG_STREAMED) != 0 && (flags & BPO_FLAG_FORCED_OUTSIDE) == 0)
            {
                Island_buf[other->owner].inside_count++;
                island_change_state(&Island_buf[other->owner], havok_bpi::ENTERING);
            }
        }
        else if ((region->flags & BPO_FLAG_FORCE_OUTSIDE_REGION) != 0 && (flags & (BPO_FLAG_FORCED_OUTSIDE | BPO_FLAG_NEVER_FORCED_OUTSIDE)) == 0)
        {
            other->flags = flags | BPO_FLAG_FORCED_OUTSIDE;
            if ((flags & BPO_FLAG_STREAMED) != 0 && (flags & BPO_FLAG_INSIDE_REGION) != 0)
            {
                havok_bpi* island = &Island_buf[other->owner];
                if (--island->inside_count == 0)
                {
                    island_change_state(island, havok_bpi::LEAVING);
                }
            }
        }
    }
}

// 0x00832270 (:918)
// Splits an island whose objects stopped overlapping: every live object gets an island of its own (in the old
// island's state), then the objects' boxes go through the Miniphase sweep, whose pairs merge them again.
void rfg_object_broadphase::rebuild_island(havok_bpi* island)
{
    const int num_objects = island->obj_count;
    havok_bpo** objects = (havok_bpo**)_alloca(num_objects * sizeof(havok_bpo*));
    // Stand-in handles for Miniphase; handle i stands for objects[i].
    hkpBroadPhaseHandle* handles = (hkpBroadPhaseHandle*)_alloca(num_objects * sizeof(hkpBroadPhaseHandle));
    if (objects == nullptr || handles == nullptr)
    {
        return;
    }
    int num_live = 0;
    while (island->obj_list != INDEX_NONE)
    {
        havok_bpo* objp = &Object_buf[island->obj_list];
        island->remove_object(objp);
        if (objp->state == havok_bpo::BPO_DELETE)
        {
            continue;
        }
        havok_bpi* new_island = island_alloc(island->bpo_index);
        island_change_state(new_island, (havok_bpi::bpi_state)island->state);
        new_island->add_object(objp);
        update_list.add((havok_bpo*)&handles[num_live]);
        hkAabb aabb;
        island_broad_phase->getAabb(objp, aabb);
        update_aabb_list.add(aabb);
        objects[num_live++] = objp;
    }
    base_array_view<hkpBroadPhaseHandle*, havok_bpo*> handle_view(update_list);
    base_array_view<hkAabb, hkAabb> aabb_view(update_aabb_list);
    Miniphase->addObjectBatch(handle_view, aabb_view, New_pairs);
    for (int i = 0; i < New_pairs.getSize(); i++)
    {
        New_pairs[i].m_a = objects[New_pairs[i].m_a - handles];
        New_pairs[i].m_b = objects[New_pairs[i].m_b - handles];
    }
    process_new_pairs(New_pairs, 3);
    New_pairs.clear();
    if (handle_view.getSize() > 0)
    {
        Miniphase->removeObjectBatch(handle_view, New_pairs);
    }
    New_pairs.clear();
    update_list.remove_all();
    update_aabb_list.remove_all();
    island_free(island);
}

// 0x008324F0 (:1000)
// Ended overlaps: the islands of linked objects are rebuilt (once each, after all pairs); an object leaving the
// streaming region stops counting toward its island (which starts leaving at zero); one leaving a force-outside
// region is no longer forced outside.
void rfg_object_broadphase::process_deleted_pairs(hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& del_pairs)
{
    const int num_pairs = del_pairs.getSize();
    if (num_pairs <= 0)
    {
        return;
    }
    unsigned short rebuild_list = INDEX_NONE;
    for (int i = 0; i < num_pairs; i++)
    {
        havok_bpo* obj_a = (havok_bpo*)del_pairs[i].m_a;
        havok_bpo* obj_b = (havok_bpo*)del_pairs[i].m_b;
        havok_bpo* region = pair_region(obj_a, obj_b);
        if (region == nullptr)
        {
            if (pair_links_islands(obj_a, obj_b))
            {
                havok_bpi* isl = &Island_buf[obj_a->owner];
                if (isl->rebuild_prev == INDEX_NONE && isl->rebuild_next == INDEX_NONE)
                {
                    REBUILD_LIST_ADD(rebuild_list, isl);
                }
            }
            continue;
        }
        havok_bpo* other = region != obj_a ? obj_a : obj_b;
        const unsigned char flags = other->flags;
        if ((region->flags & BPO_FLAG_STREAMING_REGION) != 0 && (flags & BPO_FLAG_INSIDE_REGION) != 0)
        {
            if ((flags & BPO_FLAG_STREAMED) != 0 && (flags & BPO_FLAG_FORCED_OUTSIDE) == 0)
            {
                havok_bpi* island = &Island_buf[other->owner];
                if (--island->inside_count == 0)
                {
                    island_change_state(island, havok_bpi::LEAVING);
                }
            }
            other->flags &= ~BPO_FLAG_INSIDE_REGION;
        }
        else if ((region->flags & BPO_FLAG_FORCE_OUTSIDE_REGION) != 0 && (flags & BPO_FLAG_NEVER_FORCED_OUTSIDE) == 0 &&
                 (flags & BPO_FLAG_FORCED_OUTSIDE) != 0)
        {
            if ((flags & BPO_FLAG_STREAMED) != 0 && (flags & BPO_FLAG_INSIDE_REGION) != 0)
            {
                Island_buf[other->owner].inside_count++;
                island_change_state(&Island_buf[other->owner], havok_bpi::ENTERING);
            }
            other->flags &= ~BPO_FLAG_FORCED_OUTSIDE;
        }
    }
    while (rebuild_list != INDEX_NONE)
    {
        havok_bpi* island = &Island_buf[rebuild_list];
        REBUILD_LIST_REMOVE(rebuild_list, island);
        rebuild_island(island);
    }
}

// 0x00819700 (:1161)
rfg_object_broadphase::rfg_object_broadphase()
{
    island_broad_phase = nullptr;
    my_bpo_index = -1;
    streaming_region = nullptr;
}

// ---- havok_bpo, havok_bpi
// 0x0080EED0 (:1221)
void havok_bpo::init(unsigned char _flags, void* _user_data)
{
    flags = _flags;
    bpo_index = -1;
    owner = INDEX_NONE;
    next = INDEX_NONE;
    state = BPO_NONE;
    state_index = -1;
    prev = INDEX_NONE;
    user_data = _user_data;
}

// 0x0080EF00 (:1243)
void havok_bpi::init()
{
    state = FREE;
    bpo_index = -1;
    next = INDEX_NONE;
    prev = INDEX_NONE;
    rebuild_next = INDEX_NONE;
    rebuild_prev = INDEX_NONE;
    inside_count = 0;
    obj_list = INDEX_NONE;
    obj_count = 0;
}

// 0x0080EF20 (:1260)
void havok_bpi::add_object(havok_bpo* objp)
{
    if (objp->owner != INDEX_NONE)
    {
        return;
    }
    OBJECT_LIST_ADD(obj_list, objp);
    objp->owner = (unsigned short)(this - Island_buf);
    obj_count++;
    if ((objp->flags & BPO_FLAG_INSIDE_REGION) != 0 && (objp->flags & BPO_FLAG_STREAMED) != 0 && (objp->flags & BPO_FLAG_FORCED_OUTSIDE) == 0)
    {
        inside_count++;
    }
}

// 0x0080F040 (:1272)
void havok_bpi::remove_object(havok_bpo* objp)
{
    if (&Island_buf[objp->owner] != this)
    {
        return;
    }
    OBJECT_LIST_REMOVE(obj_list, objp);
    objp->owner = INDEX_NONE;
    obj_count--;
    if ((objp->flags & BPO_FLAG_INSIDE_REGION) != 0 && (objp->flags & BPO_FLAG_STREAMED) != 0 && (objp->flags & BPO_FLAG_FORCED_OUTSIDE) == 0)
    {
        inside_count--;
    }
}

// ---- functions
// 0x0082A2A0 (:1291)
// One object and one island per object of all layers, from Permanent_mempool; the pair arrays hold 7500 pairs.
void havok_bpo_system_init(int num_broadphases, int* max_objects)
{
    Thread_id = get_current_thread_id();
    Permanent_mempool.space_free(); // a debug size check, compiled out
    Del_pairs.reserve(7500);
    New_pairs.reserve(7500);
    int total = 0;
    for (int i = 0; i < num_broadphases; i++)
    {
        total += max_objects[i];
    }
    Object_buf = (havok_bpo*)Permanent_mempool.alloc(total * sizeof(havok_bpo), 16);
    Total_objects = total;
    Island_buf = (havok_bpi*)Permanent_mempool.alloc(total * sizeof(havok_bpi), 16);
    havok_bpo_init_objects_and_islands();
    Num_broadphases = num_broadphases;
    Rfg_broadphase = (rfg_object_broadphase*)Permanent_mempool.alloc(num_broadphases * sizeof(rfg_object_broadphase), 0);
    for (int i = 0; i < Num_broadphases; i++)
    {
        new (&Rfg_broadphase[i]) rfg_object_broadphase();
        Rfg_broadphase[i].system_init(max_objects[i], i);
    }
    Permanent_mempool.space_free();
}

// 0x0082A430 (:1337)
void havok_bpo_system_shutdown()
{
    Del_pairs.clearAndDeallocate();
    New_pairs.clearAndDeallocate();
}

// 0x0080F100 (:1350)
void havok_bpo_set_thread_ownership()
{
    Thread_id = get_current_thread_id();
}

// 0x0083BB00 (:1355)
void havok_bpo_level_init_pre()
{
    Miniphase = havok_bpo_create_sweep(); // inlined
    for (int i = 0; i < Num_broadphases; i++)
    {
        Rfg_broadphase[i].level_init_pre();
    }
}

// 0x0083BC50 (:1390)
void havok_bpo_level_shutdown()
{
    havok_bpo_do_frame();
    for (int i = 0; i < Num_broadphases; i++)
    {
        Rfg_broadphase[i].level_shutdown(); // inlined
    }
    havok_bpo_init_objects_and_islands();
    if (Miniphase != nullptr && Miniphase->getReferenceCount() == 1)
    {
        Miniphase->removeReference();
        Miniphase = nullptr;
    }
}

// 0x00835280 (:1416)
void havok_bpo_do_frame()
{
    Objects_deleted = 0;
    Object_delete_pairs = 0;
    Objects_updated = 0;
    Object_update_pairs = 0;
    Objects_added = 0;
    Object_added_pairs = 0;
    for (int i = 0; i < Num_broadphases; i++)
    {
        Rfg_broadphase[i].do_frame(); // inlined
    }
}

// 0x0080F110 (:1457)
hkpBroadPhase* havok_bpo_get_broad_phase(int bpo_index)
{
    havok_bpo_validate_updates();
    return Rfg_broadphase[bpo_index].island_broad_phase;
}

// 0x0080F130 (:1466)
int havok_bpo_get_num_broadphases()
{
    return Num_broadphases;
}

// 0x0083BCE0 (:1471)
void havok_bpo_region_update(const vector& bmin, const vector& bmax)
{
    for (int i = 0; i < Num_broadphases; i++)
    {
        Rfg_broadphase[i].bpo_region_update(bmin, bmax); // inlined
    }
}

// 0x0080F140 (:1482)
havok_bpi* havok_bpo_get_first_entering()
{
    return Island_list[havok_bpi::ENTERING] == INDEX_NONE ? nullptr : &Island_buf[Island_list[havok_bpi::ENTERING]];
}

// 0x0080F160 (:1489)
havok_bpi* havok_bpo_get_next_entering(havok_bpi* island)
{
    return island->next == Island_list[havok_bpi::ENTERING] ? nullptr : &Island_buf[island->next];
}

// 0x0080F180 (:1496)
void havok_bpo_pop_entering(havok_bpi* island)
{
    island_change_state(island, havok_bpi::INSIDE);
}

// 0x0080F190 (:1503)
havok_bpi* havok_bpo_get_first_leaving()
{
    return Island_list[havok_bpi::LEAVING] == INDEX_NONE ? nullptr : &Island_buf[Island_list[havok_bpi::LEAVING]];
}

// 0x0080F1B0 (:1510)
havok_bpi* havok_bpo_get_next_leaving(havok_bpi* island)
{
    return island->next == Island_list[havok_bpi::LEAVING] ? nullptr : &Island_buf[island->next];
}

// 0x0080F1D0 (:1517)
void havok_bpo_pop_leaving(havok_bpi* island)
{
    island_change_state(island, havok_bpi::OUTSIDE);
}

// 0x0080F1E0 (:1524)
havok_bpi* havok_bpo_get_island(havok_bpo* handle)
{
    return handle == nullptr ? nullptr : &Island_buf[handle->owner];
}

// 0x00835590 (:1542)
havok_bpo* havok_bpo_create(int bpo_index, const vector& bmin, const vector& bmax, void* user_data, unsigned char flags)
{
    havok_bpo_validate_updates();
    return Rfg_broadphase[bpo_index].bpo_create(bmin, bmax, user_data, flags);
}

// 0x008355D0 (:1554)
void havok_bpo_delete(havok_bpo* handle)
{
    havok_bpo_validate_updates();
    if (handle != nullptr)
    {
        Rfg_broadphase[handle->bpo_index].bpo_delete(handle);
    }
}

// 0x00835600 (:1568)
void havok_bpo_update(havok_bpo* handle, const vector& bmin, const vector& bmax)
{
    havok_bpo_validate_updates();
    if (handle != nullptr)
    {
        Rfg_broadphase[handle->bpo_index].bpo_update(handle, bmin, bmax);
    }
}

// 0x0083BDE0 (:1582)
havok_bpo* havok_bpo_update_flags(havok_bpo* handle, unsigned char flags)
{
    havok_bpo_validate_updates();
    if (handle == nullptr)
    {
        return nullptr;
    }
    return Rfg_broadphase[handle->bpo_index].bpo_update_flags(handle, flags);
}

// 0x0080F200 (:1598)
// Makes the world revisit the object's island: outside islands leave again, inside ones enter again.
void havok_bpo_refresh_status(havok_bpo* handle)
{
    if (handle->owner == INDEX_NONE)
    {
        return;
    }
    havok_bpi* island = &Island_buf[handle->owner];
    if (island->state == havok_bpi::OUTSIDE)
    {
        island_change_state(island, havok_bpi::LEAVING);
    }
    else if (island->state == havok_bpi::INSIDE)
    {
        island_change_state(island, havok_bpi::ENTERING);
    }
}

// 0x0082A4C0 (:1622)
// Frees the pair arrays when they are empty.
void havok_bpo_emergency_cleanup()
{
    if (Del_pairs.getSize() == 0)
    {
        Del_pairs.clearAndDeallocate();
    }
    if (New_pairs.getSize() == 0)
    {
        New_pairs.clearAndDeallocate();
    }
}

// 0x00819770 (:1637)
void havok_bpo_get_bbox(havok_bpo* handle, vector& bmin, vector& bmax)
{
    if (handle != nullptr)
    {
        Rfg_broadphase[handle->bpo_index].bpo_get_bbox(handle, bmin, bmax);
    }
}

// 0x0080F240 (:1789)
void havok_bpo_allow_updates(bool allow)
{
    allow_bpo_updates = allow;
}

// 0x00835640 (:1794)
// The broadphase update as a scheduler job: the job's thread owns the updates.
void havok_bpo_job(cpu_job* lj, unsigned int arg1, unsigned int arg2)
{
    scheduler_get_this_context_controller();
    Thread_id = get_current_thread_id();
    havok_bpo_do_frame();
}

// 0x0080F250 (:1812)
bool havok_bpo_is_forced_outside(havok_bpo* handle)
{
    if (handle == nullptr)
    {
        return false;
    }
    return (handle->flags & BPO_FLAG_FORCED_OUTSIDE) != 0;
}
