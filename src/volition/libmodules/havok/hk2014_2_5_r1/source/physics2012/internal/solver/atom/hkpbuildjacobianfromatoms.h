// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkSimdFloat32;
class hkTransformf;
class hkVector4f;
class hkp1dAngularFrictionInfo;
class hkp1dAngularLimitInfo;
class hkp1dBilateralConstraintStatus;
class hkp1dConstraintMotorInfo;
class hkp2AngJacobian;
struct hkpBallSocketConstraintAtom;
struct hkpConstraintAtom;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpSetupStabilizationAtom;
class hkpSolverResults;

void setupStabilizationFromAtom_outOfLine(const hkpSetupStabilizationAtom& atom, const hkpConstraintQueryIn& in, const hkTransformf& baseA, const hkTransformf& baseB, hkVector4f& vLocalArmA, hkVector4f& vLocalArmB, hkSimdFloat32& maxAngularImpulse, hkSimdFloat32& maxLinearImpulse, const hkpConstraintQueryOut& noOut); // 0x00E8E600
void buildSchemaFromBallSocketAtom(const hkpBallSocketConstraintAtom& atom, const hkpConstraintQueryIn& in, const hkTransformf& baseA, const hkTransformf& baseB, const hkVector4f& vLocalArmA, const hkVector4f& vLocalArmB, const hkSimdFloat32& maxAngularImpulse, const hkSimdFloat32& maxLinearImpulse, hkpConstraintQueryOut& out); // 0x00E8E800
void buildChainSchemaFromBallSocketAtom(const hkpConstraintQueryIn& in, const hkVector4f& vLocalArmA, const hkVector4f& vLocalArmB, unsigned int flags, hkpConstraintQueryOut& out); // 0x00E8F020
void hkpBeginConstraints(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out, hkpSolverResults* sr, int solverResultStriding); // 0x00E8B270
void hk1dAngularLimitBuildJacobian(const hkp1dAngularLimitInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00E8D630
void hk1dAngularVelocityMotorBeginJacobian(const hkVector4f& directionOfConstraint, const hkpConstraintQueryIn& in, hkp2AngJacobian* jac, hkp1dBilateralConstraintStatus& statusOut); // 0x00E8D790
void hk1dAngularVelocityMotorCommitJacobian(hkp1dConstraintMotorInfo& info, const hkpConstraintQueryIn& in, hkp2AngJacobian* jac, hkpConstraintQueryOut& out); // 0x00E8B2C0
void hk1dAngularFrictionBuildJacobian(const hkp1dAngularFrictionInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00E8F1B0
void hkSolverBuildJacobianFromAtomsNotContact(const hkpConstraintAtom* atoms, int sizeOfAllAtoms, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00E92680
void hkSolverBuildJacobianFromAtoms(const hkpConstraintAtom* atoms, int sizeOfAllAtoms, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00DF5850
