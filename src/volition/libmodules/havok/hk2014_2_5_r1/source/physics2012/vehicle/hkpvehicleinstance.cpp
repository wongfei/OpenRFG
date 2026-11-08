// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): the vehicle action, which runs wheel collision and each component in turn and then
// the simulation, from rfg.exe. Havok's timer markers are left out, as in the other reimplemented Havok files.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/manager/multithreadedvehicle/hkpvehiclejobs.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/tyremarks/hkptyremarksinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/hkpvehicleengine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/hkpvehiclesteering.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/hkpvehiclevelocitydamper.h"

#include <cmath>

// 0x00F82100
hkpVehicleInstance::hkpVehicleInstance(hkpRigidBody* chassis)
    : hkpUnaryAction(chassis, 0)
{
    m_data = NULL;
    m_driverInput = NULL;
    m_steering = NULL;
    m_engine = NULL;
    m_transmission = NULL;
    m_brake = NULL;
    m_suspension = NULL;
    m_aerodynamics = NULL;
    m_wheelCollide = NULL;
    m_tyreMarks = NULL;
    m_velocityDamper = NULL;
    m_deviceStatus = NULL;
    m_vehicleSimulation = NULL;
}

// 0x00B632C0 (hkpvehicleinstance.inl:10)
hkpRigidBody* hkpVehicleInstance::getChassis() const
{
    return static_cast<hkpRigidBody*>(m_entity);
}

// 0x00B632D0 (hkpvehicleinstance.inl:15)
unsigned char hkpVehicleInstance::getNumWheels() const
{
    return m_data->m_numWheels;
}

// 0x00F81490
void hkpVehicleInstance::setChassis(hkpRigidBody* chassis)
{
    setEntity(chassis);
}

// 0x00F81730
void hkpVehicleInstance::WheelInfo::init()
{
    m_contactPoint.setPosition(hkVector4f::getConstant<HK_QUADREAL_0>());
    m_contactPoint.setSeparatingNormal(hkVector4f::getConstant<HK_QUADREAL_0100>());
    m_contactPoint.setDistance(1.0f);
    m_hardPointWs.setZero();
    m_rayEndPointWs.setZero();
    m_suspensionDirectionWs.setZero();
    m_spinAxisWs.setZero();
    m_contactFriction = 0.0f;
    m_contactBody = NULL;
    m_contactShapeKey[0] = 0xFFFFFFFF;
    m_currentSuspensionLength = 0.0f;
    m_spinAxisChassisSpace.set(1.0f, 0.0f, 0.0f, 0.0f);
    m_steeringOrientationChassisSpace.m_vec = hkVector4f::getConstant<HK_QUADREAL_0001>();
    m_spinVelocity = 0.0f;
    m_noSlipIdealSpinVelocity = 0.0f;
    m_spinAngle = 0.0f;
    m_skidEnergyDensity = 0.0f;
    m_sideForce = 0.0f;
    m_forwardSlipVelocity = 0.0f;
    m_sideSlipVelocity = 0.0f;
}

// 0x00F82170
hkpVehicleInstance::~hkpVehicleInstance()
{
    if (m_data != NULL)
    {
        m_data->removeReference();
    }
    if (m_driverInput != NULL)
    {
        m_driverInput->removeReference();
    }
    if (m_steering != NULL)
    {
        m_steering->removeReference();
    }
    if (m_engine != NULL)
    {
        m_engine->removeReference();
    }
    if (m_transmission != NULL)
    {
        m_transmission->removeReference();
    }
    if (m_brake != NULL)
    {
        m_brake->removeReference();
    }
    if (m_suspension != NULL)
    {
        m_suspension->removeReference();
    }
    if (m_aerodynamics != NULL)
    {
        m_aerodynamics->removeReference();
    }
    if (m_wheelCollide != NULL)
    {
        m_wheelCollide->removeReference();
    }
    if (m_velocityDamper != NULL)
    {
        m_velocityDamper->removeReference();
    }
    if (m_deviceStatus != NULL)
    {
        m_deviceStatus->removeReference();
    }
    if (m_vehicleSimulation != NULL)
    {
        m_vehicleSimulation->removeReference();
    }
    if (m_tyreMarks != NULL)
    {
        m_tyreMarks->removeReference();
    }
}

