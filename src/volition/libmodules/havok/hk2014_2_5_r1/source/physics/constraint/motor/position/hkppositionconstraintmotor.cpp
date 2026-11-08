// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/position/hkppositionconstraintmotor.cpp
//
// Havok 2014.2.5: the position motor, from rfg.exe. The solver side is the TYPE_POSITION case of hkCalcMotorData.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/position/hkppositionconstraintmotor.h"

#include <stddef.h>

static_assert(sizeof(hkpPositionConstraintMotor) == 0x24, "hkpPositionConstraintMotor");
static_assert(offsetof(hkpPositionConstraintMotor, m_tau) == 0x14, "hkpPositionConstraintMotor::m_tau");
static_assert(offsetof(hkpPositionConstraintMotor, m_constantRecoveryVelocity) == 0x20, "hkpPositionConstraintMotor::m_constantRecoveryVelocity");

// 0x00D70580: the current position is not stored.
hkpPositionConstraintMotor::hkpPositionConstraintMotor(float)
{
    m_type = hkpConstraintMotor::TYPE_POSITION;
    m_minForce = -1000000.0f;
    m_maxForce = 1000000.0f;
    m_tau = 0.8f;
    m_damping = 1.0f;
    m_proportionalRecoveryVelocity = 2.0f;
    m_constantRecoveryVelocity = 1.0f;
}

// 0x00D70650
hkpPositionConstraintMotor::~hkpPositionConstraintMotor()
{
}

// 0x00D705E0: a copy of the parameters
hkpConstraintMotor* hkpPositionConstraintMotor::clone() const
{
    hkpPositionConstraintMotor* motor = new hkpPositionConstraintMotor();
    motor->m_type = m_type;
    motor->m_minForce = m_minForce;
    motor->m_maxForce = m_maxForce;
    motor->m_tau = m_tau;
    motor->m_damping = m_damping;
    motor->m_proportionalRecoveryVelocity = m_proportionalRecoveryVelocity;
    motor->m_constantRecoveryVelocity = m_constantRecoveryVelocity;
    return motor;
}
