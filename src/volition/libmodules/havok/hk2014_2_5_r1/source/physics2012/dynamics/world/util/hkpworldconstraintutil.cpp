// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): adding constraints to and removing them from the world (the master entity's
// hkConstraintInternal array sorted by priority, the runtimes packed in the master's runtime buffer, the slave's
// back pointer) and the modifier atom chain, from rfg.exe. The binary moves runtimes with hkString::memMove
// (0x00CFF1D0), a plain memmove like hkMemUtil::memMove.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpresponsemodifier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

static inline hkpEntity* hkWorldConstraintUtil_slave(const hkConstraintInternal* internal)
{
    return internal->m_entities[1 - internal->m_whoIsMaster];
}

static inline hkpConstraintInstance*& hkWorldConstraintUtil_slaveEntry(hkpEntity* slave, int index)
{
    return *(hkpConstraintInstance**)&slave->m_constraintsSlave[index];
}

// 0x00E0EE10: points whoever referenced oldAtom (the instance's first modifier, the internal's atoms or the modifier
// before it in the chain) at updatedAtom
void hkpWorldConstraintUtil::updateFatherOfMovedAtom(hkpConstraintInstance* instance, const hkpConstraintAtom* oldAtom, const hkpConstraintAtom* updatedAtom, int updatedSizeOfAtom)
{
    hkpModifierConstraintAtom* modifier = instance->m_constraintModifiers;
    hkConstraintInternal* internal = instance->m_internal;
    if (modifier == nullptr || modifier == oldAtom)
    {
        if (modifier != nullptr)
        {
            instance->m_constraintModifiers = (hkpModifierConstraintAtom*)updatedAtom;
        }
        if (internal != nullptr)
        {
            internal->m_atoms = const_cast<hkpConstraintAtom*>(updatedAtom);
            internal->m_atomsSize = (unsigned short)updatedSizeOfAtom;
        }
        return;
    }
    hkpModifierConstraintAtom* father = modifier;
    for (hkpModifierConstraintAtom* atom = (hkpModifierConstraintAtom*)modifier->m_child; atom != oldAtom; atom = (hkpModifierConstraintAtom*)atom->m_child)
    {
        father = atom;
    }
    father->m_child = const_cast<hkpConstraintAtom*>(updatedAtom);
    father->m_childSize = (unsigned short)updatedSizeOfAtom;
}

