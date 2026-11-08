// Original: volition/rfg/code/havok/havok_bp.h
// Implementation: volition/rfg/code/havok/havok_bp.cpp
//
// The game's broadphase objects: every world object with a bounding box has a havok_bpo (object::bp_handle) in one of
// the broadphase layers (bpo_index); havok_bpi tracks which objects are inside a region (islands). Changes are queued
// by state (create/update/delete) and applied by havok_bpo_do_frame at the end of the gameplay frame.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandle.h"
#include "volition/vlib/code/util/varray.h"

struct cpu_job;
struct hkContainerHeapAllocator;
class hkp3AxisSweep;
class hkpBroadPhase;
class hkpBroadPhaseHandlePair;
class vector;

// size 0x14
class havok_bpo : public hkpBroadPhaseHandle
{
public:
    enum bpo_state
    {
        BPO_NONE = 0,
        BPO_CREATE = 1,
        BPO_DELETE = 2,
        BPO_UPDATE = 3,
        BPO_IDLE = 4,
    };

    void init(unsigned char _flags, void* _user_data); // 0x0080EED0 (:1221)

    unsigned char flags; // 0x04
    unsigned char state; // 0x05
    short bpo_index; // 0x06
    short state_index; // 0x08
    unsigned short owner; // 0x0A
    unsigned short next; // 0x0C
    unsigned short prev; // 0x0E
    void* user_data; // 0x10
};

// size 0x10
struct havok_bpi
{
    enum bpi_state
    {
        FREE = 0,
        INSIDE = 1,
        OUTSIDE = 2,
        ENTERING = 3,
        LEAVING = 4,
        NUM_ISLAND_STATES = 5,
    };

    void init(); // 0x0080EF00 (:1243)
    void add_object(havok_bpo* objp); // 0x0080EF20 (:1260)
    void remove_object(havok_bpo* objp); // 0x0080F040 (:1272)

    unsigned char state; // 0x00
    signed char bpo_index; // 0x01
    unsigned short next; // 0x02
    unsigned short prev; // 0x04
    unsigned short rebuild_next; // 0x06
    unsigned short rebuild_prev; // 0x08
    unsigned short inside_count; // 0x0A
    unsigned short obj_list; // 0x0C
    unsigned short obj_count; // 0x0E
};

// size 0x5C
class rfg_object_broadphase
{
public:
    void system_init(int max_objects, int bpo_index); // 0x00819090 (:221)
    void level_init_pre(); // 0x0083B850 (:264)
    void level_shutdown(); // 0x008191F0 (:289)
    void do_frame(); // 0x00834C50 (:307)
    void bpo_region_update(const vector& bmin, const vector& bmax); // 0x0083B990 (:432)
    havok_bpo* bpo_create(const vector& bmin, const vector& bmax, void* user_data, unsigned char flags); // 0x00834EF0 (:443)
    void bpo_delete(havok_bpo* handle); // 0x00835030 (:504)
    void bpo_update(havok_bpo* handle, const vector& bmin, const vector& bmax); // 0x00835140 (:549)
    havok_bpo* bpo_update_flags(havok_bpo* handle, unsigned char flags); // 0x0083BA60 (:601)
    void bpo_get_bbox(havok_bpo* handle, vector& bmin, vector& bmax); // 0x00819220 (:621)
    bool merge_islands(havok_bpo* obj_a, havok_bpo* obj_b); // 0x008193F0 (:665)
    void process_new_pairs(hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& new_pairs, int id); // 0x008195A0 (:765)
    void rebuild_island(havok_bpi* island); // 0x00832270 (:918)
    void process_deleted_pairs(hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& del_pairs); // 0x008324F0 (:1000)
    rfg_object_broadphase(); // 0x00819700 (:1161)

    int my_bpo_index; // 0x00
    hkp3AxisSweep* island_broad_phase; // 0x04
    havok_bpo* streaming_region; // 0x08
    base_array<havok_bpo*> delete_list; // 0x0C
    base_array<havok_bpo*> update_list; // 0x1C
    base_array<hkAabb> update_aabb_list; // 0x2C
    base_array<havok_bpo*> create_list; // 0x3C
    base_array<hkAabb> create_aabb_list; // 0x4C
};

