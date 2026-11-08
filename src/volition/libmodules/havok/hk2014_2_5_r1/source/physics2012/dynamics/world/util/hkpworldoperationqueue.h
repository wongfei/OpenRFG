// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h
//
// Havok 2014.2.5 (havok_impl.md HK7): the world operations postponed while the world's critical operations are
// locked, and their queue. Layouts and the Type values from rfg.pdb, declarations from Havok 2014.1
// (hkpWorldOperationQueue.h). Hand-maintained (nobuild in the hdrgen specs).
//
// The world's update modes are only forward declared here (hkpworld.h, which includes this header, defines them):
// MSVC sizes a forward declared enum as int, as the 4-byte AddEntity::m_activation needs.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpbodyoperation.h"

class hkAabb;
class hkVector4f;
class hkpConstraintCollisionFilter;
class hkpConstraintInstance;
class hkpAction;
class hkpEntity;
class hkpPhantom;
class hkpRigidBody;
class hkpShape;
class hkpShapeModifier;
class hkpWorld;
class hkpWorldObject;

enum hkpEntityActivation;
enum hkpUpdateCollisionFilterOnEntityMode;
enum hkpUpdateCollisionFilterOnWorldMode;
enum hkpUpdateCollectionFilterMode;

namespace hkWorldOperation
{
    enum Result
    {
        POSTPONED = 0,
        DONE = 1,
    };

    enum Type
    {
        OPERATION_ID_ANY = 0,
        OPERATION_ID_INVALID = 0,
        ENTITY_ADD = 1,
        ENTITY_REMOVE = 2,
        ENTITY_UPDATE_BROAD_PHASE = 3,
        RIGIDBODY_SET_MOTION_TYPE = 4,
        WORLD_OBJECT_SET_SHAPE = 5,
        WORLD_OBJECT_UPDATE_SHAPE = 6,
        ENTITY_BATCH_ADD = 7,
        ENTITY_BATCH_REMOVE = 8,
        CONSTRAINT_ADD = 9,
        CONSTRAINT_REMOVE = 10,
        ACTION_ADD = 11,
        ACTION_REMOVE = 12,
        ISLAND_MERGE = 13,
        PHANTOM_ADD = 14,
        PHANTOM_REMOVE = 15,
        PHANTOM_BATCH_ADD = 16,
        PHANTOM_BATCH_REMOVE = 17,
        PHANTOM_UPDATE_BROAD_PHASE = 18,
        UPDATE_FILTER_ENTITY = 19,
        UPDATE_FILTER_ENTITY_PAIR = 20,
        UPDATE_FILTER_PHANTOM = 21,
        UPDATE_FILTER_WORLD = 22,
        UPDATE_MOVED_BODY_INFO = 23,
        ENTITY_BATCH_REINTEGRATE_AND_RECOLLIDE = 24,
        RIGIDBODY_SET_POSITION_AND_ROTATION = 25,
        RIGIDBODY_SET_LINEAR_VELOCITY = 26,
        RIGIDBODY_SET_ANGULAR_VELOCITY = 27,
        RIGIDBODY_APPLY_LINEAR_IMPULSE = 28,
        RIGIDBODY_APPLY_POINT_IMPULSE = 29,
        RIGIDBODY_APPLY_ANGULAR_IMPULSE = 30,
        WORLD_OBJECT_ADD_REFERENCE = 31,
        WORLD_OBJECT_REMOVE_REFERENCE = 32,
        ACTIVATE_REGION = 33,
        ACTIVATE_ENTITY = 34,
        REQUEST_DEACTIVATE_ENTITY = 35,
        DEACTIVATE_ENTITY = 36,
        CONSTRAINT_FILTER_BROKEN_EVENT = 37,
        USER_CALLBACK = 38,
    };

