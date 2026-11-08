// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpendofstepcallbackutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK9): the end-of-step contact point callbacks, from rfg.exe. Listeners register a
// collision (a contact manager and the listener, for one side or the world); after each simulation step every
// registered collision whose island is active gets a TYPE_MANIFOLD_AT_END_OF_STEP event per contact point.
// rfg.exe never registers one (hkpContactListener::registerForEndOfStepContactPointCallbacks has no caller), and the
// world creates the util only with m_fireCollisionCallbacks, which the game leaves off.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpendofstepcallbackutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"

// 0x00E1E2E0
hkBool hkpEndOfStepCallbackUtil::Collision::operator==(const Collision& other) const
{
    return other.m_mgr == m_mgr && other.m_listener == m_listener && other.m_source == m_source;
}

// 0x00E1F0E0: the entities' uids, A's in the high word
uint64_t hkpEndOfStepCallbackUtil::Collision::getUid() const
{
    return ((uint64_t)m_mgr->m_constraint.m_entities[0]->m_uid << 32) | m_mgr->m_constraint.m_entities[1]->m_uid;
}

// 0x00E1F100: by entity uids
hkBool hkpEndOfStepCallbackUtil::Collision::operator<(const Collision& other) const
{
    const unsigned int a0 = m_mgr->m_constraint.m_entities[0]->m_uid;
    const unsigned int b0 = other.m_mgr->m_constraint.m_entities[0]->m_uid;
    return a0 <= b0 && (a0 < b0 || m_mgr->m_constraint.m_entities[1]->m_uid < other.m_mgr->m_constraint.m_entities[1]->m_uid);
}

// 0x00E1F170: by entity uids, then by registration order
hkBool hkpEndOfStepCallbackUtil::NewCollision::operator<(const NewCollision& other) const
{
    const unsigned int a1 = m_mgr->m_constraint.m_entities[1]->m_uid;
    const unsigned int a0 = m_mgr->m_constraint.m_entities[0]->m_uid;
    const unsigned int b0 = other.m_mgr->m_constraint.m_entities[0]->m_uid;
    const unsigned int b1 = other.m_mgr->m_constraint.m_entities[1]->m_uid;
    if (b0 <= a0 && (b0 < a0 || b1 <= a1) && (a1 != b1 || a0 != b0 || other.m_sequenceNumber <= m_sequenceNumber))
    {
        return false;
    }
    return true;
}

// 0x00E1E880
hkpEndOfStepCallbackUtil::hkpEndOfStepCallbackUtil()
{
    m_sequenceNumber = 0;
    m_deterministicOrder = false;
}

// 0x00DED650
hkpEndOfStepCallbackUtil::~hkpEndOfStepCallbackUtil()
{
}

// 0x00E1E420
void hkpEndOfStepCallbackUtil::registerCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source)
{
    NewCollision& c = m_newCollisions.expandOne();
    c.m_mgr = mgr;
    c.m_listener = listener;
    c.m_source = source;
    c.m_sequenceNumber = m_sequenceNumber++;
}

// 0x00E1E480
void hkpEndOfStepCallbackUtil::unregisterCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source)
{
    Collision& c = m_removedCollisions.expandOne();
    c.m_mgr = mgr;
    c.m_listener = listener;
    c.m_source = source;
}

// 0x00E1E310: the points of the manifold, last first; only the new ones unless the constraint asked for contact
// point callbacks on the full manifold
void hkpEndOfStepCallbackUtil::fireContactPointEventsForCollision(hkpSimpleConstraintContactMgr* mgr, hkpContactListener* listener, hkpCollisionEvent::CallbackSource source)
{
    hkpRigidBody* bodyA = static_cast<hkpRigidBody*>(mgr->getConstraintInstance()->m_entities[0]);
    hkpRigidBody* bodyB = static_cast<hkpRigidBody*>(mgr->getConstraintInstance()->m_entities[1]);
    hkpSimpleContactConstraintAtom* atom = mgr->m_contactConstraintData.m_atom;
    const int numContactPoints = atom->m_numContactPoints;
    const int striding = atom->getContactPointPropertiesStriding();
    hkpContactPointProperties* props = reinterpret_cast<hkpContactPointProperties*>(atom->getContactPointPropertiesStream());
    hkContactPoint* cp = atom->getContactPoints();
    const bool fullManifold = (mgr->getConstraintInstance()->m_internal->m_callbackRequest & hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK) != 0;
    const int last = numContactPoints - 1;
    for (int i = last; i >= 0; i--)
    {
        if ((props->m_flags & hkContactPointMaterial::CONTACT_IS_NEW) != 0 || fullManifold)
        {
            // the extended user datas follow the properties
            hkpContactPointEvent event(source, bodyA, bodyB, mgr, hkpContactPointEvent::TYPE_MANIFOLD_AT_END_OF_STEP, cp, props, nullptr, nullptr, fullManifold, i == last, i == 0,
                                       reinterpret_cast<unsigned int*>(props + 1), nullptr, nullptr);
            listener->contactPointCallback(event);
        }
        props = reinterpret_cast<hkpContactPointProperties*>(reinterpret_cast<char*>(props) + striding);
        cp++;
    }
}

