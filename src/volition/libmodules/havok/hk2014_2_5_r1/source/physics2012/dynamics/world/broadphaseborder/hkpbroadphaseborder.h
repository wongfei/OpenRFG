// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworlddeletionlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.h"

struct hkpCollidableAddedEvent;
struct hkpCollidableRemovedEvent;
class hkpEntity;
class hkpPhantom;
class hkpWorld;

// size 0x44
class hkpBroadPhaseBorder : public hkReferencedObject, protected hkpWorldDeletionListener, protected hkpPhantomOverlapListener, protected hkpWorldPostSimulationListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00814820, delete(void* p, unsigned int nbytes) 0x00814840
    hkpBroadPhaseBorder(hkpWorld* world, hkpWorldCinfo::BroadPhaseBorderBehaviour type, hkBool postponeAndSortCallbacks); // 0x00DF3100
    virtual ~hkpBroadPhaseBorder(); // vtbl+0x00 0x00DF34A0
    virtual void collidableAddedCallback(const hkpCollidableAddedEvent& event); // vtbl+0x00 0x00DF3090
    virtual void collidableRemovedCallback(const hkpCollidableRemovedEvent& event); // vtbl+0x04 0x00DF2EC0
    virtual void postSimulationCallback(hkpWorld* world); // vtbl+0x04 0x00DF3020
    virtual void worldDeletedCallback(hkpWorld* world); // vtbl+0x04 0x00DF2F30
    virtual void maxPositionExceededCallback(hkpEntity* entity); // vtbl+0x10 0x00DF2EF0
    virtual void deactivate(); // vtbl+0x14 0x00DF2F70

    hkpWorld* m_world; // 0x14
    hkpPhantom* m_phantoms[6]; // 0x18
    hkpWorldCinfo::BroadPhaseBorderBehaviour m_type; // 0x30
    hkBool m_postponeAndSortCallbacks; // 0x34
    hkArray<hkpEntity*,hkContainerHeapAllocator> m_entitiesExitingBroadPhase; // 0x38
};
