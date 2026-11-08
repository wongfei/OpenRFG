// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.cpp
//
// The game's Havok vehicle action. applyAction runs applyHk550Action, Havok 5.5.0's step order with every component
// inlined: unlike hkpVehicleInstance::stepVehicle it does not call m_vehicleSimulation (only its friction description
// is read) and it pushes the suspension forces straight into the contact bodies. Havok's timer markers are left out.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/hkpvehiclevelocitydamper.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.h"
#include "volition/rfg/code/vehicle/havok/hk_wheel_info_contact_body_listener.h"
#include "volition/vlib/code/math/floating.h"

#include <cmath>

// Past the minimum reverse speed the steering input is blended down to pct by the maximum speed.
static float Vehicle_reverse_steer_speed_min = 10.0f; // 0x016BA48C
static float Vehicle_reverse_steer_speed_max = 30.0f; // 0x016BA490
static float Vehicle_reverse_steer_speed_pct = 0.5f; // 0x016BA494

// The blocking setup of the brake: how long a hard pedal takes to block the wheels, and how hard it must be.
static void brake_set_blocking(hk_custom_vehicle_brakes* brake, float min_time_to_block, float min_pedal_input)
{
    brake->m_wheelsMinTimeToBlock = min_time_to_block;
    for (int w = 0; w < brake->m_wheelBrakingProperties.getSize(); w++)
    {
        brake->m_wheelBrakingProperties[w].m_minPedalInputToBlock = min_pedal_input;
    }
}

// Parked or stopping: wheels block after 1 second at a pedal of 0.1.
static void brake_set_parked(hk_custom_vehicle_brakes* brake)
{
    brake_set_blocking(brake, 1.0f, 0.1f);
}

// 0x00B63B20
hk_custom_vehicle_instance::driver_input_instance_data::driver_input_instance_data()
{
}

// 0x00B63B30
hk_custom_vehicle_instance::misc_instance_data::misc_instance_data()
{
}

// 0x00B9CF30 (:59)
hk_custom_vehicle_instance::hk_custom_vehicle_instance()
    : hkpVehicleInstance(NULL)
{
    m_misc_instance_data.m_is_trailer = false;
    m_misc_instance_data.m_is_extra_mass = false;
    m_misc_instance_data.m_is_infinite_mass = false;
    m_misc_instance_data.m_is_articulated = false;
    m_misc_instance_data.m_is_tank = false;
    m_misc_instance_data.m_ai_controlled = false;
    m_steering_instance_data.m_turn_velocity = 0.0f;
    m_engine_instance_data.m_ai_top_speed_adjustment = 0.0f;
    m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
    m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
    m_driver_input_instance_data.m_steering_wheel_input = 0.0f;
    m_transmission_instance_data.m_rpm = 0.0f;
    m_misc_instance_data.m_auto_brake_speed = 3.0f;
    m_misc_instance_data.m_enforced_max_speed = 1000.0f;
    m_misc_instance_data.m_enforced_max_rspeed = 1000.0f;
    m_extra_downforce = NULL;
    m_driver_input_instance_data.m_handbrake_on = false;
    m_misc_instance_data.m_forward_to_reverse_delay = 0;
    m_misc_instance_data.m_reverse_to_forward_delay = 0;
    m_engine_instance_data.m_ai_torque_multiplier = 1.0f;
    m_engine_instance_data.m_reverse_torque_multiplier = 1.0f;
    m_engine_instance_data.m_powerslide_torque_multiplier = 1.0f;
    m_misc_instance_data.m_extra_mass_val = 1.0f;
    m_misc_instance_data.m_original_yaw_factor = 1.0f;
    m_misc_instance_data.m_original_yaw_factor_scalar = 1.0f;
    m_misc_instance_data.tread_wheel_aabb_min.setZero();
    m_misc_instance_data.tread_wheel_aabb_max.setZero();
}

// 0x00B9D140 (:125)
hk_custom_vehicle_instance::~hk_custom_vehicle_instance()
{
    for (int i = 0; i < m_wheel_info_contact_body_listeners.getSize(); i++)
    {
        delete m_wheel_info_contact_body_listeners[i];
    }
    m_extra_downforce = NULL;
}

// 0x00B9D240 (:143)
void hk_custom_vehicle_instance::init()
{
    hkpVehicleInstance::init();
    updateWheelsHardPoints(getChassis()->getTransform());
    if (!ignore_new_driver_input())
    {
        m_tryingToReverse = false;
    }
    m_misc_instance_data.m_original_yaw_factor = m_data->m_chassisFrictionInertiaInvDiag(1);

    const int num_wheels = (unsigned char)m_data->m_numWheels;
    m_wheel_contact_p_material.setSize(num_wheels);
    m_wheel_contact_e_material.setSize(num_wheels);
    for (int w = 0; w < num_wheels; w++)
    {
        // 0xFF: no material
        m_wheel_contact_p_material[w] = 0xFF;
        m_wheel_contact_e_material[w] = 0xFF;
    }
    for (int w = 0; w < m_wheelsInfo.getSize(); w++)
    {
        m_wheelsInfo[w].m_spinAxisChassisSpace.set(1.0f, 0.0f, 0.0f, 0.0f);
    }
    m_wheel_info_contact_body_listeners.setSize(num_wheels);
    for (int w = 0; w < num_wheels; w++)
    {
        m_wheel_info_contact_body_listeners[w] = new hk_wheel_info_contact_body_listener(&m_wheelsInfo[w]);
    }
}

