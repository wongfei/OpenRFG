// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/chain/hkpchainconstraintinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
class hkp1Lin2AngJacobian;
class hkpChainLinkData;
struct hkpConstraintChainMatrixTriple;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
class hkpVelocityAccumulator;
class hkpVelocityAccumulatorOffset;

void buildConstraintMatrixLuDecompositionForBallAndSocketChain(int numConstraints, float cfm, hkp1Lin2AngJacobian* j, const hkpVelocityAccumulatorOffset* va, const hkpVelocityAccumulator* accumBase, hkpConstraintChainMatrixTriple* triples, int bufferSize); // 0x0104A120
void hkBallSocketChainBuildJacobian(int numConstraints, float tau, float damping, float cfm, hkpVelocityAccumulatorOffset* accumulators, const hkpVelocityAccumulator* accumBase, hkp1Lin2AngJacobian* jacobiansEnd, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x0104A6A0
void hkStabilizedBallSocketChainBuildJacobian(int numConstraints, float tau, float damping, float cfm, float ipm, const hkVector4f& link0PivotBVelocityIn, const hkpVelocityAccumulatorOffset* accumulators, const hkpVelocityAccumulator* accumBase, hkpChainLinkData* linkDatasEnd, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x010494E0
void hkStiffSpringChainBuildJacobian(int numConstraints, float tau, float damping, float cfm, hkpVelocityAccumulatorOffset* accumulators, const hkpVelocityAccumulator* accumBase, hkp1Lin2AngJacobian* jacobiansEnd, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x01049720
void hkBallSocketConstraintBuildJacobian_noSchema_Proj(const hkVector4f& posA, const hkVector4f& posB, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac); // 0x010497D0
void hkBallSocketConstraintBuildJacobian_noSchema_noProj(const hkVector4f& posA, const hkVector4f& posB, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac); // 0x01049C80
void hkStabilizedBallSocketConstraintBuildJacobian_noSchema(const hkVector4f& posA, const hkVector4f& posB, float maxAllowedErrorDistance, const hkpConstraintQueryIn& in, hkp1Lin2AngJacobian* jac); // 0x0104A760