// 0x00E0EF10
void hkpWorldConstraintUtil::removeConstraint(hkpConstraintInstance* constraint)
{
    hkpSimulationIsland* island = static_cast<hkpSimulationIsland*>(constraint->m_owner);
    hkConstraintInternal* internal = constraint->m_internal;
    island->m_splitCheckRequested = true;

    // hkpConstraintOwner::subConstraintInfo (inlined; the maximum schema size stays)
    {
        const unsigned int sizeOfSchemas = internal->m_sizeOfSchemas;
        const unsigned int numSolverElemTemps = internal->m_numSolverElemTemps;
        const unsigned short numSolverResults = internal->m_numSolverResults;
        if (constraint->m_internal != nullptr)
        {
            hkpConstraintOwner* owner = constraint->m_owner;
            owner->m_constraintInfo.m_sizeOfSchemas -= sizeOfSchemas;
            owner->m_constraintInfo.m_numSolverResults -= numSolverResults;
            owner->m_constraintInfo.m_numSolverElemTemps -= numSolverElemTemps;
            hkConstraintInternal* ci = constraint->m_internal;
            ci->m_numSolverResults = (unsigned short)(ci->m_numSolverResults - numSolverResults);
            ci->m_numSolverElemTemps -= numSolverElemTemps;
            ci->m_sizeOfSchemas -= sizeOfSchemas;
        }
    }
    island->m_numConstraints--;
    constraint->m_owner = nullptr;

    // the slave's back pointer: the last one takes its place
    {
        const unsigned short slaveIndex = internal->m_slaveIndex;
        hkpEntity* slave = hkWorldConstraintUtil_slave(internal);
        hkpConstraintInstance* last = hkWorldConstraintUtil_slaveEntry(slave, slave->m_constraintsSlave.getSize() - 1);
        hkWorldConstraintUtil_slaveEntry(slave, slaveIndex) = last;
        slave->m_constraintsSlave.m_size--;
        last->m_internal->m_slaveIndex = slaveIndex;
    }

    // the runtime leaves the master's buffer; the runtimes behind it move down
    hkpEntity* master = internal->m_entities[internal->m_whoIsMaster];
    int runtimeShift = 0;
    if (internal->m_runtime != nullptr)
    {
        runtimeShift = -(int)internal->m_runtimeSize;
        unsigned char* runtime = (unsigned char*)internal->m_runtime;
        unsigned char* next = runtime + internal->m_runtimeSize;
        hkMemUtil::memMove(runtime, next, (int)(master->m_constraintRuntime.begin() + master->m_constraintRuntime.getSize() - next));
        master->m_constraintRuntime.m_size -= internal->m_runtimeSize;
        internal->m_runtime = nullptr;
    }
    constraint->m_internal = nullptr;

    // the internals behind it move down (removeAtAndCopy, with the back pointers and runtimes fixed)
    hkSmallArray<hkConstraintInternal>& masters = master->m_constraintsMaster;
    for (; internal < masters.begin() + (masters.getSize() - 1); internal++)
    {
        *internal = internal[1];
        internal->m_constraint->m_internal = internal;
        internal->m_runtime = internal->m_runtime != nullptr ? (hkpConstraintRuntime*)((unsigned char*)internal->m_runtime + runtimeShift) : nullptr;
    }
    masters.m_size--;
    constraint->removeReference();
}

// 0x00E0F1A0: the first modifier of the given type in the chain
hkpModifierConstraintAtom* hkpWorldConstraintUtil::findModifier(hkpConstraintInstance* instance, hkpConstraintAtom::AtomType type)
{
    hkpModifierConstraintAtom* modifier = instance->m_constraintModifiers;
    if (modifier == nullptr)
    {
        return nullptr;
    }
    if (modifier->m_type.m_storage == type)
    {
        return modifier;
    }
    for (;;)
    {
        modifier = (hkpModifierConstraintAtom*)modifier->m_child;
        if (modifier->m_type.m_storage < hkpConstraintAtom::FIRST_MODIFIER_TYPE)
        {
            return nullptr;
        }
        if (modifier->m_type.m_storage == type)
        {
            return modifier;
        }
    }
}

// 0x00E0F1D0: the modifier whose child is the constraint's own atoms
hkpModifierConstraintAtom* hkpWorldConstraintUtil::findLastModifier(hkpConstraintInstance* instance)
{
    hkpModifierConstraintAtom* modifier = instance->m_constraintModifiers;
    if (modifier == nullptr)
    {
        return nullptr;
    }
    while (modifier->m_child->m_type.m_storage > hkpConstraintAtom::TYPE_CONTACT)
    {
        modifier = (hkpModifierConstraintAtom*)modifier->m_child;
    }
    return modifier;
}

