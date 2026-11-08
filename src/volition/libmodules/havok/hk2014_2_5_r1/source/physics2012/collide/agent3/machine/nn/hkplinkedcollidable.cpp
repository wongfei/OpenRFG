// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the collision entries of a linked collidable (its Nn agents and their
// partners), from rfg.exe; the constructors and destructor since HK7.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"

// 0x0104BF70: by the partners' broadphase handle ids
static bool cmpLessCollisionEntries(const hkpLinkedCollidable::CollisionEntry& entry0, const hkpLinkedCollidable::CollisionEntry& entry1)
{
    return entry0.m_partner->m_broadPhaseHandle.m_id < entry1.m_partner->m_broadPhaseHandle.m_id;
}

// 0x0104BFE0: a copy of the entries, sorted by partner when the owner is a fixed body
void hkpLinkedCollidable::getCollisionEntriesSorted(hkArray<CollisionEntry, hkContainerHeapAllocator>& entries) const
{
    entries = m_collisionEntries;
    // the owner read as an entity (its motion type at +0xE8)
    const hkpEntity* owner = static_cast<const hkpEntity*>(getOwner());
    if (owner->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED && entries.getSize() > 1)
    {
        hkAlgorithm::quickSortRecursive(entries.begin(), 0, entries.getSize() - 1, cmpLessCollisionEntries);
    }
}

// 0x0104BF60
hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& hkpLinkedCollidable::getCollisionEntriesDeterministicUnchecked()
{
    return m_collisionEntries;
}

// 0x0104BF50
const hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& hkpLinkedCollidable::getCollisionEntriesDeterministicUnchecked() const
{
    return m_collisionEntries;
}

// 0x0104BF90: sorted by partner, the entries' agent indices renumbered
void hkpLinkedCollidable::sortEntries()
{
    if (m_collisionEntries.getSize() > 1)
    {
        hkAlgorithm::quickSortRecursive(m_collisionEntries.begin(), 0, m_collisionEntries.getSize() - 1, cmpLessCollisionEntries);
    }
    for (int i = 0; i < m_collisionEntries.getSize(); i++)
    {
        hkpAgentNnEntry* entry = m_collisionEntries[i].m_agentEntry;
        entry->m_agentIndexOnCollidable[this == entry->m_collidable[1] ? 1 : 0] = (unsigned short)i;
    }
}

// 0x00DF4B40: no collision entries yet
hkpLinkedCollidable::hkpLinkedCollidable(const hkpShape* shape, const hkMotionState* ms, int type)
    : hkpCollidable(shape, ms, type)
{
}

// 0x00D7DB00
hkpLinkedCollidable::hkpLinkedCollidable(hkFinishLoadedObjectFlag flag)
    : hkpCollidable(flag)
{
}

// 0x00D7DB40: the entry array frees its storage
hkpLinkedCollidable::~hkpLinkedCollidable()
{
}