namespace
{
// 0x00E1E4D0 / 0x00E1E6B0: target without the collisions in removals; each removal is used once (removed from
// removals, the last one moved into its place). Removals that match nothing stay for the next step.
template<typename T>
void stripArray(hkArray<hkpEndOfStepCallbackUtil::Collision>& removals, hkArray<T>& target)
{
    const int targetSize = target.getSize();
    const int numRemovals = removals.getSize();
    if (numRemovals == 0)
    {
        return;
    }
    hkArray<T> kept(targetSize);
    int numKept = 0;
    for (int i = 0; i < targetSize; i++)
    {
        const T& c = target[i];
        int found = -1;
        for (int r = 0; r < removals.getSize(); r++)
        {
            if (removals[r] == c)
            {
                found = r;
                break;
            }
        }
        if (found != -1)
        {
            removals.removeAt(found);
            continue;
        }
        kept[numKept++] = c;
    }
    const int newSize = targetSize - numRemovals + removals.getSize();
    kept.reserve(newSize);
    kept.setSizeUnchecked(newSize);
    target.swap(kept);
}

// 0x00E1E8D0: merges the sorted new collisions into the sorted collisions (an old one first on equal uids)
void mergeArrays(hkArray<hkpEndOfStepCallbackUtil::NewCollision>& source, hkArray<hkpEndOfStepCallbackUtil::Collision>& target)
{
    const hkpEndOfStepCallbackUtil::NewCollision* n = source.begin();
    const hkpEndOfStepCallbackUtil::NewCollision* nEnd = n + source.getSize();
    if (n == nEnd)
    {
        return;
    }
    const int total = target.getSize() + source.getSize();
    hkArray<hkpEndOfStepCallbackUtil::Collision> merged(total);
    const hkpEndOfStepCallbackUtil::Collision* o = target.begin();
    const hkpEndOfStepCallbackUtil::Collision* oEnd = o + target.getSize();
    hkpEndOfStepCallbackUtil::Collision* out = merged.begin();
    while (n < nEnd && o < oEnd)
    {
        const unsigned int new0 = n->m_mgr->m_constraint.m_entities[0]->m_uid;
        const unsigned int new1 = n->m_mgr->m_constraint.m_entities[1]->m_uid;
        const unsigned int old0 = o->m_mgr->m_constraint.m_entities[0]->m_uid;
        if (old0 < new0 || (old0 <= new0 && o->m_mgr->m_constraint.m_entities[1]->m_uid <= new1))
        {
            *out++ = *o++;
        }
        else
        {
            *out++ = *n++;
        }
    }
    while (n < nEnd)
    {
        *out++ = *n++;
    }
    while (o < oEnd)
    {
        *out++ = *o++;
    }
    target.swap(merged);
    source.setSizeUnchecked(0);
}
} // namespace

// 0x00E1EA90: drops the unregistered collisions, adds the new ones (sorted by uid with m_deterministicOrder, else in
// registration order), fires the events of the collisions in active islands and forgets the collisions whose side
// entity no longer has the listener. The monitor timer "TtEndOfStepCbs" is not reproduced.
void hkpEndOfStepCallbackUtil::postSimulationCallback(hkpWorld* world)
{
    stripArray(m_removedCollisions, m_newCollisions);
    stripArray(m_removedCollisions, m_collisions);
    if (!m_deterministicOrder.m_bool)
    {
        const int oldSize = m_collisions.getSize();
        const int numNew = m_newCollisions.getSize();
        m_collisions.reserve(oldSize + numNew);
        m_collisions.setSizeUnchecked(oldSize + numNew);
        for (int i = 0; i < m_newCollisions.getSize(); i++)
        {
            m_collisions[oldSize + i] = m_newCollisions[i];
        }
        m_newCollisions.setSizeUnchecked(0);
    }
    else
    {
        if (m_newCollisions.getSize() > 1)
        {
            hkAlgorithm::quickSortRecursive(m_newCollisions.begin(), 0, m_newCollisions.getSize() - 1, hkAlgorithm::less<NewCollision>());
        }
        mergeArrays(m_newCollisions, m_collisions);
    }

    const int numCollisions = m_collisions.getSize();
    bool removed = false;
    for (int i = 0; i < numCollisions; i++)
    {
        Collision& c = m_collisions[i];
        hkpConstraintInstance* constraint = c.m_mgr->getConstraintInstance();
        hkpEntity* entity = nullptr;
        bool listening = true;
        if (c.m_source == hkpCollisionEvent::SOURCE_A || c.m_source == hkpCollisionEvent::SOURCE_B)
        {
            entity = constraint->m_entities[c.m_source == hkpCollisionEvent::SOURCE_A ? 0 : 1];
            if (entity != nullptr)
            {
                listening = false;
                for (int l = 0; l < entity->m_contactListeners.getSize(); l++)
                {
                    if (entity->m_contactListeners[l] == c.m_listener)
                    {
                        listening = true;
                        break;
                    }
                }
            }
        }
        if (!listening)
        {
            removed = true;
            c.m_listener = nullptr;
            continue;
        }
        if (constraint->getSimulationIsland()->m_activeMark)
        {
            fireContactPointEventsForCollision(c.m_mgr, c.m_listener, c.m_source);
        }
    }
    if (removed)
    {
        // the collisions still listened to, into an array of the old size
        const int oldSize = m_collisions.getSize();
        hkArray<Collision> kept(oldSize);
        int numKept = 0;
        for (int i = 0; i < oldSize; i++)
        {
            if (m_collisions[i].m_listener != nullptr)
            {
                kept[numKept++] = m_collisions[i];
            }
        }
        kept.reserve(numKept);
        kept.setSizeUnchecked(numKept);
        m_collisions.swap(kept);
    }
    m_sequenceNumber = 0;
}
