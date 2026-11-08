// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the entities' cached broadphase boxes (the collidable's bounding volume data),
// from rfg.exe. Each box is the shape's box at the end of the step (grown by half the collision tolerance), kept as
// a 32-bit integer box with the swept box (the motion over the step) packed in as its expansion. Entities with
// child box caches (shape collections, BV trees) get one such box per child, sorted by their minimum x.
//
// The arithmetic follows the binary: hkAabbUtil::sweepOffsetAabb, convertAabbToUint32 and
// compressExpandedAabbUint32 are inlined there in the same order (hkaabbutil.inl).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

namespace
{
// hkValueIndexPair: the children sorted by their first 32-bit coordinate (unsigned)
struct hkEntityAabbUtil_ValueIndexPair
{
    unsigned int m_value;
    int m_index;
};
}

// 0x00E136F0
void hkpEntityAabbUtil::entityBatchInvalidateAabb(hkpEntity* const* entityBatch, int numEntities)
{
    for (int i = 0; i < numEntities; i++)
    {
        entityBatch[i]->m_collidable.m_boundingVolumeData.invalidate();
    }
}

// The swept box packed into the 32-bit box as its expansion.
static inline void hkEntityAabbUtil_setBoundingVolume(const hkpCollisionInput* input, const hkAabb& aabb, const hkAabb& swept, hkAabbUint32& aabbOut)
{
    const hkpCollisionInput::Aabb32Info& info = input->m_aabb32Info;
    hkAabbUtil::convertAabbToUint32(aabb, info.m_bitOffsetLow, info.m_bitOffsetHigh, info.m_bitScale, aabbOut);
    hkAabbUint32 sweptAabb;
    hkAabbUtil::convertAabbToUint32(swept, info.m_bitOffsetLow, info.m_bitOffsetHigh, info.m_bitScale, sweptAabb);
    hkAabbUtil::compressExpandedAabbUint32(sweptAabb, aabbOut);
}

