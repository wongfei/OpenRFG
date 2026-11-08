// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). The constructor from the .inl (HK8). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcollisionevent.h"

class hkContactPoint;
class hkVector4f;
class hkpContactPointProperties;
class hkpRigidBody;
class hkpSimpleConstraintContactMgr;
class hkpVelocityAccumulator;

// size 0x34
class hkpContactPointEvent : public hkpCollisionEvent
{
public:
    enum Type
    {
        TYPE_TOI = 0,
        TYPE_EXPAND_MANIFOLD = 1,
        TYPE_MANIFOLD = 2,
        TYPE_MANIFOLD_AT_END_OF_STEP = 3,
        TYPE_MANIFOLD_FROM_SAVED_CONTACT_POINT = 4,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpContactPointEvent);
    // 0x00E197C0 (hkpcontactpointevent.inl)
    hkpContactPointEvent(hkpCollisionEvent::CallbackSource source, hkpRigidBody* a, hkpRigidBody* b, hkpSimpleConstraintContactMgr* mgr, Type type, hkContactPoint* cp, hkpContactPointProperties* cpp, float* sepVel, float* rotNorm, hkBool fullManifold, hkBool firstCallback, hkBool lastCallback, unsigned int* keyStorage, hkpVelocityAccumulator* accA, hkpVelocityAccumulator* accB)
        : hkpCollisionEvent(source, a, b, mgr)
        , m_type(type)
        , m_contactPoint(cp)
        , m_contactPointProperties(cpp)
        , m_firingCallbacksForFullManifold(fullManifold)
        , m_firstCallbackForFullManifold(firstCallback)
        , m_lastCallbackForFullManifold(lastCallback)
        , m_separatingVelocity(sepVel)
        , m_rotateNormal(rotNorm)
        , m_shapeKeyStorage(keyStorage)
    {
        m_accumulators[0] = accA;
        m_accumulators[1] = accB;
    }
    hkBool isToi() const; // 0x00E03BB0
    unsigned int* getShapeKeys(int) const;
    unsigned short getContactPointId() const;
    void setNormal(const hkVector4f&);
    float getSeparatingVelocity() const; // 0x00E03C00
    void setSeparatingVelocity(float sepVel) const; // 0x00E03BC0
    float getRotateNormal() const; // 0x00E03BD0
    void setRotateNormal(float rotNorm) const; // 0x00E03BE0
    void accessVelocities(int) const;
    void updateVelocities(int) const;

    Type m_type; // 0x10
    hkContactPoint* m_contactPoint; // 0x14
    hkpContactPointProperties* m_contactPointProperties; // 0x18
    hkBool m_firingCallbacksForFullManifold; // 0x1C
    hkBool m_firstCallbackForFullManifold; // 0x1D
    hkBool m_lastCallbackForFullManifold; // 0x1E
public:
    float* m_separatingVelocity; // 0x20
    float* m_rotateNormal; // 0x24
    unsigned int* m_shapeKeyStorage; // 0x28
    hkpVelocityAccumulator* m_accumulators[2]; // 0x2C
};