// 0x00B745E0 (:200)
void hk_custom_vehicle_instance::updateWheelsHardPoints(const hkTransformf& car_transform)
{
    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        const hkpVehicleSuspension::SuspensionWheelParameters& params = m_suspension->m_wheelParams[w];
        wheel.m_suspensionDirectionWs.setRotatedDir(car_transform.getRotation(), params.m_directionChassisSpace);
        wheel.m_hardPointWs.setTransformedPos(car_transform, params.m_hardpointChassisSpace);
        const float length = params.m_length + m_data->m_wheelParams[w].m_radius;
        wheel.m_rayEndPointWs.setAddMul(wheel.m_hardPointWs, wheel.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(length));
    }
}

// 0x00B63B40 (:226)
void hk_custom_vehicle_instance::reinitialize_vehicle_data()
{
    m_data->m_alreadyInitialised = false;
    m_data->init(m_suspension->m_wheelParams, getChassis());
    m_misc_instance_data.m_original_yaw_factor = m_data->m_chassisFrictionInertiaInvDiag(1);
}

// 0x00B746C0 (:241)
unsigned char hk_custom_vehicle_instance::get_physical_material_in_contact_with_wheel(unsigned int wheel_index)
{
    return m_wheel_contact_p_material[wheel_index];
}

// 0x00B746D0 (:250)
unsigned char hk_custom_vehicle_instance::get_effect_material_in_contact_with_wheel(unsigned int wheel_index)
{
    return m_wheel_contact_e_material[wheel_index];
}

// 0x00BB1800 (:263)
void hk_custom_vehicle_instance::applyAction(const hkStepInfo& step_info)
{
    applyHk550Action(step_info);
    for (int i = 0; i < m_wheel_info_contact_body_listeners.getSize(); i++)
    {
        m_wheel_info_contact_body_listeners[i]->update_contact_body();
    }

    // A wheel that takes no torque keeps no forward slip for its axle.
    const hkpVehicleDefaultTransmission* transmission = static_cast<const hkpVehicleDefaultTransmission*>(m_transmission);
    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        // 0.0001: no share of the engine
        if (transmission->m_wheelsTorqueRatio[w] < 0.0001f)
        {
            m_frictionStatus.m_axis[m_data->m_wheelParams[w].m_axle].m_forward_slip_velocity = 0.0f;
        }
    }
}

