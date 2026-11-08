// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.cpp
//
// Havok 2014.2.5: the velocity motor, from rfg.exe (havok_constraint_add_motor fills one in place).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h"

#include <stddef.h>

static_assert(sizeof(hkpVelocityConstraintMotor) == 0x20, "hkpVelocityConstraintMotor");
static_assert(offsetof(hkpVelocityConstraintMotor, m_minForce) == 0x0C, "hkpVelocityConstraintMotor::m_minForce");
static_assert(offsetof(hkpVelocityConstraintMotor, m_velocityTarget) == 0x18, "hkpVelocityConstraintMotor::m_velocityTarget");

// 0x00816280
hkpVelocityConstraintMotor::~hkpVelocityConstraintMotor()
{
}

// 0x00D6E140: a copy of the parameters
hkpConstraintMotor* hkpVelocityConstraintMotor::clone() const
{
    hkpVelocityConstraintMotor* motor = new hkpVelocityConstraintMotor();
    motor->m_type = m_type;
    motor->m_minForce = m_minForce;
    motor->m_maxForce = m_maxForce;
    motor->m_tau = m_tau;
    motor->m_velocityTarget = m_velocityTarget;
    motor->m_useVelocityTargetFromConstraintTargets = m_useVelocityTargetFromConstraintTargets;
    return motor;
}
