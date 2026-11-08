// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): an entity's own listeners, from rfg.exe. Walked from the last listener; the
// entity listeners are not compacted afterwards, the contact listeners are. The shipped build's monitor timers
// ("TtentAddCb", ... "Et") are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h"

// 0x00E159C0 (LTCG: the array in EAX): removes the null slots, keeping the order
static void hkpEntityCallbackUtil_cleanupNullPointers(hkSmallArray<hkpContactListener*>& cleanupArray)
{
    for (int i = cleanupArray.getSize() - 1; i >= 0; i--)
    {
        if (cleanupArray[i] == nullptr)
        {
            cleanupArray.removeAtAndCopy(i);
        }
    }
}

template<typename F>
static inline void hkpEntityCallbackUtil_fireEntityListeners(hkpEntity* entity, F call)
{
    hkpEntity::ExtendedListeners* extended = entity->m_extendedListeners;
    if (extended == nullptr)
    {
        return;
    }
    for (int i = extended->m_entityListeners.getSize() - 1; i >= 0; i--)
    {
        if (extended->m_entityListeners[i] != nullptr)
        {
            call(extended->m_entityListeners[i]);
        }
    }
}

template<typename F>
static inline void hkpEntityCallbackUtil_fireContactListeners(hkpEntity* entity, F call)
{
    for (int i = entity->m_contactListeners.getSize() - 1; i >= 0; i--)
    {
        if (entity->m_contactListeners[i] != nullptr)
        {
            call(entity->m_contactListeners[i]);
        }
    }
    hkpEntityCallbackUtil_cleanupNullPointers(entity->m_contactListeners);
}

// 0x00E15A10
void hkpEntityCallbackUtil::fireEntityAdded(hkpEntity* entity)
{
    hkpEntityCallbackUtil_fireEntityListeners(entity, [&](hkpEntityListener* l) { l->entityAddedCallback(entity); });
}

// 0x00E15AB0: the listeners, then every constraint (masters, then slaves in a deterministic order) and action of
// the entity is told; each removes itself from the entity's arrays, so the first element is taken until none is
// left
void hkpEntityCallbackUtil::fireEntityRemoved(hkpEntity* entity)
{
    hkpEntityCallbackUtil_fireEntityListeners(entity, [&](hkpEntityListener* l) { l->entityRemovedCallback(entity); });
    while (entity->m_constraintsMaster.getSize() != 0)
    {
        entity->m_constraintsMaster[0].m_constraint->entityRemovedCallback(entity);
    }
    entity->sortConstraintsSlavesDeterministically();
    while (entity->m_constraintsSlave.getSize() != 0)
    {
        hkpConstraintInstance* constraint = entity->m_constraintsSlave[0];
        constraint->entityRemovedCallback(entity);
    }
    while (entity->m_actions.getSize() != 0)
    {
        entity->m_actions[0]->entityRemovedCallback(entity);
    }
}

// 0x00E15CB0
void hkpEntityCallbackUtil::fireEntityShapeSet(hkpEntity* entity)
{
    hkpEntityCallbackUtil_fireEntityListeners(entity, [&](hkpEntityListener* l) { l->entityShapeSetCallback(entity); });
}

// 0x00E15D50
void hkpEntityCallbackUtil::fireEntitySetMotionType(hkpEntity* entity)
{
    hkpEntityCallbackUtil_fireEntityListeners(entity, [&](hkpEntityListener* l) { l->entitySetMotionTypeCallback(entity); });
}

// 0x00E15DF0
void hkpEntityCallbackUtil::fireEntityDeleted(hkpEntity* entity)
{
    hkpEntityCallbackUtil_fireEntityListeners(entity, [&](hkpEntityListener* l) { l->entityDeletedCallback(entity); });
}

// 0x00E15E90
void hkpEntityCallbackUtil::fireContactPointCallbackInternal(hkpEntity* entity, hkpContactPointEvent& event)
{
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->contactPointCallback(event); });
}

// 0x00E15F40
void hkpEntityCallbackUtil::fireContactConstraintAddedCallback(hkpEntity* entity, hkpCollisionEvent& event)
{
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->collisionAddedCallback(event); });
}

// 0x00E15FF0
void hkpEntityCallbackUtil::fireContactConstraintRemovedCallback(hkpEntity* entity, hkpCollisionEvent& event)
{
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->collisionRemovedCallback(event); });
}

// 0x00E160A0
void hkpEntityCallbackUtil::fireContactPointAddedInternal(hkpEntity* entity, hkpContactPointAddedEvent& event)
{
    event.m_callbackFiredFrom = entity;
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->contactPointAddedCallback(event); });
}

// 0x00E16140
void hkpEntityCallbackUtil::fireContactPointRemovedInternal(hkpEntity* entity, hkpContactPointRemovedEvent& event)
{
    event.m_callbackFiredFrom = entity;
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->contactPointRemovedCallback(event); });
}

// 0x00E161E0
void hkpEntityCallbackUtil::fireContactProcessInternal(hkpEntity* entity, hkpContactProcessEvent& event)
{
    event.m_callbackFiredFrom = entity;
    hkpEntityCallbackUtil_fireContactListeners(entity, [&](hkpContactListener* l) { l->contactProcessCallback(event); });
}
