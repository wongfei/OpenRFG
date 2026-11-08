// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h
//
// Havok 2014.2.5: the motor driving a constraint at a velocity (havok_constraint_add_motor), from rfg.pdb.
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/limitedforce/hkplimitedforceconstraintmotor.h"

// size 0x20
class hkpVelocityConstraintMotor : public hkpLimitedForceConstraintMotor
{
public:
    HK_DECLARE_CLASS_ALLOCATOR();
    hkpVelocityConstraintMotor() {}
    virtual ~hkpVelocityConstraintMotor(); // vtbl+0x00 0x00816280
    virtual hkpConstraintMotor* clone() const; // vtbl+0x10 0x00D6E140

    float m_tau; // 0x14
    float m_velocityTarget; // 0x18
    hkBool m_useVelocityTargetFromConstraintTargets; // 0x1C
};