// 0x00E13720
void hkpEntityAabbUtil::entityBatchRecalcAabb(const hkpCollisionInput* collisionInput, hkpEntity* const* entityBatch, int numEntities)
{
    const float halfTolerance = collisionInput->m_tolerance.val() * 0.5f;
    for (int e = 0; e < numEntities; e++)
    {
        hkpEntity* entity = entityBatch[e];
        const hkpShape* shape = entity->m_collidable.m_shape;
        hkpCollidable::BoundingVolumeData& bv = entity->m_collidable.m_boundingVolumeData;
        const hkMotionState& motionState = entity->m_motion.m_motionState;

        if (shape->m_type.m_storage == hkcdShapeType::STATIC_COMPOUND || bv.m_capacityChildShapeAabbs == 0)
        {
            // one box: the shape's box, grown by the rotation (deltaAngle.w * object radius) but not beyond the
            // object's sphere around its center of mass; the swept box adds the linear motion over the step
            hkAabb aabb;
            shape->getAabb(motionState.getTransform(), halfTolerance, aabb);
            const Q radius = splat(motionState.m_objectRadius);
            const Q angular = mul(bcast<3>(motionState.m_deltaAngle.m_quad), radius);
            const Q sphere = add(splat(halfTolerance), radius);
            const Q centerOfMass1 = motionState.getSweptTransform().m_centerOfMass1.m_quad;
            Q sweptMin = _mm_max_ps(sub(aabb.m_min.m_quad, angular), sub(centerOfMass1, sphere));
            const Q motion = sub(motionState.getSweptTransform().m_centerOfMass0.m_quad, centerOfMass1);
            sweptMin = _mm_min_ps(sweptMin, aabb.m_min.m_quad);
            Q sweptMax = _mm_min_ps(add(aabb.m_max.m_quad, angular), add(centerOfMass1, sphere));
            sweptMax = _mm_max_ps(sweptMax, aabb.m_max.m_quad);
            const Q zero = _mm_setzero_ps();
            hkAabb swept;
            swept.m_min.m_quad = add(_mm_min_ps(zero, motion), sweptMin);
            swept.m_max.m_quad = add(_mm_max_ps(zero, motion), sweptMax);
            hkEntityAabbUtil_setBoundingVolume(collisionInput, aabb, swept, reinterpret_cast<hkAabbUint32&>(bv));
            continue;
        }

        const hkpShapeContainer* container = shape->getContainer();
        const int numChildren = container->getNumChildShapes();
        hkLocalBuffer<hkAabbUint32> childAabbs(numChildren);
        hkLocalBuffer<unsigned int> childKeys(numChildren);
        hkAabbUtil::OffsetAabbInput sweepInput;
        hkAabbUtil::initOffsetAabbInput(&motionState, sweepInput);
        hkAabb unswept;
        unswept.m_min = hkVector4f::getConstant<HK_QUADREAL_MAX>();
        unswept.m_max.m_quad = neg(hkVector4f::getConstant<HK_QUADREAL_MAX>().m_quad);
        hkAabb swept = unswept;

        int num = 0;
        for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
        {
            hkpShapeBuffer buffer;
            const hkpShape* child = container->getChildShape(key, buffer);
            hkAabb childAabb;
            child->getAabb(motionState.getTransform(), halfTolerance, childAabb);
            hkAabb childSwept;
            hkAabbUtil::sweepOffsetAabb(sweepInput, childAabb, childSwept);
            unswept.m_min.m_quad = _mm_min_ps(unswept.m_min.m_quad, childAabb.m_min.m_quad);
            unswept.m_max.m_quad = _mm_max_ps(unswept.m_max.m_quad, childAabb.m_max.m_quad);
            swept.m_min.m_quad = _mm_min_ps(swept.m_min.m_quad, childSwept.m_min.m_quad);
            swept.m_max.m_quad = _mm_max_ps(swept.m_max.m_quad, childSwept.m_max.m_quad);
            hkAabbUint32& childAabb32 = childAabbs[num];
            hkEntityAabbUtil_setBoundingVolume(collisionInput, childAabb, childSwept, childAabb32);
            childAabb32.m_shapeKeyByte = (unsigned char)key;
            childKeys[num] = key;
            num++;
        }
        bv.m_numChildShapeAabbs = (unsigned short)num;

        // no children (an empty box): the entity's box is its center of mass, lane by lane
        const Q valid = _mm_cmple_ps(unswept.m_min.m_quad, unswept.m_max.m_quad);
        const Q centerOfMass = motionState.getSweptTransform().m_centerOfMass1.m_quad;
        hkAabb aabb;
        aabb.m_min.m_quad = selectQ(valid, unswept.m_min.m_quad, centerOfMass);
        aabb.m_max.m_quad = selectQ(valid, unswept.m_max.m_quad, centerOfMass);
        swept.m_min.m_quad = selectQ(valid, swept.m_min.m_quad, centerOfMass);
        swept.m_max.m_quad = selectQ(valid, swept.m_max.m_quad, centerOfMass);
        hkEntityAabbUtil_setBoundingVolume(collisionInput, aabb, swept, reinterpret_cast<hkAabbUint32&>(bv));

        if (num != 0)
        {
            hkLocalBuffer<hkEntityAabbUtil_ValueIndexPair> order(num);
            if (num > 1)
            {
                for (int i = 0; i < num; i++)
                {
                    order[i].m_value = childAabbs[i].m_min[0];
                    order[i].m_index = i;
                }
                hkAlgorithm::quickSortRecursive(order.begin(), 0, num - 1, [](const hkEntityAabbUtil_ValueIndexPair& a, const hkEntityAabbUtil_ValueIndexPair& b) { return a.m_value < b.m_value; });
            }
            else
            {
                order[0].m_index = 0;
            }
            for (int j = 0; j < num; j++)
            {
                bv.m_childShapeAabbs[j] = childAabbs[order[j].m_index];
                bv.m_childShapeKeys[j] = childKeys[order[j].m_index];
            }
        }
    }
}
