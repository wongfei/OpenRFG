// Original: volition/rfg/code/effects/object_decal.h
// Implementation: volition/rfg/code/effects/object_decal.cpp
//
// Object decals: bullet impact, blood splatter and blast decals applied to objects (zone decals are collected per
// render instance first), the Havok collector that finds the shapes a blast decal touches, and the tracker that fades
// blast decals overlapped by a newer one (Tracked_blast_decals, 32). Players keep their last decals in a
// decal_circ_array_queue (player::m_decals). Placement of decal_fade_data, terrain_type (held only by
// decal_collection_data) and decal_collection_data inferred.
#pragma once

#include "volition/vlib/code/util/varray.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/rfg/code/havok/havok_raycast_collectors.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/math/bbox.h"

struct hkpCdPoint;
class human;
class object;
struct rfg_effect_material;
class rl_decal_parameters;
class vector;
struct weapon_fire_info;
struct weapon_info;

enum terrain_type
{
    TERRAIN_TYPE_PATCH = 0,
    TERRAIN_TYPE_MESH = 1,
    TERRAIN_TYPE_SKIRT = 2,
    TERRAIN_TYPE_RI = 3,
};

// size 0x8
struct decal_fade_data
{
    // The defaults are inferred from decal_circ_array_queue's constructor (handles invalid, opacity 0).
    decal_fade_data(unsigned int _handle = 0xFFFFFFFF, float _opacity = 0.0f) : handle(_handle), opacity(_opacity) {} // 0x00742350 (.h:34)

    unsigned int handle; // 0x00
    float opacity; // 0x04
};

// size 0x8
struct decal_collection_data
{
    terrain_type type; // 0x00
    void* terrain_ptr; // 0x04
};

// size 0x280
class havok_blast_decal_collector : public havok_all_linear_cast_collector
{
public:
    virtual void reset(); // vtbl+0x08 0x0074D930 (:110)
protected:
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x0074D990 (:120)
public:
    havok_blast_decal_collector(); // 0x0074D750
    virtual ~havok_blast_decal_collector(); // vtbl+0x00 0x0074DC10

    hkVector4f center; // 0x1A0
    hkInplaceArray<bbox,8,hkContainerHeapAllocator> m_bboxes; // 0x1B0
};

// size 0x20
class fp_blast_decal_tracker
{
public:
    void init(const vector& bmin, const vector& bmax, unsigned int decal_handle); // 0x00743470 (:202)
    bool intersects(const fp_blast_decal_tracker& other_tracker); // 0x00743730 (:218)
    unsigned int get_handle() const; // 0x00743770 (:224)
    int get_global_index() const; // 0x00743780 (:225)

private:
    int m_min_x; // 0x00
    int m_max_x; // 0x04
    int m_min_y; // 0x08
    int m_max_y; // 0x0C
    int m_min_z; // 0x10
    int m_max_z; // 0x14
    unsigned int m_decal_handle; // 0x18
    int m_global_index; // 0x1C
    static int ms_next_global_index; // 0x02182E00
};

// size 0x5C for N = 10
// Queue of the last N decals (handle and opacity) placed by an owner; the oldest fades out when a new one is added.
template<int N>
class decal_circ_array_queue
{
public:
    decal_circ_array_queue(); // <10> 0x00A74140 (.h:60)
    ~decal_circ_array_queue(); // <10> 0x00A45FD0 (.h:65)
    void add(unsigned int handle, float opacity); // <10> 0x0074B030 (.h:68)
    void remove(); // <10> 0x00746CF0 (.h:79)
    int size() const; // <10> 0x00A21BC0 (.h:88)
    const decal_fade_data& operator[](int) const;
    decal_fade_data& operator[](int i); // <10> 0x00A45FE0 (.h:94)

private:
    circ_array<decal_fade_data, N> carray; // 0x00
    int num; // 0x58 for N = 10
};

// ---- decal_circ_array_queue template members (inline in the original header; P4)

// <10> 0x00A74140 (.h:60)
template<int N>
decal_circ_array_queue<N>::decal_circ_array_queue()
    : num(0)
{
}

// <10> 0x00A45FD0 (.h:65)
template<int N>
decal_circ_array_queue<N>::~decal_circ_array_queue()
{
}

// <10> 0x00746CF0 (.h:79): releases the oldest decal.
template<int N>
void decal_circ_array_queue<N>::remove()
{
    if (num > 0)
    {
        num--;
        rl_decal_release(carray[num].handle);
    }
}

// <10> 0x00A21BC0 (.h:88)
template<int N>
int decal_circ_array_queue<N>::size() const
{
    return num;
}

// <10> 0x00A45FE0 (.h:94): the i-th newest decal.
template<int N>
decal_fade_data& decal_circ_array_queue<N>::operator[](int i)
{
    return carray[i];
}

extern fp_blast_decal_tracker Tracked_blast_decals[32]; // 0x021828A8
extern int Num_tracked_blast_decals; // 0x02182E04

void fade_overlapped_blast_decals(const vector& bmin, const vector& bmax, unsigned int decal_handle); // 0x00745E70 (:336)
void unused_dcf_blast_decal(); // 0x0074EF00 (:631)
void unused_dcf_blood_decal(); // 0x0074C490 (:662)
void object_zone_decal_collect(object* op, decal_collection_data* buffer, unsigned int& num_instances, const rl_decal_parameters& decal_params, float decal_radius); // 0x00748340 (:697)
unsigned int object_zone_decal_apply_collection(object* op, decal_collection_data* buffer, unsigned int num_instances, const rl_decal_parameters& decal_params, float decal_radius); // 0x007487B0 (:859)
unsigned int object_decal_apply(object* op, const rl_decal_parameters& decal_params, float decal_radius, unsigned int hit_shape_key, unsigned int hit_alt_body_idx, unsigned int decal_handle); // 0x00748A60 (:1022)
void object_decal_create_blood_splatter_decal(const human* hp, const human* attacker, const vector& start_pos, const vector& dir, bool killed, damaged_by_types dbt); // 0x0074B940 (:1294)
void object_decal_create_bullet_impact_decal(object* target, const human* attacker, const weapon_info* w_info, const vector& pos, const vector& normal, float damage, const weapon_fire_info* fire_info); // 0x0074C6C0 (:1401)
rfg_effect_material* object_decal_create_blast_decals(const vector& position, float radius); // 0x0074E340 (:1535)
void object_decal_player_track_blood_decal(object* op, unsigned int decal_handle); // 0x0074BE60 (:1740)
void object_decal_system_init(); // 0x00743AD0 (:1755)
void object_decal_level_init(); // 0x00743AE0 (:1762)
