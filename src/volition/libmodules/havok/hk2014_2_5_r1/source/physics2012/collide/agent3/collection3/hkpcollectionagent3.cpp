// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collection3/hkpcollectionagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the collection agent3, from rfg.exe: a convex body against a collection, run as
// the collection-collection agent3 with body A wrapped in a one-child list shape (and a temporary collidable keeping
// A's motion, parent and cached box).
//
// The shipped build records the "TtColl3" monitor timer; it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collection3/hkpcollectionagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/list3/hkplistagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DB9EB0: the list agent3 with its own process
void hkpCollectionAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    hkListAgent3::initAgentFunc(f);
    f.m_processFunc = process;
}

// 0x00DB9ED0
void hkpCollectionAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::CONVEX, hkcdShapeType::COLLECTION);
}

// 0x00DB9CD0: A in a list shape that does not reference it (its child info is emptied before the list is destroyed,
// so A loses no reference); the real A goes in as the override body.
void* hkpCollectionAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    hkpAgent3ProcessInput in2 = input;
    const hkpCdBody* bodyA = input.m_bodyA.val();
    const hkpShape* shapeA = bodyA->getShape();
    hkpCollidable collidable(nullptr, static_cast<const hkMotionState*>(nullptr), 0);
    hkpListShape list(&shapeA, 1, hkpShapeContainer::REFERENCE_POLICY_IGNORE);
    collidable.m_shape = &list;
    in2.m_overrideBodyA = bodyA;
    if (bodyA->m_parent == nullptr)
    {
        collidable.m_boundingVolumeData = reinterpret_cast<const hkpCollidable*>(bodyA)->m_boundingVolumeData; // shallow
    }
    collidable.m_motion = bodyA->m_motion;
    collidable.m_parent = bodyA->m_parent;
    in2.m_bodyA = &collidable;
    collidable.m_shapeKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    void* end = hkpCollectionCollectionAgent3::process(in2, entry, agentData, separatingNormal, output);
    list.m_childInfo.clear();
    return end;
}