// 0x00BAE410 (:303)
void hk_custom_vehicle_instance::applyHk550Action(const hkStepInfo& stepInfo)
{
    const float delta_time = stepInfo.m_deltaTime;
    hkpRigidBody* chassis = getChassis();
    hkpMotion* motion = chassis->getMotion();
    const hkTransformf& transform = chassis->getTransform();
    updateWheelsHardPoints(transform);

    hkLocalBuffer<hkpVehicleWheelCollide::CollisionDetectionWheelOutput> cd(m_data->m_numWheels);
    m_wheelCollide->collideWheels(delta_time, this, cd.begin());
    for (int w = 0; w < m_wheelsInfo.getSize(); w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        wheel.m_currentSuspensionLength = cd[w].m_currentSuspensionLength;
        wheel.m_contactPoint = cd[w].m_contactPoint;
        wheel.m_contactFriction = cd[w].m_contactFriction;
        wheel.m_contactBody = cd[w].m_contactBody;
        for (int k = 0; k < WheelInfo::MAX_NUM_SHAPE_KEYS; k++)
        {
            wheel.m_contactShapeKey[k] = cd[w].m_contactShapeKey[k];
        }
    }

    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        // Steering about the suspension direction, by the small angle form of a quaternion.
        const hkSimdFloat32 half_angle = hkSimdFloat32::fromFloat(m_wheelsSteeringAngle[w] * 0.5f);
        hkVector4f steering;
        steering.setMul(half_angle, m_suspension->m_wheelParams[w].m_directionChassisSpace);
        steering(3) = 1.0f;
        steering.normalize<4>();
        wheel.m_steeringOrientationChassisSpace.m_vec = steering;
        wheel.m_spinAxisChassisSpace = m_data->m_chassisOrientation.getColumn<2>();
        hkVector4f spin_axis;
        spin_axis.setRotatedDir(wheel.m_steeringOrientationChassisSpace, wheel.m_spinAxisChassisSpace);
        wheel.m_spinAxisWs.setRotatedDir(transform.getRotation(), spin_axis);
    }

    hkpVehicleDriverInput::FilteredDriverInputOutput filtered;
    filtered.m_tryingToReverse = m_tryingToReverse;
    m_driverInput->calcDriverInput(delta_time, this, m_deviceStatus, filtered);
    m_tryingToReverse = filtered.m_tryingToReverse;

    // m_mainSteeringAngleAssumingNoReduction belongs to the later Havok step: 5.5.0 neither reads nor writes it.
    hkpVehicleSteering::SteeringAnglesOutput steering;
    steering.m_mainSteeringAngle = m_mainSteeringAngle;
    steering.m_wheelsSteeringAngle.setSize(m_wheelsSteeringAngle.getSize());
    for (int w = 0; w < m_wheelsSteeringAngle.getSize(); w++)
    {
        steering.m_wheelsSteeringAngle[w] = m_wheelsSteeringAngle[w];
    }
    m_steering->calcSteering(delta_time, this, filtered, steering);
    m_mainSteeringAngle = steering.m_mainSteeringAngle;
    for (int w = 0; w < m_wheelsSteeringAngle.getSize(); w++)
    {
        m_wheelsSteeringAngle[w] = steering.m_wheelsSteeringAngle[w];
    }

    hkLocalBuffer<float> transmitted(m_data->m_numWheels);
    hkpVehicleTransmission::TransmissionOutput transmission;
    transmission.m_wheelsTransmittedTorque = transmitted.begin();
    transmission.m_numWheelsTramsmittedTorque = m_data->m_numWheels;
    transmission.m_isReversing = m_isReversing;
    transmission.m_currentGear = m_currentGear;
    transmission.m_delayed = m_delayed;
    transmission.m_clutchDelayCountdown = m_clutchDelayCountdown;
    m_transmission->calcTransmission(delta_time, this, transmission);
    m_isReversing = transmission.m_isReversing;
    m_currentGear = transmission.m_currentGear;
    m_delayed = transmission.m_delayed;
    m_clutchDelayCountdown = transmission.m_clutchDelayCountdown;

    hkpVehicleEngine::EngineOutput engine;
    m_engine->calcEngineInfo(delta_time, this, filtered, transmission, engine);
    m_rpm = engine.m_rpm;
    m_torque = engine.m_torque;

    hkpVehicleBrake::WheelBreakingOutput braking;
    braking.m_isFixed.setSize(m_isFixed.getSize());
    braking.m_brakingTorque.setSize(m_isFixed.getSize());
    for (int w = 0; w < m_isFixed.getSize(); w++)
    {
        braking.m_isFixed[w] = m_isFixed[w];
    }
    braking.m_wheelsTimeSinceMaxPedalInput = m_wheelsTimeSinceMaxPedalInput;
    m_brake->calcBreakingInfo(delta_time, this, filtered, braking);
    for (int w = 0; w < braking.m_isFixed.getSize(); w++)
    {
        m_isFixed[w] = braking.m_isFixed[w];
    }
    m_wheelsTimeSinceMaxPedalInput = braking.m_wheelsTimeSinceMaxPedalInput;

    hkInplaceArray<float,16,hkContainerHeapAllocator> suspension_force;
    suspension_force.setSize(m_data->m_numWheels);
    m_suspension->calcSuspension(delta_time, this, cd.begin(), suspension_force.begin());

    hkpVehicleAerodynamics::AerodynamicsDragOutput aerodynamics;
    m_aerodynamics->calcAerodynamics(delta_time, this, aerodynamics);

    // The vehicle's own gravity replaces the world's on the chassis.
    const hkSimdFloat32 dt = hkSimdFloat32::fromFloat(delta_time);
    hkVector4f linear_impulse;
    linear_impulse.setMul(dt, aerodynamics.m_aerodynamicsForce);
    hkVector4f angular_impulse;
    angular_impulse.setMul(dt, aerodynamics.m_aerodynamicsTorque);
    linear_impulse.addMul(hkSimdFloat32::fromFloat(motion->getMass() * delta_time), m_data->m_gravity);
    hkVector4f anti_gravity;
    anti_gravity.setMul(hkSimdFloat32::fromFloat(-motion->getMass()), m_data->m_gravity);
    motion->applyForce(delta_time, anti_gravity);
    motion->applyLinearImpulse(linear_impulse);
    motion->applyAngularImpulse(angular_impulse);
    if (m_velocityDamper != NULL)
    {
        m_velocityDamper->applyVelocityDamping(delta_time, *this);
    }

    hkpVehicleFrictionSolverParams params;
    params.m_axleParams[0].initialize();
    params.m_axleParams[1].initialize();

    const float estimated_speed = chassis->getLinearVelocity().length<3>().getReal();
    hkpRigidBody* ground_body[2] = { NULL, NULL };
    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        const hkpVehicleData::WheelComponentParams& wheel_params = m_data->m_wheelParams[w];
        const WheelInfo& wheel = m_wheelsInfo[w];
        const int axle = wheel_params.m_axle;
        const hkVector4f& normal = wheel.m_contactPoint.getNormal();

        // The contact body takes its share of the suspension at once.
        hkpRigidBody* body = cd[w].m_contactBody;
        if (body != NULL)
        {
            hkVector4f impulse;
            impulse.setMul(hkSimdFloat32::fromFloat(suspension_force[w] * delta_time), normal);
            impulse.setMul(hkSimdFloat32::fromFloat(-wheel_params.m_forceFeedbackMultiplier), impulse);
            const float max_impulse = wheel_params.m_maxContactBodyAcceleration * body->getMass() * delta_time;
            if (max_impulse * max_impulse < impulse.lengthSquared<3>().getReal())
            {
                impulse.normalize<3>();
                impulse.setMul(hkSimdFloat32::fromFloat(wheel_params.m_maxContactBodyAcceleration * body->getMass() * delta_time), impulse);
            }
            body->getMotion()->applyPointImpulse(impulse, wheel.m_hardPointWs);

            // Each axle's ground is its heaviest contact body.
            if (ground_body[axle] == NULL || body->getMotion()->m_inertiaAndMassInv(3) < ground_body[axle]->getMotion()->m_inertiaAndMassInv(3))
            {
                ground_body[axle] = body;
            }
        }

        hkVector4f forward;
        forward.setCross(normal, wheel.m_spinAxisWs);
        // 1.1920929e-07: 2^-23. A normal along the spin axis: forward from the suspension.
        if (forward.lengthSquared<3>().getReal() < 1.1920929e-07f)
        {
            forward.setCross(wheel.m_spinAxisWs, wheel.m_suspensionDirectionWs);
        }
        forward.normalize<3>();

        hkpVehicleFrictionSolverAxleParams& axle_params = params.m_axleParams[axle];
        const float share = 1.0f / (float)(int)m_data->m_numWheelsPerAxle[axle];
        const float friction_share = wheel.m_contactFriction * share;
        axle_params.m_contactPointWs.addMul(hkSimdFloat32::fromFloat(share), wheel.m_contactPoint.getPosition());
        hkVector4f side;
        side.setCross(forward, normal);
        axle_params.m_constraintNormalWs.add(side);
        axle_params.m_forwardDirWs.add(forward);
        axle_params.m_frictionCoefficient = wheel_params.m_friction * friction_share + axle_params.m_frictionCoefficient;
        axle_params.m_viscosityFrictionCoefficient = wheel_params.m_viscosityFriction * friction_share + axle_params.m_viscosityFrictionCoefficient;
        axle_params.m_maxFrictionCoefficient = wheel_params.m_maxFriction * friction_share + axle_params.m_maxFrictionCoefficient;
        axle_params.m_wheelDownForce = axle_params.m_wheelDownForce + suspension_force[w];
        axle_params.m_forwardForce = (transmission.m_wheelsTransmittedTorque[w] + braking.m_brakingTorque[w]) / wheel_params.m_radius + axle_params.m_forwardForce;
        axle_params.m_wheelFixed = (axle_params.m_wheelFixed != 0 || m_isFixed[w].m_bool != 0) ? 1 : 0;
        axle_params.m_slipVelocityFactor = wheel_params.m_slipAngle * share * estimated_speed + axle_params.m_slipVelocityFactor;
    }

    params.m_maxVelocityForPositionalFriction = m_data->m_maxVelocityForPositionalFriction;
    hkpVelocityAccumulator ground_accum[2];
    hkpVelocityAccumulator ground_accum_at_integration[2];
    for (int a = 0; a < m_data->m_numWheelsPerAxle.getSize(); a++)
    {
        hkpVehicleFrictionSolverAxleParams& axle = params.m_axleParams[a];
        axle.m_constraintNormalWs.normalize<3>();

        // A forward direction that cancelled out between the wheels: the chassis' column 2.
        const float length = axle.m_forwardDirWs.length<3>().getReal();
        if (1.1920929e-07f <= length)
        {
            axle.m_forwardDirWs.setMul(hkSimdFloat32::fromFloat(1.0f / length), axle.m_forwardDirWs);
        }
        else
        {
            axle.m_forwardDirWs = transform.getRotation().getColumn<2>();
        }

        hkpRigidBody* ground = ground_body[a];
        hkpVelocityAccumulator& accum = ground_accum[a];
        axle.m_groundObject = &accum;
        axle.m_groundObjectAtLastIntegration = &accum;
        if (ground == NULL)
        {
            accum.setFixed();
        }
        // 5: hkpMotion::MOTION_FIXED
        else if (ground->getMotion()->m_type.m_storage == 5)
        {
            handleFixedGroundAccum(ground, accum);
        }
        else if (a < 1 || ground != ground_body[0])
        {
            hkpMotion* ground_motion = ground->getMotion();
            hkRigidMotionUtilApplyForcesAndBuildAccumulators(stepInfo, &ground_motion, 1, 0, &accum);

            // A ground body too light for the solver takes a limited inverse mass.
            const float chassis_mass_inv = motion->m_inertiaAndMassInv(3);
            const float ground_mass_inv = ground_motion->m_inertiaAndMassInv(3);
            if (m_data->m_maxFrictionSolverMassRatio * chassis_mass_inv < ground_mass_inv)
            {
                accum.m_invMasses.setMul(hkSimdFloat32::fromFloat((chassis_mass_inv / ground_mass_inv) * m_data->m_maxFrictionSolverMassRatio), accum.m_invMasses);
            }

            // The velocities the last integration really used, from the swept transform (its w is the inverse time).
            const hkMotionState& state = ground_motion->m_motionState;
            const hkSimdFloat32 inv_time = state.getSweptTransform().m_centerOfMass1.getComponent<3>();
            hkpVelocityAccumulator& at_integration = ground_accum_at_integration[a];
            at_integration = accum;
            axle.m_groundObjectAtLastIntegration = &at_integration;
            hkVector4f delta_angle;
            delta_angle.setMul(state.m_deltaAngle, inv_time);
            at_integration.m_angularVel._setRotatedDir(reinterpret_cast<const hkMatrix3f&>(accum.m_scratch0), delta_angle);
            at_integration.m_linearVel.setSub(state.getSweptTransform().m_centerOfMass1, state.getSweptTransform().m_centerOfMass0);
            at_integration.m_linearVel.setMul(at_integration.m_linearVel, inv_time);
        }
        else
        {
            axle.m_groundObject = params.m_axleParams[0].m_groundObject;
            axle.m_groundObjectAtLastIntegration = params.m_axleParams[0].m_groundObjectAtLastIntegration;
        }
    }

    hkRigidMotionUtilApplyForcesAndBuildAccumulators(stepInfo, &motion, 1, 0, &params.m_chassis);
    {
        const hkMotionState& state = motion->m_motionState;
        const hkSimdFloat32 inv_time = state.getSweptTransform().m_centerOfMass1.getComponent<3>();
        hkVector4f linear;
        linear.setSub(state.getSweptTransform().m_centerOfMass1, state.getSweptTransform().m_centerOfMass0);
        linear.setMul(linear, inv_time);
        hkVector4f delta_angle;
        delta_angle.setMul(state.m_deltaAngle, inv_time);
        params.m_chassis.m_invMasses = m_data->m_chassisFrictionInertiaInvDiag;
        params.m_chassisAtLastIntegration = params.m_chassis;
        params.m_chassisAtLastIntegration.m_angularVel.setRotatedDir(reinterpret_cast<const hkMatrix3f&>(params.m_chassis.m_scratch0), delta_angle);
        params.m_chassisAtLastIntegration.m_linearVel = linear;
    }

    if (m_data->m_extraTorqueFactor != 0.0f)
    {
        hkVector4f torque;
        torque.setMul(hkSimdFloat32::fromFloat(m_mainSteeringAngle * m_data->m_extraTorqueFactor * delta_time), m_data->m_chassisOrientation.getColumn<0>());
        torque.mul(m_data->m_chassisFrictionInertiaInvDiag);
        params.m_chassis.m_angularVel.add(torque);
    }

    hkpVehicleStepInfo step;
    step.m_deltaTime = stepInfo.m_deltaTime;
    step.m_invDeltaTime = stepInfo.m_invDeltaTime;
    hkVehicleFrictionApplyVehicleFriction(step, *static_cast<hkpVehicleDefaultSimulation*>(m_vehicleSimulation)->m_frictionDescription, params, m_frictionStatus);

    hkVector4f chassis_angular;
    chassis_angular._setRotatedDir(transform.getRotation(), params.m_chassis.m_angularVel);
    motion->setAngularVelocity(chassis_angular);
    motion->setLinearVelocity(params.m_chassis.m_linearVel);

    for (int a = 0; a < m_data->m_numWheelsPerAxle.getSize(); a++)
    {
        hkpRigidBody* ground = ground_body[a];
        // 5: hkpMotion::MOTION_FIXED
        if (ground == NULL || ground->getMotion()->m_type.m_storage == 5 || (a != 0 && ground_body[0] == ground_body[1]))
        {
            continue;
        }
        // 10: the largest velocity change the vehicle gives a ground body per second
        const hkpVelocityAccumulator* accum = params.m_axleParams[a].m_groundObject;
        const float limit = delta_time * 10.0f;
        hkVector4f max_change;
        max_change.set(limit, limit, limit, 0.0f);
        hkVector4f min_change;
        min_change.set(-limit, -limit, -limit, -0.0f);

        hkpMotion* ground_motion = ground->getMotion();
        hkVector4f angular;
        angular._setRotatedInverseDir(reinterpret_cast<const hkMatrix3f&>(accum->m_scratch0), accum->m_angularVel);
        angular.sub(ground_motion->m_angularVelocity);
        angular.setMin(angular, max_change);
        angular.setMax(angular, min_change);
        angular.add(ground_motion->m_angularVelocity);
        hkVector4f linear;
        linear.setSub(accum->m_linearVel, ground_motion->m_linearVelocity);
        linear.setMin(linear, max_change);
        linear.setMax(linear, min_change);
        linear.add(ground_motion->m_linearVelocity);
        ground_motion->setAngularVelocity(angular);
        ground_motion->setLinearVelocity(linear);
    }

    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        const hkpVehicleData::WheelComponentParams& wheel_params = m_data->m_wheelParams[w];
        const int axle = wheel_params.m_axle;
        const hkpVehicleFrictionStatus::AxisStatus& axis = m_frictionStatus.m_axis[axle];
        // Only a fixed body takes skid marks.
        wheel.m_skidEnergyDensity = (cd[w].m_contactBody != NULL && cd[w].m_contactBody->getMotion()->m_type.m_storage == 5) ? axis.m_skid_energy_density : 0.0f;
        wheel.m_sideForce = axis.m_side_force;
        wheel.m_sideSlipVelocity = axis.m_side_slip_velocity;
        wheel.m_forwardSlipVelocity = axis.m_forward_slip_velocity;
        hkVector4f forward;
        forward.setRotatedDir(transform.getRotation(), m_data->m_chassisOrientation.getColumn<1>());
        if (m_isFixed[w].m_bool == 0)
        {
            float speed = chassis->getLinearVelocity().dot<3>(forward).getReal() + wheel.m_forwardSlipVelocity;
            if (ground_body[axle] != NULL)
            {
                speed = speed - params.m_axleParams[axle].m_groundObject->m_linearVel.dot<3>(forward).getReal();
            }
            const float spin = speed / wheel_params.m_radius;
            wheel.m_spinVelocity = spin;
            wheel.m_spinAngle = spin * delta_time + wheel.m_spinAngle;
        }
        else
        {
            wheel.m_spinVelocity = 0.0f;
        }
    }
}

