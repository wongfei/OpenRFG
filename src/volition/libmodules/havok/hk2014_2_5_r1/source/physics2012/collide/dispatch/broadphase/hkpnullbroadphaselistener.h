// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpnullbroadphaselistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpbroadphaselistener.h"

class hkpTypedBroadPhaseHandlePair;

// size 0xC
class hkpNullBroadPhaseListener : public hkReferencedObject, public hkpBroadPhaseListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D9AD00
    hkpNullBroadPhaseListener(); // 0x00D9AC10
    virtual ~hkpNullBroadPhaseListener(); // vtbl+0x00 0x00D9AC40
    virtual void addCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x04 0x00DB1160
    virtual void removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x08 0x00DB1170

};
