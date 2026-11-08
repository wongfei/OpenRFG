// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/wrapper/hkpwrappedconstraintdata.cpp
//
// Havok 2014.2.5: the base of constraint data wrapping another (hkpBreakableConstraintData), from rfg.exe. Most virtuals
// forward to the wrapped data.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/wrapper/hkpwrappedconstraintdata.h"

// 0x01038ED0 (stub_impl.md S4 SD)
hkpWrappedConstraintData::hkpWrappedConstraintData(hkFinishLoadedObjectFlag f)
    : hkpConstraintData(f)
{
}

// 0x01038EF0: the wrapped data is referenced
hkpWrappedConstraintData::hkpWrappedConstraintData(hkpConstraintData* wrappedConstraintData)
{
    if (wrappedConstraintData != nullptr)
    {
        wrappedConstraintData->addReference();
    }
    // the member is not constructed (hkRefPtr has no constructor that clears it): set without releasing
    *reinterpret_cast<hkpConstraintData**>(&m_constraintData) = wrappedConstraintData;
}

// 0x00DF5900: the wrapped data's reference goes
hkpWrappedConstraintData::~hkpWrappedConstraintData()
{
    m_constraintData = nullptr;
}

// 0x01038E50
void hkpWrappedConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    m_constraintData->getConstraintInfo(infoOut);
}

// 0x01038E60
hkBool hkpWrappedConstraintData::isValid() const
{
    return m_constraintData->isValid();
}

// 0x01038EC0
void hkpWrappedConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const
{
    m_constraintData->getAppliedLinearImpulse(worldFromBodyA, worldFromBodyB, runtimeIn, impulseOut);
}

// 0x01038E80
void hkpWrappedConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    m_constraintData->getRuntimeInfo(wantRuntime, infoOut);
}

// 0x01038EA0
hkpSolverResults* hkpWrappedConstraintData::getSolverResults(hkpConstraintRuntime* runtime) const
{
    return m_constraintData->getSolverResults(runtime);
}

// 0x01038EB0
hkpConstraintData::UpdateAtomsResult::Enum hkpWrappedConstraintData::updateDirtyAtoms()
{
    return m_constraintData->updateDirtyAtoms();
}

// 0x01038F30
const hkpConstraintData* hkpWrappedConstraintData::getWrappedConstraintData() const
{
    return m_constraintData;
}

// 0x01038F40
hkpConstraintData* hkpWrappedConstraintData::accessWrappedConstraintData()
{
    return m_constraintData;
}
