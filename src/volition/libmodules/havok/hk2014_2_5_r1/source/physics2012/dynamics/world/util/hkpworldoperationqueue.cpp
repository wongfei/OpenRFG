// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the operations postponed while the world's critical operations are locked, and
// their execution, from rfg.exe. A queued operation holds references on the objects it names (and copies of the
// arrays and boxes it points to), released after it ran.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

#include <string.h>

using namespace hkWorldOperation;

// 0x00E16760
static void addReferenceTo(const hkReferencedObject* obj)
{
    obj->addReference();
}

// The queued arrays and boxes live on the thread's heap.
static void* hkWorldOperationQueue_alloc(int numBytes)
{
    return hkMemoryRouter::getInstance().heap().blockAlloc(numBytes);
}

static void hkWorldOperationQueue_free(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().heap().blockFree(p, numBytes);
}

// 0x00E167A0: by entity uid; the same entity's operations by descending priority
static bool hkpWorldBodyOperationQueueSortCmp(const hkpBodyOperationEntry& a, const hkpBodyOperationEntry& b)
{
    if (a.m_entity == b.m_entity)
    {
        return (unsigned int)b.m_priority < (unsigned int)a.m_priority;
    }
    return a.m_entity->m_uid < b.m_entity->m_uid;
}

// 0x00E167D0: island merges by the islands' storage indices (first entity, then second)
static bool islandLess(const BiggestOperation& a, const BiggestOperation& b)
{
    const MergeIslands& ma = reinterpret_cast<const MergeIslands&>(a);
    const MergeIslands& mb = reinterpret_cast<const MergeIslands&>(b);
    const unsigned short a0 = ma.m_entities[0]->m_simulationIsland->m_storageIndex;
    const unsigned short b0 = mb.m_entities[0]->m_simulationIsland->m_storageIndex;
    if (a0 < b0)
    {
        return true;
    }
    return a0 == b0 && ma.m_entities[1]->m_simulationIsland->m_storageIndex < mb.m_entities[1]->m_simulationIsland->m_storageIndex;
}

// 0x00E168F0
hkpWorldOperationQueue::hkpWorldOperationQueue(hkpWorld* world)
{
    m_world = world;
}

// 0x00E16920: the arrays free their storage
hkpWorldOperationQueue::~hkpWorldOperationQueue()
{
}

// A queued array of world objects: copied to the heap, each element referenced.
static void hkWorldOperationQueue_copyArray(BiggestOperation& op, const BaseOperation& operation)
{
    void*& data = *reinterpret_cast<void**>(&op.dummy[0]);
    const unsigned short num = *reinterpret_cast<const unsigned short*>(&op.dummy[1]);
    data = hkWorldOperationQueue_alloc(num * 4);
    const AddEntityBatch& batch = static_cast<const AddEntityBatch&>(operation); // the same layout for every batch
    memcpy(data, batch.m_entities, batch.m_numEntities * 4); // hkString::memCpy
    hkReferencedObject** objects = static_cast<hkReferencedObject**>(data);
    for (int i = 0; i < num; i++)
    {
        addReferenceTo(objects[i]);
    }
}

