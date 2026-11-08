// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/default/hkpvehicledefaultsuspension.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): spring and damper force per wheel on the ground, scaled by the chassis mass, from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/default/hkpvehicledefaultsuspension.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

// 0x00F80DD0
void hkpVehicleDefaultSuspension::calcSuspension(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut)
{
    const float mass = vehicle->getChassis()->getMass();
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        const hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cd = cdInfo[w];
        if (cd.m_contactBody == NULL)
        {
            suspensionForceOut[w] = 0.0f;
            continue;
        }
        const WheelSpringSuspensionParameters& spring = m_wheelSpringParams[w];
        const float damping = 0.0f <= cd.m_suspensionClosingSpeed ? spring.m_dampingRelaxation : spring.m_dampingCompression;
        suspensionForceOut[w] = ((m_wheelParams[w].m_length - cd.m_currentSuspensionLength) * spring.m_strength * cd.m_suspensionScalingFactor
            - damping * cd.m_suspensionClosingSpeed) * mass;
    }
}

// 0x00B9B2F0 (.h:25)
hkpVehicleDefaultSuspension::hkpVehicleDefaultSuspension()
{
}

// 0x00B9B360
hkpVehicleDefaultSuspension::~hkpVehicleDefaultSuspension()
{
}

// 0x00B9AF70 (hkpvehiclesuspension.h:33)
hkpVehicleSuspension::hkpVehicleSuspension()
{
}

// 0x00B9AFD0
hkpVehicleSuspension::~hkpVehicleSuspension()
{
}
