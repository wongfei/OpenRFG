// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpcollisioncallbackutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK9): the world extension that turns contact constraints into collision added and
// removed callbacks and runs the end-of-step contact point callbacks, from rfg.exe. The world requires it when
// hkpWorldCinfo::m_fireCollisionCallbacks is set; the game leaves that off, so no RFG world has one.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpcollisioncallbackutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcollisionevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"

const int hkpCollisionCallbackUtil::s_id = 1001; // 0x0135BB8C, the extension id

// 0x00DED740
hkpCollisionCallbackUtil::hkpCollisionCallbackUtil()
    : hkpWorldExtension(s_id)
{
}

// 0x00DED7C0
hkpCollisionCallbackUtil::~hkpCollisionCallbackUtil()
{
}

// 0x00DED8C0: creates the extension on first use (the world takes the only reference); counts the requests
void hkpCollisionCallbackUtil::requireCollisionCallbackUtil(hkpWorld* world)
{
    hkpWorldExtension* util = world->findWorldExtension(s_id);
    if (util == nullptr)
    {
        util = new hkpCollisionCallbackUtil();
        world->addWorldExtension(util);
        util->removeReference();
    }
    util->m_attachmentCount++;
}

// 0x00F21F60: the last release removes the extension
void hkpCollisionCallbackUtil::releaseCollisionCallbackUtil(hkpWorld* world)
{
    hkpWorldExtension* util = world->findWorldExtension(s_id);
    if (--util->m_attachmentCount == 0)
    {
        world->removeWorldExtension(util);
    }
}

// 0x00DEBAE0
hkpCollisionCallbackUtil* hkpCollisionCallbackUtil::findCollisionCallbackUtil(const hkpWorld* world)
{
    return static_cast<hkpCollisionCallbackUtil*>(world->findWorldExtension(s_id));
}

// 0x00E1F9E0
void hkpCollisionCallbackUtil::performAttachments(hkpWorld* world)
{
    world->addConstraintListener(this);
    world->addWorldPostSimulationListener(&m_endOfStepCallbackUtil);
}

// 0x00E1FA30
void hkpCollisionCallbackUtil::performDetachments(hkpWorld* world)
{
    world->removeWorldPostSimulationListener(&m_endOfStepCallbackUtil);
    world->removeConstraintListener(this);
}

// 0x00E1FA80: a contact constraint fires collision added to the world's listeners, then to A's and B's. The monitor
// timer "TtCollLtUtil" is not reproduced.
void hkpCollisionCallbackUtil::constraintAddedCallback(hkpConstraintInstance* constraint)
{
    hkpConstraintData* data = constraint->m_data;
    if (data->getType() != hkpConstraintData::CONSTRAINT_TYPE_CONTACT)
    {
        return;
    }
    hkpEntity* entityA = constraint->m_entities[0];
    hkpEntity* entityB = constraint->m_entities[1];
    hkpCollisionEvent event(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), static_cast<hkpSimpleContactConstraintData*>(data)->getSimpleConstraintContactMgr());
    hkpWorldCallbackUtil::fireContactConstraintAddedCallback(entityA->m_world, event);
    event.m_source = hkpCollisionEvent::SOURCE_A;
    hkpEntityCallbackUtil::fireContactConstraintAddedCallback(entityA, event);
    event.m_source = hkpCollisionEvent::SOURCE_B;
    hkpEntityCallbackUtil::fireContactConstraintAddedCallback(entityB, event);
}

// 0x00E1FB60: as constraintAddedCallback, for collision removed. The monitor timer "TtCollLfUtil" is not
// reproduced.
void hkpCollisionCallbackUtil::constraintRemovedCallback(hkpConstraintInstance* constraint)
{
    hkpConstraintData* data = constraint->m_data;
    if (data->getType() != hkpConstraintData::CONSTRAINT_TYPE_CONTACT)
    {
        return;
    }
    hkpEntity* entityA = constraint->m_entities[0];
    hkpEntity* entityB = constraint->m_entities[1];
    hkpCollisionEvent event(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), static_cast<hkpSimpleContactConstraintData*>(data)->getSimpleConstraintContactMgr());
    hkpWorldCallbackUtil::fireContactConstraintRemovedCallback(entityA->m_world, event);
    event.m_source = hkpCollisionEvent::SOURCE_A;
    hkpEntityCallbackUtil::fireContactConstraintRemovedCallback(entityA, event);
    event.m_source = hkpCollisionEvent::SOURCE_B;
    hkpEntityCallbackUtil::fireContactConstraintRemovedCallback(entityB, event);
}