// 0x00E169C0: the operation is stored in a 32-byte slot (the original copies 32 bytes whatever the operation's size)
void hkpWorldOperationQueue::queueOperation(const BaseOperation& operation)
{
    hkReferencedObject::lockAll();
    BiggestOperation& op = m_pending.expandOne();
    op.m_type.m_storage = OPERATION_ID_INVALID; // (the BiggestOperation constructor)
    m_world->m_pendingOperationsCount++;
    memcpy(&op, &operation, sizeof(BiggestOperation));
    hkReferencedObject* obj0 = reinterpret_cast<hkReferencedObject*>(op.dummy[0]);
    hkReferencedObject* obj1 = reinterpret_cast<hkReferencedObject*>(op.dummy[1]);
    switch (op.m_type.m_storage)
    {
    case ENTITY_ADD:
    case ENTITY_REMOVE:
    case ENTITY_UPDATE_BROAD_PHASE:
    case RIGIDBODY_SET_MOTION_TYPE:
    case WORLD_OBJECT_UPDATE_SHAPE:
    case CONSTRAINT_ADD:
    case CONSTRAINT_REMOVE:
    case ACTION_ADD:
    case ACTION_REMOVE:
    case PHANTOM_ADD:
    case PHANTOM_REMOVE:
    case UPDATE_FILTER_ENTITY:
    case UPDATE_FILTER_PHANTOM:
    case UPDATE_MOVED_BODY_INFO:
    case RIGIDBODY_SET_POSITION_AND_ROTATION:
    case RIGIDBODY_SET_LINEAR_VELOCITY:
    case RIGIDBODY_SET_ANGULAR_VELOCITY:
    case RIGIDBODY_APPLY_LINEAR_IMPULSE:
    case RIGIDBODY_APPLY_POINT_IMPULSE:
    case RIGIDBODY_APPLY_ANGULAR_IMPULSE:
    case ACTIVATE_ENTITY:
    case REQUEST_DEACTIVATE_ENTITY:
    case DEACTIVATE_ENTITY:
    case USER_CALLBACK:
        addReferenceTo(obj0);
        break;
    case WORLD_OBJECT_SET_SHAPE:
        addReferenceTo(obj0);
        obj1->addReference();
        break;
    case ENTITY_BATCH_ADD:
    case ENTITY_BATCH_REMOVE:
    case PHANTOM_BATCH_ADD:
    case PHANTOM_BATCH_REMOVE:
    case ENTITY_BATCH_REINTEGRATE_AND_RECOLLIDE:
        hkWorldOperationQueue_copyArray(op, operation);
        break;
    case ISLAND_MERGE:
        // the merges go to their own list (sorted and run first)
        addReferenceTo(obj0);
        addReferenceTo(obj1);
        m_islandMerges.pushBack(op);
        m_pending.popBack();
        break;
    case PHANTOM_UPDATE_BROAD_PHASE:
    {
        addReferenceTo(obj0);
        hkAabb* aabb = static_cast<hkAabb*>(hkWorldOperationQueue_alloc(sizeof(hkAabb)));
        *aabb = *static_cast<const UpdatePhantomBP&>(operation).m_aabb;
        op.dummy[1] = reinterpret_cast<unsigned long>(aabb);
        break;
    }
    case UPDATE_FILTER_ENTITY_PAIR:
        addReferenceTo(obj0);
        addReferenceTo(obj1);
        break;
    case WORLD_OBJECT_ADD_REFERENCE:
        // done at once (the queue only supplies the lock)
        addReferenceTo(obj0);
        m_pending.popBack();
        m_world->m_pendingOperationsCount--;
        break;
    case WORLD_OBJECT_REMOVE_REFERENCE:
        obj0->removeReference();
        m_pending.popBack();
        m_world->m_pendingOperationsCount--;
        break;
    case CONSTRAINT_FILTER_BROKEN_EVENT:
        obj0->addReference();
        obj1->addReference();
        break;
    default: // UPDATE_FILTER_WORLD; ACTIVATE_REGION (the world allocated the box)
        break;
    }
    hkReferencedObject::unlockAll();
}

// 0x00E16830
void hkpWorldOperationQueue::queueBodyOperation(hkpEntity* entity, hkpBodyOperation* operation, int priority, hkpBodyOperation::ExecutionState hint)
{
    hkReferencedObject::lockAll();
    m_world->m_pendingBodyOperationsCount++;
    hkpBodyOperationEntry& entry = m_pendingBodyOperations.expandOne();
    entry.m_priority = priority;
    entry.m_entity = entity;
    entry.m_operation = operation;
    entry.m_hint = hint;
    addReferenceTo(entity);
    addReferenceTo(operation);
    hkReferencedObject::unlockAll();
}

// 0x00E16E70: in batches (operations may queue more): sorted by entity and priority, executed, then released
void hkpWorldOperationQueue::executeAllPendingBodyOperations()
{
    while (m_pendingBodyOperations.getSize() != 0)
    {
        hkArray<hkpBodyOperationEntry, hkContainerHeapAllocator> operations;
        operations.swap(m_pendingBodyOperations);
        m_world->m_pendingBodyOperationsCount = 0;
        m_pendingBodyOperations.reserve(16);
        const int num = operations.getSize();
        if (num > 1)
        {
            hkAlgorithm::quickSortRecursive(operations.begin(), 0, num - 1, hkpWorldBodyOperationQueueSortCmp);
        }
        for (int i = 0; i < num; i++)
        {
            hkpBodyOperation::UpdateInfo updateInfo;
            operations[i].m_operation->execute(static_cast<hkpRigidBody*>(operations[i].m_entity), updateInfo);
        }
        hkReferencedObject::lockAll();
        for (int i = 0; i < num; i++)
        {
            operations[i].m_entity->removeReference();
            operations[i].m_operation->removeReference();
        }
        hkReferencedObject::unlockAll();
    }
}