// 0x00B89EA0 (:951)
void hk_custom_vehicle_instance::set_forward_or_reverse_accelerator(float input)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    if (m_misc_instance_data.m_is_trailer)
    {
        input = 0.0f;
    }
    const hk_custom_vehicle_transmission* transmission = static_cast<const hk_custom_vehicle_transmission*>(m_transmission);
    // 0.5: half throttle while the gear changes
    if (transmission->getDesiredGear() != m_currentGear && 0.0f <= input - 0.5f)
    {
        input = 0.5f;
    }
    if (input - -1.0f < 0.0f)
    {
        input = -1.0f;
    }
    if (0.0f <= input - 1.0f)
    {
        input = 1.0f;
    }

    const hkVector4f& velocity = getChassis()->getLinearVelocity();
    const hkVector4f& forward = getChassis()->getTransform().getRotation().getColumn<2>();
    const float speed = velocity(1) * forward(1) + velocity(0) * forward(0) + velocity(2) * forward(2);

    hk_custom_vehicle_brakes* brake = static_cast<hk_custom_vehicle_brakes*>(m_brake);
    brake_set_blocking(brake, m_misc_instance_data.m_ai_controlled ? brake->m_ai_min_time_to_block : brake->m_min_time_to_block, brake->m_min_pedal_input_to_block);

    if (input == 0.0f && m_misc_instance_data.m_is_tank)
    {
        // 0.7: a tank without throttle brakes on its treads
        m_driver_input_instance_data.m_brake_pedal_input = 0.7f;
        m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
        m_driver_input_instance_data.m_handbrake_on = true;
        // 0.1: stopped
        if (fabs((double)speed) < 0.1)
        {
            brake_set_parked(brake);
        }
        return;
    }

    if (static_cast<const hk_custom_vehicle_driver_input*>(m_driverInput)->m_autoReverse.m_bool == 0)
    {
        m_misc_instance_data.f2r_ts.invalidate();
        m_misc_instance_data.r2f_ts.invalidate();
        if (0.0f <= input)
        {
            m_driver_input_instance_data.m_accelerator_pedal_input = input;
            m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
            return;
        }
        m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
        m_driver_input_instance_data.m_brake_pedal_input = -input;
        return;
    }

    const float auto_brake_speed = m_misc_instance_data.m_auto_brake_speed;
    if (auto_brake_speed < speed)
    {
        // Moving forward: back input brakes.
        if (input < 0.0f)
        {
            m_driver_input_instance_data.m_brake_pedal_input = -input;
            m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
        }
        else
        {
            m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
            m_driver_input_instance_data.m_accelerator_pedal_input = input;
        }
        m_misc_instance_data.f2r_ts.set(m_misc_instance_data.m_forward_to_reverse_delay);
        m_misc_instance_data.r2f_ts.invalidate();
        return;
    }
    if (speed < -auto_brake_speed)
    {
        // Moving backward: forward input brakes.
        if (input <= 0.0f)
        {
            m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
            m_driver_input_instance_data.m_accelerator_pedal_input = -input;
        }
        else
        {
            m_driver_input_instance_data.m_brake_pedal_input = input;
            m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
        }
        m_misc_instance_data.f2r_ts.invalidate();
        m_misc_instance_data.r2f_ts.set(m_misc_instance_data.m_reverse_to_forward_delay);
        return;
    }
    // 0.01: no input at low speed brakes to a stop
    if (fabs((double)input) < 0.01)
    {
        m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
        // 0.95, 0.7: harder as the vehicle slows
        m_driver_input_instance_data.m_brake_pedal_input = ((auto_brake_speed - fabsf(speed * 0.95f)) / auto_brake_speed) * 0.7f;
        brake_set_parked(brake);
        m_tryingToReverse = false;
        m_misc_instance_data.f2r_ts.invalidate();
        m_misc_instance_data.r2f_ts.invalidate();
        return;
    }
    if (0.0f < input)
    {
        if (m_misc_instance_data.r2f_ts.elapsed() || !m_misc_instance_data.r2f_ts.valid())
        {
            m_tryingToReverse = false;
            m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
            m_driver_input_instance_data.m_accelerator_pedal_input = input;
            return;
        }
    }
    else
    {
        if (m_misc_instance_data.f2r_ts.elapsed() || !m_misc_instance_data.f2r_ts.valid())
        {
            m_tryingToReverse = true;
            m_driver_input_instance_data.m_brake_pedal_input = 0.0f;
            m_driver_input_instance_data.m_accelerator_pedal_input = -input;
            return;
        }
        input = -input;
    }
    // Waiting out the direction change delay.
    m_driver_input_instance_data.m_brake_pedal_input = input;
    m_driver_input_instance_data.m_accelerator_pedal_input = 0.0f;
}

