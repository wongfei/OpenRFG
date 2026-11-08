// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/bridge/hkpbridgeconstraintatom.cpp
//
// Havok 2014.2.5: the atom that hands the Jacobian building to its constraint data (hkpBreakableConstraintData), from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/bridge/hkpbridgeconstraintatom.h"

static_assert(sizeof(hkpBridgeConstraintAtom) == 0x10, "hkpBridgeConstraintAtom");

// 0x00D71090: the data's buildJacobian
static void hkBridgeConstraintAtom_callData(hkpConstraintData* m_constraintData, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    m_constraintData->buildJacobian(in, out);
}

// 0x00D70490
hkpBridgeConstraintAtom::hkpBridgeConstraintAtom()
    : hkpConstraintAtom(TYPE_BRIDGE)
{
}

// 0x00D710B0
void hkpBridgeConstraintAtom::init(hkpConstraintData* constraintData)
{
    m_constraintData = constraintData;
    m_buildJacobianFunc = hkBridgeConstraintAtom_callData;
}

// 0x00D704B0
hkpBridgeAtoms::hkpBridgeAtoms()
{
}
