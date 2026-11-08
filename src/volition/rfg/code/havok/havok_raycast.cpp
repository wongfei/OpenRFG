// Original: volition/rfg/code/havok/havok_raycast.cpp
//
// Ray, sphere and closest point queries against the Havok world (decompiled). Every query holds Havok_raycast_cs,
// clamps its points into the safe broadphase, hands the object filter callback and ray flags to the game filter
// (havok_game_filter statics) for the duration of the query and collects closest or all hits. raycast_output keeps
// its hits sorted by fraction; each hit carries the hit object's handle and its physical and effect materials (from
// the object, or from properties 3/4 of the Havok world object when there is no object).
// The ray hit collectors (havok_raycast_collectors.h) are defined here too.
// Compiled in rfg_havok_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - havok_raycast_group builds its collectors with a placement new[] whose array cookie is not part of the
//   Common_mempool allocation, so the last collector reaches 16 bytes past it. The collectors are never destroyed
//   (the pool is restored to its mark), so hits beyond a collector's in-place storage leak.
// - havok_closest_point_per_object_collector compares the objects of two bodies, and two bodies without an object
//   (property 0) count as the same object.
// - havok_get_objects_closest_point reports the distance divided by the query radius as the hit fraction.

#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_raycast_collectors.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycaster.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

#include "keen/base/trace.hpp"

#include <float.h>
#include <math.h>
#include <new>

// ---- globals
cs_wrapper Havok_raycast_cs(0); // 0x02B9516C, held by every query

// HK_REAL_EPSILON (FLT_EPSILON, 0x34000000), Havok's default cast tolerances: common/base/math/hkmath.h (since HK2)

// The topmost body of a body hierarchy: the root collidable.
static const hkpCollidable* havok_root_collidable(const hkpCdBody* body)
{
    while (body->m_parent != nullptr)
    {
        body = body->m_parent;
    }
    return (const hkpCollidable*)body;
}

// A safe broadphase point as a Havok vector (w = 0).
static void havok_ray_point(hkVector4f& out, const vector& point, const vector& bmin, const vector& bmax)
{
    vector clamped;
    bbox_find_closest_point(clamped, bmin, bmax, point);
    out.set(clamped.x, clamped.y, clamped.z, 0.0f);
}

// The query's object filter for the game filter.
static void havok_raycast_set_filter(const raycast_input& in)
{
    havok_game_filter::filter_callback = in.filter_callback;
    havok_game_filter::filter_params = in.filter_params;
    havok_game_filter::m_current_ray_flags = in.m_flags;
}

static void havok_raycast_clear_filter()
{
    havok_game_filter::filter_callback = nullptr;
    havok_game_filter::filter_params = nullptr;
    havok_game_filter::m_current_ray_flags = 0;
}