extern havok_bpo* Object_buf; // 0x02B82200, every broadphase object (indexed by havok_bpo::next/prev, havok_bpi::obj_list)
extern havok_bpi* Island_buf; // 0x0276D2B4, every island (one per broadphase object at most)
extern unsigned short Island_list[havok_bpi::NUM_ISLAND_STATES]; // 0x02622710, head of each state's island list
extern int Total_objects; // 0x02622724
extern unsigned short Object_free_list; // 0x02642728
extern int Object_free_list_size; // 0x02642724
extern int Object_free_list_size_low_water; // 0x0262270C
extern hkp3AxisSweep* Miniphase; // 0x02B82258, scratch broadphase of rfg_object_broadphase::rebuild_island

havok_bpi* island_alloc(int bpo_index); // 0x0080EC10 (:71)
void island_free(havok_bpi* isl); // 0x00818AC0 (:87)
void island_change_state(havok_bpi* island, havok_bpi::bpi_state new_state); // 0x0080ED40 (:97)
havok_bpo* object_alloc(int bpo_index, unsigned char flags, void* user_data, havok_bpi* island); // 0x00818BF0 (:116)
void object_free(havok_bpo* objp); // 0x00818CD0 (:149)
void havok_bpo_system_init(int num_broadphases, int* max_objects); // 0x0082A2A0 (:1291)
void havok_bpo_system_shutdown(); // 0x0082A430 (:1337)
void havok_bpo_set_thread_ownership(); // 0x0080F100 (:1350)
void havok_bpo_level_init_pre(); // 0x0083BB00 (:1355)
void havok_bpo_level_shutdown(); // 0x0083BC50 (:1390)
void havok_bpo_do_frame(); // 0x00835280 (:1416)
hkpBroadPhase* havok_bpo_get_broad_phase(int bpo_index); // 0x0080F110 (:1457)
int havok_bpo_get_num_broadphases(); // 0x0080F130 (:1466)
void havok_bpo_region_update(const vector& bmin, const vector& bmax); // 0x0083BCE0 (:1471)
havok_bpi* havok_bpo_get_first_entering(); // 0x0080F140 (:1482)
havok_bpi* havok_bpo_get_next_entering(havok_bpi* island); // 0x0080F160 (:1489)
void havok_bpo_pop_entering(havok_bpi* island); // 0x0080F180 (:1496)
havok_bpi* havok_bpo_get_first_leaving(); // 0x0080F190 (:1503)
havok_bpi* havok_bpo_get_next_leaving(havok_bpi* island); // 0x0080F1B0 (:1510)
void havok_bpo_pop_leaving(havok_bpi* island); // 0x0080F1D0 (:1517)
havok_bpi* havok_bpo_get_island(havok_bpo* handle); // 0x0080F1E0 (:1524)
havok_bpo* havok_bpo_create(int bpo_index, const vector& bmin, const vector& bmax, void* user_data, unsigned char flags); // 0x00835590 (:1542)
void havok_bpo_delete(havok_bpo* handle); // 0x008355D0 (:1554)
void havok_bpo_update(havok_bpo* handle, const vector& bmin, const vector& bmax); // 0x00835600 (:1568)
havok_bpo* havok_bpo_update_flags(havok_bpo* handle, unsigned char flags); // 0x0083BDE0 (:1582)
void havok_bpo_refresh_status(havok_bpo* handle); // 0x0080F200 (:1598)
void havok_bpo_emergency_cleanup(); // 0x0082A4C0 (:1622)
void havok_bpo_get_bbox(havok_bpo* handle, vector& bmin, vector& bmax); // 0x00819770 (:1637)
void havok_bpo_allow_updates(bool allow); // 0x0080F240 (:1789)
void havok_bpo_job(cpu_job* lj, unsigned int arg1, unsigned int arg2); // 0x00835640 (:1794)
bool havok_bpo_is_forced_outside(havok_bpo* handle); // 0x0080F250 (:1812)
