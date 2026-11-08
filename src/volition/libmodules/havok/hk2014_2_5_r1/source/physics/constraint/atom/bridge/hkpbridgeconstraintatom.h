// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/bridge/hkpbridgeconstraintatom.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"

class hkClass;
class hkFinishLoadedObjectFlag;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;

// size 0x10
struct hkpBridgeConstraintAtom : public hkpConstraintAtom
{
    static const hkClass& staticClass(); // 0x010384B0
    // 0x00DF58C0: the constraint data pointer was loaded, the solver functions are re-fetched from it
    hkpBridgeConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f)
    {
        if (f.m_finishing)
        {
            init(m_constraintData);
        }
    }
    hkpBridgeConstraintAtom(); // 0x00D70490
    void afterReflectNew(); // 0x00DF5830
    void init(hkpConstraintData* constraintData); // 0x00D710B0
    hkpConstraintAtom* next();
    int numSolverResults() const; // 0x00E986B0

    void (*m_buildJacobianFunc)(hkpConstraintData*, const hkpConstraintQueryIn&, hkpConstraintQueryOut&); // 0x04
    hkViewPtr<hkpConstraintData> m_constraintData; // 0x08
    unsigned char m_padding[4]; // 0x0C
};

// size 0x10
struct hkpBridgeAtoms
{
    static const hkClass& staticClass(); // 0x01038490
    hkpBridgeAtoms(hkFinishLoadedObjectFlag f) : m_bridgeAtom(f) {} // 0x00DF58E0
    hkpBridgeAtoms(); // 0x00D704B0
    const hkpConstraintAtom* getAtoms() const; // 0x00D70340
    int getSizeOfAllAtoms() const; // 0x00D70350

    hkpBridgeConstraintAtom m_bridgeAtom; // 0x00
};