// ---- raycast_output
// 0x00835790 (:44)
// Inserts the hit after the hits with a smaller or equal fraction. Rigid bodies (broadphase type 1) give their
// object and Havok handle, phantoms (type 2) their object; world objects without an object give their material
// properties.
void raycast_output::add_hit(const hkVector4f& ray_start, const hkVector4f& ray_end, float hit_fraction, const hkVector4f& hit_normal, const hkVector4f* contact_point, const hkpCollidable* hk_collidable, unsigned int hit_shape_key)
{
    hit_info hit;
    hit.hit_center_pos.x = (ray_end.m_quad.m128_f32[0] - ray_start.m_quad.m128_f32[0]) * hit_fraction + ray_start.m_quad.m128_f32[0];
    hit.hit_center_pos.y = (ray_end.m_quad.m128_f32[1] - ray_start.m_quad.m128_f32[1]) * hit_fraction + ray_start.m_quad.m128_f32[1];
    hit.hit_center_pos.z = (ray_end.m_quad.m128_f32[2] - ray_start.m_quad.m128_f32[2]) * hit_fraction + ray_start.m_quad.m128_f32[2];
    hit.hit_fraction = hit_fraction;
    if (contact_point == nullptr)
    {
        hit.hit_pos = hit.hit_center_pos;
    }
    else
    {
        hit.hit_pos.set(contact_point->m_quad.m128_f32[0], contact_point->m_quad.m128_f32[1], contact_point->m_quad.m128_f32[2]);
    }
    hit.hit_normal.set(hit_normal.m_quad.m128_f32[0], hit_normal.m_quad.m128_f32[1], hit_normal.m_quad.m128_f32[2]);
    hit.hit_physical_material = Rfg_physical_material_default_index;
    hit.hit_havok_shape_key = hit_shape_key;
    hit.hit_effect_material = Rfg_effect_material_default_index;
    hit.hit_havok_handle = 0xFFFFFFFF;
    hit.hit_object_handle = 0xFFFFFFFF;
    const hkpWorldObject* wp = hk_collidable != nullptr ? (const hkpWorldObject*)hk_collidable->getOwner() : nullptr;
    if (wp != nullptr)
    {
        const signed char type = hk_collidable->m_broadPhaseHandle.m_type;
        if (type == 1 || type == 2) // hkpWorldObject::BROAD_PHASE_ENTITY, BROAD_PHASE_PHANTOM
        {
            object* objp = object_from_world_obj(wp);
            if (objp != nullptr)
            {
                hit.hit_object_handle = objp->get_handle();
                if (type == 1)
                {
                    hit.hit_havok_handle = havok_rigid_body_get_handle((const hkpRigidBody*)wp);
                }
                hit.hit_physical_material = objp->get_physical_material(hit_shape_key, &hit.hit_pos, wp);
                hit.hit_effect_material = objp->get_effect_material(hit_shape_key, &hit.hit_pos, wp);
            }
            else
            {
                hit.hit_physical_material = (unsigned char)(unsigned int)havok_get_property((hkpWorldObject*)wp, HAVOK_PROPERTY_PHYSICAL_MATERIAL, (void*)0xFF, true);
                hit.hit_effect_material = (unsigned char)(unsigned int)havok_get_property((hkpWorldObject*)wp, HAVOK_PROPERTY_EFFECT_MATERIAL, (void*)0xFF, true);
            }
        }
    }
    int index = 0;
    while (index < m_hits.getSize() && !(hit_fraction < m_hits[index].hit_fraction))
    {
        index++;
    }
    // hkArrayBase::_spliceInto(index, 0, &hit, 1), inlined
    m_hits.expandOne();
    for (int i = m_hits.getSize() - 1; i > index; i--)
    {
        m_hits[i] = m_hits[i - 1];
    }
    m_hits[index] = hit;
}

// ---- functions
// 0x0083CA30 (:156)
// Whether the segment hits anything (closest hit).
bool havok_raycast(const vector& start, const vector& end, unsigned int filter_info, unsigned int in_flags, bool (*filter_callback)(object*, void*), void* filter_params)
{
    Havok_raycast_cs.enter();
    raycast_input in;
    in.radius = 0.0f;
    in.start = start;
    in.end = end;
    in.m_flags = in_flags;
    in.filter_callback = filter_callback;
    in.m_filter_info = filter_info;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.filter_params = filter_params;
    raycast_output out;
    const bool hit = havok_raycast(in, out);
    Havok_raycast_cs.leave();
    return hit;
}