// 0x00B746E0 (:1155). The sign is flipped: the game steers the other way round.
void hk_custom_vehicle_instance::set_steering_wheel_input(float input)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    if (m_misc_instance_data.m_is_trailer)
    {
        input = 0.0f;
    }
    float steering = input * -1.0f;
    if (steering - -1.0f < 0.0f)
    {
        steering = -1.0f;
    }
    if (0.0f <= steering - 1.0f)
    {
        steering = 1.0f;
    }
    if (!m_misc_instance_data.m_ai_controlled)
    {
        // The player's stick has two slopes.
        const hk_custom_vehicle_driver_input* driver = static_cast<const hk_custom_vehicle_driver_input*>(m_driverInput);
        const float sign = steering < 0.0f ? -1.0f : 1.0f;
        const float amount = fabsf(steering);
        if (driver->m_slopeChangePointX < amount)
        {
            const float at_change = driver->m_initialSlope * driver->m_slopeChangePointX;
            const float rest = 1.0f - driver->m_slopeChangePointX;
            const float slope = rest <= 0.0f ? 0.0f : (1.0f - at_change) / rest;
            steering = ((amount - driver->m_slopeChangePointX) * slope + at_change) * sign;
        }
        else
        {
            steering = driver->m_initialSlope * sign * amount;
        }

        // Less steering at reverse speed.
        m_driver_input_instance_data.m_steering_wheel_input = steering;
        const hkVector4f& velocity = getChassis()->getLinearVelocity();
        const hkVector4f& forward = getChassis()->getTransform().getRotation().getColumn<2>();
        const float reverse_speed = -(velocity(1) * forward(1) + velocity(0) * forward(0) + velocity(2) * forward(2));
        if (Vehicle_reverse_steer_speed_min < reverse_speed)
        {
            float t = (reverse_speed - Vehicle_reverse_steer_speed_min) / (Vehicle_reverse_steer_speed_max - Vehicle_reverse_steer_speed_min);
            if (t - 0.0f < 0.0f)
            {
                t = 0.0f;
            }
            if (0.0f <= t - 1.0f)
            {
                t = 1.0f;
            }
            m_driver_input_instance_data.m_steering_wheel_input = (Vehicle_reverse_steer_speed_pct * t + (1.0f - t)) * m_driver_input_instance_data.m_steering_wheel_input;
        }
        steering = m_driver_input_instance_data.m_steering_wheel_input;
        if (steering - -1.0f < 0.0f)
        {
            steering = -1.0f;
        }
        if (0.0f <= steering - 1.0f)
        {
            steering = 1.0f;
        }
    }
    m_driver_input_instance_data.m_steering_wheel_input = steering;
}

