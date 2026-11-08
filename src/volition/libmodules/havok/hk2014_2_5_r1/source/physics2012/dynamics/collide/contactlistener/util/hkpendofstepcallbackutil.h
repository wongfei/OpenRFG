// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpendofstepcallbackutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcollisionevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.h"

class hkpContactListener;
class hkpSimpleConstraintContactMgr;
class hkpWorld;

// size 0x38
class hkpEndOfStepCallbackUtil : public hkReferencedObject, public hkpWorldPostSimulationListener
{
public:
    // size 0xC
    struct Collision
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Collision);
        hkBool operator==(const Collision& other) const; // 0x00E1E2E0
        uint64_t getUid() const; // 0x00E1F0E0
        hkBool operator<(const Collision& other) const; // 0x00E1F100

        hkpSimpleConstraintContactMgr* m_mgr; // 0x00
        hkpContactListener* m_listener; // 0x04
        hkpCollisionEvent::CallbackSource m_source; // 0x08
    };

    // size 0x10
    struct NewCollision : public Collision
    {
        hkBool operator<(const NewCollision& other) const; // 0x00E1F170

        int m_sequenceNumber; // 0x0C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E1F860
    void registerCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source); // 0x00E1E420
    void unregisterCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source); // 0x00E1E480
    hkpEndOfStepCallbackUtil(); // 0x00E1E880
    static void fireContactPointEventsForCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source); // 0x00E1E310
    virtual ~hkpEndOfStepCallbackUtil(); // vtbl+0x00 0x00DED650
    virtual void postSimulationCallback(hkpWorld* world); // vtbl+0x04 0x00E1EA90

    int m_sequenceNumber; // 0x0C
    hkBool m_deterministicOrder; // 0x10
    hkArray<Collision,hkContainerHeapAllocator> m_collisions; // 0x14
    hkArray<NewCollision,hkContainerHeapAllocator> m_newCollisions; // 0x20
    hkArray<Collision,hkContainerHeapAllocator> m_removedCollisions; // 0x2C
};
