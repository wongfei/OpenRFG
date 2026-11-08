// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h
//
// Havok 2014.2.5: the virtual interface (vtable layout) and data from rfg.pdb; the inline members (.h/.inl) added
// from the binary (havok_impl.md HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkContactPoint;
class hkpCdBody;
class hkpCollidable;
class hkpConstraintOwner;
class hkpContactPointProperties;
class hkpGskCache;
struct hkpProcessCollisionData;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0xC
class hkpContactMgr : public hkReferencedObject
{
public:
    enum Type
    {
        TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR = 0,
        TYPE_REPORT_CONTACT_MGR = 1,
        TYPE_CONVEX_LIST_CONTACT_MGR = 2,
        TYPE_NULL_CONTACT_MGR = 3,
        TYPE_USER_CONTACT_MGR = 4,
        TYPE_MAX = 5,
    };

    enum ToiAccept
    {
        TOI_ACCEPT = 0,
        TOI_REJECT = 1,
    };

    hkpContactMgr(Type type) : m_type(type) {} // 0x00DB56E0
    virtual ~hkpContactMgr() {} // vtbl+0x00 0x00DB5710
    virtual unsigned short addContactPointImpl(const hkpCdBody&, const hkpCdBody&, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&, const hkpGskCache*, hkContactPoint&) = 0; // vtbl+0x10
    virtual hkResult reserveContactPointsImpl(int) = 0; // vtbl+0x14
    virtual void removeContactPointImpl(unsigned short, hkpConstraintOwner&) = 0; // vtbl+0x18
    virtual void processContactImpl(const hkpCollidable&, const hkpCollidable&, const hkpProcessCollisionInput&, hkpProcessCollisionData&) = 0; // vtbl+0x1C
    virtual ToiAccept addToiImpl(const hkpCdBody&, const hkpCdBody&, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&, float, hkContactPoint&, const hkpGskCache*, float&, hkpContactPointProperties&) = 0; // vtbl+0x20
    virtual void removeToiImpl(hkpConstraintOwner&, hkpContactPointProperties&) = 0; // vtbl+0x24
    virtual void cleanup() {} // vtbl+0x28 0x00DB5700

    // the public interface (hkpcontactmgr.inl): forwards to the implementation
    unsigned short addContactPoint(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp) { return addContactPointImpl(a, b, input, output, contactCache, cp); } // 0x00D90CA0
    hkResult reserveContactPoints(int numPoints) { return reserveContactPointsImpl(numPoints); } // 0x00DB5850
    void removeContactPoint(unsigned short cpId, hkpConstraintOwner& constraintOwner) { removeContactPointImpl(cpId, constraintOwner); } // 0x00D8FAA0
    void processContact(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData) { processContactImpl(a, b, input, collisionData); } // 0x00DB5870
    ToiAccept addToi(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& propertiesOut) { return addToiImpl(a, b, input, output, toi, cp, gskCache, projectedVelocity, propertiesOut); } // 0x00DB5880
    void removeToi(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties) { removeToiImpl(constraintOwner, properties); } // 0x00DB5720

    Type m_type; // 0x08
};
