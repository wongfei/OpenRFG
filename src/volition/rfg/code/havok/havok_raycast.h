// Original: volition/rfg/code/havok/havok_raycast.h
// Implementation: volition/rfg/code/havok/havok_raycast.cpp
//
// Ray, sphere and closest point queries against the Havok world: raycast_input (segment, radius, collision filter,
// object filter callback, closest or all hits) and raycast_output (the hits). Variants restrict the query to the
// static level, destroyable geometry or the (vehicle) navmesh.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/vlib/code/math/vector3.h"

class hkVector4f;
class hkpCollidable;
class object;

// size 0x30
class raycast_input
{
public:
    enum hit_collector_types
    {
        COLLECT_CLOSEST_HIT = 0,
        COLLECT_ALL_HITS = 1,
    };

    raycast_input(const raycast_input& __that); // 0x00A0BB50
    raycast_input(); // 0x005E2370 (.h:50)

    vector start; // 0x00
    vector end; // 0x0C
    float radius; // 0x18
    void* filter_params; // 0x1C
    bool (*filter_callback)(object*, void*); // 0x20
    unsigned int m_filter_info; // 0x24
    hit_collector_types m_hit_collector_type; // 0x28
    unsigned int m_flags; // 0x2C
};

// size 0xC
class raycast_output
{
public:
    // size 0x38
    struct hit_info
    {
        hit_info(const hit_info& __that); // 0x00813E40
        hit_info(); // 0x00812640

        vector hit_pos; // 0x00
        vector hit_center_pos; // 0x0C
        vector hit_normal; // 0x18
        float hit_fraction; // 0x24
        unsigned int hit_object_handle; // 0x28
        unsigned int hit_havok_shape_key; // 0x2C
        unsigned int hit_havok_handle; // 0x30
        unsigned char hit_physical_material; // 0x34
        unsigned char hit_effect_material; // 0x35
    };

    raycast_output(); // 0x00675880 (.h:75)
    void add_hit(const hkVector4f& ray_start, const hkVector4f& ray_end, float hit_fraction, const hkVector4f& hit_normal, const hkVector4f* contact_point, const hkpCollidable* hk_collidable, unsigned int hit_shape_key); // 0x00835790 (:44)
    void reset(bool release_memory); // 0x00827B60 (.h:84)
    ~raycast_output(); // 0x006758A0

    hkArray<hit_info,hkContainerHeapAllocator> m_hits; // 0x00
};

bool havok_raycast(const vector& start, const vector& end, unsigned int filter_info, unsigned int in_flags, bool (*filter_callback)(object*, void*), void* filter_params); // 0x0083CA30 (:156)
bool havok_raycast(const raycast_input& in, raycast_output& out); // 0x00835A30 (:182)
bool havok_raycast_group(const raycast_input* in, int count, raycast_output* out, bool same_start); // 0x008360E0 (:274)
bool havok_spherecast(const raycast_input& in, raycast_output& out); // 0x00836AA0 (:426)
bool havok_get_objects_closest_point(const raycast_input& in, raycast_output& out); // 0x00837020 (:541)
bool havok_raycast_against_static_level_only(raycast_input& in, raycast_output& out); // 0x00837440 (:609)
bool havok_spherecast_against_static_level_only(raycast_input& in, raycast_output& out); // 0x00837460 (:623)
bool havok_raycast_against_destroyable_geometry(raycast_input& in, raycast_output& out); // 0x00837480 (:639)
bool havok_raycast_navmesh(raycast_input& in, raycast_output& out); // 0x008374A0 (:653)
bool havok_spherecast_navmesh(raycast_input& in, raycast_output& out); // 0x008374C0 (:667)
bool havok_raycast_vehicle_navmesh(raycast_input& in, raycast_output& out); // 0x008374E0 (:681)
bool havok_spherecast_vehicle_navmesh(raycast_input& in, raycast_output& out); // 0x00837500 (:695)