// 0x00F81C80
void hkpVehicleInstance::init()
{
    if (m_data->m_alreadyInitialised.m_bool == 0)
    {
        m_data->init(m_suspension->m_wheelParams, getChassis());
    }

    // The chassis inertia: the unit inertias through the chassis orientation, on the diagonal.
    const hkRotationf& orientation = m_data->m_chassisOrientation;
    const float yaw = m_data->m_chassisUnitInertiaYaw;
    const float roll = m_data->m_chassisUnitInertiaRoll;
    const float pitch = m_data->m_chassisUnitInertiaPitch;
    const float mass = getChassis()->getMass();
    hkVector4f diagonal;
    diagonal.set(
        (fabsf(orientation.getColumn<0>()(0)) * yaw + fabsf(orientation.getColumn<1>()(0)) * roll + fabsf(orientation.getColumn<2>()(0)) * pitch) * mass,
        (fabsf(orientation.getColumn<0>()(1)) * yaw + fabsf(orientation.getColumn<1>()(1)) * roll + fabsf(orientation.getColumn<2>()(1)) * pitch) * mass,
        (fabsf(orientation.getColumn<0>()(2)) * yaw + fabsf(orientation.getColumn<1>()(2)) * roll + fabsf(orientation.getColumn<2>()(2)) * pitch) * mass,
        (fabsf(orientation.getColumn<0>()(3)) * yaw + fabsf(orientation.getColumn<1>()(3)) * roll + fabsf(orientation.getColumn<2>()(3)) * pitch) * mass);
    hkMatrix3f inertia;
    inertia.getColumn<0>().setMul(diagonal, hkVector4f::getConstant<HK_QUADREAL_1000>());
    inertia.getColumn<1>().setMul(diagonal, hkVector4f::getConstant<HK_QUADREAL_0100>());
    inertia.getColumn<2>().setMul(diagonal, hkVector4f::getConstant<HK_QUADREAL_0010>());
    getChassis()->setInertiaLocal(inertia);

    m_wheelsInfo.setSize(m_data->m_numWheels);
    for (int w = 0; w < m_wheelsInfo.getSize(); w++)
    {
        m_wheelsInfo[w].init();
    }

    m_isFixed.setSize(m_data->m_numWheels);
    for (int w = 0; w < m_isFixed.getSize(); w++)
    {
        m_isFixed[w] = false;
    }

    m_wheelsTimeSinceMaxPedalInput = 0.0f;
    m_mainSteeringAngle = 0.0f;
    m_mainSteeringAngleAssumingNoReduction = 0.0f;
    m_wheelsSteeringAngle.setSize(m_data->m_numWheels);
    for (int w = 0; w < m_wheelsSteeringAngle.getSize(); w++)
    {
        m_wheelsSteeringAngle[w] = 0.0f;
    }

    m_torque = 0.0f;
    m_rpm = 0.0f;
    m_isReversing = false;
    m_currentGear = 0;
    m_delayed = false;
    m_clutchDelayCountdown = 0.0f;

    m_wheelCollide->init(this);
    m_wheelCollide->m_alreadyUsed = true;
    if (m_vehicleSimulation == NULL)
    {
        m_vehicleSimulation = new hkpVehicleDefaultSimulation();
    }
    m_vehicleSimulation->init(this);
}

// 0x00F814A0
void hkpVehicleInstance::getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantomsOut)
{
    m_wheelCollide->getPhantoms(phantomsOut);
}