// 0x00835A30 (:182)
bool havok_raycast(const raycast_input& in, raycast_output& out)
{
    Havok_raycast_cs.enter();
    havok_raycast_set_filter(in);
    hkpWorldRayCastInput input;
    havok_ray_point(input.m_from, in.start, Havok_safe_broadphase.min, Havok_safe_broadphase.max);
    havok_ray_point(input.m_to, in.end, Havok_safe_broadphase.min, Havok_safe_broadphase.max);
    input.m_filterInfo = in.m_filter_info;
    out.m_hits.clear();
    input.m_userData = 0;
    input.m_enableShapeCollectionFilter.m_bool = 1;
    hkArray<hkpWorldRayCastOutput, hkContainerHeapAllocator> results;
    if (in.m_hit_collector_type == raycast_input::COLLECT_CLOSEST_HIT)
    {
        havok_closest_ray_hit_collector collector;
        Havok_world->castRay(input, collector);
        if (collector.m_rayHit.m_rootCollidable != nullptr) // hasHit, inlined
        {
            results.pushBack(collector.m_rayHit);
        }
    }
    else if (in.m_hit_collector_type == raycast_input::COLLECT_ALL_HITS)
    {
        havok_all_ray_hit_collector collector;
        Havok_world->castRay(input, collector);
        for (int i = 0; i < collector.m_hits.getSize(); i++)
        {
            results.pushBack(collector.m_hits[i]);
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    for (int i = 0; i < results.getSize(); i++)
    {
        out.add_hit(input.m_from, input.m_to, results[i].m_hitFraction, results[i].m_normal, nullptr, results[i].m_rootCollidable, results[i].m_shapeKeys[0]);
    }
    const int num_hits = out.m_hits.getSize();
    havok_raycast_clear_filter();
    results.clearAndDeallocate();
    Havok_raycast_cs.leave();
    return num_hits != 0;
}

// 0x008360E0 (:274)
// count rays at once (one Havok query), each with its own output; the inputs share in[0]'s collector type and
// object filter. With same_start the broadphase is queried once for the box around all rays.
bool havok_raycast_group(const raycast_input* in, int count, raycast_output* out, bool same_start)
{
    Havok_raycast_cs.enter();
    if (count < 1)
    {
        Havok_raycast_cs.leave();
        return false;
    }
    havok_raycast_set_filter(in[0]);
    const unsigned int mark = Common_mempool.mark();
    hkpWorldRayCastInput* inputs = (hkpWorldRayCastInput*)Common_mempool.alloc(count * sizeof(hkpWorldRayCastInput), 0);
    if (inputs != nullptr)
    {
        for (int i = 0; i < count; i++)
        {
            new (&inputs[i]) hkpWorldRayCastInput();
        }
    }
    havok_closest_ray_hit_collector* closest_collectors = nullptr;
    havok_all_ray_hit_collector* all_collectors = nullptr;
    hkpRayHitCollector* collectors;
    int collector_striding;
    void* collector_buffer;
    if (in[0].m_hit_collector_type == raycast_input::COLLECT_CLOSEST_HIT)
    {
        collector_striding = sizeof(havok_closest_ray_hit_collector);
        collector_buffer = Common_mempool.alloc(count * sizeof(havok_closest_ray_hit_collector), 0);
        if (collector_buffer != nullptr)
        {
            closest_collectors = new (collector_buffer) havok_closest_ray_hit_collector[count]; // see the notes
        }
        collectors = closest_collectors;
    }
    else if (in[0].m_hit_collector_type == raycast_input::COLLECT_ALL_HITS)
    {
        collector_striding = sizeof(havok_all_ray_hit_collector);
        collector_buffer = Common_mempool.alloc(count * sizeof(havok_all_ray_hit_collector), 0);
        if (collector_buffer != nullptr)
        {
            all_collectors = new (collector_buffer) havok_all_ray_hit_collector[count];
        }
        collectors = all_collectors;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Havok_raycast_cs.leave();
        return false;
    }
    if (collector_buffer == nullptr)
    {
        Common_mempool.restore_to_mark(mark);
        Havok_raycast_cs.leave();
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        havok_ray_point(inputs[i].m_from, in[i].start, Havok_safe_broadphase.min, Havok_safe_broadphase.max);
        havok_ray_point(inputs[i].m_to, in[i].end, Havok_safe_broadphase.min, Havok_safe_broadphase.max);
        inputs[i].m_enableShapeCollectionFilter.m_bool = 1;
        inputs[i].m_filterInfo = in[i].m_filter_info;
    }
    hkpWorldRayCaster ray_caster;
    const hkpBroadPhase* broad_phase = Havok_world->m_broadPhase;
    const hkpCollisionFilter* filter = Havok_world->m_collisionFilter;
    if (same_start)
    {
        vector bmin(FLT_MAX, FLT_MAX, FLT_MAX);
        vector bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (int i = 0; i < count; i++)
        {
            bbox_expand(bmin, bmax, in[i].start);
            bbox_expand(bmin, bmax, in[i].end);
        }
        hkAabb aabb;
        game_to_havok_aabb(aabb, bmin, bmax);
        // The broadphase cache of the box, on the thread's Havok stack (128-byte aligned size).
        const int cache_size = (Havok_world->m_broadPhase->getAabbCacheSize() + 0x7F) & ~0x7F;
        hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
        char* cache = (char*)stack.fastBlockAlloc(cache_size);
        Havok_world->m_broadPhase->calcAabbCache(aabb, cache);
        ray_caster.castRaysFromSinglePoint(*broad_phase, inputs, count, filter, cache, collectors, collector_striding);
        stack.fastBlockFree(cache, cache_size);
    }
    else
    {
        ray_caster.castRayGroup(*broad_phase, inputs, count, filter, collectors, collector_striding);
    }
    bool any_hit = false;
    hkArray<hkpWorldRayCastOutput, hkContainerHeapAllocator> results;
    for (int i = 0; i < count; i++)
    {
        out[i].m_hits.clear();
        results.clear();
        if (in[0].m_hit_collector_type == raycast_input::COLLECT_CLOSEST_HIT)
        {
            if (closest_collectors[i].m_rayHit.m_rootCollidable == nullptr)
            {
                continue;
            }
            results.pushBack(closest_collectors[i].m_rayHit);
        }
        else if (in[0].m_hit_collector_type == raycast_input::COLLECT_ALL_HITS)
        {
            for (int j = 0; j < all_collectors[i].m_hits.getSize(); j++)
            {
                results.pushBack(all_collectors[i].m_hits[j]);
            }
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        if (results.getSize() != 0)
        {
            any_hit = true;
            for (int j = 0; j < results.getSize(); j++)
            {
                out[i].add_hit(inputs[i].m_from, inputs[i].m_to, results[j].m_hitFraction, results[j].m_normal, nullptr, results[j].m_rootCollidable, results[j].m_shapeKeys[0]);
            }
        }
    }
    Common_mempool.restore_to_mark(mark);
    havok_raycast_clear_filter();
    results.clearAndDeallocate();
    Havok_raycast_cs.leave();
    return any_hit;
}

// 0x00836AA0 (:426)
// A sphere of the query radius swept from start to end (a phantom linear cast); the points stay a radius inside
// the safe broadphase.
bool havok_spherecast(const raycast_input& in, raycast_output& out)
{
    Havok_raycast_cs.enter();
    havok_raycast_set_filter(in);
    const float radius = in.radius;
    const vector safe_min(Havok_safe_broadphase.min.x + radius, Havok_safe_broadphase.min.y + radius, Havok_safe_broadphase.min.z + radius);
    const vector safe_max(Havok_safe_broadphase.max.x - radius, Havok_safe_broadphase.max.y - radius, Havok_safe_broadphase.max.z - radius);
    hkVector4f from;
    havok_ray_point(from, in.start, safe_min, safe_max);
    hkVector4f to;
    havok_ray_point(to, in.end, safe_min, safe_max);
    out.m_hits.clear();
    hkpLinearCastInput input;
    input.m_to = to;
    input.m_maxExtraPenetration = HK_REAL_EPSILON;
    input.m_startPointTolerance = HK_REAL_EPSILON;
    hkArray<hkpRootCdPoint, hkContainerHeapAllocator> results;
    hkpSphereShape sphere(in.radius);
    hkTransformf transform;
    transform.m_rotation.setIdentity();
    transform.m_translation = from;
    hkpSimpleShapePhantom phantom(&sphere, transform, in.m_filter_info);
    if (in.m_hit_collector_type == raycast_input::COLLECT_CLOSEST_HIT)
    {
        havok_closest_linear_cast_collector collector;
        Havok_world->linearCast(phantom.getCollidable(), input, collector, nullptr);
        if (collector.m_hitPoint.m_rootCollidableA != nullptr) // hasHit, inlined
        {
            results.pushBack(collector.m_hitPoint);
        }
    }
    else if (in.m_hit_collector_type == raycast_input::COLLECT_ALL_HITS)
    {
        havok_all_linear_cast_collector collector;
        Havok_world->linearCast(phantom.getCollidable(), input, collector, nullptr);
        for (int i = 0; i < collector.m_hits.getSize(); i++)
        {
            results.pushBack(collector.m_hits[i]);
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    for (int i = 0; i < results.getSize(); i++)
    {
        const hkpRootCdPoint& point = results[i];
        out.add_hit(from, to, point.m_contact.m_separatingNormal.m_quad.m128_f32[3], point.m_contact.m_separatingNormal, &point.m_contact.m_position,
                    point.m_rootCollidableB, point.m_shapeKeyB);
    }
    const int num_hits = out.m_hits.getSize();
    havok_raycast_clear_filter();
    results.clearAndDeallocate();
    Havok_raycast_cs.leave();
    return num_hits != 0;
}

// 0x00837020 (:541)
// The closest point of every object within the query radius of start.
bool havok_get_objects_closest_point(const raycast_input& in, raycast_output& out)
{
    Havok_raycast_cs.enter();
    havok_raycast_set_filter(in);
    hkVector4f center;
    havok_ray_point(center, in.start, Havok_safe_broadphase.min, Havok_safe_broadphase.max);
    hkpSphereShape sphere(in.radius);
    hkTransformf transform;
    transform.m_rotation.setIdentity();
    transform.m_translation = center;
    hkpSimpleShapePhantom phantom(&sphere, transform, COLLIDE_RAY_DEFAULT);
    havok_closest_point_per_object_collector collector;
    collector.center = center;
    Havok_world->getClosestPoints(phantom.getCollidable(), *Havok_world->m_collisionInput, collector);
    hkArray<hkpRootCdPoint, hkContainerHeapAllocator> results;
    for (int i = 0; i < collector.m_hits.getSize(); i++)
    {
        results.pushBack(collector.m_hits[i]);
    }
    for (int i = 0; i < results.getSize(); i++)
    {
        const hkpRootCdPoint& point = results[i];
        const float dx = point.m_contact.m_position.m_quad.m128_f32[0] - in.start.x;
        const float dy = point.m_contact.m_position.m_quad.m128_f32[1] - in.start.y;
        const float dz = point.m_contact.m_position.m_quad.m128_f32[2] - in.start.z;
        out.add_hit(center, center, sqrtf(dy * dy + dx * dx + dz * dz) / sphere.m_radius, point.m_contact.m_separatingNormal, &point.m_contact.m_position,
                    point.m_rootCollidableB, point.m_shapeKeyB);
    }
    havok_raycast_clear_filter();
    const int num_hits = out.m_hits.getSize();
    results.clearAndDeallocate();
    Havok_raycast_cs.leave();
    return num_hits != 0;
}

// 0x00837440 (:609)
bool havok_raycast_against_static_level_only(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
    return havok_raycast(in, out);
}

// 0x00837460 (:623)
bool havok_spherecast_against_static_level_only(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
    return havok_spherecast(in, out);
}

// 0x00837480 (:639)
bool havok_raycast_against_destroyable_geometry(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_RFG_DESTROYABLE;
    return havok_raycast(in, out);
}

// 0x008374A0 (:653)
bool havok_raycast_navmesh(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_NAVMESH_ONLY;
    return havok_raycast(in, out);
}

// 0x008374C0 (:667)
bool havok_spherecast_navmesh(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_NAVMESH_ONLY;
    return havok_spherecast(in, out);
}

// 0x008374E0 (:681)
bool havok_raycast_vehicle_navmesh(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_VEHICLE_NAVMESH_ONLY;
    return havok_raycast(in, out);
}

// 0x00837500 (:695)
bool havok_spherecast_vehicle_navmesh(raycast_input& in, raycast_output& out)
{
    in.m_filter_info = COLLIDE_RAY_VEHICLE_NAVMESH_ONLY;
    return havok_spherecast(in, out);
}

// ---- raycast_input, raycast_output (header inlines)
// 0x005E2370 (.h:50)
raycast_input::raycast_input()
{
    radius = 0.0f;
    filter_params = nullptr;
    filter_callback = nullptr;
    m_filter_info = COLLIDE_RAY_DEFAULT;
    m_hit_collector_type = COLLECT_CLOSEST_HIT;
    m_flags = 0;
}

// 0x00A0BB50 (the compiler-generated copy, for havok_collect_objects_in_capsule's by-value input)
raycast_input::raycast_input(const raycast_input& __that) = default;

// 0x00675880 (.h:75)
raycast_output::raycast_output()
{
}

// 0x00827B60 (.h:84)
void raycast_output::reset(bool release_memory)
{
    m_hits.clear();
    if (release_memory)
    {
        m_hits.clearAndDeallocate();
    }
}

// 0x006758A0
raycast_output::~raycast_output()
{
}

// 0x00812640
raycast_output::hit_info::hit_info()
{
}

// 0x00813E40: the member copy.
raycast_output::hit_info::hit_info(const hit_info& that)
    : hit_pos(that.hit_pos),
      hit_center_pos(that.hit_center_pos),
      hit_normal(that.hit_normal),
      hit_fraction(that.hit_fraction),
      hit_object_handle(that.hit_object_handle),
      hit_havok_shape_key(that.hit_havok_shape_key),
      hit_havok_handle(that.hit_havok_handle),
      hit_physical_material(that.hit_physical_material),
      hit_effect_material(that.hit_effect_material)
{
}

// ---- havok_closest_ray_hit_collector
// 0x00812650
havok_closest_ray_hit_collector::havok_closest_ray_hit_collector()
{
}

// 0x00812680
havok_closest_ray_hit_collector::~havok_closest_ray_hit_collector()
{
}

// The leaf shape key of a hit: the hit's own key, or the last key of the body hierarchy (shapeKeysFromCdBody).
static void havok_ray_hit_set_shape_key(hkpWorldRayCastOutput& hit, const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    if ((unsigned int)hitInfo.m_extraInfo != 0xFFFFFFFF)
    {
        hit.m_shapeKeys[0] = hitInfo.m_extraInfo;
        hit.m_shapeKeys[1] = 0xFFFFFFFF;
        return;
    }
    hkpRayHitCollector::shapeKeysFromCdBody(hit.m_shapeKeys, hkpShapeRayCastOutput::MAX_HIERARCHY_DEPTH, cd_body);
    for (int i = 0; i < 7; i++)
    {
        if (hit.m_shapeKeys[i + 1] == 0xFFFFFFFF)
        {
            hit.m_shapeKeys[0] = hit.m_shapeKeys[i];
            break;
        }
    }
    hit.m_shapeKeys[1] = 0xFFFFFFFF;
}

// 0x00815C40 (.h:26)
// Keeps the closest hit with its root collidable and leaf shape key.
void havok_closest_ray_hit_collector::addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    if (!(hitInfo.m_hitFraction < m_rayHit.m_hitFraction))
    {
        return;
    }
    m_rayHit.m_normal = hitInfo.m_normal;
    m_rayHit.m_hitFraction = hitInfo.m_hitFraction;
    m_rayHit.m_extraInfo = hitInfo.m_extraInfo;
    m_rayHit.m_pad[0] = hitInfo.m_pad[0];
    m_rayHit.m_pad[1] = hitInfo.m_pad[1];
    m_rayHit.m_rootCollidable = havok_root_collidable(&cd_body);
    m_earlyOutHitFraction = hitInfo.m_hitFraction;
    havok_ray_hit_set_shape_key(m_rayHit, cd_body, hitInfo);
}

// ---- havok_all_ray_hit_collector
// 0x00833190
havok_all_ray_hit_collector::havok_all_ray_hit_collector()
{
}

// 0x008333B0
havok_all_ray_hit_collector::~havok_all_ray_hit_collector()
{
}

// 0x008332B0 (.h:69)
// Every hit, with its root collidable and leaf shape key.
void havok_all_ray_hit_collector::addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    hkpWorldRayCastOutput& hit = m_hits.expandOne();
    hit.m_hitFraction = 1.0f; // hkpWorldRayCastOutput(), inlined
    hit.m_extraInfo = -1;
    hit.m_shapeKeyIndex = 0;
    hit.m_shapeKeys[0] = 0xFFFFFFFF;
    hit.m_rootCollidable = nullptr;
    hit.m_normal = hitInfo.m_normal;
    hit.m_hitFraction = hitInfo.m_hitFraction;
    hit.m_extraInfo = hitInfo.m_extraInfo;
    hit.m_pad[0] = hitInfo.m_pad[0];
    hit.m_pad[1] = hitInfo.m_pad[1];
    hit.m_rootCollidable = havok_root_collidable(&cd_body);
    havok_ray_hit_set_shape_key(hit, cd_body, hitInfo);
}

// ---- havok_closest_linear_cast_collector
// 0x00815D00 (.h:109)
havok_closest_linear_cast_collector::havok_closest_linear_cast_collector()
{
}

// 0x00815D60 (.h:118)
void havok_closest_linear_cast_collector::reset()
{
    m_hitPoint.m_rootCollidableA = nullptr;
    m_hitPoint.m_contact.m_separatingNormal.m_quad.m128_f32[3] = FLT_MAX;
    m_earlyOutDistance = FLT_MAX;
}

// The root point of a cd point: its contact and both root collidables and shape keys.
static void havok_root_cd_point(hkpRootCdPoint& root, const hkpCdPoint& point)
{
    root.m_contact.m_position = point.m_contact.m_position;
    root.m_contact.m_separatingNormal = point.m_contact.m_separatingNormal;
    root.m_rootCollidableA = havok_root_collidable(&point.m_cdBodyA);
    root.m_shapeKeyA = point.m_cdBodyA.m_shapeKey;
    root.m_rootCollidableB = havok_root_collidable(&point.m_cdBodyB);
    root.m_shapeKeyB = point.m_cdBodyB.m_shapeKey;
}

// 0x00815D50 (.h:137)
// Keeps the point with the smallest distance (fraction).
void havok_closest_linear_cast_collector::addCdPoint(const hkpCdPoint& point)
{
    const float distance = point.m_contact.m_separatingNormal.m_quad.m128_f32[3];
    if (m_hitPoint.m_rootCollidableA != nullptr && !(distance < m_hitPoint.m_contact.m_separatingNormal.m_quad.m128_f32[3]))
    {
        return;
    }
    havok_root_cd_point(m_hitPoint, point);
    m_earlyOutDistance = distance;
    m_unweldedNormal = point.m_unweldedNormal;
}

// 0x0080D940
havok_closest_linear_cast_collector::~havok_closest_linear_cast_collector()
{
}

// ---- havok_all_linear_cast_collector
// 0x0074D550
havok_all_linear_cast_collector::havok_all_linear_cast_collector()
{
}

// 0x0074D590 (.h:171)
void havok_all_linear_cast_collector::reset()
{
    m_hits.clear();
    m_earlyOutDistance = FLT_MAX;
}

// 0x0074D580 (.h:190)
void havok_all_linear_cast_collector::addCdPoint(const hkpCdPoint& point)
{
    havok_root_cd_point(m_hits.expandOne(), point);
}

// 0x0074D5B0
havok_all_linear_cast_collector::~havok_all_linear_cast_collector()
{
}

// ---- havok_closest_point_per_object_collector
// 0x008334E0
havok_closest_point_per_object_collector::havok_closest_point_per_object_collector()
{
}

// 0x00833510 (.h:214)
void havok_closest_point_per_object_collector::reset()
{
    m_hits.clear();
    m_earlyOutDistance = FLT_MAX;
    center.setZero();
}

// The object (property 0) of a root collidable's world object, or null.
static void* havok_collidable_object(const hkpCollidable* collidable)
{
    const hkpWorldObject* owner = (const hkpWorldObject*)collidable->getOwner();
    if (owner == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < owner->m_properties.getSize(); i++)
    {
        if (owner->m_properties[i].m_key == HAVOK_PROPERTY_OBJECT)
        {
            return (void*)owner->m_properties[i].m_value.m_data;
        }
    }
    return nullptr;
}

// 0x00833530 (.h:224)
// One point per object: the one closest to center.
void havok_closest_point_per_object_collector::addCdPoint(const hkpCdPoint& point)
{
    for (int i = 0; i < m_hits.getSize(); i++)
    {
        if (havok_collidable_object(havok_root_collidable(&point.m_cdBodyB)) != havok_collidable_object(m_hits[i].m_rootCollidableB))
        {
            continue;
        }
        const hkVector4f& hit_pos = m_hits[i].m_contact.m_position;
        const float px = center.m_quad.m128_f32[0] - point.m_contact.m_position.m_quad.m128_f32[0];
        const float py = center.m_quad.m128_f32[1] - point.m_contact.m_position.m_quad.m128_f32[1];
        const float pz = center.m_quad.m128_f32[2] - point.m_contact.m_position.m_quad.m128_f32[2];
        const float hx = center.m_quad.m128_f32[0] - hit_pos.m_quad.m128_f32[0];
        const float hy = center.m_quad.m128_f32[1] - hit_pos.m_quad.m128_f32[1];
        const float hz = center.m_quad.m128_f32[2] - hit_pos.m_quad.m128_f32[2];
        if (hy * hy + hx * hx + hz * hz <= py * py + px * px + pz * pz)
        {
            return;
        }
        m_hits.removeAt(i);
        break;
    }
    hkpAllCdPointCollector::addCdPoint(point);
}

// 0x00833720
havok_closest_point_per_object_collector::~havok_closest_point_per_object_collector()
{
}
