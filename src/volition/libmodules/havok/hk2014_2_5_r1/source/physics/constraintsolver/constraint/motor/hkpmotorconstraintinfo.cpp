// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/motor/hkpmotorconstraintinfo.cpp
//
// Havok 2014.2.5: the motor parameters of a motor row, from rfg.exe. The velocity motor (havok_constraint_add_motor)
// and the position motor (turret_internal.cpp) are ported; the spring damper and callback motors report an error and
// leave the output unset.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/motor/hkpmotorconstraintinfo.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/position/hkppositionconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"

#include <math.h>

// 0x01049040: a velocity motor drives towards its velocity target (or, when told to, the constraint target's change
// over the frame) with its force range and tau (damping = tau); the target position is the last internal result
void hkCalcMotorData(const hkpConstraintMotor* someMotor, const hkpConstraintMotorInput* input, hkpConstraintMotorOutput* output)
{
    switch (someMotor->m_type.m_storage)
    {
    case hkpConstraintMotor::TYPE_VELOCITY:
    {
        const hkpVelocityConstraintMotor* motor = static_cast<const hkpVelocityConstraintMotor*>(someMotor);
        float targetVelocity;
        if (!motor->m_useVelocityTargetFromConstraintTargets.m_bool)
        {
            targetVelocity = motor->m_velocityTarget;
        }
        else
        {
            targetVelocity = (input->m_positionError + input->m_deltaTarget) * input->m_stepInfo->m_frameInvDeltaTime.val();
        }
        output->m_targetVelocity = targetVelocity;
        output->m_targetPosition = input->m_lastResults.m_internalSolverData;
        output->m_maxForce = motor->m_maxForce;
        output->m_minForce = motor->m_minForce;
        output->m_tau = motor->m_tau;
        output->m_damping = motor->m_tau;
        return;
    }
    case hkpConstraintMotor::TYPE_POSITION:
    {
        // The turret aim motors: the recovery velocity is limited to the constant one and to the position error.
        const hkpPositionConstraintMotor* motor = static_cast<const hkpPositionConstraintMotor*>(someMotor);
        const float positionError = input->m_positionError;
        const float deltaTime = input->m_stepInfo->m_frameDeltaTime.val();
        const float internalResult = input->m_lastResults.m_internalSolverData;
        float maxRecovery = motor->m_constantRecoveryVelocity * deltaTime;
        const float proportional = motor->m_proportionalRecoveryVelocity * positionError * deltaTime;
        const float remaining = positionError - proportional;
        float correction = positionError;
        if (maxRecovery < fabsf(remaining))
        {
            if (remaining <= 0.0f)
            {
                maxRecovery = -maxRecovery;
            }
            correction = maxRecovery + proportional;
        }
        const float upper = fabsf(positionError) - internalResult;
        const float lower = -fabsf(positionError) - internalResult;
        if (correction <= lower)
        {
            correction = lower;
        }
        if (upper <= correction)
        {
            correction = upper;
        }
        output->m_targetVelocity = (correction + input->m_deltaTarget) * input->m_stepInfo->m_frameInvDeltaTime.val();
        output->m_targetPosition = internalResult;
        output->m_maxForce = motor->m_maxForce;
        output->m_minForce = -motor->m_maxForce; // Original: the negated maximum, not m_minForce
        output->m_tau = motor->m_tau;
        output->m_damping = motor->m_damping;
        return;
    }
    case hkpConstraintMotor::TYPE_SPRING_DAMPER:
    case hkpConstraintMotor::TYPE_CALLBACK:
        hkErrorFwd::messageError(0x4F70455A, "OpenRFG: hkCalcMotorData: motor type not ported", "hkpmotorconstraintinfo.cpp", __LINE__);
        return;
    default:
        return;
    }
}