// 0x00F81650
void hkpVehicleInstance::handleFixedGroundAccum(hkpRigidBody* ground, hkpVelocityAccumulator& accum)
{
    accum.setFixed();
}

// 0x00F817E0
void hkpVehicleInstance::updateBeforeCollisionDetection()
{
    const hkTransformf& transform = getChassis()->getTransform();
    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        const hkpVehicleSuspension::SuspensionWheelParameters& params = m_suspension->m_wheelParams[w];
        wheel.m_suspensionDirectionWs.setRotatedDir(transform.getRotation(), params.m_directionChassisSpace);
        wheel.m_hardPointWs.setTransformedPos(transform, params.m_hardpointChassisSpace);
        const float length = params.m_length + m_data->m_wheelParams[w].m_radius;
        wheel.m_rayEndPointWs.setAddMul(wheel.m_hardPointWs, wheel.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(length));
    }
    m_wheelCollide->updateBeforeCollisionDetection(this);
}

// 0x00F822D0
void hkpVehicleInstance::updateWheels(float deltaTime, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo)
{
    const hkpRigidBody* chassis = getChassis();
    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        WheelInfo& wheel = m_wheelsInfo[w];
        const hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cd = cdInfo[w];
        wheel.m_currentSuspensionLength = cd.m_currentSuspensionLength;
        wheel.m_contactPoint = cd.m_contactPoint;
        wheel.m_contactFriction = cd.m_contactFriction;
        wheel.m_contactBody = cd.m_contactBody;
        for (int k = 0; k < WheelInfo::MAX_NUM_SHAPE_KEYS; k++)
        {
            wheel.m_contactShapeKey[k] = cd.m_contactShapeKey[k];
        }

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
        wheel.m_spinAxisWs.setRotatedDir(chassis->getTransform().getRotation(), spin_axis);
    }
}

// 0x00F814B0
void hkpVehicleInstance::updateDriverInput(float deltaTime, hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo)
{
    filteredDriverInputInfo.m_tryingToReverse = m_tryingToReverse;
    m_driverInput->calcDriverInput(deltaTime, this, m_deviceStatus, filteredDriverInputInfo);
    m_tryingToReverse = filteredDriverInputInfo.m_tryingToReverse;
}

// 0x00F82710
void hkpVehicleInstance::updateSteering(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo)
{
    hkpVehicleSteering::SteeringAnglesOutput steering;
    steering.m_mainSteeringAngle = m_mainSteeringAngle;
    steering.m_mainSteeringAngleAssumingNoReduction = m_mainSteeringAngleAssumingNoReduction;
    steering.m_wheelsSteeringAngle.setSize(m_wheelsSteeringAngle.getSize());
    for (int w = 0; w < m_wheelsSteeringAngle.getSize(); w++)
    {
        steering.m_wheelsSteeringAngle[w] = m_wheelsSteeringAngle[w];
    }

    m_steering->calcSteering(deltaTime, this, filteredDriverInputInfo, steering);

    m_mainSteeringAngle = steering.m_mainSteeringAngle;
    m_mainSteeringAngleAssumingNoReduction = steering.m_mainSteeringAngleAssumingNoReduction;
    for (int w = 0; w < m_wheelsSteeringAngle.getSize(); w++)
    {
        m_wheelsSteeringAngle[w] = steering.m_wheelsSteeringAngle[w];
    }
}

// 0x00F814F0
void hkpVehicleInstance::updateTransmission(float deltaTime, hkpVehicleTransmission::TransmissionOutput& transmissionInfo)
{
    transmissionInfo.m_numWheelsTramsmittedTorque = m_data->m_numWheels;
    transmissionInfo.m_isReversing = m_isReversing;
    transmissionInfo.m_currentGear = m_currentGear;
    transmissionInfo.m_delayed = m_delayed;
    transmissionInfo.m_clutchDelayCountdown = m_clutchDelayCountdown;
    m_transmission->calcTransmission(deltaTime, this, transmissionInfo);
    m_isReversing = transmissionInfo.m_isReversing;
    m_currentGear = transmissionInfo.m_currentGear;
    m_delayed = transmissionInfo.m_delayed;
    m_clutchDelayCountdown = transmissionInfo.m_clutchDelayCountdown;
}

