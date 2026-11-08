// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkStepInfo;
class hkVector4f;
class hkpMotion;
struct hkpSolverInfo;
class hkpVelocityAccumulator;

int hkRigidMotionUtilCheckDeactivation(const hkpSolverInfo& si, hkpMotion& motion); // 0x00E125E0
bool hkRigidMotionUtilCanDeactivateFinal(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset); // 0x00E0FBA0
void hkRigidMotionUtilStep(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset); // 0x00E10430
int hkRigidMotionUtilApplyForcesAndStep(const hkpSolverInfo& solverInfo, const hkStepInfo& info, const hkVector4f& deltaVel, hkpMotion* const* motions, int numMotions, int motionOffset); // 0x00E10940
hkpVelocityAccumulator* hkRigidMotionUtilBuildAccumulators(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset, hkpVelocityAccumulator* accumulatorsOut); // 0x00E0FCA0
hkpVelocityAccumulator* hkRigidMotionUtilApplyForcesAndBuildAccumulators(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset, hkpVelocityAccumulator* accumulatorsOut); // 0x00E10010
int hkRigidMotionUtilApplyAccumulators(const hkpSolverInfo& solverInfo, const hkStepInfo& info, const hkpVelocityAccumulator* accumulators, hkpMotion* const* motions, int numMotions, int motionOffset); // 0x00E11950
