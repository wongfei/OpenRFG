// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the entity (material, motion, island membership, constraints, listeners,
// activation), from rfg.exe, with the out-of-line copies of hkpentity.inl/.h.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpspheremotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

#include <new>

// 0x00DDDFC0: no island, uid or storage index yet; default material without a response type (the rigid body
// sets it)
hkpEntity::hkpEntity(const hkpShape* shape)
    : hkpWorldObject(shape, BROAD_PHASE_ENTITY)
{
    *(hkLocalFrame**)&m_localFrame = nullptr; // hkRefPtr is a seam without constructors here
    m_storageIndex = 0xFFFF;
    m_uid = 0xFFFFFFFF;
    m_npData = 0xFFFFFFFF;
    m_simulationIsland = nullptr;
    m_extendedListeners = nullptr;
    m_limitContactImpulseUtilAndFlag = nullptr;
    m_solverData = 0;
    m_numShapeKeysInContactPointProperties = 0;
    m_breakableBody = nullptr;
    m_damageMultiplier = 1.0f;
}

// 0x00DDE0F0
hkpEntity::hkpEntity(hkFinishLoadedObjectFlag flag)
    : hkpWorldObject(flag)
    , m_material(flag)
    , m_constraintsSlave(flag)
    , m_constraintRuntime(flag)
    , m_spuCollisionCallback(flag)
    , m_motion(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00DDE1B0: the contact impulse limit util (if any) goes first, the listeners hear of the deletion, then the
// members free their storage
hkpEntity::~hkpEntity()
{
    hkReferencedObject* limitUtil = (hkReferencedObject*)((uintptr_t)m_limitContactImpulseUtilAndFlag & ~(uintptr_t)1);
    if (limitUtil != nullptr)
    {
        removeContactListener((hkpContactListener*)((char*)limitUtil + 8)); // the util's contact listener base
        delete limitUtil; // scalar deleting destructor
        m_limitContactImpulseUtilAndFlag = nullptr;
    }
    hkpEntityCallbackUtil::fireEntityDeleted(this);
    if (m_extendedListeners != nullptr)
    {
        delete m_extendedListeners;
    }
    hkLocalFrame* localFrame = m_localFrame.val(); // (the hkRefPtr destructor)
    if (localFrame != nullptr)
    {
        ((hkReferencedObject*)localFrame)->removeReference();
    }
    *(hkLocalFrame**)&m_localFrame = nullptr;
}

// 0x00DDD5A0: the loaded motion gets the vtable of its type
void hkpEntity::afterReflectNew()
{
    hkpMotion* motion = &m_motion;
    hkFinishLoadedObjectFlag finishing;
    finishing.m_finishing = 1;
    switch (motion->m_type.m_storage)
    {
    case hkpMotion::MOTION_SPHERE_INERTIA:
        new (motion) hkpSphereMotion(finishing);
        break;
    case hkpMotion::MOTION_BOX_INERTIA:
        new (motion) hkpBoxMotion(finishing);
        break;
    case hkpMotion::MOTION_KEYFRAMED:
        new (motion) hkpKeyframedRigidMotion(finishing);
        break;
    case hkpMotion::MOTION_FIXED:
        new (motion) hkpFixedRigidMotion(finishing);
        break;
    case hkpMotion::MOTION_THIN_BOX_INERTIA:
        new (motion) hkpThinBoxMotion(finishing);
        break;
    case hkpMotion::MOTION_CHARACTER:
        new (motion) hkpCharacterMotion(finishing);
        break;
    default:
        break;
    }
}

// 0x00DDD290: original bug kept: a listener that is not there clears the slot before the array (index -1)
void hkpEntity::removeContactListener(hkpContactListener* cl)
{
    const int i = m_contactListeners.indexOf(cl);
    m_contactListeners.m_data[i] = nullptr;
}

// 0x00DDD340: active when its island is (an entity outside the world is inactive)
hkBool hkpEntity::isActive() const
{
    hkBool result;
    result.m_bool = m_simulationIsland != nullptr ? (char)m_simulationIsland->m_activeMark : 0;
    return result;
}

// 0x00DDD370
void hkpEntity::requestDeactivation()
{
    if (m_simulationIsland != nullptr && m_simulationIsland->m_activeMark && m_motion.m_deactivationIntegrateCounter != 0xFF)
    {
        m_motion.requestDeactivation();
    }
}

// 0x00DDD3B0: an island waiting for a split (or sparse) is split first with no minimum island size; every entity
// of the island gets HK_REAL_MAX in w of its reference positions, then the island goes inactive
void hkpEntity::deactivate()
{
    hkpSimulationIsland* island = m_simulationIsland;
    if (island == nullptr || !island->m_activeMark)
    {
        return;
    }
    if (island->m_splitCheckRequested || island->m_isSparse)
    {
        hkpWorld* world = m_world;
        const unsigned int minDesiredIslandSize = world->m_minDesiredIslandSize;
        world->m_minDesiredIslandSize = 0;
        m_simulationIsland->m_isSparse = false;
        m_simulationIsland->m_splitCheckRequested = true;
        hkpWorldOperationUtil::splitSimulationIsland(m_world, m_simulationIsland);
        m_world->m_minDesiredIslandSize = minDesiredIslandSize;
    }
    for (int i = 0; i < m_simulationIsland->m_entities.getSize(); i++)
    {
        hkpMotion& motion = m_simulationIsland->m_entities[i]->m_motion;
        motion.m_deactivationRefPosition[0].setW(hkSimdFloat32::getConstant<HK_QUADREAL_MAX>());
        motion.m_deactivationRefPosition[1].setW(hkSimdFloat32::getConstant<HK_QUADREAL_MAX>());
    }
    hkpWorldOperationUtil::markIslandInactive(m_world, m_simulationIsland);
}

// 0x00DDD4F0: queued (DEACTIVATE_ENTITY) while the world's critical operations are locked
void hkpEntity::deactivateAsCriticalOperation()
{
    if (m_world != nullptr && m_world->areCriticalOperationsLocked())
    {
        hkWorldOperation::DeactivateEntity op;
        op.m_entity = this;
        m_world->queueOperation(op);
        return;
    }
    deactivate();
}

// 0x00DDD760: the inactive frame counters restart with the world's select flags
void hkpEntity::activate()
{
    hkpSimulationIsland* island = m_simulationIsland;
    if ((island == nullptr || !island->m_activeMark) && m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && m_world != nullptr)
    {
        m_motion.m_deactivationNumInactiveFrames[0] = 0;
        m_motion.m_deactivationNumInactiveFrames[1] = 0;
        hkpWorldOperationUtil::markIslandActive(m_world, island);
        const hkpSolverInfo& solverInfo = m_world->m_dynamicsStepInfo.m_solverInfo;
        m_motion.setWorldSelectFlagsNeg(solverInfo.m_deactivationNumInactiveFramesSelectFlag[0], solverInfo.m_deactivationNumInactiveFramesSelectFlag[1], solverInfo.m_deactivationIntegrateCounter);
    }
}

// 0x00DDD7D0: queued (ACTIVATE_ENTITY) while the world's critical operations are locked
void hkpEntity::activateAsCriticalOperation()
{
    if (m_world != nullptr && m_world->areCriticalOperationsLocked())
    {
        hkWorldOperation::ActivateEntity op;
        op.m_entity = this;
        m_world->queueOperation(op);
        return;
    }
    activate();
}

// 0x00DDD1F0
void hkpEntity::updateCachedAabb()
{
    if (m_world != nullptr && m_collidable.m_shape != nullptr)
    {
        hkpEntity* self = this;
        hkpEntityAabbUtil::entityBatchRecalcAabb(m_world->m_collisionInput, &self, 1);
    }
}

// 0x00DDD620: shape collections and BV trees whose type the dispatcher knows get child box caches (not for fixed
// or keyframed BV trees); a fixed entity fills its cache now
void hkpEntity::setCachedShapeData(const hkpWorld* world, const hkpShape* shape)
{
    if (m_collidable.m_boundingVolumeData.m_childShapeAabbs != nullptr)
    {
        m_collidable.m_boundingVolumeData.deallocate();
    }
    if (world == nullptr || shape == nullptr)
    {
        return;
    }
    const unsigned char type = shape->m_type.m_storage;
    if (type == hkcdShapeType::STATIC_COMPOUND || type == hkcdShapeType::BV_COMPRESSED_MESH)
    {
        return;
    }
    const unsigned int alternateTypes = world->m_collisionDispatcher->m_hasAlternateType[type];
    bool isBvTree = false;
    const hkpShapeContainer* container;
    if ((alternateTypes & (1u << hkcdShapeType::COLLECTION)) != 0)
    {
        container = shape->getContainer();
    }
    else if ((alternateTypes & (1u << hkcdShapeType::BV_TREE)) != 0)
    {
        container = shape->getContainer();
        isBvTree = true;
    }
    else
    {
        return;
    }
    if (isFixedOrKeyframed().m_bool && isBvTree)
    {
        return;
    }
    int numChildShapes;
    if (type == hkcdShapeType::LIST)
    {
        numChildShapes = static_cast<const hkpListShape*>(shape)->m_childInfo.getSize(); // inlined
    }
    else
    {
        numChildShapes = container->getNumChildShapes();
    }
    m_collidable.m_boundingVolumeData.allocate(numChildShapes);
    m_collidable.m_boundingVolumeData.invalidate();
    if (m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        updateCachedAabb();
    }
}

// cmpLessConstraint (anonymous in Havok): by uid of the other entity
namespace
{
struct cmpLessConstraint
{
    const hkpEntity* m_entity;

    bool operator()(const hkpConstraintInstance* a, const hkpConstraintInstance* b) const
    {
        const hkpEntity* otherA = (const hkpEntity*)((uintptr_t)a->m_entities[0] ^ (uintptr_t)a->m_entities[1] ^ (uintptr_t)m_entity);
        const hkpEntity* otherB = (const hkpEntity*)((uintptr_t)b->m_entities[0] ^ (uintptr_t)b->m_entities[1] ^ (uintptr_t)m_entity);
        return otherA->m_uid < otherB->m_uid;
    }
};
}

// 0x00DDD8C0: a fixed entity's slave constraints sorted by the other entity's uid; the masters (on the other
// entities) get their new slave index
void hkpEntity::sortConstraintsSlavesDeterministically()
{
    if (m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
    {
        return;
    }
    const int numSlaves = m_constraintsSlave.getSize();
    if (numSlaves > 1)
    {
        cmpLessConstraint cmp = { this };
        hkAlgorithm::quickSortRecursive((hkpConstraintInstance**)&m_constraintsSlave[0], 0, numSlaves - 1, cmp);
    }
    for (int i = 0; i < m_constraintsSlave.getSize(); i++)
    {
        hkpConstraintInstance* constraint = m_constraintsSlave[i];
        hkpEntity* master = (hkpEntity*)((uintptr_t)constraint->m_entities[0] ^ (uintptr_t)constraint->m_entities[1] ^ (uintptr_t)this);
        for (int j = 0; j < master->m_constraintsMaster.getSize(); j++)
        {
            if (master->m_constraintsMaster[j].m_constraint == constraint)
            {
                master->m_constraintsMaster[j].m_slaveIndex = (unsigned short)i;
                break;
            }
        }
    }
}

// 0x00DDDB80: frees the empty arrays (and the limit util); the extended listeners go when both of their arrays are
// empty
void hkpEntity::deallocateInternalArrays()
{
    hkReferencedObject* limitUtil = (hkReferencedObject*)((uintptr_t)m_limitContactImpulseUtilAndFlag & ~(uintptr_t)1);
    if (limitUtil != nullptr)
    {
        removeContactListener((hkpContactListener*)((char*)limitUtil + 8));
        delete limitUtil;
        m_limitContactImpulseUtilAndFlag = nullptr;
    }
    if (m_collidable.m_collisionEntries.getSize() == 0)
    {
        m_collidable.m_collisionEntries.clearAndDeallocate();
    }
    if (m_constraintsMaster.getSize() == 0)
    {
        m_constraintsMaster.clearAndDeallocate();
    }
    if (m_constraintsSlave.getSize() == 0)
    {
        m_constraintsSlave.clearAndDeallocate();
    }
    if (m_constraintRuntime.getSize() == 0)
    {
        m_constraintRuntime.clearAndDeallocate();
    }
    // the contact listener slots are cleared, not removed: the array goes when every slot is null
    bool hasContactListeners = false;
    for (int i = 0; i < m_contactListeners.getSize(); i++)
    {
        if (m_contactListeners[i] != nullptr)
        {
            hasContactListeners = true;
            break;
        }
    }
    if (!hasContactListeners)
    {
        m_contactListeners.clearAndDeallocate();
    }
    bool hasActivationListeners = false;
    if (m_extendedListeners != nullptr)
    {
        hkSmallArray<hkpEntityActivationListener*>& listeners = m_extendedListeners->m_activationListeners;
        for (int i = 0; i < listeners.getSize(); i++)
        {
            if (listeners[i] != nullptr)
            {
                hasActivationListeners = true;
                break;
            }
        }
        if (!hasActivationListeners)
        {
            listeners.clearAndDeallocate();
        }
    }
    bool hasEntityListeners = false;
    if (m_extendedListeners != nullptr)
    {
        hkSmallArray<hkpEntityListener*>& listeners = m_extendedListeners->m_entityListeners;
        for (int i = 0; i < listeners.getSize(); i++)
        {
            if (listeners[i] != nullptr)
            {
                hasEntityListeners = true;
                break;
            }
        }
        if (!hasEntityListeners)
        {
            listeners.clearAndDeallocate();
        }
    }
    if (!hasActivationListeners && !hasEntityListeners)
    {
        if (m_extendedListeners != nullptr)
        {
            delete m_extendedListeners;
        }
        m_extendedListeners = nullptr;
    }
    if (m_actions.getSize() == 0)
    {
        m_actions.clearAndDeallocate();
    }
}

// 0x00DDFAC0 (hkpentity.h): the rigid body has the motion state
hkMotionState* hkpEntity::getMotionState()
{
    return nullptr;
}

// 0x00DDECF0 (hkpentity.inl)
hkBool hkpEntity::isFixedOrKeyframed() const
{
    hkBool result;
    result.m_bool = m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED || m_motion.m_type.m_storage == hkpMotion::MOTION_KEYFRAMED;
    return result;
}

// 0x00B71280 (hkpentity.inl)
hkBool hkpEntity::isFixed() const
{
    hkBool result;
    result.m_bool = m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED;
    return result;
}

// 0x0080CD70 (hkpentity.inl)
hkpMotion* hkpEntity::getMotion()
{
    return &m_motion;
}

// 0x00DDE780
hkpSimulationIsland* hkpEntity::getSimulationIsland() const
{
    return m_simulationIsland;
}

// 0x00DDE790
unsigned int hkpEntity::getUid() const
{
    return m_uid;
}

// 0x0080CD80 (hkpentity.inl), 0x00DF2980
hkpMaterial& hkpEntity::getMaterial()
{
    return m_material;
}

const hkpMaterial& hkpEntity::getMaterial() const
{
    return m_material;
}

// 0x00A2C680, 0x00A2C670 (hkpentity.inl)
int hkpEntity::getNumActions() const
{
    return m_actions.getSize();
}

hkpAction* hkpEntity::getAction(int i)
{
    return m_actions[i];
}

// 0x00DDE7A0, 0x00DDE7B0, 0x00DDE7C0, 0x00DDD220, 0x00DDD230, 0x00DDD810
const hkSmallArray<hkConstraintInternal>& hkpEntity::getConstraintMasters() const
{
    return m_constraintsMaster;
}

hkSmallArray<hkConstraintInternal>& hkpEntity::getConstraintMastersRw()
{
    return m_constraintsMaster;
}

const hkArray<hkViewPtr<hkpConstraintInstance>, hkContainerHeapAllocator>& hkpEntity::getConstraintSlaves() const
{
    return m_constraintsSlave;
}

const hkSmallArray<hkConstraintInternal>& hkpEntity::getConstraintMastersImpl() const
{
    return m_constraintsMaster;
}

hkSmallArray<hkConstraintInternal>& hkpEntity::getConstraintMastersRwImpl()
{
    return m_constraintsMaster;
}

const hkArray<hkViewPtr<hkpConstraintInstance>, hkContainerHeapAllocator>& hkpEntity::getConstraintSlavesImpl() const
{
    return m_constraintsSlave;
}

// 0x00DDF930
hkpEntity::ExtendedListeners::ExtendedListeners()
{
}

// 0x00DDF8C0: the arrays free their storage
hkpEntity::ExtendedListeners::~ExtendedListeners()
{
}

// 0x00DDD950 (player_minimal.md P1): a removed listener leaves a null slot, which the next listener reuses
void hkpEntity::addEntityListener(hkpEntityListener* el)
{
    if (m_extendedListeners == nullptr)
    {
        m_extendedListeners = new ExtendedListeners();
    }
    hkSmallArray<hkpEntityListener*>& listeners = m_extendedListeners->m_entityListeners;
    hkpEntityListener* nullListener = nullptr;
    const int emptyIndex = listeners.indexOf(nullListener);
    if (emptyIndex >= 0)
    {
        listeners[emptyIndex] = el;
        return;
    }
    listeners.pushBack(el);
}

// 0x00DDDA20: the slot is cleared, not removed (callbacks may be running over the array). A listener that is not
// there clears the slot before the array, as the original does (indexOf -1).
void hkpEntity::removeEntityListener(hkpEntityListener* el)
{
    if (m_extendedListeners == nullptr)
    {
        m_extendedListeners = new ExtendedListeners();
    }
    hkSmallArray<hkpEntityListener*>& listeners = m_extendedListeners->m_entityListeners;
    const int i = listeners.indexOf(el);
    listeners.begin()[i] = nullptr;
}

// 0x00DDE760
hkpEntity::SpuCollisionCallback::SpuCollisionCallback()
{
    m_util = nullptr;
    m_capacity = 0;
    m_eventFilter = SPU_SEND_CONTACT_POINT_ADDED_OR_PROCESS;
    m_userFilter = 1;
}

// 0x00DDE770
hkpEntity::SpuCollisionCallback::SpuCollisionCallback(hkFinishLoadedObjectFlag flag)
{
}
