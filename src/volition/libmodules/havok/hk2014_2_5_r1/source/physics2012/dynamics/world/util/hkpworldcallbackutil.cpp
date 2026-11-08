// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the world's listener lists, from rfg.exe. Every list is walked from the last
// listener to the first; a listener that removes itself during its callback leaves a null slot, and the nulls are
// compacted afterwards (worldCallbackUtil_cleanupNullPointers).
//
// The shipped build records a monitor timer around each callback ("TtentAddCb", "TtpostSimCb", ... then "Et");
// they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpactionlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactimpulselimitbreachedlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentityactivationlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpislandactivationlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpislandpostcollidelistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpislandpostintegratelistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworlddeletionlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostcollidelistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostintegratelistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.h"

// 0x00E181B0 (LTCG: the array in ESI): removes the null slots, keeping the order
template<typename T>
static void worldCallbackUtil_cleanupNullPointers(hkArray<T*, hkContainerHeapAllocator>& cleanupArray)
{
    for (int i = cleanupArray.getSize() - 1; i >= 0; i--)
    {
        if (cleanupArray[i] == nullptr)
        {
            cleanupArray.removeAtAndCopy(i);
        }
    }
}

// 0x00E18160: the same for the entities' small arrays
template<typename T>
static void worldCallbackUtil_cleanupNullPointers(hkSmallArray<T*>& cleanupArray)
{
    for (int i = cleanupArray.getSize() - 1; i >= 0; i--)
    {
        if (cleanupArray[i] == nullptr)
        {
            cleanupArray.removeAtAndCopy(i);
        }
    }
}

// The common loop: from the last listener, skipping null slots, then the cleanup.
template<typename T, typename F>
static inline void worldCallbackUtil_fire(hkArray<T*, hkContainerHeapAllocator>& listeners, F call)
{
    for (int i = listeners.getSize() - 1; i >= 0; i--)
    {
        if (listeners[i] != nullptr)
        {
            call(listeners[i]);
        }
    }
    worldCallbackUtil_cleanupNullPointers(listeners);
}

// 0x00E18200
void hkpWorldCallbackUtil::fireActionAdded(hkpWorld* world, hkpAction* action)
{
    worldCallbackUtil_fire(world->m_actionListeners, [&](hkpActionListener* l) { l->actionAddedCallback(action); });
}

// 0x00E182A0
void hkpWorldCallbackUtil::fireActionRemoved(hkpWorld* world, hkpAction* action)
{
    worldCallbackUtil_fire(world->m_actionListeners, [&](hkpActionListener* l) { l->actionRemovedCallback(action); });
}

// 0x00E18340
void hkpWorldCallbackUtil::fireEntityAdded(hkpWorld* world, hkpEntity* entity)
{
    worldCallbackUtil_fire(world->m_entityListeners, [&](hkpEntityListener* l) { l->entityAddedCallback(entity); });
}

// 0x00E183E0
void hkpWorldCallbackUtil::fireEntityRemoved(hkpWorld* world, hkpEntity* entity)
{
    worldCallbackUtil_fire(world->m_entityListeners, [&](hkpEntityListener* l) { l->entityRemovedCallback(entity); });
}

// 0x00E18480
void hkpWorldCallbackUtil::fireEntityShapeSet(hkpWorld* world, hkpEntity* entity)
{
    worldCallbackUtil_fire(world->m_entityListeners, [&](hkpEntityListener* l) { l->entityShapeSetCallback(entity); });
}

// 0x00E18520
void hkpWorldCallbackUtil::fireEntitySetMotionType(hkpWorld* world, hkpEntity* entity)
{
    worldCallbackUtil_fire(world->m_entityListeners, [&](hkpEntityListener* l) { l->entitySetMotionTypeCallback(entity); });
}

// 0x00E185C0
void hkpWorldCallbackUtil::firePhantomAdded(hkpWorld* world, hkpPhantom* phantom)
{
    worldCallbackUtil_fire(world->m_phantomListeners, [&](hkpPhantomListener* l) { l->phantomAddedCallback(phantom); });
}

// 0x00E18660
void hkpWorldCallbackUtil::firePhantomRemoved(hkpWorld* world, hkpPhantom* phantom)
{
    worldCallbackUtil_fire(world->m_phantomListeners, [&](hkpPhantomListener* l) { l->phantomRemovedCallback(phantom); });
}

