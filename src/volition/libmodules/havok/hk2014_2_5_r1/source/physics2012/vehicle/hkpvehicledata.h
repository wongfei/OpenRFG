// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"

class hkClass;
struct hkInternalClassMember;
class hkpRigidBody;

// size 0xD0
class alignas(16) hkpVehicleData : public hkReferencedObject
{
public:
    // size 0x28
    struct WheelComponentParams
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(WheelComponentParams);
        static const hkClass& staticClass();

        static const hkInternalClassMember Members[];
        float m_radius; // 0x00
        float m_mass; // 0x04
        float m_width; // 0x08
        float m_friction; // 0x0C
        float m_viscosityFriction; // 0x10
        float m_maxFriction; // 0x14
        float m_slipAngle; // 0x18
        float m_forceFeedbackMultiplier; // 0x1C
        float m_maxContactBodyAcceleration; // 0x20
        signed char m_axle; // 0x24
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71540, delete(void* p, unsigned int nbytes) 0x00F81360
    static const hkClass& staticClass();
    hkpVehicleData(hkFinishLoadedObjectFlag);
    hkpVehicleData(); // 0x00F81070
    virtual ~hkpVehicleData(); // vtbl+0x00 0x00F813A0
    virtual void init(const hkArray<hkpVehicleSuspension::SuspensionWheelParameters,hkContainerHeapAllocator>& suspensionWheelParams, hkpRigidBody* chassis); // vtbl+0x10 0x00F80EE0

    static const hkInternalClassMember Members[];
    alignas(16) hkVector4f m_gravity; // 0x10
    signed char m_numWheels; // 0x20
    hkRotationf m_chassisOrientation; // 0x30
    float m_torqueRollFactor; // 0x60
    float m_torquePitchFactor; // 0x64
    float m_torqueYawFactor; // 0x68
    float m_extraTorqueFactor; // 0x6C
    float m_maxVelocityForPositionalFriction; // 0x70
    float m_chassisUnitInertiaYaw; // 0x74
    float m_chassisUnitInertiaRoll; // 0x78
    float m_chassisUnitInertiaPitch; // 0x7C
    float m_frictionEqualizer; // 0x80
    float m_normalClippingAngleCos; // 0x84
    float m_maxFrictionSolverMassRatio; // 0x88
    hkArray<WheelComponentParams,hkContainerHeapAllocator> m_wheelParams; // 0x8C
    hkArray<signed char,hkContainerHeapAllocator> m_numWheelsPerAxle; // 0x98
    hkVector4f m_chassisFrictionInertiaInvDiag; // 0xB0
    hkBool m_alreadyInitialised; // 0xC0
};