    // size 0x8
    class UserCallback : public hkReferencedObject
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR();
        virtual void worldOperationUserCallback(unsigned long userData) = 0; // vtbl+0x10
    };

    // size 0x1
    struct BaseOperation
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(BaseOperation);
        BaseOperation(Type type) { m_type.m_storage = (unsigned char)type; }

        hkEnum<Type, unsigned char> m_type; // 0x00
    };

    // size 0xC
    struct AddEntity : public BaseOperation
    {
        AddEntity() : BaseOperation(ENTITY_ADD) {}
        hkpEntity* m_entity; // 0x04
        hkpEntityActivation m_activation; // 0x08
    };

    // size 0x8
    struct RemoveEntity : public BaseOperation
    {
        RemoveEntity() : BaseOperation(ENTITY_REMOVE) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0xC
    struct SetRigidBodyMotionType : public BaseOperation
    {
        SetRigidBodyMotionType() : BaseOperation(RIGIDBODY_SET_MOTION_TYPE) {}
        hkpRigidBody* m_rigidBody; // 0x04
        hkEnum<hkpMotion::MotionType, unsigned char> m_motionType; // 0x08
        hkEnum<hkpEntityActivation, unsigned char> m_activation; // 0x09
        hkEnum<hkpUpdateCollisionFilterOnEntityMode, unsigned char> m_collisionFilterUpdateMode; // 0x0A
        hkEnum<hkpUpdateCollectionFilterMode, unsigned char> m_updateShapeCollections; // 0x0B
    };

    // size 0xC
    struct SetWorldObjectShape : public BaseOperation
    {
        SetWorldObjectShape() : BaseOperation(WORLD_OBJECT_SET_SHAPE) {}
        hkpWorldObject* m_worldObject; // 0x04
        const hkpShape* m_shape; // 0x08
    };

    // size 0xC
    struct UpdateWorldObjectShape : public BaseOperation
    {
        UpdateWorldObjectShape() : BaseOperation(WORLD_OBJECT_UPDATE_SHAPE), m_shapeModifier(nullptr) {}
        hkpWorldObject* m_worldObject; // 0x04
        hkpShapeModifier* m_shapeModifier; // 0x08
    };

    // size 0xC
    struct AddEntityBatch : public BaseOperation
    {
        AddEntityBatch() : BaseOperation(ENTITY_BATCH_ADD) {}
        hkpEntity** m_entities; // 0x04
        unsigned short m_numEntities; // 0x08 (hkObjectIndex)
        hkEnum<hkpEntityActivation, unsigned char> m_activation; // 0x0A
    };

    // size 0xC
    struct RemoveEntityBatch : public BaseOperation
    {
        RemoveEntityBatch() : BaseOperation(ENTITY_BATCH_REMOVE) {}
        hkpEntity** m_entities; // 0x04
        unsigned short m_numEntities; // 0x08
    };

    // size 0x8
    struct AddConstraint : public BaseOperation
    {
        AddConstraint() : BaseOperation(CONSTRAINT_ADD) {}
        hkpConstraintInstance* m_constraint; // 0x04
    };

    // size 0x8
    struct RemoveConstraint : public BaseOperation
    {
        RemoveConstraint() : BaseOperation(CONSTRAINT_REMOVE) {}
        hkpConstraintInstance* m_constraint; // 0x04
    };

    // size 0x8
    struct AddAction : public BaseOperation
    {
        AddAction() : BaseOperation(ACTION_ADD) {}
        hkpAction* m_action; // 0x04
    };

    // size 0x8
    struct RemoveAction : public BaseOperation
    {
        RemoveAction() : BaseOperation(ACTION_REMOVE) {}
        hkpAction* m_action; // 0x04
    };

    // size 0xC
    struct MergeIslands : public BaseOperation
    {
        MergeIslands() : BaseOperation(ISLAND_MERGE) {}
        hkpEntity* m_entities[2]; // 0x04
    };

    // size 0x8
    struct AddPhantom : public BaseOperation
    {
        AddPhantom() : BaseOperation(PHANTOM_ADD) {}
        hkpPhantom* m_phantom; // 0x04
    };

    // size 0x8
    struct RemovePhantom : public BaseOperation
    {
        RemovePhantom() : BaseOperation(PHANTOM_REMOVE) {}
        hkpPhantom* m_phantom; // 0x04
    };

    // size 0xC
    struct AddPhantomBatch : public BaseOperation
    {
        AddPhantomBatch() : BaseOperation(PHANTOM_BATCH_ADD) {}
        hkpPhantom** m_phantoms; // 0x04
        unsigned short m_numPhantoms; // 0x08
    };

    // size 0xC
    struct RemovePhantomBatch : public BaseOperation
    {
        RemovePhantomBatch() : BaseOperation(PHANTOM_BATCH_REMOVE) {}
        hkpPhantom** m_phantoms; // 0x04
        unsigned short m_numPhantoms; // 0x08
    };

    // size 0x8
    struct UpdateEntityBP : public BaseOperation
    {
        UpdateEntityBP() : BaseOperation(ENTITY_UPDATE_BROAD_PHASE) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0xC
    struct UpdatePhantomBP : public BaseOperation
    {
        UpdatePhantomBP() : BaseOperation(PHANTOM_UPDATE_BROAD_PHASE) {}
        hkpPhantom* m_phantom; // 0x04
        hkAabb* m_aabb; // 0x08
    };

    // size 0xC
    struct UpdateFilterOnEntity : public BaseOperation
    {
        UpdateFilterOnEntity() : BaseOperation(UPDATE_FILTER_ENTITY) {}
        hkpEntity* m_entity; // 0x04
        hkEnum<hkpUpdateCollisionFilterOnEntityMode, unsigned char> m_collisionFilterUpdateMode; // 0x08
        hkEnum<hkpUpdateCollectionFilterMode, unsigned char> m_updateShapeCollections; // 0x09
    };

    // size 0xC
    struct UpdateFilterOnEntityPair : public BaseOperation
    {
        UpdateFilterOnEntityPair() : BaseOperation(UPDATE_FILTER_ENTITY_PAIR) {}
        hkpEntity* m_entityA; // 0x04
        hkpEntity* m_entityB; // 0x08
    };

    // size 0xC
    struct UpdateFilterOnPhantom : public BaseOperation
    {
        UpdateFilterOnPhantom() : BaseOperation(UPDATE_FILTER_PHANTOM) {}
        hkpPhantom* m_phantom; // 0x04
        hkEnum<hkpUpdateCollectionFilterMode, unsigned char> m_updateShapeCollections; // 0x08
    };

    // size 0x3
    struct UpdateFilterOnWorld : public BaseOperation
    {
        UpdateFilterOnWorld() : BaseOperation(UPDATE_FILTER_WORLD) {}
        hkEnum<hkpUpdateCollisionFilterOnWorldMode, unsigned char> m_collisionFilterUpdateMode; // 0x01
        hkEnum<hkpUpdateCollectionFilterMode, unsigned char> m_updateShapeCollections; // 0x02
    };

    // size 0xC
    struct ReintegrateAndRecollideEntityBatch : public BaseOperation
    {
        ReintegrateAndRecollideEntityBatch() : BaseOperation(ENTITY_BATCH_REINTEGRATE_AND_RECOLLIDE) {}
        hkpEntity** m_entities; // 0x04
        unsigned short m_numEntities; // 0x08
        unsigned char m_mode; // 0x0A
        hkEnum<hkpEntityActivation, unsigned char> m_activation; // 0x0B
    };

    // size 0x8
    struct UpdateMovedBodyInfo : public BaseOperation
    {
        UpdateMovedBodyInfo() : BaseOperation(UPDATE_MOVED_BODY_INFO) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0xC
    struct SetRigidBodyPositionAndRotation : public BaseOperation
    {
        SetRigidBodyPositionAndRotation() : BaseOperation(RIGIDBODY_SET_POSITION_AND_ROTATION) {}
        hkpRigidBody* m_rigidBody; // 0x04
        hkVector4f* m_positionAndRotation; // 0x08 (allocated by the caller)
    };

    // size 0x10
    struct SetRigidBodyLinearVelocity : public BaseOperation
    {
        SetRigidBodyLinearVelocity() : BaseOperation(RIGIDBODY_SET_LINEAR_VELOCITY) {}
        hkpRigidBody* m_rigidBody; // 0x04
        float m_linearVelocity[3]; // 0x08
    };

    // size 0x10
    struct SetRigidBodyAngularVelocity : public BaseOperation
    {
        SetRigidBodyAngularVelocity() : BaseOperation(RIGIDBODY_SET_ANGULAR_VELOCITY) {}
        hkpRigidBody* m_rigidBody; // 0x04
        float m_angularVelocity[3]; // 0x08
    };

    // size 0x10
    struct ApplyRigidBodyLinearImpulse : public BaseOperation
    {
        ApplyRigidBodyLinearImpulse() : BaseOperation(RIGIDBODY_APPLY_LINEAR_IMPULSE) {}
        hkpRigidBody* m_rigidBody; // 0x04
        float m_linearImpulse[3]; // 0x08
    };

    // size 0xC
    struct ApplyRigidBodyPointImpulse : public BaseOperation
    {
        ApplyRigidBodyPointImpulse() : BaseOperation(RIGIDBODY_APPLY_POINT_IMPULSE) {}
        hkpRigidBody* m_rigidBody; // 0x04
        hkVector4f* m_pointAndImpulse; // 0x08 (allocated by the caller)
    };

    // size 0x10
    struct ApplyRigidBodyAngularImpulse : public BaseOperation
    {
        ApplyRigidBodyAngularImpulse() : BaseOperation(RIGIDBODY_APPLY_ANGULAR_IMPULSE) {}
        hkpRigidBody* m_rigidBody; // 0x04
        float m_angularImpulse[3]; // 0x08
    };

    // size 0x8: not postponed (only takes the queue's critical section in multithreaded builds)
    struct AddReference : public BaseOperation
    {
        AddReference() : BaseOperation(WORLD_OBJECT_ADD_REFERENCE) {}
        hkpWorldObject* m_worldObject; // 0x04
    };

    // size 0x8
    struct RemoveReference : public BaseOperation
    {
        RemoveReference() : BaseOperation(WORLD_OBJECT_REMOVE_REFERENCE) {}
        hkpWorldObject* m_worldObject; // 0x04
    };

    // size 0x8
    struct ActivateRegion : public BaseOperation
    {
        ActivateRegion() : BaseOperation(ACTIVATE_REGION) {}
        hkAabb* m_aabb; // 0x04
    };

    // size 0x8
    struct ActivateEntity : public BaseOperation
    {
        ActivateEntity() : BaseOperation(ACTIVATE_ENTITY) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0x8
    struct RequestDeactivateEntity : public BaseOperation
    {
        RequestDeactivateEntity() : BaseOperation(REQUEST_DEACTIVATE_ENTITY) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0x8 (deprecated)
    struct DeactivateEntity : public BaseOperation
    {
        DeactivateEntity() : BaseOperation(DEACTIVATE_ENTITY) {}
        hkpEntity* m_entity; // 0x04
    };

    // size 0x10
    struct ConstraintCollisionFilterConstraintBroken : public BaseOperation
    {
        ConstraintCollisionFilterConstraintBroken() : BaseOperation(CONSTRAINT_FILTER_BROKEN_EVENT) {}
        hkpConstraintCollisionFilter* m_filter; // 0x04
        hkpConstraintInstance* m_constraintInstance; // 0x08
        hkBool m_constraintBroken; // 0x0C
    };

    // size 0xC
    struct UserCallbackOperation : public BaseOperation
    {
        UserCallbackOperation() : BaseOperation(USER_CALLBACK) {}
        UserCallback* m_userCallback; // 0x04
        unsigned long m_userData; // 0x08
    };

    // size 0x20: the storage of a queued operation
    struct BiggestOperation : public BaseOperation
    {
        BiggestOperation() : BaseOperation(OPERATION_ID_INVALID) {}
        unsigned long dummy[6 + 1]; // 0x04
    };
}

// size 0x10
struct hkpBodyOperationEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBodyOperationEntry);

    hkpEntity* m_entity; // 0x00
    hkpBodyOperation* m_operation; // 0x04
    int m_priority; // 0x08
    hkpBodyOperation::ExecutionState m_hint; // 0x0C
};

// size 0x28
class hkpWorldOperationQueue
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWorldOperationQueue); // new(unsigned int nbytes) 0x00DEA9A0, delete(void* p, unsigned int nbytes) 0x00DEA9C0
    hkpWorldOperationQueue(hkpWorld* world); // 0x00E168F0
    ~hkpWorldOperationQueue(); // 0x00E16920
    void queueOperation(const hkWorldOperation::BaseOperation& operation); // 0x00E169C0
    void executeAllPending(); // 0x00E17040
    void purgeAllPending() { m_pending.clear(); }
    void queueBodyOperation(hkpEntity* entity, hkpBodyOperation* operation, int priority, hkpBodyOperation::ExecutionState hint); // 0x00E16830
    void executeAllPendingBodyOperations(); // 0x00E16E70

    hkArray<hkWorldOperation::BiggestOperation, hkContainerHeapAllocator> m_pending; // 0x00
    hkpWorld* m_world; // 0x0C
    hkArray<hkWorldOperation::BiggestOperation, hkContainerHeapAllocator> m_islandMerges; // 0x10
    hkArray<hkpBodyOperationEntry, hkContainerHeapAllocator> m_pendingBodyOperations; // 0x1C
};

// size 0x10: the chain of the queues being executed (nested executions), the world's m_pendingOperationQueues
struct hkpDebugInfoOnPendingOperationQueues
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpDebugInfoOnPendingOperationQueues);
    hkpDebugInfoOnPendingOperationQueues(hkArray<hkWorldOperation::BiggestOperation, hkContainerHeapAllocator>* pending)
        : m_pending(pending)
        , m_nextPendingOperationIndex(0)
        , m_nextQueue(nullptr)
        , m_prevQueue(nullptr)
    {
    }

    hkArray<hkWorldOperation::BiggestOperation, hkContainerHeapAllocator>* m_pending; // 0x00
    int m_nextPendingOperationIndex; // 0x04
    hkpDebugInfoOnPendingOperationQueues* m_nextQueue; // 0x08
    hkpDebugInfoOnPendingOperationQueues* m_prevQueue; // 0x0C
};