// 0x00E0F200: moving surface modifiers go last (next to the constraint's atoms), everything else first, except
// behind an ignore modifier, which stays first
void hkpWorldConstraintUtil::addModifier(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner, hkpModifierConstraintAtom* newModifier)
{
    constraintOwner.checkAccessRw();
    hkpModifierConstraintAtom* last = findLastModifier(instance);
    hkConstraintInternal* internal = instance->m_internal;
    if (newModifier->m_type.m_storage == hkpConstraintAtom::TYPE_MODIFIER_MOVING_SURFACE && last != nullptr)
    {
        newModifier->m_child = last->m_child;
        newModifier->m_childSize = last->m_childSize;
        last->m_child = newModifier;
        last->m_childSize = newModifier->m_modifierAtomSize;
    }
    else
    {
        hkpModifierConstraintAtom* first = instance->m_constraintModifiers;
        if (first != nullptr && first->m_type.m_storage == hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT)
        {
            newModifier->m_child = first->m_child;
            newModifier->m_childSize = instance->m_constraintModifiers->m_childSize;
            instance->m_constraintModifiers->m_child = newModifier;
            instance->m_constraintModifiers->m_childSize = newModifier->m_modifierAtomSize;
        }
        else
        {
            instance->m_constraintModifiers = newModifier;
            if (internal == nullptr)
            {
                // not in a world: the modifier sits in front of the data's atoms
                hkpConstraintData::ConstraintInfo info;
                info.m_extraSchemaSize = 0;
                instance->m_data->getConstraintInfo(info);
                newModifier->m_child = info.m_atoms;
                newModifier->m_childSize = (unsigned short)info.m_sizeOfAllAtoms;
                return;
            }
            newModifier->m_child = internal->m_atoms;
            newModifier->m_childSize = internal->m_atomsSize;
            internal->m_atoms = newModifier;
            internal->m_atomsSize = newModifier->m_modifierAtomSize;
        }
    }

    if (internal != nullptr)
    {
        hkpConstraintInfo info;
        info.m_maxSizeOfSchema = 0;
        info.m_sizeOfSchemas = 0;
        info.m_numSolverResults = 0;
        info.m_numSolverElemTemps = 0;
        unsigned char usedFlags = 0;
        const int callbackRequest = newModifier->addModifierDataToConstraintInfo(info, usedFlags);
        constraintOwner.addConstraintInfo(instance, info);
        // the response modifier flags the modifier handles itself need no extra schema space
        const unsigned char flags = (unsigned char)(usedFlags & (instance->m_entities[1]->m_responseModifierFlags | instance->m_entities[0]->m_responseModifierFlags));
        const unsigned short extra = hkpResponseModifier::tableOfAdditionalSchemaSizes[flags & 7];
        constraintOwner.m_constraintInfo.m_sizeOfSchemas -= extra;
        instance->m_internal->m_sizeOfSchemas -= extra;
        if (instance->m_internal != nullptr)
        {
            instance->m_internal->m_callbackRequest |= (unsigned char)callbackRequest;
        }
    }
}

