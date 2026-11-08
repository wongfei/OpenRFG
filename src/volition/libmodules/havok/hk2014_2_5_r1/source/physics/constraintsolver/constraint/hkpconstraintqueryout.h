// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h
//
// Havok 2014.2.5 (havok_impl.md HK8): where a constraint's buildJacobian writes, from rfg.pdb and the Havok 2014.1
// header. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpJacobianSchema;
struct hkpConstraintRuntime;

// size 0xC
class hkpConstraintQueryOut
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintQueryOut);
    hkpConstraintQueryOut() {} // 0x00E0D550

    hkPadSpu<hkpJacobianSchema*> m_jacobianSchemas; // 0x00: advanced by every schema written
    hkPadSpu<hkpConstraintRuntime*> m_constraintRuntime; // 0x04
    hkPadSpu<hkpConstraintRuntime*> m_instanceRuntime; // 0x08
};