// 0x00E18700
void hkpWorldCallbackUtil::firePhantomShapeSet(hkpWorld* world, hkpPhantom* phantom)
{
    worldCallbackUtil_fire(world->m_phantomListeners, [&](hkpPhantomListener* l) { l->phantomShapeSetCallback(phantom); });
}

// 0x00E187A0
void hkpWorldCallbackUtil::fireConstraintAdded(hkpWorld* world, hkpConstraintInstance* constraint)
{
    worldCallbackUtil_fire(world->m_constraintListeners, [&](hkpConstraintListener* l) { l->constraintAddedCallback(constraint); });
}

// 0x00E18840
void hkpWorldCallbackUtil::fireConstraintRemoved(hkpWorld* world, hkpConstraintInstance* constraint)
{
    worldCallbackUtil_fire(world->m_constraintListeners, [&](hkpConstraintListener* l) { l->constraintRemovedCallback(constraint); });
}

// 0x00E188E0
void hkpWorldCallbackUtil::fireConstraintViolated(hkpWorld* world, hkpConstraintInstance* constraint)
{
    worldCallbackUtil_fire(world->m_constraintListeners, [&](hkpConstraintListener* l) { l->constraintViolatedCallback(constraint); });
}

// 0x00E18980
void hkpWorldCallbackUtil::fireConstraintBroken(hkpWorld* world, const hkpConstraintBrokenEvent& event)
{
    worldCallbackUtil_fire(world->m_constraintListeners, [&](hkpConstraintListener* l) { l->constraintBreakingCallback(event); });
}

// 0x00E18A10
void hkpWorldCallbackUtil::fireConstraintRepaired(hkpWorld* world, const hkpConstraintRepairedEvent& event)
{
    worldCallbackUtil_fire(world->m_constraintListeners, [&](hkpConstraintListener* l) { l->constraintRepairedCallback(event); });
}

// 0x00E18AA0
void hkpWorldCallbackUtil::fireContactPointCallback(hkpWorld* world, hkpContactPointEvent& event)
{
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->contactPointCallback(event); });
}

// 0x00E18B40
void hkpWorldCallbackUtil::fireContactConstraintAddedCallback(hkpWorld* world, hkpCollisionEvent& event)
{
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->collisionAddedCallback(event); });
}

// 0x00E18BE0
void hkpWorldCallbackUtil::fireContactConstraintRemovedCallback(hkpWorld* world, hkpCollisionEvent& event)
{
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->collisionRemovedCallback(event); });
}

// 0x00E18C80
void hkpWorldCallbackUtil::fireWorldDeleted(hkpWorld* world)
{
    worldCallbackUtil_fire(world->m_worldDeletionListeners, [&](hkpWorldDeletionListener* l) { l->worldDeletedCallback(world); });
}

// 0x00E18D10
void hkpWorldCallbackUtil::fireWorldRemoveAll(hkpWorld* world)
{
    worldCallbackUtil_fire(world->m_worldDeletionListeners, [&](hkpWorldDeletionListener* l) { l->worldRemoveAllCallback(world); });
}

// The island activation callbacks run with the critical operations locked: the island's listeners, then the
// activation listeners of each of its entities.
template<typename F, typename G>
static void worldCallbackUtil_fireIslandActivation(hkpWorld* world, hkpSimulationIsland* island, F islandCall, G entityCall)
{
    world->lockCriticalOperations();
    worldCallbackUtil_fire(world->m_islandActivationListeners, islandCall);
    for (int e = 0; e < island->m_entities.getSize(); e++)
    {
        hkpEntity* entity = island->m_entities[e];
        hkpEntity::ExtendedListeners* extended = entity->m_extendedListeners;
        if (extended == nullptr)
        {
            continue;
        }
        hkSmallArray<hkpEntityActivationListener*>& listeners = extended->m_activationListeners;
        for (int i = listeners.getSize() - 1; i >= 0; i--)
        {
            if (listeners[i] != nullptr)
            {
                entityCall(listeners[i], island->m_entities[e]);
            }
        }
        worldCallbackUtil_cleanupNullPointers(listeners);
    }
    world->unlockAndAttemptToExecutePendingOperations();
}

// 0x00E18DA0
void hkpWorldCallbackUtil::fireIslandActivated(hkpWorld* world, hkpSimulationIsland* island)
{
    worldCallbackUtil_fireIslandActivation(world, island,
        [&](hkpIslandActivationListener* l) { l->islandActivatedCallback(island); },
        [&](hkpEntityActivationListener* l, hkpEntity* entity) { l->entityActivatedCallback(entity); });
}

