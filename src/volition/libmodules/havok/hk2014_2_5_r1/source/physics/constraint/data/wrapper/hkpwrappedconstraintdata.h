// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/wrapper/hkpwrappedconstraintdata.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
class hkTransformf;
class hkVector4f;
struct hkpConstraintRuntime;
class hkpSolverResults;

// size 0x10
class hkpWrappedConstraintData : public hkpConstraintData
{
public:
    static const hkClass& staticClass(); // 0x01038E30
    hkpWrappedConstraintData(hkFinishLoadedObjectFlag f); // 0x01038ED0
    hkpWrappedConstraintData(hkpConstraintData* wrappedConstraintData); // 0x01038EF0
    virtual ~hkpWrappedConstraintData(); // vtbl+0x00 0x00DF5900
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x01038E50
    virtual hkBool isValid() const; // vtbl+0x18 0x01038E60
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const; // vtbl+0x48 0x01038EC0
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x01038E80
    virtual hkpSolverResults* getSolverResults(hkpConstraintRuntime* runtime) const; // vtbl+0x50 0x01038EA0
    virtual hkpConstraintData::UpdateAtomsResult::Enum updateDirtyAtoms(); // vtbl+0x58 0x01038EB0
    virtual const hkpConstraintData* getWrappedConstraintData() const; // vtbl+0x68 0x01038F30
    virtual hkpConstraintData* accessWrappedConstraintData(); // vtbl+0x6C 0x01038F40
    virtual hkpWrappedConstraintData* deepClone() const = 0; // vtbl+0x70

public:
    hkRefPtr<hkpConstraintData> m_constraintData; // 0x0C
};
