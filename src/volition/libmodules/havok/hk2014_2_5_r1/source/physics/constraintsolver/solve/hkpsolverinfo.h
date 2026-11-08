// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x150
struct alignas(16) hkpSolverInfo
{
    enum DeactivationClass
    {
        DEACTIVATION_CLASS_INVALID = 0,
        DEACTIVATION_CLASS_OFF = 1,
        DEACTIVATION_CLASS_LOW = 2,
        DEACTIVATION_CLASS_MEDIUM = 3,
        DEACTIVATION_CLASS_HIGH = 4,
        DEACTIVATION_CLASS_AGGRESSIVE = 5,
        DEACTIVATION_CLASSES_END = 6,
    };

    // size 0x20
    struct DeactivationInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DeactivationInfo);

        float m_linearVelocityThresholdInv; // 0x00
        float m_angularVelocityThresholdInv; // 0x04
        float m_slowObjectVelocityMultiplier; // 0x08
        float m_relativeSleepVelocityThreshold; // 0x0C
        float m_maxDistSqrd[2]; // 0x10
        float m_maxRotSqrd[2]; // 0x18
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSolverInfo);
    void setTauAndDamping(float tau, float damping); // 0x00DE9E10
    void incrementDeactivationFlags(); // 0x00DF5BB0
    hkpSolverInfo(); // 0x00DE8900

    float m_padding; // 0x00
    float m_tau; // 0x04
    float m_damping; // 0x08
    float m_frictionTau; // 0x0C
    hkVector4f m_globalAccelerationPerSubStep; // 0x10
    hkVector4f m_globalAccelerationPerStep; // 0x20
    hkVector4f m_integrateVelocityFactor; // 0x30
    hkVector4f m_invIntegrateVelocityFactor; // 0x40
    float m_dampDivTau; // 0x50
    float m_tauDivDamp; // 0x54
    float m_dampDivFrictionTau; // 0x58
    float m_frictionTauDivDamp; // 0x5C
    float m_contactRestingVelocity; // 0x60
    DeactivationInfo m_deactivationInfo[6]; // 0x64
    float m_deltaTime; // 0x124
    float m_invDeltaTime; // 0x128
    int m_numSteps; // 0x12C
    int m_numMicroSteps; // 0x130
    float m_invNumMicroSteps; // 0x134
    float m_invNumSteps; // 0x138
    hkBool m_forceCoherentConstraintOrderingInSolver; // 0x13C
    unsigned char m_deactivationNumInactiveFramesSelectFlag[2]; // 0x13D
    unsigned char m_deactivationIntegrateCounter; // 0x13F
    float m_maxConstraintViolationSqrd; // 0x140
};
