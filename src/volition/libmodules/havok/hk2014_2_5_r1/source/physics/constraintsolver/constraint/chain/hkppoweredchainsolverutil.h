// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/chain/hkppoweredchainsolverutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkBool;
class hkMatrix6f;
struct hkSolverStepTemp;
class hkVector8f;
class hkp1Lin2AngJacobian;
class hkp2AngJacobian;
struct hkp3dAngularMotorSolverInfo;
struct hkpCfmParam;
struct hkpChainSolverInfo;
struct hkpConstraintChainMatrix6Triple;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpPoweredChainBuildJacobianParams;
class hkpVelocityAccumulator;
class hkpVelocityAccumulatorOffset;

void hkPoweredChainBuildJacobian(const hkpPoweredChainBuildJacobianParams& params, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x01048350
void hkPoweredChain_ComputeConstraintMatrix_NextOffDiagonalAtRow(int row, hkp1Lin2AngJacobian* j, hkp2AngJacobian* jAng, hkpVelocityAccumulatorOffset* va, const hkpVelocityAccumulator* accumsBase, hkMatrix6f& mtxNextOffdiag); // 0x01048890
void hkPoweredChain_ComputeConstraintMatrix_DiagonalAtRow(int row, const hkpCfmParam& cfm, hkp1Lin2AngJacobian* j, hkp2AngJacobian* jAng, hkpVelocityAccumulatorOffset* va, const hkpVelocityAccumulator* accumsBase, hkMatrix6f& mtxDiag); // 0x01048B90
void hkPoweredChain_BuildConstraintMatrixAndLuDecomposition(int numConstraints, const hkpCfmParam& cfm, hkp3dAngularMotorSolverInfo* motorsState, hkp1Lin2AngJacobian* j, hkp2AngJacobian* jAng, hkpVelocityAccumulatorOffset* va, const hkpVelocityAccumulator* accumsBase, hkpConstraintChainMatrix6Triple* triples, int bufferSize); // 0x010478A0
void hkPoweredChain_UpdateLuDecomposition(int firstConstraintToProcess, int numConstraints, hkp3dAngularMotorSolverInfo* motorsState, hkpConstraintChainMatrix6Triple* triples); // 0x010481D0
void hkPoweredChain_ComputeConstraintMatrixLuDecomposition_ForOneRow(hkBool isNotLastRow, hkpConstraintChainMatrix6Triple& triple, hkp3dAngularMotorSolverInfo* motorsState, hkMatrix6f& mtxPrevOffdiag, hkMatrix6f*& triplePrevUpper); // 0x01047530
void hkPoweredChain_DisableMotorInMatrixRow_ThisConstraint(const hkp3dAngularMotorSolverInfo& motorsState, hkBool isNotLastRow, hkMatrix6f& mtxDiag, hkMatrix6f& mtxNextOffdiag); // 0x010467C0
void hkPoweredChain_DisableMotorInMatrixRow_NextConstraint(const hkp3dAngularMotorSolverInfo& motorsState, hkMatrix6f& mtxNextOffdiag); // 0x01046720
void hkPoweredChain_CalculateVelocities(const hkSolverStepTemp& temp, const hkpChainSolverInfo& info, hkVector8f* velocities); // 0x01046C70
void hkPoweredChain_OverwriteVelocityWithExplicitImpulse(int modifiedConstraintIndex, int modifiedCoordinateIndex, const hkp3dAngularMotorSolverInfo* motorsState, hkVector8f* velocities); // 0x01046760
void hkPoweredChain_RestoreVelocityValue(int modifiedConstraintIndex, int modifiedCoordinateIndex, const hkpChainSolverInfo& info, hkVector8f* velocities); // 0x01046F50
void hkPoweredChain_SolveConstraintMatrix_ComputeVectorG(const hkpChainSolverInfo& info, hkVector8f* tempBufferG); // 0x010469D0
void hkPoweredChain_ScanAndDisableMotors(const hkpChainSolverInfo& info, int& modifiedConstraintIndex, int& modifiedCoordinateIndex, float& impulse, hkp3dAngularMotorSolverInfo* motorsState); // 0x01046A90
void hkPoweredChain_ScanAndEnableMotors(const hkpChainSolverInfo& info, int& modifiedConstraintIndex, int& modifiedCoordinateIndex, float& impulse, hkp3dAngularMotorSolverInfo* motorsState); // 0x01047060
