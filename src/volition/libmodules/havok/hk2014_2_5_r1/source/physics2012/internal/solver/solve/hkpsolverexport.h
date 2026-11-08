// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolverexport.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpJacobianSchema;
struct hkpSolverElemTemp;
struct hkpSolverInfo;
class hkpVelocityAccumulator;

void hkExportImpulsesAndRhs(const hkpSolverInfo& i, const hkpSolverElemTemp* temp, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums); // 0x00EB6EA0

namespace hkSolverExport
{
    void exportImpulsesAndRhs(const hkpSolverInfo& info, const hkpSolverElemTemp* solverInputs, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums); // 0x00EB5B10
    // template instance: const hkSimdFloat32 integrateRhs<hkpVelocityAccumulator>(const hkpVelocityAccumulator& ma, const hkpVelocityAccumulator& mb, const hkp1Lin2AngJacobian* jac, const hkSimdFloat32& sumVelocitiesFactor, const hkSimdFloat32& oldRhs); // 0x00EB7010
    // template instance: void integrateRhsPair<hkpVelocityAccumulator>(const hkpVelocityAccumulator& ma, const hkpVelocityAccumulator& mb, const hkp1Lin2AngJacobian* jac, const hkSimdFloat32& oldRhsFactor, const hkSimdFloat32& sumVelocitiesFactor, hkSimdFloat32& rhsOut0, hkSimdFloat32& rhsOut1); // 0x00EB71B0
    // template instance: const hkSimdFloat32 integrateRhs2Lin2Ang<hkpVelocityAccumulator>(const hkpVelocityAccumulator& ma, const hkpVelocityAccumulator& mb, const hkp2Lin2AngJacobian* jac, const hkSimdFloat32& oldRhsFactor, const hkSimdFloat32& sumVelocitiesFactor); // 0x00EB70C0
    // template instance: const hkSimdFloat32 integrateAngularRhs<hkpVelocityAccumulator>(const hkpVelocityAccumulator& ma, const hkpVelocityAccumulator& mb, const hkp2AngJacobian* jac, const hkSimdFloat32& sumVelocitiesFactor, const hkSimdFloat32& oldRhs); // 0x00EB7070
}
