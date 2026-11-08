// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMotionState;
struct hkpAgentNnEntry;
class hkpShape;

// size 0x5C
class hkpLinkedCollidable : public hkpCollidable
{
public:
    // size 0x8
    struct CollisionEntry
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CollisionEntry);

        hkpAgentNnEntry* m_agentEntry; // 0x00
        hkpLinkedCollidable* m_partner; // 0x04
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpLinkedCollidable); // new(unsigned int n, void* p) 0x00D7D910, delete(void* p, unsigned int nbytes) 0x00D7D9A0
    static const hkClass& staticClass(); // 0x00D7D800
    hkpLinkedCollidable(hkFinishLoadedObjectFlag flag); // 0x00D7DB00
    hkpLinkedCollidable(const hkpShape* shape, const hkMotionState* ms, int type); // 0x00DF4B40
    ~hkpLinkedCollidable(); // 0x00D7DB40
    void getCollisionEntriesSorted(hkArray<CollisionEntry,hkContainerHeapAllocator>& entries) const; // 0x0104BFE0
    hkArray<CollisionEntry,hkContainerHeapAllocator>& getCollisionEntriesDeterministicUnchecked(); // 0x0104BF60
    const hkArray<CollisionEntry,hkContainerHeapAllocator>& getCollisionEntriesDeterministicUnchecked() const; // 0x0104BF50
    const hkArray<CollisionEntry,hkContainerHeapAllocator>& getCollisionEntriesNonDeterministic() const; // 0x00DDCC60
    hkArray<CollisionEntry,hkContainerHeapAllocator>& getCollisionEntriesNonDeterministic(); // 0x00DDCC50
    void sortEntries(); // 0x0104BF90

    static const hkInternalClassMember Members[]; // 0x01355898
public:
    hkArray<CollisionEntry,hkContainerHeapAllocator> m_collisionEntries; // 0x50
};
