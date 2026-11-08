// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolvergettoivelocities.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkSimdFloat32;
class hkVector4f;
class hkp1Lin2AngJacobian;
class hkpJacobianSchema;
struct hkpSolverInfo;
class hkpVelocityAccumulator;

hkSimdFloat32 hkSolver_calcVelocity(const hkpVelocityAccumulator& ma, const hkpVelocityAccumulator& mb, const hkp1Lin2AngJacobian* jac); // 0x00E9BA90
void hkSolver_computeLinearRelativeVelocity(const hkpVelocityAccumulator* regA, const hkpVelocityAccumulator* regB, const hkVector4f& armA, const hkVector4f& armB, hkVector4f& velocityOut); // 0x00E9BAE0
int hkSolveGetToiViolatingConstraintVelocity(hkpSolverInfo& i, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums, int maxNumVelocities, float* velocitiesOut); // 0x00E9BC30