// 0x00E0F360: the master is the first entity unless that one is fixed; the islands of two non-fixed entities merge
void hkpWorldConstraintUtil::addConstraint(hkpWorld* world, hkpConstraintInstance* constraint)
{
    constraint->addReference();

    hkpConstraintData::ConstraintInfo info;
    info.m_extraSchemaSize = 0;
    constraint->m_data->getConstraintInfo(info);

    // not constructed: every field the array keeps is written below (the runtime pointer after insertion)
    alignas(16) unsigned char internalStorage[sizeof(hkConstraintInternal)];
    hkConstraintInternal& internal = *reinterpret_cast<hkConstraintInternal*>(internalStorage);
    internal.m_constraint = constraint;
    internal.m_entities[0] = constraint->m_entities[0];
    internal.m_entities[1] = constraint->m_entities[1];
    if (constraint->m_constraintModifiers != nullptr)
    {
        internal.m_atoms = constraint->m_constraintModifiers;
        internal.m_atomsSize = constraint->m_constraintModifiers->m_modifierAtomSize;
    }
    else
    {
        internal.m_atoms = info.m_atoms;
        internal.m_atomsSize = (unsigned short)info.m_sizeOfAllAtoms;
    }
    internal.m_priority = constraint->m_priority;

    hkpEntity* entityA = internal.m_entities[0];
    hkpEntity* entityB = internal.m_entities[1];
    if (entityA->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityB->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityA->m_simulationIsland != entityB->m_simulationIsland)
    {
        hkpWorldOperationUtil::mergeIslands(entityA->m_world, entityA, entityB);
    }
    internal.m_whoIsMaster = entityA->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED;
    hkpEntity* master = internal.m_entities[internal.m_whoIsMaster];
    hkpSimulationIsland* island = master->m_simulationIsland;
    constraint->m_owner = island;
    island->m_numConstraints++;

    // the callback requests of the first real atom (behind the local transforms; behind one modifier, read as the
    // first atom's child)
    internal.m_constraintType.m_storage = hkpConstraintInstance::TYPE_NORMAL;
    internal.m_callbackRequest = 0;
    {
        const hkpConstraintAtom* atoms = internal.m_atoms;
        unsigned int type = atoms->m_type.m_storage;
        switch (type)
        {
        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSFORMS:
            type = ((const hkpConstraintAtom*)((const char*)atoms + 0x90))->m_type.m_storage; // sizeof(hkpSetLocalTransformsConstraintAtom)
            break;
        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSLATIONS:
            type = ((const hkpConstraintAtom*)((const char*)atoms + 0x30))->m_type.m_storage; // sizeof(hkpSetLocalTranslationsConstraintAtom)
            break;
        case hkpConstraintAtom::TYPE_SET_LOCAL_ROTATIONS:
            type = ((const hkpConstraintAtom*)((const char*)atoms + 0x70))->m_type.m_storage; // sizeof(hkpSetLocalRotationsConstraintAtom)
            break;
        default:
            break;
        }
        if (type - hkpConstraintAtom::FIRST_MODIFIER_TYPE <= hkpConstraintAtom::LAST_MODIFIER_TYPE - hkpConstraintAtom::FIRST_MODIFIER_TYPE)
        {
            type = ((const hkpModifierConstraintAtom*)atoms)->m_child->m_type.m_storage;
        }
        if (type == hkpConstraintAtom::TYPE_BRIDGE)
        {
            internal.m_callbackRequest = hkpConstraintAtom::CALLBACK_REQUEST_SETUP_PPU_ONLY;
            if (constraint->m_data->isBuildJacobianCallbackRequired().m_bool)
            {
                internal.m_callbackRequest |= hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK;
            }
            internal.m_constraintType.m_storage = (unsigned char)constraint->getType();
        }
        else if (type == hkpConstraintAtom::TYPE_PULLEY)
        {
            internal.m_callbackRequest = hkpConstraintAtom::CALLBACK_REQUEST_SETUP_PPU_ONLY;
        }
        else if (type == hkpConstraintAtom::TYPE_WHEEL_FRICTION)
        {
            internal.m_constraintType.m_storage = hkpConstraintInstance::TYPE_DISABLE_SPU;
            internal.m_callbackRequest = hkpConstraintAtom::CALLBACK_REQUEST_SETUP_PPU_ONLY;
        }
    }

    unsigned char usedFlags = 0;
    if (constraint->m_constraintModifiers != nullptr)
    {
        internal.m_callbackRequest |= (unsigned char)hkpModifierConstraintAtom::addAllModifierDataToConstraintInfo(constraint->m_constraintModifiers, info, usedFlags);
    }
    {
        const hkpConstraintAtom* atom = internal.m_atoms;
        while (atom->m_type.m_storage >= hkpConstraintAtom::FIRST_MODIFIER_TYPE)
        {
            atom = ((const hkpModifierConstraintAtom*)atom)->m_child;
        }
        if (atom->m_type.m_storage == hkpConstraintAtom::TYPE_CONTACT)
        {
            // the response modifiers no modifier atom handles get their extra schema space
            const unsigned char flags = (unsigned char)(constraint->m_entities[0]->m_responseModifierFlags | constraint->m_entities[1]->m_responseModifierFlags);
            info.m_sizeOfSchemas += hkpResponseModifier::tableOfAdditionalSchemaSizes[(unsigned char)(flags - (flags & usedFlags)) & 7];
        }
    }

    internal.m_numSolverResults = 0;
    internal.m_sizeOfSchemas = 0;
    internal.m_numSolverElemTemps = 0;
    constraint->m_internal = &internal;
    constraint->m_owner->addConstraintInfo(constraint, info);

    // sorted by priority, then constraint uid, then the uid of the other entity
    hkSmallArray<hkConstraintInternal>& masters = master->m_constraintsMaster;
    const int size = masters.getSize();
    int insertAt = 0;
    for (const hkConstraintInternal* ci = masters.begin(); insertAt < size; insertAt++, ci++)
    {
        const unsigned char priority = ci->m_priority.m_storage;
        if (internal.m_priority.m_storage < priority)
        {
            break;
        }
        if (internal.m_priority.m_storage > priority)
        {
            continue;
        }
        const unsigned int uid = ci->m_constraint->m_uid;
        if (constraint->m_uid < uid)
        {
            break;
        }
        if (constraint->m_uid > uid)
        {
            continue;
        }
        const hkpEntity* other = (const hkpEntity*)((size_t)ci->m_entities[1] ^ (size_t)ci->m_entities[0] ^ (size_t)master);
        const hkpEntity* newOther = (const hkpEntity*)((size_t)master ^ (size_t)internal.m_entities[1] ^ (size_t)internal.m_entities[0]);
        if (newOther->m_uid < other->m_uid)
        {
            break;
        }
    }
    // without a reallocation only the internals from insertAt on move
    const int firstMoved = size < masters.getCapacity() ? insertAt : 0;
    masters.insertAt(insertAt, internal);
    for (int i = firstMoved; i < masters.getSize(); i++)
    {
        masters[i].m_constraint->m_internal = &masters[i];
    }

    hkConstraintInternal* ci = constraint->m_internal;
    hkpConstraintData::RuntimeInfo runtimeInfo;
    ci->m_constraint->m_data->getRuntimeInfo(constraint->m_wantRuntime, runtimeInfo);
    const int runtimeSize = (runtimeInfo.m_sizeOfExternalRuntime + 15) & ~15; // 16-byte aligned
    ci->m_runtimeSize = (unsigned short)runtimeSize;
    if (runtimeSize == 0)
    {
        ci->m_runtime = nullptr;
    }
    else
    {
        // the runtime is inserted behind those of the internals in front of it; the buffer may move
        hkArray<unsigned char, hkContainerHeapAllocator>& buffer = master->m_constraintRuntime;
        unsigned char* oldData = buffer.begin();
        const int oldSize = buffer.getSize();
        const int newSize = oldSize + runtimeSize;
        if (buffer.getCapacity() < newSize)
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &buffer, newSize, 1);
        }
        buffer.m_size = newSize;
        unsigned char* runtime = buffer.begin();
        const int delta = (int)(runtime - oldData);
        for (int i = 0; i < insertAt; i++)
        {
            hkConstraintInternal& before = masters[i];
            if (before.m_runtime != nullptr)
            {
                before.m_runtime = (hkpConstraintRuntime*)((unsigned char*)before.m_runtime + delta);
                runtime = (unsigned char*)before.m_runtime + before.m_runtimeSize;
            }
        }
        hkMemUtil::memMove(runtime + ci->m_runtimeSize, runtime, (int)(buffer.begin() - runtime - ci->m_runtimeSize + buffer.getSize()));
        ci->m_runtime = (hkpConstraintRuntime*)runtime;
        const int shift = delta + ci->m_runtimeSize;
        for (int i = insertAt + 1; i < masters.getSize(); i++)
        {
            hkConstraintInternal& behind = masters[i];
            behind.m_runtime = behind.m_runtime != nullptr ? (hkpConstraintRuntime*)((unsigned char*)behind.m_runtime + shift) : nullptr;
        }
    }
    ci->m_constraint->m_data->addInstance(ci->m_runtime, ci->m_runtimeSize);

    hkpEntity* slave = hkWorldConstraintUtil_slave(ci);
    ci->m_slaveIndex = (unsigned short)slave->m_constraintsSlave.getSize();
    slave->m_constraintsSlave.pushBack(*(hkViewPtr<hkpConstraintInstance>*)&constraint);
}
