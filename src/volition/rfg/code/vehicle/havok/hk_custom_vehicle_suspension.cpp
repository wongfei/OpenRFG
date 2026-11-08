// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_suspension.cpp
//
// Suspension: calcSuspension steps a copy of the chassis motion through the frame at SUSPENSION_SIMULATION_FREQUENCY,
// with the spring forces applied at each step, and writes the copy's velocities back to the chassis. The output is the
// average force over the steps.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_suspension.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

#include <cmath>

static float SUSPENSION_SIMULATION_FREQUENCY = 60.0f; // 0x016BA4A8

// 0x00BA4EF0 (.h:20)
hk_custom_vehicle_suspension::hk_custom_vehicle_suspension()
{
}

// 0x00BA4F40
hk_custom_vehicle_suspension::~hk_custom_vehicle_suspension()
{
}

// 0x00B75400 (:55)
float hk_custom_vehicle_suspension::calculate_suspension_length(const hkVector4f& ws_hardpoint, const hkVector4f& ws_suspension_direction, float wheel_radius, float spring_max_length, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo)
{
    if (cdInfo->m_contactBody == NULL)
    {
        if (cdInfo->m_currentSuspensionLength == 0.0f)
        {
            return 0.0f;
        }
        return spring_max_length;
    }

    const hkVector4f& normal = cdInfo->m_contactPoint.getNormal();
    hkVector4f relative;
    relative.setSub(cdInfo->m_contactPoint.getPosition(), ws_hardpoint);
    const float distance = relative.dot<3>(normal).getReal();
    const float slope = ws_suspension_direction.dot<3>(normal).getReal();
    // 1.1920929e-07: 2^-23. A suspension along the contact plane has no length to give.
    if (1.1920929e-07f <= fabsf(slope))
    {
        float length = distance / slope - wheel_radius;
        if (length <= 0.0f)
        {
            length = 0.0f;
        }
        if (spring_max_length <= length)
        {
            length = spring_max_length;
        }
        return length;
    }
    if (fabsf(distance) < wheel_radius)
    {
        return 0.0f;
    }
    return spring_max_length;
}

// 0x00B75540 (:138)
float hk_custom_vehicle_suspension::calculate_suspension_relative_velocity(const hkpMotion& proxy_motion, const hkVector4f& ws_contact_position, const hkVector4f& ws_suspension_direction)
{
    hkVector4f r;
    r.setSub(ws_contact_position, proxy_motion.getCenterOfMassInWorld());
    hkVector4f velocity;
    velocity.setCross(proxy_motion.m_angularVelocity, r);
    velocity.add(proxy_motion.m_linearVelocity);
    return 0.0f - velocity.dot<3>(ws_suspension_direction).getReal();
}

// 0x00B755B0 (:166)
void hk_custom_vehicle_suspension::calcSuspension_core(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut)
{
    const float mass = vehicle->getChassis()->getMass();
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        const hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cd = cdInfo[w];
        if (cd.m_contactBody == NULL && cd.m_currentSuspensionLength != 0.0f)
        {
            suspensionForceOut[w] = 0.0f;
            continue;
        }

        // 100000: the largest compression
        float compression = zero_point[w] - cd.m_currentSuspensionLength;
        if (compression - 0.0f < 0.0f)
        {
            compression = 0.0f;
        }
        if (0.0f <= compression - 100000.0f)
        {
            compression = 100000.0f;
        }
        const WheelSpringSuspensionParameters& spring = m_wheelSpringParams[w];
        float force = spring.m_strength * compression * cd.m_suspensionScalingFactor;
        if (spring_pow[w] != 1.0f)
        {
            force = (float)pow((double)force, (double)spring_pow[w]);
        }
        const float damping = 0.0f <= cd.m_suspensionClosingSpeed ? spring.m_dampingRelaxation : spring.m_dampingCompression;
        suspensionForceOut[w] = (force - damping * cd.m_suspensionClosingSpeed) * mass;
    }
}

// 0x00BA7FE0 (:269)
void hk_custom_vehicle_suspension::calcSuspension(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut)
{
    const int num_wheels = vehicle->m_data->m_numWheels;
    for (int w = 0; w < num_wheels; w++)
    {
        suspensionForceOut[w] = 0.0f;
    }

    hkpEntity* chassis = vehicle->m_entity;
    const hkpMotion* chassis_motion = chassis->getMotion();
    // A box motion over the chassis' own state.
    hkpBoxMotion proxy(chassis_motion->getCenterOfMassInWorld(), chassis_motion->m_motionState.getSweptTransform().m_rotation1);
    static_cast<hkpMotion&>(proxy) = *chassis_motion;

    const float max_step = 1.0f / SUSPENSION_SIMULATION_FREQUENCY;
    float steps = 1.0f;
    float remaining = deltaTime;
    while (0.0f < remaining)
    {
        const float step = 0.0f <= remaining - max_step ? max_step : remaining;

        hkLocalBuffer<hkVector4f> hardpoints(num_wheels);
        hkLocalBuffer<hkVector4f> directions(num_wheels);
        for (int w = 0; w < num_wheels; w++)
        {
            hardpoints[w].setTransformedPos(proxy.m_motionState.getTransform(), m_wheelParams[w].m_hardpointChassisSpace);
            directions[w].setRotatedDir(proxy.m_motionState.getSweptTransform().m_rotation1, m_wheelParams[w].m_directionChassisSpace);
        }

        hkLocalBuffer<hkpVehicleWheelCollide::CollisionDetectionWheelOutput> step_cd(num_wheels);
        for (int w = 0; w < num_wheels; w++)
        {
            step_cd[w].m_contactBody = cdInfo[w].m_contactBody;
            step_cd[w].m_suspensionScalingFactor = cdInfo[w].m_suspensionScalingFactor;
            step_cd[w].m_currentSuspensionLength = calculate_suspension_length(hardpoints[w], directions[w], vehicle->m_data->m_wheelParams[w].m_radius, m_wheelParams[w].m_length, &cdInfo[w]);
            step_cd[w].m_suspensionClosingSpeed = calculate_suspension_relative_velocity(proxy, cdInfo[w].m_contactPoint.getPosition(), directions[w]);
        }

        hkLocalBuffer<float> forces(num_wheels);
        calcSuspension_core(step, vehicle, step_cd.begin(), forces.begin());
        for (int w = 0; w < num_wheels; w++)
        {
            if (forces[w] <= 0.0f)
            {
                forces[w] = 0.0f;
            }
            suspensionForceOut[w] = (forces[w] + suspensionForceOut[w] * (steps - 1.0f)) / steps;
        }

        for (int w = 0; w < num_wheels; w++)
        {
            hkVector4f impulse;
            impulse.setMul(hkSimdFloat32::fromFloat(forces[w] * step), cdInfo[w].m_contactPoint.getSeparatingNormal());
            proxy.applyPointImpulse(impulse, hardpoints[w]);
        }

        hkStepInfo info(0.0f, step);
        hkSweptTransformUtil::setTimeInformation(hkTime(0.0f), 0.0f, proxy.m_motionState);
        hkpMotion* motions = &proxy;
        hkRigidMotionUtilStep(info, &motions, 1, 0);

        remaining = remaining - step;
        steps = steps + 1.0f;
    }

    chassis->activate();
    chassis->getMotion()->setLinearVelocity(proxy.m_linearVelocity);
    chassis->activate();
    chassis->getMotion()->setAngularVelocity(proxy.m_angularVelocity);
}
