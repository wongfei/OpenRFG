// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the owner (island) side of the constraint bookkeeping, from rfg.exe: the
// solver sizes of its constraints, kept in step with each constraint's own (hkConstraintInternal).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"

// 0x00E0E770
hkpConstraintOwner::hkpConstraintOwner()
{
}

// 0x00E0E7C0
hkpConstraintOwner::~hkpConstraintOwner()
{
}

// 0x00E0E780
void hkpConstraintOwner::addConstraintToCriticalLockedIsland(hkpConstraintInstance* constraint)
{
}

// 0x00E0E790
void hkpConstraintOwner::removeConstraintFromCriticalLockedIsland(hkpConstraintInstance* constraint)
{
}

// 0x00E0E7A0
void hkpConstraintOwner::addCallbackRequest(hkpConstraintInstance* constraint, int request)
{
}

// 0x00E0E7B0
void hkpConstraintOwner::checkAccessRw()
{
}

// 0x00E0B080: the largest schema also covers the delta's whole size
void hkpConstraintInfo::add(const hkpConstraintInfo& delta)
{
    const int maxSize = m_maxSizeOfSchema > delta.m_maxSizeOfSchema ? m_maxSizeOfSchema : delta.m_maxSizeOfSchema;
    m_maxSizeOfSchema = maxSize > delta.m_sizeOfSchemas ? maxSize : delta.m_sizeOfSchemas;
    m_sizeOfSchemas += delta.m_sizeOfSchemas;
    m_numSolverResults += delta.m_numSolverResults;
    m_numSolverElemTemps += delta.m_numSolverElemTemps;
}

// 0x00E0B0C0: the owner (as hkpConstraintInfo::add) and the constraint grow by the delta; the largest schema covers
// the constraint's whole schema
void hkpConstraintOwner::addConstraintInfo(hkpConstraintInstance* constraint, hkpConstraintInfo& delta)
{
    if (constraint->m_internal == nullptr)
    {
        return;
    }
    m_constraintInfo.add(delta);
    hkConstraintInternal* ci = constraint->m_internal;
    ci->m_numSolverResults = (unsigned short)(ci->m_numSolverResults + delta.m_numSolverResults);
    ci->m_numSolverElemTemps += delta.m_numSolverElemTemps;
    ci->m_sizeOfSchemas += delta.m_sizeOfSchemas;
    const int size = (int)constraint->m_internal->m_sizeOfSchemas;
    m_constraintInfo.m_maxSizeOfSchema = size < m_constraintInfo.m_maxSizeOfSchema ? m_constraintInfo.m_maxSizeOfSchema : size;
}

// 0x00E0AC50: the largest schema stays
void hkpConstraintOwner::subConstraintInfo(hkpConstraintInstance* constraint, hkpConstraintInfo& delta)
{
    if (constraint->m_internal == nullptr)
    {
        return;
    }
    m_constraintInfo.m_sizeOfSchemas -= delta.m_sizeOfSchemas;
    m_constraintInfo.m_numSolverResults -= delta.m_numSolverResults;
    m_constraintInfo.m_numSolverElemTemps -= delta.m_numSolverElemTemps;
    hkConstraintInternal* ci = constraint->m_internal;
    ci->m_numSolverResults = (unsigned short)(ci->m_numSolverResults - delta.m_numSolverResults);
    ci->m_sizeOfSchemas -= delta.m_sizeOfSchemas;
    ci->m_numSolverElemTemps -= delta.m_numSolverElemTemps;
}
