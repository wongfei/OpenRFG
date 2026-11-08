// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/position/hkppositionconstraintmotor.h
//
// Havok 2014.2.5: the motor driving a constraint to a target position (turret_internal.cpp's aim motors), from rfg.exe
// with the layout of the 2014.1 header. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/limitedforce/hkplimitedforceconstraintmotor.h"

// size 0x24
class hkpPositionConstraintMotor : public hkpLimitedForceConstraintMotor
{
public:
    HK_DECLARE_CLASS_ALLOCATOR();
    hkpPositionConstraintMotor(float currentPosition = 0.0f); // 0x00D70580
    virtual ~hkpPositionConstraintMotor(); // vtbl+0x00 0x00D70650
    virtual hkpConstraintMotor* clone() const; // vtbl+0x10 0x00D705E0

    float m_tau; // 0x14
    float m_damping; // 0x18
    float m_proportionalRecoveryVelocity; // 0x1C
    float m_constantRecoveryVelocity; // 0x20
};
