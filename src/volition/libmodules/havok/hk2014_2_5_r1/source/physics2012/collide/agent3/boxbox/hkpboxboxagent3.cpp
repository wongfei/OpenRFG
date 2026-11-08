// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/boxbox/hkpboxboxagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the box-box agent3, from rfg.exe. The agent data is the box-box feature manifold
// (0x40 bytes). No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtBoxBox3" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/boxbox/hkpboxboxagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcollisiondetection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <new>

using namespace hkGskExact;

static inline hkpBoxBoxManifold& hkBoxBoxAgent3_manifold(void* agentData)
{
    return *static_cast<hkpBoxBoxManifold*>(agentData);
}

static inline void* hkBoxBoxAgent3_end(void* agentData)
{
    return static_cast<char*>(agentData) + sizeof(hkpBoxBoxManifold);
}

// 0x00DC3450
void hkBoxBoxAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = nullptr;
    f.m_cleanupFunc = cleanup;
    f.m_removePointFunc = removePoint;
    f.m_commitPotentialFunc = commitPotential;
    f.m_createZombieFunc = createZombie;
    f.m_updateFilterFunc = nullptr;
    f.m_destroyFunc = destroy;
    f.m_isPredictive = false;
}

// 0x00DC3490
void hkBoxBoxAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::BOX, hkcdShapeType::BOX);
}

// 0x00DC3250
void* hkBoxBoxAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    if (agentData != nullptr)
    {
        new (agentData) hkpBoxBoxManifold();
    }
    return hkBoxBoxAgent3_end(agentData);
}

// 0x00DC3270: the points with ids removed; the point counts cleared
void* hkBoxBoxAgent3::cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkpBoxBoxManifold& manifold = hkBoxBoxAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numPoints; i++)
    {
        const unsigned short id = manifold.m_contactPoints[i].m_contactPointId;
        if (id != 0xFFFF)
        {
            mgr->removeContactPointImpl(id, constraintOwner);
        }
    }
    // Original behaviour kept: the face-vertex count stays as it is.
    manifold.m_numPoints = 0;
    entry->m_numContactPoints = 0;
    return hkBoxBoxAgent3_end(agentData);
}

// 0x00DC31C0
void hkBoxBoxAgent3::removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove)
{
    hkpBoxBoxManifold& manifold = hkBoxBoxAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numPoints; i++)
    {
        if (manifold.m_contactPoints[i].m_contactPointId == idToRemove)
        {
            manifold.removePoint(i);
            entry->m_numContactPoints--;
            return;
        }
    }
}

// 0x00DC3200: the first point without an id gets it
void hkBoxBoxAgent3::commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit)
{
    hkpBoxBoxManifold& manifold = hkBoxBoxAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numPoints; i++)
    {
        if (manifold.m_contactPoints[i].m_contactPointId == 0xFFFF)
        {
            manifold.m_contactPoints[i].m_contactPointId = idToCommit;
            return;
        }
    }
}

// 0x00DC3240
void hkBoxBoxAgent3::createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert)
{
}

// 0x00DC32D0
void hkBoxBoxAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    cleanup(entry, agentData, mgr, constraintOwner);
}

// 0x00DC32F0
void* hkBoxBoxAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    const hkpCdBody& bodyA = *input.m_bodyA;
    const hkpCdBody& bodyB = *input.m_bodyB;
    const hkpBoxShape* boxA = static_cast<const hkpBoxShape*>(bodyA.getShape());
    const hkpBoxShape* boxB = static_cast<const hkpBoxShape*>(bodyB.getShape());
    hkVector4f radiusA;
    radiusA.m_quad = add(boxA->m_halfExtents.m_quad, splat(boxA->m_radius));
    hkVector4f radiusB;
    radiusB.m_quad = add(boxB->m_halfExtents.m_quad, splat(boxB->m_radius));
    hkSimdFloat32 tolerance;
    tolerance.m_real = splat(input.m_input->m_tolerance);
    hkpBoxBoxCollisionDetection detection(bodyA, bodyB, input.m_input, input.m_contactMgr, &output, input.m_aTb, bodyA.getTransform(), radiusA, bodyB.getTransform(), radiusB, tolerance);
    hkpBoxBoxManifold& manifold = hkBoxBoxAgent3_manifold(agentData);
    detection.calcManifold(manifold);
    entry->m_numContactPoints = manifold.m_numPoints;
    return hkBoxBoxAgent3_end(agentData);
}