// 0x00B749B0 (:1243)
void hk_custom_vehicle_instance::set_parking(bool parking_on)
{
    hk_custom_vehicle_brakes* brake = static_cast<hk_custom_vehicle_brakes*>(m_brake);
    if (parking_on)
    {
        brake_set_parked(brake);
    }
    else
    {
        brake_set_blocking(brake, m_misc_instance_data.m_ai_controlled ? brake->m_ai_min_time_to_block : brake->m_min_time_to_block, brake->m_min_pedal_input_to_block);
    }
}

// 0x00B74A40 (:1270)
bool hk_custom_vehicle_instance::get_parking()
{
    const hk_custom_vehicle_brakes* brake = static_cast<const hk_custom_vehicle_brakes*>(m_brake);
    if (brake->m_wheelsMinTimeToBlock != 1.0f)
    {
        return false;
    }
    for (int w = 0; w < brake->m_wheelBrakingProperties.getSize(); w++)
    {
        if (brake->m_wheelBrakingProperties[w].m_minPedalInputToBlock != 0.1f)
        {
            return false;
        }
    }
    return true;
}

// 0x00B8A310 (:1295): 1000 ms of ignored input after the crash
void hk_custom_vehicle_instance::set_inputs_due_to_crash(float steering_param)
{
    m_driver_input_instance_data.m_ignore_new_driver_input_timer.invalidate();
    set_steering_wheel_input(steering_param);
    set_forward_or_reverse_accelerator(0.0f);
    if (!ignore_new_driver_input())
    {
        m_driver_input_instance_data.m_handbrake_on = false;
    }
    m_driver_input_instance_data.m_ignore_new_driver_input_timer.set(1000);
}

