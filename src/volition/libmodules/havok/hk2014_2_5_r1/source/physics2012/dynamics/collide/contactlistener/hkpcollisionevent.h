// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcollisionevent.h
//
// Seam only (Havok 2014.2.5). The constructor from the .inl (HK8). Hand-maintained.
#pragma once


class hkpRigidBody;
class hkpSimpleConstraintContactMgr;
class hkpSimulationIsland;

// size 0x10
class hkpCollisionEvent
{
public:
    enum CallbackSource
    {
        SOURCE_A = 0,
        SOURCE_B = 1,
        SOURCE_WORLD = 2,
    };

    // 0x00E197A0 (hkpcollisionevent.inl)
    hkpCollisionEvent(CallbackSource source, hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpSimpleConstraintContactMgr* mgr)
        : m_source(source)
        , m_contactMgr(mgr)
    {
        m_bodies[0] = bodyA;
        m_bodies[1] = bodyB;
    }
    hkpRigidBody* getBody(int bodyIdx) const; // 0x00F20C50
    hkpSimulationIsland* getSimulationIsland() const;

    CallbackSource m_source; // 0x00
    hkpRigidBody* m_bodies[2]; // 0x04
    hkpSimpleConstraintContactMgr* m_contactMgr; // 0x0C
};
