// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7, HK8): the constraint instance, from rfg.exe (what a contact constraint uses; the
// cloning, transform and pivot functions have no in-scope caller).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"

// 0x00DEEF30 (stub_impl.md S4 SD): the listeners are not serialized (an empty array); the uid is renumbered
// (0xFFFFFFF0) as in the priority constructor
hkpConstraintInstance::hkpConstraintInstance(hkFinishLoadedObjectFlag f)
    : hkReferencedObject(f), m_name(f)
{
    m_uid = 0xFFFFFFF0;
}

// 0x00E05F10: no entities and no data yet; the uid is 0xFFFFFFF0 until the world numbers it
hkpConstraintInstance::hkpConstraintInstance(ConstraintPriority priority)
{
    m_owner = nullptr;
    m_constraintModifiers = nullptr;
    m_priority.m_storage = (unsigned char)priority;
    m_wantRuntime = true;
    m_destructionRemapInfo.m_storage = ON_DESTRUCTION_REMAP;
    m_userData = 0;
    m_internal = nullptr;
    m_uid = 0xFFFFFFF0;
}

// 0x00E05E40 (multiplayer.md MP7): between two entities (B may be null: the world's fixed body later); the entities
// and the data are referenced under the global reference lock (A without a null test, as the binary)
hkpConstraintInstance::hkpConstraintInstance(hkpEntity* entityA, hkpEntity* entityB, hkpConstraintData* data, ConstraintPriority priority)
{
    m_data = data;
    m_owner = nullptr;
    m_priority.m_storage = (unsigned char)priority;
    m_wantRuntime = true;
    m_destructionRemapInfo.m_storage = ON_DESTRUCTION_REMAP;
    m_userData = 0;
    m_internal = nullptr;
    m_uid = 0xFFFFFFF0;
    m_entities[0] = entityA;
    m_entities[1] = entityB;
    m_constraintModifiers = nullptr;
    hkReferencedObject::lockAll();
    m_entities[0]->addReference();
    if (m_entities[1] != nullptr)
    {
        m_entities[1]->addReference();
    }
    m_data->addReference();
    hkReferencedObject::unlockAll();
}

// 0x00E060B0: the modifiers are freed along the chain (each by its own size) down to the constraint's atoms
hkpConstraintInstance::~hkpConstraintInstance()
{
    hkpConstraintCallbackUtil::fireConstraintDeleted(this);
    if (m_entities[0] != nullptr)
    {
        m_entities[0]->removeReference();
    }
    if (m_entities[1] != nullptr)
    {
        m_entities[1]->removeReference();
    }
    hkpModifierConstraintAtom* modifier = m_constraintModifiers;
    if (modifier != nullptr)
    {
        while (modifier->getType() > hkpConstraintAtom::TYPE_CONTACT)
        {
            hkpModifierConstraintAtom* child = static_cast<hkpModifierConstraintAtom*>(modifier->m_child);
            hkMemoryRouter::getInstance().heap().blockFree(modifier, modifier->m_modifierAtomSize);
            modifier = child;
        }
    }
    if (m_data != nullptr)
    {
        m_data->removeReference();
    }
}

// 0x00E05A30
void hkpConstraintInstance::entityAddedCallback(hkpEntity* entity)
{
}

// 0x00E05A40
void hkpConstraintInstance::entityDeletedCallback(hkpEntity* entity)
{
}

// 0x00E05A90: a constraint in a world leaves it with its entity
void hkpConstraintInstance::entityRemovedCallback(hkpEntity* entity)
{
    if (m_owner != nullptr)
    {
        hkpWorldOperationUtil::removeConstraintImmediately(entity->m_world, this, hkpWorldOperationUtil::DO_FIRE_CALLBACKS);
    }
}

// 0x00DEEF70
hkpConstraintInstance::InstanceType hkpConstraintInstance::getType() const
{
    return TYPE_NORMAL;
}

// 0x00E05A70
void hkpConstraintInstance::setPriority(ConstraintPriority priority)
{
    m_priority.m_storage = (unsigned char)priority;
    if (m_internal != nullptr)
    {
        m_internal->m_priority.m_storage = (unsigned char)priority;
    }
}

// 0x00E05CB0: the island of the first entity, or of the second when the first is fixed
hkpSimulationIsland* hkpConstraintInstance::getSimulationIsland()
{
    hkpEntity* entity = m_entities[0];
    if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        entity = m_entities[1];
    }
    return entity->m_simulationIsland;
}

// 0x00E05D70: a null body becomes (a reference to) the fixed rigid body of the other body's world
void hkpConstraintInstance::pointNullsToFixedRigidBody()
{
    for (int i = 0; i < 2; i++)
    {
        if (m_entities[i] != nullptr)
        {
            continue;
        }
        // with one body null, the xor of the two is the other one
        hkpEntity* other = (hkpEntity*)((unsigned int)m_entities[0] ^ (unsigned int)m_entities[1]);
        if (other != nullptr && other->m_world != nullptr)
        {
            m_entities[i] = other->m_world->m_fixedRigidBody;
            m_entities[i]->addReference();
        }
    }
}

// 0x00E05C30: the world's fixed rigid body goes back to null (and its reference with it)
void hkpConstraintInstance::setFixedRigidBodyPointersToZero(hkpWorld* world)
{
    for (int i = 0; i < 2; i++)
    {
        if (m_entities[i] == world->m_fixedRigidBody)
        {
            m_entities[i]->removeReference();
            m_entities[i] = nullptr;
        }
    }
}