// 0x00B63BB0 (:1312)
void hk_custom_vehicle_instance::set_ai_controlled(bool ai_controlled)
{
    m_misc_instance_data.m_ai_controlled = ai_controlled;
}

// 0x00B63BD0 (:1321)
bool hk_custom_vehicle_instance::get_ai_controlled()
{
    return m_misc_instance_data.m_ai_controlled;
}

// 0x00B63BE0 (:1331)
void hk_custom_vehicle_instance::set_ai_torque_multiplier(float ai_torque_multiplier)
{
    m_engine_instance_data.m_ai_torque_multiplier = ai_torque_multiplier;
}

// 0x00B63C00 (:1341)
void hk_custom_vehicle_instance::set_powerslide_torque_multiplier(float powerslide_torque_multiplier)
{
    m_engine_instance_data.m_powerslide_torque_multiplier = powerslide_torque_multiplier;
}

// 0x00B63C20 (:1350)
void hk_custom_vehicle_instance::set_ai_top_speed_adjustment(float ai_top_speed_adjustment)
{
    m_engine_instance_data.m_ai_top_speed_adjustment = ai_top_speed_adjustment;
}

// 0x00B63C40 (:1359)
float hk_custom_vehicle_instance::get_ai_top_speed_adjustment()
{
    return m_engine_instance_data.m_ai_top_speed_adjustment;
}

// 0x00B63C50 (:1368)
void hk_custom_vehicle_instance::set_reverse_torque_multiplier(float reverse_torque_multiplier)
{
    m_engine_instance_data.m_reverse_torque_multiplier = reverse_torque_multiplier;
}

// 0x00B63C70 (:1377): -1 to 1 over the steering range
float hk_custom_vehicle_instance::get_steering_t() const
{
    const float max_angle = static_cast<const hk_custom_vehicle_steering*>(m_steering)->m_maxSteeringAngle;
    const float t = fl_parameter(m_mainSteeringAngle, max_angle, -max_angle, true);
    return (t + t) - 1.0f;
}

// 0x00B63CC0 (:1386): a negative reverse speed means the forward one
void hk_custom_vehicle_instance::set_enforced_max_speed(float ems, float emrs)
{
    m_misc_instance_data.m_enforced_max_speed = ems;
    m_misc_instance_data.m_enforced_max_rspeed = emrs < 0.0f ? ems : emrs;
}

// 0x00B63D00 (:1398)
void hk_custom_vehicle_instance::set_extra_aabb_limits(hkVector4f& min, hkVector4f& max)
{
    m_misc_instance_data.tread_wheel_aabb_min = min;
    m_misc_instance_data.tread_wheel_aabb_max = max;
}

// 0x00B63D20 (:1404)
void hk_custom_vehicle_instance::get_extra_aabb_limits(hkVector4f& min, hkVector4f& max)
{
    min = m_misc_instance_data.tread_wheel_aabb_min;
    max = m_misc_instance_data.tread_wheel_aabb_max;
}

// 0x00B74AA0 (:1411)
void hk_custom_vehicle_instance::set_npc_ray_override(bool ride)
{
    static_cast<hk_custom_vehicle_raycast_wheel_collide*>(m_wheelCollide)->set_npc_ray_override(ride);
}

// 0x00B8A380 (:1418)
void hk_custom_vehicle_instance::clear_wheel_caches()
{
    static_cast<hk_custom_vehicle_raycast_wheel_collide*>(m_wheelCollide)->clear_wheel_caches();
}

// 0x00B63D40 (:1430)
bool hk_custom_vehicle_instance::ignore_new_driver_input() const
{
    const timestamp& timer = m_driver_input_instance_data.m_ignore_new_driver_input_timer;
    return timer.valid() && !timer.elapsed();
}

