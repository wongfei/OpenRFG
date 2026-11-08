// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/limitedforce/hkplimitedforceconstraintmotor.h
//
// Havok 2014.2.5: a motor with a force range, from rfg.pdb. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/hkpconstraintmotor.h"

// size 0x14
class hkpLimitedForceConstraintMotor : public hkpConstraintMotor
{
public:
    float m_minForce; // 0x0C
    float m_maxForce; // 0x10
};