// 0x00F81570
void hkpVehicleInstance::updateEngine(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, const hkpVehicleTransmission::TransmissionOutput& transmissionInfo)
{
    hkpVehicleEngine::EngineOutput engine;
    m_engine->calcEngineInfo(deltaTime, this, filteredDriverInputInfo, transmissionInfo, engine);
    m_rpm = engine.m_rpm;
    m_torque = engine.m_torque;
}

// 0x00F81F30
void hkpVehicleInstance::updateBrake(float deltaTime, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, hkpVehicleBrake::WheelBreakingOutput& wheelBreakingInfo)
{
    wheelBreakingInfo.m_isFixed.setSize(m_isFixed.getSize());
    wheelBreakingInfo.m_brakingTorque.setSize(m_isFixed.getSize());
    for (int w = 0; w < m_isFixed.getSize(); w++)
    {
        wheelBreakingInfo.m_isFixed[w] = m_isFixed[w];
    }
    wheelBreakingInfo.m_wheelsTimeSinceMaxPedalInput = m_wheelsTimeSinceMaxPedalInput;

    m_brake->calcBreakingInfo(deltaTime, this, filteredDriverInputInfo, wheelBreakingInfo);

    for (int w = 0; w < wheelBreakingInfo.m_isFixed.getSize(); w++)
    {
        m_isFixed[w] = wheelBreakingInfo.m_isFixed[w];
    }
    m_wheelsTimeSinceMaxPedalInput = wheelBreakingInfo.m_wheelsTimeSinceMaxPedalInput;
}

// 0x00F816A0
void hkpVehicleInstance::updateSuspension(float deltaTime, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, hkArray<float,hkContainerHeapAllocator>& suspensionForces)
{
    m_suspension->calcSuspension(deltaTime, this, cdInfo, suspensionForces.begin());
}

// 0x00F815D0
void hkpVehicleInstance::updateAerodynamics(float deltaTime, hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo)
{
    m_aerodynamics->calcAerodynamics(deltaTime, this, aerodynamicsDragInfo);
}

// 0x00F82850
void hkpVehicleInstance::updateComponents(const hkStepInfo& stepInfo, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel)
{
    hkpVehicleDriverInput::FilteredDriverInputOutput filtered;
    hkpVehicleBrake::WheelBreakingOutput braking;
    hkpVehicleTransmission::TransmissionOutput transmission;
    hkLocalBuffer<float> transmitted(m_data->m_numWheels);
    transmission.m_wheelsTransmittedTorque = transmitted.begin();

    const float delta_time = stepInfo.m_deltaTime;
    updateWheels(delta_time, cdInfo);
    updateDriverInput(delta_time, filtered);
    updateSteering(delta_time, filtered);
    updateTransmission(delta_time, transmission);
    updateEngine(delta_time, filtered, transmission);
    updateBrake(delta_time, filtered, braking);
    updateSuspension(delta_time, cdInfo, suspensionForceAtWheel);
    updateAerodynamics(delta_time, aerodynamicsDragInfo);

    for (int w = 0; w < m_data->m_numWheels; w++)
    {
        totalLinearForceAtWheel[w] = (braking.m_brakingTorque[w] + transmission.m_wheelsTransmittedTorque[w]) / m_data->m_wheelParams[w].m_radius;
    }
}

// 0x00F81930
void hkpVehicleInstance::simulateVehicle(const hkStepInfo& stepInfo, const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel, hkpVehicleJobResults* vehicleJobResults)
{
    hkpVehicleSimulation::SimulationInput input(aerodynamicsDragInfo, suspensionForceAtWheel, totalLinearForceAtWheel);
    m_vehicleSimulation->simulateVehicle(this, stepInfo, input, vehicleJobResults);
}

