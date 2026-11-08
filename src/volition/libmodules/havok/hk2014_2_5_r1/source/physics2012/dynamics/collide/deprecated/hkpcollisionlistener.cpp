// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionlistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK9): the deprecated collision listener, from rfg.exe. Its contactPointCallback turns
// the new contact point events into contactPointConfirmedCallback; the other callbacks are inline in the SDK header.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionlistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpointmaterial.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

// 0x0081B220
hkpCollisionListener::hkpCollisionListener()
{
}

// 0x0080D3F0
hkpCollisionListener::~hkpCollisionListener()
{
}

// 0x00E03CB0: new points only, and not the end-of-step manifold callbacks. A TOI keeps its rotate normal (manifold
// points get 0); the listener's changes to the projected velocity and the rotate normal go back into the event.
void hkpCollisionListener::contactPointCallback(const hkpContactPointEvent& event)
{
    if ((event.m_contactPointProperties->m_flags & hkContactPointMaterial::CONTACT_IS_NEW) == 0 || event.m_type == hkpContactPointEvent::TYPE_MANIFOLD_AT_END_OF_STEP)
    {
        return;
    }
    hkpContactPointConfirmedEvent confirmed;
    float rotateNormal = 0.0f;
    hkpContactPointAddedEvent::Type type = hkpContactPointAddedEvent::TYPE_MANIFOLD;
    if (event.m_type <= hkpContactPointEvent::TYPE_TOI)
    {
        rotateNormal = *event.m_rotateNormal;
        type = hkpContactPointAddedEvent::TYPE_TOI;
    }
    confirmed.m_projectedVelocity = event.getSeparatingVelocity();
    confirmed.m_collidableA = &event.m_bodies[0]->m_collidable;
    confirmed.m_collidableB = &event.m_bodies[1]->m_collidable;
    confirmed.m_contactPoint = event.m_contactPoint;
    confirmed.m_contactPointProperties = event.m_contactPointProperties;
    // m_contactData is set for TOIs too (the SDK documents null there)
    confirmed.m_contactData = &event.m_contactMgr->m_contactConstraintData;
    confirmed.m_type = type;
    confirmed.m_rotateNormal = rotateNormal;
    switch (event.m_source)
    {
    case hkpCollisionEvent::SOURCE_A:
        confirmed.m_callbackFiredFrom = event.m_bodies[0];
        break;
    case hkpCollisionEvent::SOURCE_B:
        confirmed.m_callbackFiredFrom = event.m_bodies[1];
        break;
    default:
        confirmed.m_callbackFiredFrom = nullptr;
        break;
    }
    contactPointConfirmedCallback(confirmed);
    *event.m_separatingVelocity = confirmed.m_projectedVelocity;
    if (event.m_type <= hkpContactPointEvent::TYPE_TOI)
    {
        *event.m_rotateNormal = confirmed.m_rotateNormal;
    }
}

// 0x0081B230
void hkpCollisionListener::contactPointAddedCallback(hkpContactPointAddedEvent& event)
{
}

// 0x0081B240
void hkpCollisionListener::contactPointRemovedCallback(hkpContactPointRemovedEvent& event)
{
}

// 0x0081B250
void hkpCollisionListener::contactProcessCallback(hkpContactProcessEvent& event)
{
}

// 0x0081B260
void hkpCollisionListener::contactPointConfirmedCallback(hkpContactPointConfirmedEvent& event)
{
}
