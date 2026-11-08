// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpreportcontactmgr.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the contact manager of the reporting response, from rfg.exe. It creates no
// constraint: the points only fire the contact point added/removed/process callbacks (without properties), and a
// TOI only fires the contact point callback.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpreportcontactmgr.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisiondata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h"

// 0x00E198A0: the process callbacks wait the smaller of the bodies' delays
hkpReportContactMgr::hkpReportContactMgr(hkpWorld* sm, hkpRigidBody* bodyA, hkpRigidBody* bodyB)
    : hkpDynamicsContactMgr(hkpContactMgr::TYPE_REPORT_CONTACT_MGR)
{
    const unsigned short delayA = bodyA->m_contactPointCallbackDelay;
    const unsigned short delayB = bodyB->m_contactPointCallbackDelay;
    m_skipNextNprocessCallbacks = (short)(delayA < delayB ? delayA : delayB);
    m_bodyA = bodyA;
    m_bodyB = bodyB;
    m_world = sm;
}

// 0x00E198F0
hkpReportContactMgr::~hkpReportContactMgr()
{
}

// 0x00E19900: every point has id 0. The projected velocity is B's point velocity minus A's along the normal. A
// rejected point is removed again; an accepted one resets the process callback delay to 0 (the added event's delay
// would be the high half of m_bodyA for a simple constraint manager, which this is not).
unsigned short hkpReportContactMgr::addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& pcp)
{
    hkpEntity* entityB = m_bodyB;
    hkpEntity* entityA = m_bodyA;
    hkVector4f armB;
    armB.setSub(pcp.m_position, entityB->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1);
    hkVector4f armA;
    armA.setSub(pcp.m_position, entityA->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1);
    hkVector4f velA;
    velA.setCross(entityA->m_motion.m_angularVelocity, armA);
    hkVector4f velB;
    velB.setCross(entityB->m_motion.m_angularVelocity, armB);
    velA.add(entityA->m_motion.m_linearVelocity);
    velB.add(entityB->m_motion.m_linearVelocity);
    hkVector4f relVel;
    relVel.setSub(velB, velA);
    const float projectedVelocity = relVel.dot<3>(pcp.m_separatingNormal).getReal();

    hkpManifoldPointAddedEvent event(0, this, &input, &output, &a, &b, &pcp, contactCache, nullptr, projectedVelocity);
    event.m_nextProcessCallbackDelay = m_type == TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR ? (unsigned short)((unsigned int)m_bodyA >> 16) : 0;
    hkpWorldCallbackUtil::fireContactPointAdded(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityB, event);
    }
    if (event.m_status == HK_CONTACT_POINT_REJECT)
    {
        removeContactPointImpl(0, *output.m_constraintOwner.val());
        return 0xFFFF;
    }
    m_skipNextNprocessCallbacks = (short)event.m_nextProcessCallbackDelay;
    return 0;
}

// 0x00E1A280
hkResult hkpReportContactMgr::reserveContactPointsImpl(int numPoints)
{
    return HK_SUCCESS;
}

// 0x00E19C10
void hkpReportContactMgr::removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& info)
{
    hkpEntity* entityA = m_bodyA;
    hkpEntity* entityB = m_bodyB;
    hkpContactPointRemovedEvent event(cpId, this, &info, nullptr, entityA, entityB);
    hkpWorldCallbackUtil::fireContactPointRemoved(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityB, event);
    }
}

