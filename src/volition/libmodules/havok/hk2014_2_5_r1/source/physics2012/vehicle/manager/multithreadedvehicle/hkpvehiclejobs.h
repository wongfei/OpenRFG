// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/manager/multithreadedvehicle/hkpvehiclejobs.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

class hkpRigidBody;
class hkpVehicleInstance;

// size 0x270
struct hkpVehicleJobResults
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleJobResults);
    void applyForcesFromStep(const hkpVehicleInstance& vehicleInstance); // 0x00F83BE0

    static const int s_maxNumWheels; // 0x012C9178
    hkVector4f m_chassisLinearVel; // 0x00
    hkVector4f m_chassisAngularVel; // 0x10
    hkVector4f m_groundBodyLinearVel[2]; // 0x20
    hkVector4f m_groundBodyAngularVel[2]; // 0x40
    hkpRigidBody* m_groundBodyPtr[2]; // 0x60
    hkVector4f m_groundBodyImpulses[32]; // 0x70
};
