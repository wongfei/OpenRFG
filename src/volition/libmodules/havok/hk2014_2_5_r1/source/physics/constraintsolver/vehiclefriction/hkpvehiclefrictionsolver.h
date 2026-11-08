// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianelement.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefriction.h"

// size 0x60
struct alignas(16) hkpVehicleFrictionSolverAxleParams
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleFrictionSolverAxleParams);
    void initialize(); // 0x00B63730 (.h:76)

    hkVector4f m_contactPointWs; // 0x00
    hkVector4f m_constraintNormalWs; // 0x10
    hkVector4f m_forwardDirWs; // 0x20
    hkpVelocityAccumulator* m_groundObject; // 0x30
    hkpVelocityAccumulator* m_groundObjectAtLastIntegration; // 0x34
    float m_frictionCoefficient; // 0x38
    float m_viscosityFrictionCoefficient; // 0x3C
    float m_maxFrictionCoefficient; // 0x40
    float m_wheelDownForce; // 0x44
    float m_forwardForce; // 0x48
    float m_slipVelocityFactor; // 0x4C
    unsigned char m_wheelFixed; // 0x50
};

// size 0x1D0
struct hkpVehicleFrictionSolverParams
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleFrictionSolverParams);
    hkpVehicleFrictionSolverParams(); // 0x00B896C0

    hkpVehicleFrictionSolverAxleParams m_axleParams[2]; // 0x00
    float m_maxVelocityForPositionalFriction; // 0xC0
    hkpVelocityAccumulator m_chassis; // 0xD0
    hkpVelocityAccumulator m_chassisAtLastIntegration; // 0x150
};

// size 0x8
struct hkpVehicleStepInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleStepInfo);

    float m_deltaTime; // 0x00
    float m_invDeltaTime; // 0x04
};

// size 0xA0
class hkpVehicleFrictionAxisTmp
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleFrictionAxisTmp);

    hkp1Lin2AngJacobian m_forwardJac; // 0x00
    hkp1Lin2AngJacobian m_sideJac; // 0x30
    float m_forward_vel; // 0x60
    float m_forwardSlipVel; // 0x64
    float m_sideSlipVel; // 0x68
    float m_wheel_down_force; // 0x6C
    float m_sideVirtMass; // 0x70
    float m_invSideVirtMass; // 0x74
    float m_wheelForwardVirtMass; // 0x78
    float m_invWheelForwardVirtMass; // 0x7C
    float m_forwardImpulse; // 0x80
    float m_sideImpulse; // 0x84
    float m_maxForwardImpulse; // 0x88
    float m_maxSideImpulse; // 0x8C
    float m_invMaxForwardImpulse; // 0x90
    float m_invMaxSideImpulse; // 0x94
    float m_restForwardSlipImpulse; // 0x98
};

void hkVehicleFrictionDescriptionInitValues(const hkpVehicleFrictionDescription::Cinfo& ci, hkpVehicleFrictionDescription& out); // 0x00D71510
void hkVehicleFrictionApplyVehicleFriction(const hkpVehicleStepInfo& stepInfo, const hkpVehicleFrictionDescription& descr, hkpVehicleFrictionSolverParams& params, hkpVehicleFrictionStatus& status); // 0x00D718C0