// 0x00F82AB0
void hkpVehicleInstance::stepVehicleUsingWheelCollideOutput(const hkStepInfo& stepInfo, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo)
{
    hkpVehicleAerodynamics::AerodynamicsDragOutput aerodynamics;
    hkInplaceArray<float,16,hkContainerHeapAllocator> suspension_force;
    hkInplaceArray<float,16,hkContainerHeapAllocator> total_linear_force;
    suspension_force.setSizeUnchecked(m_data->m_numWheels);
    total_linear_force.setSizeUnchecked(m_data->m_numWheels);
    updateComponents(stepInfo, cdInfo, aerodynamics, suspension_force, total_linear_force);
    simulateVehicle(stepInfo, aerodynamics, suspension_force, total_linear_force, NULL);
}

// 0x00F82D00
void hkpVehicleInstance::applyAction(const hkStepInfo& stepInfo)
{
    stepVehicle(stepInfo);
}

// 0x00F82C10
void hkpVehicleInstance::stepVehicle(const hkStepInfo& stepInfo)
{
    updateBeforeCollisionDetection();
    hkLocalBuffer<hkpVehicleWheelCollide::CollisionDetectionWheelOutput> cd(m_data->m_numWheels);
    m_wheelCollide->collideWheels(stepInfo.m_deltaTime, this, cd.begin());
    stepVehicleUsingWheelCollideOutput(stepInfo, cd.begin());
}

// 0x00F819D0: chassis * steering * chassis orientation * spin
void hkpVehicleInstance::calcCurrentPositionAndRotation(const hkpRigidBody* chassis, const hkpVehicleSuspension* suspension, int wheelNo, hkVector4f& posOut, hkQuaternionf& rotOut)
{
    const WheelInfo& wheel = m_wheelsInfo[wheelNo];
    hkRotationf wheel_frame;
    wheel_frame.setCols(m_data->m_chassisOrientation.getColumn<1>(), m_data->m_chassisOrientation.getColumn<0>(), m_data->m_chassisOrientation.getColumn<2>());
    hkQuaternionf frame;
    frame.set(wheel_frame);

    hkVector4f axis;
    axis.setRotatedInverseDir(frame, wheel.m_spinAxisChassisSpace);
    hkQuaternionf spin;
    spin.setAxisAngle(axis, -wheel.m_spinAngle);

    hkQuaternionf steered;
    steered.setMul(chassis->getRotation(), wheel.m_steeringOrientationChassisSpace);
    hkQuaternionf framed;
    framed.setMul(steered, frame);
    rotOut.setMul(framed, spin);

    const float length = 0.0f <= wheel.m_currentSuspensionLength ? wheel.m_currentSuspensionLength : 0.0f;
    posOut._setTransformedPos(chassis->getTransform(), suspension->m_wheelParams[wheelNo].m_hardpointChassisSpace);
    posOut.addMul(hkSimdFloat32::fromFloat(length), wheel.m_suspensionDirectionWs);
}

// 0x00F81600
float hkpVehicleInstance::calcRPM() const
{
    return m_rpm;
}

// 0x00F82020
float hkpVehicleInstance::calcKMPH() const
{
    return getChassis()->getLinearVelocity().length<3>().getReal() * 0.001f * 3600.0f;
}

// 0x00F82090: 0.00062138814 miles per metre
float hkpVehicleInstance::calcMPH() const
{
    return getChassis()->getLinearVelocity().length<3>().getReal() * 0.00062138814f * 3600.0f;
}

