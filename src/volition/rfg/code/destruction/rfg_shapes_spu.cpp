// Original: volition/rfg/code/destruction/rfg_shapes_spu.cpp
//
// The rfg_destroyable_shape functions shared with the SPU build on PS3 (decompiled): the Havok bv-tree queries over
// the shape's rbb tree, its AABB and child shapes. Compiled in rfg_destruction_unity.cpp.

#include "volition/rfg/code/destruction/rfg_shapes.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/rfg/code/destruction/rfg_rbb_tree.h"

#include <math.h>

// 0x007375E0 (:24)
// Collects the keys into the caller's buffer (an hkArray over it that never grows past maxNumKeys).
unsigned int rfg_destroyable_shape::queryAabb(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const
{
    const float* bmin = (const float*)&aabb.m_min.m_quad;
    const float* bmax = (const float*)&aabb.m_max.m_quad;
    vector qmin(bmin[0], bmin[1], bmin[2]);
    vector qmax(bmax[0], bmax[1], bmax[2]);
    hkArray<unsigned int,hkContainerHeapAllocator> keys;
    keys.m_data = hits;
    keys.m_capacityAndFlags = maxNumKeys | hkArrayBase<unsigned int>::DONT_DEALLOCATE_FLAG;
    keys.m_size = 0;
    rfg_rbb_node::query_aabb(this, qmin, qmax, keys);
    return (unsigned int)keys.getSize();
}

// 0x00717E50 (:38)
unsigned int rfg_destroyable_shape::queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const
{
    return queryAabb(aabb, hits, maxNumKeys);
}

// 0x007203F0 (:43)
// The local bounds' box transformed into world space (centre transformed, half extents through |rotation|), grown by
// the tolerance.
void rfg_destroyable_shape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    const float half[3] = {(local_bmax.x - local_bmin.x) * 0.5f, (local_bmax.y - local_bmin.y) * 0.5f, (local_bmax.z - local_bmin.z) * 0.5f};
    const float centre[3] = {(local_bmin.x + local_bmax.x) * 0.5f, (local_bmin.y + local_bmax.y) * 0.5f, (local_bmin.z + local_bmax.z) * 0.5f};
    const float* c0 = (const float*)&localToWorld.m_rotation.m_col0.m_quad;
    const float* c1 = (const float*)&localToWorld.m_rotation.m_col1.m_quad;
    const float* c2 = (const float*)&localToWorld.m_rotation.m_col2.m_quad;
    const float* t = (const float*)&localToWorld.m_translation.m_quad;
    float world_centre[4];
    float extent[4];
    for (int k = 0; k < 4; k++)
    {
        world_centre[k] = centre[0] * c0[k] + centre[1] * c1[k] + centre[2] * c2[k] + t[k];
        extent[k] = fabsf(half[0] * c0[k]) + fabsf(half[1] * c1[k]) + fabsf(half[2] * c2[k]) + tolerance;
    }
    out.m_max.set(world_centre[0] + extent[0], world_centre[1] + extent[1], world_centre[2] + extent[2], world_centre[3] + extent[3]);
    out.m_min.set(-extent[0] + world_centre[0], -extent[1] + world_centre[1], -extent[2] + world_centre[2], -extent[3] + world_centre[3]);
}

// 0x007283B0 (:58)
const hkpShape* rfg_destroyable_shape::getChildShapeImpl(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    return parent->base_extra_data.to_pointer()[key].shape.to_pointer();
}
