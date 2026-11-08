// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): a pair filter that disables collisions between the two bodies of each
// constraint (contact constraints excepted) while the constraint is in the world, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"

// hkWorldOperation::ConstraintFilterBrokenEvent (type 37): queued while the world is locked.
struct hkpConstraintFilterBrokenEventOperation
{
    unsigned char m_type; // 0x00
    hkpConstraintCollisionFilter* m_filter; // 0x04
    hkpConstraintInstance* m_constraintInstance; // 0x08
    hkBool m_constraintBroken; // 0x0C
};

// 0x00DEFDE0 (stub_impl.md S4 SD)
hkpConstraintCollisionFilter::hkpConstraintCollisionFilter(hkFinishLoadedObjectFlag flag)
    : hkpPairCollisionFilter(flag)
{
}

// 0x00E0ECF0
hkpConstraintCollisionFilter::hkpConstraintCollisionFilter(const hkpCollisionFilter* childFilter)
    : hkpPairCollisionFilter(childFilter)
{
    m_type.m_storage = HK_FILTER_CONSTRAINT;
}

// 0x00E0EA10
hkpConstraintCollisionFilter::~hkpConstraintCollisionFilter()
{
}

// 0x00E0EA80: listens to the world's constraints (once).
void hkpConstraintCollisionFilter::init(hkpWorld* world)
{
    hkpConstraintListener* listener = this;
    if (world->m_constraintListeners.indexOf(listener) >= 0)
    {
        return;
    }
    world->addConstraintListener(listener);
}

// 0x00E0EBB0
void hkpConstraintCollisionFilter::constraintAddedCallback(hkpConstraintInstance* constraint)
{
    if (constraint == nullptr)
    {
        return;
    }
    if (constraint->m_data->getType() != hkpConstraintData::CONSTRAINT_TYPE_CONTACT && constraint->m_entities[0]->m_collidable.m_shape != nullptr && constraint->m_entities[1]->m_collidable.m_shape != nullptr)
    {
        disableCollisionsBetween(constraint->m_entities[0], constraint->m_entities[1]);
    }
}

// 0x00E0EBF0
void hkpConstraintCollisionFilter::constraintRemovedCallback(hkpConstraintInstance* constraint)
{
    if (constraint == nullptr)
    {
        return;
    }
    if (constraint->m_data->getType() != hkpConstraintData::CONSTRAINT_TYPE_CONTACT && constraint->m_entities[0]->m_collidable.m_shape != nullptr && constraint->m_entities[1]->m_collidable.m_shape != nullptr)
    {
        enableCollisionsBetween(constraint->m_entities[0], constraint->m_entities[1]);
    }
}

// 0x00E0EA40: a broken constraint counts as removed, a repaired one as added.
void hkpConstraintCollisionFilter::_constraintBreakingCallbackImmediate(hkpConstraintInstance* instance, hkBool constraintBroken)
{
    hkpConstraintListener* listener = this;
    if (constraintBroken.m_bool)
    {
        listener->constraintRemovedCallback(instance);
        return;
    }
    listener->constraintAddedCallback(instance);
}

// 0x00E0EC30
void hkpConstraintCollisionFilter::constraintBreakingCallback(const hkpConstraintBrokenEvent& event)
{
    if (event.m_world->m_criticalOperationsLockCount != 0)
    {
        hkpConstraintFilterBrokenEventOperation operation;
        operation.m_type = 37; // hkWorldOperation::CONSTRAINT_FILTER_BROKEN_EVENT
        operation.m_filter = this;
        operation.m_constraintInstance = event.m_constraintInstance;
        operation.m_constraintBroken = true;
        event.m_world->queueOperation(*(const hkWorldOperation::BaseOperation*)&operation);
        return;
    }
    _constraintBreakingCallbackImmediate(event.m_constraintInstance, true);
}

// 0x00E0EC90
void hkpConstraintCollisionFilter::constraintRepairedCallback(const hkpConstraintRepairedEvent& event)
{
    if (event.m_world->m_criticalOperationsLockCount != 0)
    {
        hkpConstraintFilterBrokenEventOperation operation;
        operation.m_type = 37; // hkWorldOperation::CONSTRAINT_FILTER_BROKEN_EVENT
        operation.m_filter = this;
        operation.m_constraintInstance = event.m_constraintInstance;
        operation.m_constraintBroken = false;
        event.m_world->queueOperation(*(const hkWorldOperation::BaseOperation*)&operation);
        return;
    }
    _constraintBreakingCallbackImmediate(event.m_constraintInstance, false);
}