// 0x00F824C0
hkpAction* hkpVehicleInstance::clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& newEntities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const
{
    hkpRigidBody* chassis = static_cast<hkpRigidBody*>(newEntities[0]);
    hkpVehicleInstance* instance = new hkpVehicleInstance(chassis);
    *instance = *this;
    instance->m_world = NULL;
    instance->m_island = NULL;
    instance->m_entity = chassis;
    instance->m_wheelCollide = m_wheelCollide->clone(*chassis, newPhantoms);
    instance->m_wheelCollide->m_alreadyUsed = true;
    instance->m_vehicleSimulation = m_vehicleSimulation->clone(instance);
    instance->m_deviceStatus = m_deviceStatus->clone();
    instance->m_data->addReference();
    instance->m_driverInput->addReference();
    instance->m_steering->addReference();
    instance->m_engine->addReference();
    instance->m_transmission->addReference();
    instance->m_brake->addReference();
    instance->m_suspension->addReference();
    instance->m_aerodynamics->addReference();
    instance->m_velocityDamper->addReference();
    if (m_tyreMarks != NULL)
    {
        instance->m_tyreMarks->addReference();
    }
    return instance;
}

// 0x00F816D0
void hkpVehicleInstance::addToWorld(hkpWorld* world)
{
    world->addEntity(m_entity, HK_ENTITY_ACTIVATION_DO_ACTIVATE);
    m_wheelCollide->addToWorld(world);
    m_vehicleSimulation->addToWorld(world);
}

// 0x00F81700
void hkpVehicleInstance::removeFromWorld()
{
    m_vehicleSimulation->removeFromWorld();
    m_wheelCollide->removeFromWorld();
    m_entity->m_world->removeEntity(m_entity);
}

// 0x00B63290 (hkpvehiclesimulation.h:47)
void hkpVehicleSimulation::addToWorld(hkpWorld* world)
{
}

// 0x00B632A0 (hkpvehiclesimulation.h:48)
void hkpVehicleSimulation::removeFromWorld()
{
}

// 0x00F82E20 (hkpvehiclesimulation.h:28)
hkpVehicleSimulation::SimulationInput::SimulationInput(const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamics, const hkArray<float,hkContainerHeapAllocator>& suspensions, const hkArray<float,hkContainerHeapAllocator>& linearForces)
    : aerodynamicsDragInfo(aerodynamics), suspensionForceAtWheel(suspensions), totalLinearForceAtWheel(linearForces)
{
}

// 0x00F83BE0
void hkpVehicleJobResults::applyForcesFromStep(const hkpVehicleInstance& vehicleInstance)
{
    // 0.001: smaller impulses on the ground are ignored
    const hkSimdFloat32 threshold = hkSimdFloat32::fromFloat(0.001f);
    const int num_wheels = (unsigned char)vehicleInstance.m_data->m_numWheels;
    for (int w = 0; w < num_wheels; w++)
    {
        if (!m_groundBodyImpulses[w].allEqualZero<3>(threshold))
        {
            const hkpVehicleInstance::WheelInfo& wheel = vehicleInstance.m_wheelsInfo[w];
            wheel.m_contactBody->activate();
            wheel.m_contactBody->getMotion()->applyPointImpulse(m_groundBodyImpulses[w], wheel.m_contactPoint.getPosition());
        }
    }

    hkpRigidBody* chassis = vehicleInstance.getChassis();
    chassis->activate();
    chassis->getMotion()->setAngularVelocity(m_chassisAngularVel);
    chassis->activate();
    chassis->getMotion()->setLinearVelocity(m_chassisLinearVel);
    for (int a = 0; a < 2; a++)
    {
        if (m_groundBodyPtr[a] != NULL)
        {
            m_groundBodyPtr[a]->activate();
            m_groundBodyPtr[a]->getMotion()->setAngularVelocity(m_groundBodyAngularVel[a]);
            m_groundBodyPtr[a]->activate();
            m_groundBodyPtr[a]->getMotion()->setLinearVelocity(m_groundBodyLinearVel[a]);
        }
    }
}