// 0x00E18F50
void hkpWorldCallbackUtil::fireIslandDeactivated(hkpWorld* world, hkpSimulationIsland* island)
{
    worldCallbackUtil_fireIslandActivation(world, island,
        [&](hkpIslandActivationListener* l) { l->islandDeactivatedCallback(island); },
        [&](hkpEntityActivationListener* l, hkpEntity* entity) { l->entityDeactivatedCallback(entity); });
}

// 0x00E19100: a listener may remove others too; when the list shrank, the index moves down with it
void hkpWorldCallbackUtil::firePostSimulationCallback(hkpWorld* world)
{
    hkArray<hkpWorldPostSimulationListener*, hkContainerHeapAllocator>& listeners = world->m_worldPostSimulationListeners;
    int size = listeners.getSize();
    for (int i = size - 1; i >= 0; i--)
    {
        if (size > listeners.getSize())
        {
            i += listeners.getSize() - size;
            size = listeners.getSize();
        }
        if (listeners[i] != nullptr)
        {
            listeners[i]->postSimulationCallback(world);
        }
    }
    worldCallbackUtil_cleanupNullPointers(listeners);
}

// 0x00E191B0
void hkpWorldCallbackUtil::firePostIntegrateCallback(hkpWorld* world, const hkStepInfo& info)
{
    worldCallbackUtil_fire(world->m_worldPostIntegrateListeners, [&](hkpWorldPostIntegrateListener* l) { l->postIntegrateCallback(world, info); });
}

// 0x00E19250
void hkpWorldCallbackUtil::firePostCollideCallback(hkpWorld* world, const hkStepInfo& info)
{
    worldCallbackUtil_fire(world->m_worldPostCollideListeners, [&](hkpWorldPostCollideListener* l) { l->postCollideCallback(world, info); });
}

// 0x00E192F0
void hkpWorldCallbackUtil::fireIslandPostIntegrateCallback(hkpWorld* world, hkpSimulationIsland* island, const hkStepInfo& info)
{
    worldCallbackUtil_fire(world->m_islandPostIntegrateListeners, [&](hkpIslandPostIntegrateListener* l) { l->postIntegrateCallback(island, info); });
}

// 0x00E19390
void hkpWorldCallbackUtil::fireIslandPostCollideCallback(hkpWorld* world, hkpSimulationIsland* island, const hkStepInfo& info)
{
    worldCallbackUtil_fire(world->m_islandPostCollideListeners, [&](hkpIslandPostCollideListener* l) { l->postCollideCallback(island, info); });
}

// 0x00E19430
void hkpWorldCallbackUtil::fireContactImpulseLimitBreached(hkpWorld* world, const hkpContactImpulseLimitBreachedListenerInfo* breachedContacts, int numBreachedContacts)
{
    worldCallbackUtil_fire(world->m_contactImpulseLimitBreachedListeners, [&](hkpContactImpulseLimitBreachedListener* l) { l->contactImpulseLimitBreachedCallback(breachedContacts, numBreachedContacts); });
}

// 0x00E194D0: the post-simulation listeners hear of an inactive entity that was moved
void hkpWorldCallbackUtil::fireInactiveEntityMoved(hkpWorld* world, hkpEntity* entity)
{
    worldCallbackUtil_fire(world->m_worldPostSimulationListeners, [&](hkpWorldPostSimulationListener* l) { l->inactiveEntityMovedCallback(entity); });
}

// 0x00E19570: fired from the world (no entity)
void hkpWorldCallbackUtil::fireContactPointAdded(hkpWorld* world, hkpContactPointAddedEvent& event)
{
    event.m_callbackFiredFrom = nullptr;
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->contactPointAddedCallback(event); });
}

// 0x00E19610
void hkpWorldCallbackUtil::fireContactPointRemoved(hkpWorld* world, hkpContactPointRemovedEvent& event)
{
    event.m_callbackFiredFrom = nullptr;
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->contactPointRemovedCallback(event); });
}

// 0x00E196B0
void hkpWorldCallbackUtil::fireContactProcess(hkpWorld* world, hkpContactProcessEvent& event)
{
    event.m_callbackFiredFrom = nullptr;
    worldCallbackUtil_fire(world->m_contactListeners, [&](hkpContactListener* l) { l->contactProcessCallback(event); });
}
