// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/motor/hkpmotorconstraintinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the motor input and output structs came with
// the hinge motors (havok_constraint_add_motor).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

class hkVector4f;
class hkp1Lin2AngJacobian;
class hkpConstraintMotor;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
class hkpConstraintQueryStepInfo;

// size 0x4
class hkp1dBilateralConstraintStatus
{
public:
    float m_effMass; // 0x00
};

// size 0x18: what a motor gets: the row's effective mass, the step, the last solver results, the target's change
// and the position error
class hkpConstraintMotorInput : public hkp1dBilateralConstraintStatus
{
public:
    const hkpConstraintQueryStepInfo* m_stepInfo; // 0x04
    hkpSolverResults m_lastResults; // 0x08
    float m_deltaTarget; // 0x10
    float m_positionError; // 0x14
};

// size 0x18
class hkp1dConstraintMotorInfo
{
public:
    float m_targetPosition; // 0x00
    float m_targetVelocity; // 0x04
    float m_maxForce; // 0x08
    float m_minForce; // 0x0C
    float m_tau; // 0x10
    float m_damping; // 0x14
};

// size 0x18
class hkpConstraintMotorOutput : public hkp1dConstraintMotorInfo
{
};

void hkCalcMotorData(const hkpConstraintMotor* someMotor, const hkpConstraintMotorInput* input, hkpConstraintMotorOutput* output); // 0x01049040
void hk1dLinearVelocityMotorBeginJacobian(const hkVector4f& directionOfConstraint, const hkVector4f& pivot, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac, hkp1dBilateralConstraintStatus& statusOut); // 0x01049310
void hk1dLinearVelocityMotorCommitJacobian(hkp1dConstraintMotorInfo& info, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac, hkpConstraintQueryOut& out); // 0x010492A0
