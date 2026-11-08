// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the phantom base (listeners, broadphase update, bounding volume), from rfg.exe,
// with the out-of-line copies of hkpphantom.inl/.h.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

// 0x00DF4580: the collidable at +0x10 (owner offset -0x10)
hkpPhantom::hkpPhantom(const hkpShape* shape)
    : hkpWorldObject(shape, BROAD_PHASE_PHANTOM)
{
    m_collidable.m_ownerOffset = -0x10;
}

// 0x00DEE0C0
hkpPhantom::hkpPhantom(hkFinishLoadedObjectFlag flag)
    : hkpWorldObject(flag)
{
}

// 0x00E05610: the listeners hear of the deletion; the arrays free their storage
hkpPhantom::~hkpPhantom()
{
    firePhantomDeleted();
}

// 0x00811820 (hkpphantom.h)
void hkpPhantom::updateShapeCollectionFilter()
{
}

// 0x00E04FC0
void hkpPhantom::addPhantomListener(hkpPhantomListener* el)
{
    m_phantomListeners.pushBack(el);
}

// 0x00E04EE0: original bug kept: a listener that is not there removes index -1 (the whole array shifts down onto the
// slot before it)
void hkpPhantom::removePhantomListener(hkpPhantomListener* el)
{
    const int i = m_phantomListeners.indexOf(el);
    m_phantomListeners.removeAtAndCopy(i);
}

// 0x00E05000
void hkpPhantom::addPhantomOverlapListener(hkpPhantomOverlapListener* el)
{
    m_overlapListeners.pushBack(el);
}

// 0x00E04F50: as removePhantomListener
void hkpPhantom::removePhantomOverlapListener(hkpPhantomOverlapListener* el)
{
    const int i = m_overlapListeners.indexOf(el);
    m_overlapListeners.removeAtAndCopy(i);
}

// 0x00E04D60, 0x00E04DC0: listeners that removed themselves during a callback leave null slots
void hkpPhantom::removeNullOverlapListeners()
{
    for (int i = m_overlapListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_overlapListeners[i] == nullptr)
        {
            m_overlapListeners.removeAtAndCopy(i);
        }
    }
}

void hkpPhantom::removeNullPhantomListeners()
{
    for (int i = m_phantomListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_phantomListeners[i] == nullptr)
        {
            m_phantomListeners.removeAtAndCopy(i);
        }
    }
}

// 0x00E04D20: from the last listener (no cleanup: the phantom goes away)
void hkpPhantom::firePhantomDeleted()
{
    for (int i = m_phantomListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_phantomListeners[i] != nullptr)
        {
            m_phantomListeners[i]->phantomDeletedCallback(this);
        }
    }
}

// 0x00E04E20
void hkpPhantom::firePhantomRemoved()
{
    for (int i = m_phantomListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_phantomListeners[i] != nullptr)
        {
            m_phantomListeners[i]->phantomRemovedCallback(this);
        }
    }
    removeNullPhantomListeners();
}

// 0x00E04E60
void hkpPhantom::firePhantomAdded()
{
    for (int i = m_phantomListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_phantomListeners[i] != nullptr)
        {
            m_phantomListeners[i]->phantomAddedCallback(this);
        }
    }
    removeNullPhantomListeners();
}

// 0x00E04EA0
void hkpPhantom::firePhantomShapeSet()
{
    for (int i = m_phantomListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_phantomListeners[i] != nullptr)
        {
            m_phantomListeners[i]->phantomShapeSetCallback(this);
        }
    }
    removeNullPhantomListeners();
}

// 0x00DE05B0 (hkpphantom.inl): any listener may reject the collidable
hkpCollidableAccept hkpPhantom::fireCollidableAdded(const hkpCollidable* collidable)
{
    hkpCollidableAddedEvent event;
    event.m_collidableAccept = HK_COLLIDABLE_ACCEPT;
    event.m_phantom = this;
    event.m_collidable = collidable;
    for (int i = m_overlapListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_overlapListeners[i] != nullptr)
        {
            m_overlapListeners[i]->collidableAddedCallback(event);
        }
    }
    return event.m_collidableAccept;
}

// 0x00DE0910 (hkpphantom.inl)
void hkpPhantom::fireCollidableRemoved(const hkpCollidable* collidable, hkBool collidableWasAdded)
{
    hkpCollidableRemovedEvent event;
    event.m_phantom = this;
    event.m_collidable = collidable;
    event.m_collidableWasAdded = collidableWasAdded;
    for (int i = m_overlapListeners.getSize() - 1; i >= 0; i--)
    {
        if (m_overlapListeners[i] != nullptr)
        {
            m_overlapListeners[i]->collidableRemovedCallback(event);
        }
    }
}

// 0x00E05040: the box quantized into the collidable's bounding volume (no expansion)
void hkpPhantom::setBoundingVolumeData(const hkAabb& aabb)
{
    hkAabbUint32& bv = reinterpret_cast<hkAabbUint32&>(m_collidable.m_boundingVolumeData);
    const hkpCollisionInput* input = m_world->m_collisionInput;
    hkAabbUtil::convertAabbToUint32(aabb, input->m_aabb32Info.m_bitOffsetLow, input->m_aabb32Info.m_bitOffsetHigh, input->m_aabb32Info.m_bitScale, bv);
    hkAabbUtil::compressExpandedAabbUint32(bv, bv);
}

// 0x00E05250: frees the empty listener arrays
void hkpPhantom::deallocateInternalArrays()
{
    if (m_overlapListeners.getSize() == 0)
    {
        m_overlapListeners.clearAndDeallocate();
    }
    if (m_phantomListeners.getSize() == 0)
    {
        m_phantomListeners.clearAndDeallocate();
    }
}

// 0x00E052F0: queued (PHANTOM_UPDATE_BROAD_PHASE, with a pointer to the caller's box) while the world's critical
// operations are locked, also for phantoms only; else the broadphase moves the phantom and its new and lost pairs
// go to the dispatcher
void hkpPhantom::updateBroadPhase(const hkAabb& aabb)
{
    hkpWorld* world = m_world;
    if (world == nullptr)
    {
        return;
    }
    if (world->m_criticalOperationsLockCountForPhantoms + world->m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdatePhantomBP op;
        op.m_phantom = this;
        op.m_aabb = const_cast<hkAabb*>(&aabb);
        world->queueOperation(op);
        return;
    }
    world->lockCriticalOperations();
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(m_world->m_broadPhaseUpdateSize);
    hkLocalArray<hkpBroadPhaseHandlePair> delPairs(m_world->m_broadPhaseUpdateSize);
    hkpBroadPhaseHandle* handle = &m_collidable.m_broadPhaseHandle;
    m_world->m_broadPhase->lock();
    m_world->m_broadPhase->updateAabbs(&handle, &aabb, 1, newPairs, delPairs);
    if (newPairs.getSize() + delPairs.getSize() != 0)
    {
        hkpTypedBroadPhaseDispatcher::removeDuplicates(newPairs, delPairs);
        m_world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(delPairs.begin()), delPairs.getSize());
        const hkpCollidableCollidableFilter* filter = static_cast<const hkpCollidableCollidableFilter*>(m_world->m_collisionFilter);
        m_world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), filter);
        removeNullOverlapListeners();
    }
    m_world->m_broadPhase->unlock();
    setBoundingVolumeData(aabb);
    m_world->unlockAndAttemptToExecutePendingOperations();
}
