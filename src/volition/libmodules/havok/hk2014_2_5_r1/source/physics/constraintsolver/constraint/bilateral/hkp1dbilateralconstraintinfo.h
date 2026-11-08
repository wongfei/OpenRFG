// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/bilateral/hkp1dbilateralconstraintinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkMatrix3f;
class hkp1Lin2AngJacobian;
class hkp1dLinearBilateralUserTauConstraintInfo;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
class hkpSolverResults;

// size 0x30 (a prismatic constraint's friction)
class hkp1dLinearFrictionInfo
{
public:
    hkVector4f m_pivot; // 0x00
    hkVector4f m_constrainedDofW; // 0x10
    float m_maxFrictionForce; // 0x20
    hkpSolverResults* m_lastSolverResults; // 0x24
};

// size 0x30 (a prismatic constraint's linear rows)
class hkp1dLinearBilateralConstraintInfo
{
public:
    hkVector4f m_pivotA; // 0x00
    hkVector4f m_pivotB; // 0x10
    hkVector4f m_constrainedDofW; // 0x20
};

// size 0x40 (a prismatic constraint's range)
class hkp1dLinearLimitInfo : public hkp1dLinearBilateralConstraintInfo
{
public:
    float m_min; // 0x30
    float m_max; // 0x34
};

// size 0x30 (multiplayer.md MP7: a hinge's two constrained axes)
class hkp1dAngularBilateralConstraintInfo
{
public:
    hkVector4f m_zeroErrorAxisAinW; // 0x00
    hkVector4f m_constrainedDofW; // 0x10
    hkVector4f m_perpZeroErrorAxisBinW; // 0x20
};

void hk1dAngularBilateralConstraintBuildJacobian(const hkp1dAngularBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00D730B0
void hk1dLinearFrictionBuildJacobian(const hkp1dLinearFrictionInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00D73200
void hk1dLinearBilateralConstraintBuildJacobian(const hkp1dLinearBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00D73380
void hk1dLinearBilateralConstraintBuildJacobianWithCustomRhs(const hkp1dLinearBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out, float customRhs); // 0x00D736B0
void hk1dLinearBilateralConstraintBuildJacobianWithCustomRhs_noSchema(const hkp1dLinearBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac, float customRhs); // 0x00D73820
void hk1dLinearBilateralConstraintUserTauBuildJacobian(const hkp1dLinearBilateralUserTauConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00D73980
void hk1dLinearLimitBuildJacobian(const hkp1dLinearLimitInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00D73B30
void hkSetInvMassBuildJacobian(const hkVector4f& invMassA, const hkVector4f& invMassB, hkpConstraintQueryOut& out); // 0x00D72FF0
void hkAddVelocityBuildJacobian(const hkVector4f& deltaVel, int bodyIndex, hkpConstraintQueryOut& out); // 0x00D73020
void hkSetCenterOfMassBuildJacobian(const hkMatrix3f& angToLinVelA, const hkMatrix3f& angToLinVelB, hkpConstraintQueryOut& out); // 0x00D73050