// A world object array of a batch operation: references released, storage freed.
static void hkWorldOperationQueue_releaseArray(const BiggestOperation& op)
{
    hkReferencedObject** objects = reinterpret_cast<hkReferencedObject**>(op.dummy[0]);
    const unsigned short num = *reinterpret_cast<const unsigned short*>(&op.dummy[1]);
    hkReferencedObject::removeReferences(objects, num, sizeof(hkReferencedObject*));
    hkWorldOperationQueue_free(objects, num * 4);
}

// A velocity or impulse stored as three floats (w becomes 0).
static inline hkVector4f hkWorldOperationQueue_vector(const BiggestOperation& op)
{
    const float* f = reinterpret_cast<const float*>(&op.dummy[1]);
    hkVector4f v;
    v.set(f[0], f[1], f[2], 0.0f);
    return v;
}

// 0x00E17040: the island merges first (sorted by island), then the operations in their order; an operation that
// queues more runs them at once (recursion). Every operation still checks that its objects are where it expects
// (in this world, not fixed, ...).
void hkpWorldOperationQueue::executeAllPending()
{
    hkReferencedObject::lockAll();
    hkArray<BiggestOperation, hkContainerHeapAllocator> pending;
    pending.swap(m_pending);
    m_world->m_pendingOperationQueueCount++;

    if (m_islandMerges.getSize() != 0)
    {
        if (m_islandMerges.getSize() > 1)
        {
            hkAlgorithm::quickSortRecursive(m_islandMerges.begin(), 0, m_islandMerges.getSize() - 1, islandLess);
        }
        while (m_islandMerges.getSize() != 0)
        {
            const MergeIslands merge = reinterpret_cast<const MergeIslands&>(m_islandMerges.back());
            m_islandMerges.popBack();
            hkpEntity* entityA = merge.m_entities[0];
            hkpEntity* entityB = merge.m_entities[1];
            if (entityA->m_world == m_world && entityB->m_world == m_world && entityA->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityB->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityA->m_simulationIsland != entityB->m_simulationIsland)
            {
                hkpWorldOperationUtil::mergeIslands(entityA->m_world, entityA, entityB);
            }
            entityA->removeReference();
            entityB->removeReference();
        }
    }

    for (int i = 0; i < pending.getSize(); i++)
    {
        BiggestOperation& op = pending[i];
        bool releaseObject0 = true; // the reference the queue took on the operation's first object
        switch (op.m_type.m_storage)
        {
        case ENTITY_ADD:
        {
            AddEntity& o = reinterpret_cast<AddEntity&>(op);
            if (o.m_entity->m_world == nullptr)
            {
                m_world->addEntity(o.m_entity, o.m_activation);
            }
            break;
        }
        case ENTITY_REMOVE:
        {
            RemoveEntity& o = reinterpret_cast<RemoveEntity&>(op);
            if (o.m_entity->m_world == m_world)
            {
                m_world->removeEntity(o.m_entity);
            }
            break;
        }
        case ENTITY_UPDATE_BROAD_PHASE:
        {
            UpdateEntityBP& o = reinterpret_cast<UpdateEntityBP&>(op);
            if (o.m_entity->m_world == m_world)
            {
                hkpWorldOperationUtil::updateEntityBP(m_world, o.m_entity);
            }
            break;
        }
        case RIGIDBODY_SET_MOTION_TYPE:
        {
            SetRigidBodyMotionType& o = reinterpret_cast<SetRigidBodyMotionType&>(op);
            o.m_rigidBody->setMotionType((hkpMotion::MotionType)o.m_motionType.m_storage, (hkpEntityActivation)o.m_activation.m_storage, (hkpUpdateCollisionFilterOnEntityMode)o.m_collisionFilterUpdateMode.m_storage);
            break;
        }
        case WORLD_OBJECT_SET_SHAPE:
        {
            SetWorldObjectShape& o = reinterpret_cast<SetWorldObjectShape&>(op);
            o.m_worldObject->setShape(o.m_shape);
            o.m_worldObject->removeReference();
            o.m_shape->removeReference();
            releaseObject0 = false;
            break;
        }
        case WORLD_OBJECT_UPDATE_SHAPE:
        {
            UpdateWorldObjectShape& o = reinterpret_cast<UpdateWorldObjectShape&>(op);
            o.m_worldObject->updateShape(o.m_shapeModifier);
            break;
        }
        case ENTITY_BATCH_ADD:
        {
            AddEntityBatch& o = reinterpret_cast<AddEntityBatch&>(op);
            m_world->addEntityBatch(o.m_entities, o.m_numEntities, (hkpEntityActivation)o.m_activation.m_storage);
            hkWorldOperationQueue_releaseArray(op);
            releaseObject0 = false;
            break;
        }
        case ENTITY_BATCH_REMOVE:
        {
            RemoveEntityBatch& o = reinterpret_cast<RemoveEntityBatch&>(op);
            m_world->removeEntityBatch(o.m_entities, o.m_numEntities);
            hkWorldOperationQueue_releaseArray(op);
            releaseObject0 = false;
            break;
        }
        case CONSTRAINT_ADD:
        {
            hkpConstraintInstance* constraint = reinterpret_cast<AddConstraint&>(op).m_constraint;
            hkpEntity* entityB = constraint->m_entities[1];
            if (constraint->m_owner == nullptr && constraint->m_entities[0]->m_world == m_world && (entityB == nullptr || entityB->m_world == m_world))
            {
                m_world->addConstraint(constraint);
            }
            break;
        }
        case CONSTRAINT_REMOVE:
        {
            hkpConstraintInstance* constraint = reinterpret_cast<RemoveConstraint&>(op).m_constraint;
            if (constraint->m_owner != nullptr)
            {
                m_world->removeConstraint(constraint);
            }
            break;
        }
        case ACTION_ADD:
        {
            hkpAction* action = reinterpret_cast<AddAction&>(op).m_action;
            if (action->m_world == nullptr)
            {
                // only when every entity of the action is in this world
                hkArray<hkpEntity*, hkContainerHeapAllocator> entities;
                action->getEntities(entities);
                bool allInWorld = true;
                for (int e = 0; e < entities.getSize(); e++)
                {
                    if (entities[e]->m_world != m_world)
                    {
                        allInWorld = false;
                        break;
                    }
                }
                if (allInWorld)
                {
                    m_world->addAction(action);
                }
            }
            break;
        }
        case ACTION_REMOVE:
        {
            hkpAction* action = reinterpret_cast<RemoveAction&>(op).m_action;
            if (action->m_island != nullptr)
            {
                m_world->removeAction(action);
            }
            break;
        }
        case PHANTOM_ADD:
        {
            hkpPhantom* phantom = reinterpret_cast<AddPhantom&>(op).m_phantom;
            if (phantom->m_world == nullptr)
            {
                m_world->addPhantom(phantom);
            }
            break;
        }
        case PHANTOM_REMOVE:
        {
            hkpPhantom* phantom = reinterpret_cast<RemovePhantom&>(op).m_phantom;
            if (phantom->m_world == m_world)
            {
                m_world->removePhantom(phantom);
            }
            break;
        }
        case PHANTOM_BATCH_ADD:
        {
            AddPhantomBatch& o = reinterpret_cast<AddPhantomBatch&>(op);
            m_world->addPhantomBatch(o.m_phantoms, o.m_numPhantoms);
            hkWorldOperationQueue_releaseArray(op);
            releaseObject0 = false;
            break;
        }
        case PHANTOM_BATCH_REMOVE:
        {
            RemovePhantomBatch& o = reinterpret_cast<RemovePhantomBatch&>(op);
            m_world->removePhantomBatch(o.m_phantoms, o.m_numPhantoms);
            hkWorldOperationQueue_releaseArray(op);
            releaseObject0 = false;
            break;
        }
        case PHANTOM_UPDATE_BROAD_PHASE:
        {
            UpdatePhantomBP& o = reinterpret_cast<UpdatePhantomBP&>(op);
            if (o.m_phantom->m_world == m_world)
            {
                o.m_phantom->updateBroadPhase(*o.m_aabb);
            }
            o.m_phantom->removeReference();
            if (o.m_aabb != nullptr)
            {
                hkWorldOperationQueue_free(o.m_aabb, sizeof(hkAabb));
            }
            releaseObject0 = false;
            break;
        }
        case UPDATE_FILTER_ENTITY:
        {
            UpdateFilterOnEntity& o = reinterpret_cast<UpdateFilterOnEntity&>(op);
            if (o.m_entity->m_world == m_world)
            {
                m_world->updateCollisionFilterOnEntity(o.m_entity, (hkpUpdateCollisionFilterOnEntityMode)o.m_collisionFilterUpdateMode.m_storage, (hkpUpdateCollectionFilterMode)o.m_updateShapeCollections.m_storage);
            }
            break;
        }
        case UPDATE_FILTER_ENTITY_PAIR:
        {
            UpdateFilterOnEntityPair& o = reinterpret_cast<UpdateFilterOnEntityPair&>(op);
            if (o.m_entityA->m_world == m_world && o.m_entityB->m_world == m_world)
            {
                m_world->reenableCollisionBetweenEntityPair(o.m_entityA, o.m_entityB);
            }
            o.m_entityA->removeReference();
            o.m_entityB->removeReference();
            releaseObject0 = false;
            break;
        }
        case UPDATE_FILTER_PHANTOM:
        {
            UpdateFilterOnPhantom& o = reinterpret_cast<UpdateFilterOnPhantom&>(op);
            if (o.m_phantom->m_world == m_world)
            {
                m_world->updateCollisionFilterOnPhantom(o.m_phantom, (hkpUpdateCollectionFilterMode)o.m_updateShapeCollections.m_storage);
            }
            break;
        }
        case UPDATE_FILTER_WORLD:
        {
            UpdateFilterOnWorld& o = reinterpret_cast<UpdateFilterOnWorld&>(op);
            m_world->updateCollisionFilterOnWorld((hkpUpdateCollisionFilterOnWorldMode)o.m_collisionFilterUpdateMode.m_storage, (hkpUpdateCollectionFilterMode)o.m_updateShapeCollections.m_storage);
            releaseObject0 = false;
            break;
        }
        case UPDATE_MOVED_BODY_INFO:
            hkpRigidBody::updateBroadphaseAndResetCollisionInformationOfWarpedBody(reinterpret_cast<UpdateMovedBodyInfo&>(op).m_entity);
            break;
        case ENTITY_BATCH_REINTEGRATE_AND_RECOLLIDE:
        {
            // the entities that left the world move to the end (swapped in from the back) and are skipped
            ReintegrateAndRecollideEntityBatch& o = reinterpret_cast<ReintegrateAndRecollideEntityBatch&>(op);
            int numInWorld = o.m_numEntities;
            hkpEntity** last = o.m_entities + (o.m_numEntities - 1);
            for (hkpEntity** e = last; e >= o.m_entities; e--)
            {
                hkpEntity* entity = *e;
                if (entity->m_world != m_world)
                {
                    *e = *last;
                    *last = entity;
                    last--;
                    numInWorld--;
                }
            }
            if (numInWorld != 0)
            {
                m_world->m_simulation->reintegrateAndRecollideEntities(o.m_entities, numInWorld, m_world, o.m_mode);
            }
            for (int e = 0; e < o.m_numEntities; e++)
            {
                o.m_entities[e]->removeReference();
            }
            hkWorldOperationQueue_free(o.m_entities, o.m_numEntities * 4);
            releaseObject0 = false;
            break;
        }
        case RIGIDBODY_SET_POSITION_AND_ROTATION:
        {
            SetRigidBodyPositionAndRotation& o = reinterpret_cast<SetRigidBodyPositionAndRotation&>(op);
            o.m_rigidBody->setPositionAndRotation(o.m_positionAndRotation[0], reinterpret_cast<const hkQuaternionf&>(o.m_positionAndRotation[1]));
            hkWorldOperationQueue_free(o.m_positionAndRotation, 2 * sizeof(hkVector4f));
            break;
        }
        case RIGIDBODY_SET_LINEAR_VELOCITY:
        {
            hkpRigidBody* body = reinterpret_cast<SetRigidBodyLinearVelocity&>(op).m_rigidBody;
            const hkVector4f v = hkWorldOperationQueue_vector(op);
            body->activate();
            body->m_motion.setLinearVelocity(v);
            break;
        }
        case RIGIDBODY_SET_ANGULAR_VELOCITY:
        {
            hkpRigidBody* body = reinterpret_cast<SetRigidBodyAngularVelocity&>(op).m_rigidBody;
            const hkVector4f v = hkWorldOperationQueue_vector(op);
            body->activate();
            body->m_motion.setAngularVelocity(v);
            break;
        }
        case RIGIDBODY_APPLY_LINEAR_IMPULSE:
        {
            hkpRigidBody* body = reinterpret_cast<ApplyRigidBodyLinearImpulse&>(op).m_rigidBody;
            const hkVector4f v = hkWorldOperationQueue_vector(op);
            body->activate();
            body->m_motion.applyLinearImpulse(v);
            break;
        }
        case RIGIDBODY_APPLY_POINT_IMPULSE:
        {
            // the point first, then the impulse
            ApplyRigidBodyPointImpulse& o = reinterpret_cast<ApplyRigidBodyPointImpulse&>(op);
            hkVector4f* pointAndImpulse = o.m_pointAndImpulse;
            o.m_rigidBody->activate();
            o.m_rigidBody->m_motion.applyPointImpulse(pointAndImpulse[1], pointAndImpulse[0]);
            hkWorldOperationQueue_free(o.m_pointAndImpulse, 2 * sizeof(hkVector4f));
            break;
        }
        case RIGIDBODY_APPLY_ANGULAR_IMPULSE:
        {
            hkpRigidBody* body = reinterpret_cast<ApplyRigidBodyAngularImpulse&>(op).m_rigidBody;
            const hkVector4f v = hkWorldOperationQueue_vector(op);
            body->activate();
            body->m_motion.applyAngularImpulse(v);
            break;
        }
        case ACTIVATE_REGION:
        {
            hkAabb* aabb = reinterpret_cast<ActivateRegion&>(op).m_aabb;
            m_world->activateRegion(*aabb);
            hkWorldOperationQueue_free(aabb, sizeof(hkAabb));
            releaseObject0 = false;
            break;
        }
        case ACTIVATE_ENTITY:
            reinterpret_cast<ActivateEntity&>(op).m_entity->activate();
            break;
        case REQUEST_DEACTIVATE_ENTITY:
        {
            hkpEntity* entity = reinterpret_cast<RequestDeactivateEntity&>(op).m_entity;
            if (entity->m_world == m_world)
            {
                entity->requestDeactivation();
            }
            break;
        }
        case DEACTIVATE_ENTITY:
        {
            hkpEntity* entity = reinterpret_cast<DeactivateEntity&>(op).m_entity;
            if (entity->m_world == m_world)
            {
                entity->deactivate();
            }
            break;
        }
        case CONSTRAINT_FILTER_BROKEN_EVENT:
        {
            ConstraintCollisionFilterConstraintBroken& o = reinterpret_cast<ConstraintCollisionFilterConstraintBroken&>(op);
            o.m_filter->_constraintBreakingCallbackImmediate(o.m_constraintInstance, o.m_constraintBroken);
            o.m_filter->removeReference();
            o.m_constraintInstance->removeReference();
            releaseObject0 = false;
            break;
        }
        case USER_CALLBACK:
        {
            UserCallbackOperation& o = reinterpret_cast<UserCallbackOperation&>(op);
            o.m_userCallback->worldOperationUserCallback(o.m_userData);
            break;
        }
        default:
            releaseObject0 = false;
            break;
        }
        if (releaseObject0)
        {
            reinterpret_cast<hkReferencedObject*>(op.dummy[0])->removeReference();
        }
        if (m_pending.getSize() != 0)
        {
            executeAllPending();
        }
    }

    pending.swap(m_pending);
    m_pending.clear();
    m_world->m_pendingOperationQueueCount--;
    hkReferencedObject::unlockAll();
}