// 0x00E19C90: every (delay + 1)th step, with no properties
void hkpReportContactMgr::processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData)
{
    const short skip = m_skipNextNprocessCallbacks;
    m_skipNextNprocessCallbacks = (short)(skip - 1);
    if (skip != 0)
    {
        return;
    }
    hkpEntity* entityA = static_cast<hkpEntity*>(a.getOwner());
    hkpEntity* entityB = static_cast<hkpEntity*>(b.getOwner());
    const unsigned short delayA = entityA->m_contactPointCallbackDelay;
    const unsigned short delayB = entityB->m_contactPointCallbackDelay;
    m_skipNextNprocessCallbacks = (short)(delayA < delayB ? delayA : delayB);
    hkpContactProcessEvent event(this, &a, &b, &collisionData);
    for (int i = collisionData.getNumContactPoints() - 1; i >= 0; i--)
    {
        event.m_contactPointProperties[i] = nullptr;
    }
    hkpWorldCallbackUtil::fireContactProcess(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactProcessInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactProcessInternal(entityB, event);
    }
}

// 0x00E19A80: the TOI's material is cleared (new point, no friction or restitution)
hkpContactMgr::ToiAccept hkpReportContactMgr::addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties)
{
    properties.m_friction.m_value = 0;
    properties.m_restitution = 0;
    properties.m_maxImpulse.m_value = 0;
    properties.m_flags = hkContactPointMaterial::CONTACT_IS_NEW;
    hkpEntity* entityA = m_bodyA;
    hkpEntity* entityB = m_bodyB;
    hkpToiPointAddedEvent event(this, &input, &output, &a, &b, &cp, gskCache, &properties, toi, projectedVelocity);
    hkpWorldCallbackUtil::fireContactPointAdded(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityB, event);
    }
    if (event.m_status == HK_CONTACT_POINT_REJECT)
    {
        removeToiImpl(*output.m_constraintOwner.val(), properties);
        return TOI_REJECT;
    }
    projectedVelocity = event.m_projectedVelocity;
    return TOI_ACCEPT;
}

// 0x00E19B90: the world of body A
void hkpReportContactMgr::removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties)
{
    hkpEntity* entityA = m_bodyA;
    hkpEntity* entityB = m_bodyB;
    hkpContactPointRemovedEvent event(0xFFFF, this, &constraintOwner, &properties, entityA, entityB);
    hkpWorldCallbackUtil::fireContactPointRemoved(entityA->m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityB, event);
    }
}

// 0x00E1A290
void hkpReportContactMgr::cleanup()
{
    delete this;
}

// 0x00E1A2A0
hkpContactMgr::Type hkpReportContactMgr::getType() const
{
    return TYPE_REPORT_CONTACT_MGR;
}

// 0x00E19D80: only the contact point callbacks (no response)
void hkpReportContactMgr::confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& outToBeActivated)
{
    hkpContactPointEvent cpEvent(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(event.m_entities[0]), static_cast<hkpRigidBody*>(event.m_entities[1]), reinterpret_cast<hkpSimpleConstraintContactMgr*>(event.m_contactMgr), hkpContactPointEvent::TYPE_TOI, &event.m_contactPoint, &event.m_properties, &event.m_seperatingVelocity, &rotateNormal, false, false, false, event.m_extendedUserDatas, nullptr, nullptr);
    hkpWorldCallbackUtil::fireContactPointCallback(event.m_entities[0]->m_world, cpEvent);
    cpEvent.m_source = hkpCollisionEvent::SOURCE_A;
    if (event.m_entities[0]->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointCallbackInternal(event.m_entities[0], cpEvent);
    }
    cpEvent.m_source = hkpCollisionEvent::SOURCE_B;
    if (event.m_entities[1]->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointCallbackInternal(event.m_entities[1], cpEvent);
    }
}

// 0x00E19E40
hkpReportContactMgr::Factory::Factory(hkpWorld* mgr)
    : m_world(mgr)
{
}

// 0x00E1A2F0
hkpReportContactMgr::Factory::~Factory()
{
}

// 0x00E19E60
hkpContactMgr* hkpReportContactMgr::Factory::createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& env)
{
    return new hkpReportContactMgr(m_world, static_cast<hkpRigidBody*>(static_cast<hkpEntity*>(a.getOwner())), static_cast<hkpRigidBody*>(static_cast<hkpEntity*>(b.getOwner())));
}
