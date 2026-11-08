// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolver.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). The integrateVelocities template declared (HK8).
// Hand-maintained.
#pragma once


class hkSimdFloat32;
struct hkSolverStepTemp;
class hkVector4f;
class hkp1Lin2AngJacobian;
class hkpJacobianSchema;
struct hkpSolverElemTemp;
struct hkpSolverInfo;
class hkpVelocityAccumulator;

namespace hkSolver
{
    int _stepJacobian(const hkpSolverInfo& info, hkSolverStepTemp& temp, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accums, hkpSolverElemTemp* srIn); // 0x00EB1680
    int stepJacobian(const hkpSolverInfo& info, hkSolverStepTemp& temp, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accums, hkpSolverElemTemp* srIn); // 0x00EA0520
    void integrateVelocityAccumulator(const hkpSolverInfo& info, hkpVelocityAccumulator* m); // 0x00EA9350
    template<int LAST_STEP> void integrateVelocities(const hkpSolverInfo& info, const hkVector4f& velocityField, const hkSimdFloat32& sumVelFactor, hkpVelocityAccumulator* acc, hkpVelocityAccumulator* accEnd); // <1> 0x00EABBE0, <0> 0x00EABEA0
    void _applyVelField(const hkpSolverInfo& info, const hkVector4f& glovalVelField, hkpVelocityAccumulator* acc, hkpVelocityAccumulator* accEnd); // 0x00EAF890
    void applyVelField(const hkpSolverInfo& info, const hkVector4f& glovalVelField, hkpVelocityAccumulator* acc, hkpVelocityAccumulator* accEnd); // 0x00EA0110
    void solve(const hkpSolverInfo& info, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* elemTemp); // 0x00EB35E0
    // template instance: hkSimdFloat32 getLinearDv0UserTauWithCustomRhs<hkpVelocityAccumulator,hkpSolverInfo>(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping, const hkSimdFloat32& rhs); // 0x00EA9850
    hkSimdFloat32 getLinearDv0UserTauWithCustomRhs_zeroSumVel(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping, const hkSimdFloat32& customRhs); // 0x00EAA2E0
    // template instance: hkSimdFloat32 getLinearDv0UserTau<hkpVelocityAccumulator,hkpSolverInfo>(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping); // 0x00EAA440
    hkSimdFloat32 getLinearDv0UserTau_zeroSumVel(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping); // 0x00EAA360
    // template instance: hkSimdFloat32 getLinearDv0<hkpVelocityAccumulator,hkpSolverInfo>(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info); // 0x00EA9A80
    // template instance: hkSimdFloat32 getVelocity<hkpVelocityAccumulator>(const hkp1Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB); // 0x00D72DE0
    // template instance: hkSimdFloat32 getAngularDv0UserTauWithCustomRhs<hkpVelocityAccumulator,hkpSolverInfo>(const hkp2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping, const hkSimdFloat32& customAngularRhs); // 0x00EA9DA0
    // template instance: hkSimdFloat32 getAngularDv0UserTau<hkpVelocityAccumulator,hkpSolverInfo>(const hkp2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, const hkSimdFloat32& tau, const hkSimdFloat32& damping); // 0x00EAC2F0
    // template instance: hkSimdFloat32 getAngularDv0<hkpVelocityAccumulator,hkpSolverInfo>(const hkp2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info); // 0x00EA9C80
    // template instance: hkSimdFloat32 getDv2Lin2Ang<hkpVelocityAccumulator,hkpSolverInfo>(const hkp2Lin2AngJacobian& jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info); // 0x00EA9E60
    // template instance: void getDv01<hkpVelocityAccumulator,hkpSolverInfo>(const hkp1Lin2AngJacobian* jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, hkSimdFloat32& vel0Out, hkSimdFloat32& vel1Out); // 0x00EA9AE0
    // template instance: void getDv012<hkpVelocityAccumulator,hkpSolverInfo>(const hkpJacTriple2Bil1Ang* jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const hkpSolverInfo& info, hkSimdFloat32& velOut0, hkSimdFloat32& velOut1, hkSimdFloat32& velOut2); // 0x00EAC210
    // template instance: void applyImpulse<hkpVelocityAccumulator>(const hkSimdFloat32& imp, const hkp1Lin2AngJacobian& jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* sr); // 0x00D72990
    // template instance: void applyImpulse2Lin2Ang<hkpVelocityAccumulator>(const hkSimdFloat32& imp, const hkp2Lin2AngJacobian& jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* sr); // 0x00EA9EC0
    // template instance: void applyAngularImpulse<hkpVelocityAccumulator>(const hkSimdFloat32& imp, const hkp2AngJacobian& jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* sr); // 0x00EA9680
    // template instance: void applyImpulse01<hkpVelocityAccumulator>(const hkSimdFloat32& imp0, const hkSimdFloat32& imp1, const hkp1Lin2AngJacobian* jacs, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* srs); // 0x00EA9B90
    // template instance: void prefetchVelocityAccumulators<hkpVelocityAccumulator>(hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB); // 0x00EA8C50
    // template instance: void applyLinearImpulse<hkpVelocityAccumulator>(const hkVector4f& vLinImpulse, const hkVector4f& vLocalAngImpulseA, const hkVector4f& vLocalAngImpulseB, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB); // 0x00EA9D40
    // template instance: void applyLocalAngularImpulse<hkpVelocityAccumulator>(const hkVector4f& vLocalAngImpulseA, const hkVector4f& vLocalAngImpulseB, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB); // 0x00EA9FC0
}
