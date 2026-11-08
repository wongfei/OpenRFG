// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/rejectchassislistener/hkprejectchassislistener.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): keeps the chassis and everything that is not an entity out of a wheel phantom, from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/rejectchassislistener/hkprejectchassislistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"

// 0x00F7F240 (hkprejectchassislistener.h:27). m_chassis stays unset, as in rfg.exe: init of the wheel collide sets it.
hkpRejectChassisListener::hkpRejectChassisListener()
{
}

// 0x00F83D90
hkpRejectChassisListener::~hkpRejectChassisListener()
{
}

// 0x00F83DA0
void hkpRejectChassisListener::collidableAddedCallback(const hkpCollidableAddedEvent& event)
{
    // 1: hkpWorldObject::BROAD_PHASE_ENTITY
    if (event.m_collidable == m_chassis || event.m_collidable->getType() != 1)
    {
        event.m_collidableAccept = HK_COLLIDABLE_REJECT;
    }
}

// 0x00F83D80
void hkpRejectChassisListener::collidableRemovedCallback(const hkpCollidableRemovedEvent& event)
{
}
