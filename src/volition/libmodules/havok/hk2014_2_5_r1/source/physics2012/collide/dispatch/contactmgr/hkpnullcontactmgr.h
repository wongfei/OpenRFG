// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"

class hkContactPoint;
struct hkResult;
class hkpCdBody;
class hkpCollidable;
class hkpConstraintOwner;
class hkpContactPointProperties;
class hkpGskCache;
struct hkpProcessCollisionData;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0xC
class hkpNullContactMgr : public hkpContactMgr
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DE9EF0
    hkpNullContactMgr(); // 0x00DE85C0
    virtual ~hkpNullContactMgr(); // vtbl+0x00 0x00DE8650
    virtual unsigned short addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& env, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp); // vtbl+0x10 0x00DE85E0
    virtual hkResult reserveContactPointsImpl(int numPoints); // vtbl+0x14 0x00DE8630
    virtual void removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& constraintOwner); // vtbl+0x18 0x00DE85F0
    virtual void processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData); // vtbl+0x1C 0x00DE8600
    virtual hkpContactMgr::ToiAccept addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties); // vtbl+0x20 0x00DE8610
    virtual void removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties); // vtbl+0x24 0x00DE8620
    virtual void cleanup(); // vtbl+0x28 0x00DE8640

};