// 0x00B71630 (.h:61)
void hk_custom_vehicle_instance::set_accelerator_pedal_input(float input)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    if (m_misc_instance_data.m_is_trailer)
    {
        input = 0.0f;
    }
    // 0.5: half throttle while the gear changes
    if (static_cast<const hk_custom_vehicle_transmission*>(m_transmission)->getDesiredGear() != m_currentGear && 0.0f <= input - 0.5f)
    {
        input = 0.5f;
    }
    if (input - 0.0f < 0.0f)
    {
        input = 0.0f;
    }
    if (0.0f <= input - 1.0f)
    {
        input = 1.0f;
    }
    m_driver_input_instance_data.m_accelerator_pedal_input = input;
}

// 0x00B632F0 (.h:88)
float hk_custom_vehicle_instance::get_accelerator_pedal_input() const
{
    return m_driver_input_instance_data.m_accelerator_pedal_input;
}

// 0x00B716F0 (.h:97)
void hk_custom_vehicle_instance::set_brake_pedal_input(float input)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    if (m_misc_instance_data.m_is_trailer)
    {
        input = 0.0f;
    }
    if (input - 0.0f < 0.0f)
    {
        input = 0.0f;
    }
    if (0.0f <= input - 1.0f)
    {
        input = 1.0f;
    }
    m_driver_input_instance_data.m_brake_pedal_input = input;
}

// 0x00B63300 (.h:116)
float hk_custom_vehicle_instance::get_brake_pedal_input() const
{
    return m_driver_input_instance_data.m_brake_pedal_input;
}

// 0x00B63310 (.h:141)
float hk_custom_vehicle_instance::get_steering_wheel_input() const
{
    return m_driver_input_instance_data.m_steering_wheel_input;
}

// 0x00B63320 (.h:148)
bool hk_custom_vehicle_instance::get_trying_to_reverse() const
{
    return m_tryingToReverse.m_bool != 0;
}

// 0x00B71780 (.h:157)
void hk_custom_vehicle_instance::set_handbrake_on(bool handbrake_on)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    m_driver_input_instance_data.m_handbrake_on = m_misc_instance_data.m_is_trailer ? false : handbrake_on;
}

// 0x00B63330 (.h:176)
bool hk_custom_vehicle_instance::get_handbrake_on() const
{
    return m_driver_input_instance_data.m_handbrake_on;
}

// 0x00B717D0 (.h:188)
void hk_custom_vehicle_instance::set_trying_to_reverse(bool trying_to_reverse)
{
    if (ignore_new_driver_input())
    {
        return;
    }
    m_tryingToReverse = m_misc_instance_data.m_is_trailer ? false : trying_to_reverse;
}

// 0x00B71820 (.h:203)
float hk_custom_vehicle_instance::get_wheel_spin_velocity(int wheel)
{
    return m_wheelsInfo[wheel].m_spinVelocity;
}

// 0x00B63340 (.h:237)
bool hk_custom_vehicle_instance::get_is_trailer() const
{
    return m_misc_instance_data.m_is_trailer;
}

// 0x00B63350 (.h:239)
void hk_custom_vehicle_instance::set_is_tank(bool is_tank)
{
    m_misc_instance_data.m_is_tank = is_tank;
}

// 0x00B63370 (.h:240)
bool hk_custom_vehicle_instance::get_is_tank() const
{
    return m_misc_instance_data.m_is_tank;
}

// 0x00B63380 (.h:245)
void hk_custom_vehicle_instance::set_is_infinite_mass(bool is_inf)
{
    m_misc_instance_data.m_is_infinite_mass = is_inf;
}

// 0x00B633A0 (.h:246)
bool hk_custom_vehicle_instance::get_is_infinite_mass() const
{
    return m_misc_instance_data.m_is_infinite_mass;
}

// 0x00B633B0 (.h:248)
void hk_custom_vehicle_instance::set_is_extra_mass(bool is_ext)
{
    m_misc_instance_data.m_is_extra_mass = is_ext;
}

// 0x00B633D0 (.h:250)
void hk_custom_vehicle_instance::set_extra_mass_val(float ext_val)
{
    m_misc_instance_data.m_extra_mass_val = ext_val;
}

// 0x00B633F0 (.h:253)
void hk_custom_vehicle_instance::set_is_trying_to_recover(bool is_rec)
{
    m_misc_instance_data.m_is_trying_to_recover = is_rec;
}

// 0x00B63410 (.h:256)
void hk_custom_vehicle_instance::set_is_engine_running(bool is_run)
{
    m_misc_instance_data.m_is_engine_running = is_run;
}

// 0x00B63430 (.h:257)
bool hk_custom_vehicle_instance::get_is_engine_running() const
{
    return m_misc_instance_data.m_is_engine_running;
}

// 0x00B63440 (.h:259)
void hk_custom_vehicle_instance::set_auto_brake_speed(float speed)
{
    m_misc_instance_data.m_auto_brake_speed = speed;
}

// 0x00B63460 (.h:260)
float hk_custom_vehicle_instance::get_auto_brake_speed() const
{
    return m_misc_instance_data.m_auto_brake_speed;
}

// 0x00B63490 (.h:266)
void hk_custom_vehicle_instance::set_handbrake_yaw_factor_scalar(float scalar)
{
    m_misc_instance_data.m_original_yaw_factor_scalar = scalar;
}

// 0x00B634B0 (.h:271)
void hk_custom_vehicle_instance::set_transition_delays(unsigned int f2r, unsigned int r2f)
{
    m_misc_instance_data.m_forward_to_reverse_delay = f2r;
    m_misc_instance_data.m_reverse_to_forward_delay = r2f;
}
