// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/rejectchassislistener/hkprejectchassislistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCollidable;

// size 0x10
class hkpRejectChassisListener : public hkReferencedObject, public hkpPhantomOverlapListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7F320
    static const hkClass& staticClass();
    hkpRejectChassisListener(hkFinishLoadedObjectFlag);
    hkpRejectChassisListener(); // 0x00F7F240
    virtual ~hkpRejectChassisListener(); // vtbl+0x00 0x00F83D90
    virtual void collidableAddedCallback(const hkpCollidableAddedEvent& event); // vtbl+0x00 0x00F83DA0
    virtual void collidableRemovedCallback(const hkpCollidableRemovedEvent& event); // vtbl+0x04 0x00F83D80

    static const hkInternalClassMember Members[];
    const hkpCollidable* m_chassis; // 0x0C
};
