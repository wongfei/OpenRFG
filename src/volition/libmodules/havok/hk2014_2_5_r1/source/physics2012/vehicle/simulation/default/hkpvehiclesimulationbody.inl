// New file, no original: the body of the vehicle simulation step, shared by hkpVehicleDefaultSimulation and the game's
// hkpCustomVehicleSimulation. rfg.exe has two copies. Volition copied Havok's file, and their decompiles differ only in
// register allocation and in which helpers were inlined (stub_impl.md S8), so they are written once here. The including
// file defines VEHICLE_SIMULATION_CLASS.

// Applies a world space angular impulse through a motion's inverse inertia, which is diagonal in body space.
static void vehicle_simulation_apply_angular(const hkpMotion* motion, hkVector4fParameter impulse, hkVector4f& angular)
{
    const hkRotationf& rotation = motion->getTransform().getRotation();
    hkVector4f local;
    local._setRotatedInverseDir(rotation, impulse);
    local.mul(motion->m_inertiaAndMassInv);
    hkVector4f world;
    world._setRotatedDir(rotation, local);
    angular.add(world);
}

// The accumulator's scratch registers hold its matrix from world to solver space.
static const hkMatrix3f& vehicle_simulation_core_from_world(const hkpVelocityAccumulator& accumulator)
{
    return reinterpret_cast<const hkMatrix3f&>(accumulator.m_scratch0);
}

// Damps both velocities of an accumulator and moves its angular velocity into solver space. The damped world velocities
// are kept for the ground bodies' velocity change.
static void vehicle_simulation_damp_accumulator(const hkStepInfo& stepInfo, hkpVelocityAccumulator& accumulator, const hkpMotion* motion, hkVector4f* originalLinearVel, hkVector4f* originalAngularVel)
{
    const float delta_time = stepInfo.m_deltaTime;
    const float linear = 1.0f - motion->m_motionState.m_linearDamping.getReal() * delta_time;
    const float angular = 1.0f - motion->m_motionState.m_angularDamping.getReal() * delta_time;
    accumulator.m_linearVel.setMul(accumulator.m_linearVel, hkSimdFloat32::fromFloat(0.0f < linear ? linear : 0.0f));
    accumulator.m_angularVel.setMul(accumulator.m_angularVel, hkSimdFloat32::fromFloat(0.0f < angular ? angular : 0.0f));
    if (originalLinearVel != NULL)
    {
        *originalLinearVel = accumulator.m_linearVel;
        *originalAngularVel = accumulator.m_angularVel;
    }
    const hkVector4f world_angular = accumulator.m_angularVel;
    accumulator.m_angularVel._setRotatedDir(vehicle_simulation_core_from_world(accumulator), world_angular);
}

void VEHICLE_SIMULATION_CLASS::simulateVehicle(hkpVehicleInstance* instance, const hkStepInfo& stepInfo, const hkpVehicleSimulation::SimulationInput& simulInput, hkpVehicleJobResults* vehicleJobResults)
{
    const hkpVehicleData* data = instance->m_data;
    hkpVehicleFrictionSolverParams params;
    params.m_axleParams[0].initialize();
    params.m_axleParams[1].initialize();

    const float delta_time = stepInfo.m_deltaTime;
    hkpRigidBody* ground_body[2] = { NULL, NULL };
    hkpVelocityAccumulator ground_accum[2];
    hkpVelocityAccumulator ground_accum_at_integration[2];
    hkVector4f original_linear[2];
    hkVector4f original_angular[2];
    hkpVehicleJobResults local_results;
    hkpVehicleJobResults* results = vehicleJobResults != NULL ? vehicleJobResults : &local_results;

    prepareAxleParams(*instance, delta_time, simulInput.suspensionForceAtWheel, simulInput.totalLinearForceAtWheel, ground_body, params, stepInfo, ground_accum, ground_accum_at_integration);
    prepareChassisParams(*instance, stepInfo, params);
    applyAerodynamicDrag(*instance, simulInput.aerodynamicsDragInfo, params, delta_time);
    applyVelocityDamping(delta_time, params, instance->m_velocityDamper);
    applySuspensionForces(*instance, delta_time, simulInput.suspensionForceAtWheel, ground_body, params, results->m_groundBodyImpulses);
    applyDampingToAxleAccumulators(stepInfo, ground_body, params, original_linear, original_angular);
    applyDampingToChassisAccumulator(stepInfo, params.m_chassis, instance->m_entity->getMotion());
    getExtraTorqueFactor(*instance, delta_time, params.m_chassis);
    applyFrictionSolver(*instance, stepInfo, params, m_frictionStatus);
    calcChassisVelocities(*instance, params.m_chassis, *results);
    calcGroundBodyVelocities(*instance, delta_time, params.m_axleParams, ground_body, *results, original_linear, original_angular);
    if (vehicleJobResults == NULL)
    {
        results->applyForcesFromStep(*instance);
    }

    // The wheels: slip, side force and spin from the solved axles.
    hkVector4f forward;
    forward.setRotatedDir(instance->getChassis()->getTransform().getRotation(), data->m_chassisOrientation.getColumn<1>());
    const float chassis_speed = params.m_chassis.m_linearVel.dot<3>(forward).getReal();
    for (int w = 0; w < (int)(unsigned char)instance->m_data->m_numWheels; w++)
    {
        hkpVehicleInstance::WheelInfo& wheel = instance->m_wheelsInfo[w];
        const hkpVehicleData::WheelComponentParams& params_wheel = data->m_wheelParams[w];
        const int axle = params_wheel.m_axle;
        const hkpVehicleFrictionStatus::AxisStatus& axis = m_frictionStatus.m_axis[axle];
        wheel.m_forwardSlipVelocity = axis.m_forward_slip_velocity;
        wheel.m_sideForce = axis.m_side_force;
        wheel.m_sideSlipVelocity = axis.m_side_slip_velocity;
        wheel.m_skidEnergyDensity = axis.m_skid_energy_density;
        if (instance->m_isFixed[w].m_bool == 0)
        {
            float speed = chassis_speed;
            if (ground_body[axle] != NULL)
            {
                speed = chassis_speed - params.m_axleParams[axle].m_groundObject->m_linearVel.dot<3>(forward).getReal();
            }
            wheel.m_noSlipIdealSpinVelocity = speed / params_wheel.m_radius;
            const float spin = (wheel.m_forwardSlipVelocity + speed) / params_wheel.m_radius;
            wheel.m_spinVelocity = spin;
            wheel.m_spinAngle = spin * delta_time + wheel.m_spinAngle;
        }
        else
        {
            wheel.m_spinVelocity = 0.0f;
            wheel.m_noSlipIdealSpinVelocity = 0.0f;
        }
        // 3141.5928: 1000 pi, keeps the angle's precision
        if (3141.5928f < wheel.m_spinAngle)
        {
            wheel.m_spinAngle = wheel.m_spinAngle - 3141.5928f;
        }
        else if (wheel.m_spinAngle < -3141.5928f)
        {
            wheel.m_spinAngle = wheel.m_spinAngle + 3141.5928f;
        }
    }
}

void VEHICLE_SIMULATION_CLASS::prepareAxleParams(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, const hkArray<float,hkContainerHeapAllocator>& totalLinearForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, const hkStepInfo& stepInfo, hkpVelocityAccumulator* groundAccum, hkpVelocityAccumulator* groundAccumAtIntegration)
{
    const float estimated_speed = vehicleInstance.getChassis()->getLinearVelocity().length<3>().getReal();
    const hkpVehicleData* data = vehicleInstance.m_data;
    for (int w = 0; w < data->m_numWheels; w++)
    {
        // Each axle's ground is its heaviest contact body.
        hkpRigidBody* body = vehicleInstance.m_wheelsInfo[w].m_contactBody;
        const int axle = data->m_wheelParams[w].m_axle;
        if (body != NULL && (groundBody[axle] == NULL || body->getMotion()->m_inertiaAndMassInv(3) < groundBody[axle]->getMotion()->m_inertiaAndMassInv(3)))
        {
            groundBody[axle] = body;
        }
        getAxleParamsFromWheel(vehicleInstance, w, totalLinearForceAtWheel[w], suspensionForceAtWheel[w], estimated_speed, frictionParams.m_axleParams[axle]);
    }

    frictionParams.m_maxVelocityForPositionalFriction = data->m_maxVelocityForPositionalFriction;
    for (int a = 0; a < data->m_numWheelsPerAxle.getSize(); a++)
    {
        hkpVehicleFrictionSolverAxleParams& axle = frictionParams.m_axleParams[a];
        axle.m_constraintNormalWs.normalize<3>();
        axle.m_forwardDirWs.normalize<3>();

        hkpRigidBody* ground = groundBody[a];
        hkpVelocityAccumulator& accum = groundAccum[a];
        axle.m_groundObject = &accum;
        axle.m_groundObjectAtLastIntegration = &accum;
        // 5: hkpMotion::MOTION_FIXED
        if (ground == NULL || ground->getMotion()->m_type.m_storage == 5)
        {
            handleFixedGroundAccum(ground, accum);
        }
        else if (a < 1 || ground != groundBody[0])
        {
            hkpMotion* motion = ground->getMotion();
            hkRigidMotionUtilBuildAccumulators(stepInfo, &motion, 1, 0, &accum);

            // A ground body too light for the solver takes a limited inverse mass.
            const float chassis_mass_inv = vehicleInstance.m_entity->getMotion()->m_inertiaAndMassInv(3);
            const float ground_mass_inv = motion->m_inertiaAndMassInv(3);
            const float max_mass_inv = data->m_maxFrictionSolverMassRatio * chassis_mass_inv;
            if (max_mass_inv < ground_mass_inv)
            {
                hkSimdFloat32 inv;
                inv.setReciprocal(hkSimdFloat32::fromFloat(ground_mass_inv));
                accum.m_invMasses.setMul(accum.m_invMasses, hkSimdFloat32::fromFloat(inv.getReal() * chassis_mass_inv * data->m_maxFrictionSolverMassRatio));
            }

            // The velocities the last integration really used, from the swept transform (its w is the inverse time).
            const hkMotionState& state = motion->m_motionState;
            const hkSimdFloat32 inv_time = state.getSweptTransform().m_centerOfMass1.getComponent<3>();
            hkpVelocityAccumulator& at_integration = groundAccumAtIntegration[a];
            at_integration = accum;
            axle.m_groundObjectAtLastIntegration = &at_integration;
            hkVector4f delta_angle;
            delta_angle.setMul(state.m_deltaAngle, inv_time);
            at_integration.m_angularVel._setRotatedDir(vehicle_simulation_core_from_world(accum), delta_angle);
            at_integration.m_linearVel.setSub(state.getSweptTransform().m_centerOfMass1, state.getSweptTransform().m_centerOfMass0);
            at_integration.m_linearVel.setMul(at_integration.m_linearVel, inv_time);
        }
        else
        {
            axle.m_groundObject = frictionParams.m_axleParams[0].m_groundObject;
            axle.m_groundObjectAtLastIntegration = frictionParams.m_axleParams[0].m_groundObjectAtLastIntegration;
        }
    }
}

void VEHICLE_SIMULATION_CLASS::handleFixedGroundAccum(hkpRigidBody* ground, hkpVelocityAccumulator& accum)
{
    accum.setFixed();
}

void VEHICLE_SIMULATION_CLASS::getAxleParamsFromWheel(const hkpVehicleInstance& vehicleInstance, int w_it, float linearForceAtWheel, float suspensionForceAtWheel, float estimatedCarSpeed, hkpVehicleFrictionSolverAxleParams& axle_params)
{
    const hkpVehicleInstance::WheelInfo& wheel = vehicleInstance.m_wheelsInfo[w_it];
    const hkpVehicleData::WheelComponentParams& wheel_params = vehicleInstance.m_data->m_wheelParams[w_it];
    const hkVector4f& normal = wheel.m_contactPoint.getNormal();

    hkVector4f forward;
    forward.setCross(normal, wheel.m_spinAxisWs);
    // A wheel without contact, or a normal along the spin axis: forward from the suspension.
    if (forward.lengthSquared<3>().getReal() < hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal())
    {
        forward.setCross(wheel.m_spinAxisWs, wheel.m_suspensionDirectionWs);
    }
    forward.normalize<3>();

    const float share = 1.0f / (float)(int)vehicleInstance.m_data->m_numWheelsPerAxle[wheel_params.m_axle];
    const float friction_share = wheel.m_contactFriction * share;
    axle_params.m_contactPointWs.addMul(hkSimdFloat32::fromFloat(share), wheel.m_contactPoint.getPosition());
    hkVector4f side;
    side.setCross(forward, normal);
    axle_params.m_constraintNormalWs.add(side);
    axle_params.m_forwardDirWs.add(forward);
    axle_params.m_frictionCoefficient = wheel_params.m_friction * friction_share + axle_params.m_frictionCoefficient;
    axle_params.m_viscosityFrictionCoefficient = wheel_params.m_viscosityFriction * friction_share + axle_params.m_viscosityFrictionCoefficient;
    axle_params.m_maxFrictionCoefficient = wheel_params.m_maxFriction * friction_share + axle_params.m_maxFrictionCoefficient;
    axle_params.m_wheelDownForce = axle_params.m_wheelDownForce + suspensionForceAtWheel;
    axle_params.m_forwardForce = linearForceAtWheel + axle_params.m_forwardForce;
    axle_params.m_wheelFixed = (axle_params.m_wheelFixed != 0 || vehicleInstance.m_isFixed[w_it].m_bool != 0) ? 1 : 0;
    axle_params.m_slipVelocityFactor = wheel_params.m_slipAngle * share * estimatedCarSpeed + axle_params.m_slipVelocityFactor;
}

void VEHICLE_SIMULATION_CLASS::prepareChassisParams(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams)
{
    hkpMotion* motion = vehicleInstance.m_entity->getMotion();
    hkRigidMotionUtilBuildAccumulators(stepInfo, &motion, 1, 0, &frictionParams.m_chassis);

    const hkMotionState& state = motion->m_motionState;
    const hkSimdFloat32 inv_time = state.getSweptTransform().m_centerOfMass1.getComponent<3>();
    hkVector4f linear;
    linear.setSub(state.getSweptTransform().m_centerOfMass1, state.getSweptTransform().m_centerOfMass0);
    linear.setMul(linear, inv_time);
    hkVector4f delta_angle;
    delta_angle.setMul(state.m_deltaAngle, inv_time);

    frictionParams.m_chassis.m_invMasses = vehicleInstance.m_data->m_chassisFrictionInertiaInvDiag;
    frictionParams.m_chassisAtLastIntegration = frictionParams.m_chassis;
    frictionParams.m_chassisAtLastIntegration.m_angularVel.setRotatedDir(vehicle_simulation_core_from_world(frictionParams.m_chassis), delta_angle);
    frictionParams.m_chassisAtLastIntegration.m_linearVel = linear;
}

void VEHICLE_SIMULATION_CLASS::applyAerodynamicDrag(const hkpVehicleInstance& vehicleInstance, const hkpVehicleAerodynamics::AerodynamicsDragOutput& aerodynamicsDragInfo, hkpVehicleFrictionSolverParams& frictionParams, const float deltaTime)
{
    const hkpMotion* motion = vehicleInstance.m_entity->getMotion();
    const hkSimdFloat32 dt = hkSimdFloat32::fromFloat(deltaTime);
    hkVector4f impulse;
    impulse.setMul(motion->m_inertiaAndMassInv.getComponent<3>(), aerodynamicsDragInfo.m_aerodynamicsForce);
    frictionParams.m_chassis.m_linearVel.addMul(impulse, dt);
    hkVector4f torque;
    torque.setMul(aerodynamicsDragInfo.m_aerodynamicsTorque, dt);
    vehicle_simulation_apply_angular(motion, torque, frictionParams.m_chassis.m_angularVel);
}

// Original quirk kept: reads the default damper's fields whatever the damper's class is.
void VEHICLE_SIMULATION_CLASS::applyVelocityDamping(const float deltaTime, hkpVehicleFrictionSolverParams& frictionParams, const hkpVehicleVelocityDamper* damper)
{
    const hkpVehicleDefaultVelocityDamper* default_damper = static_cast<const hkpVehicleDefaultVelocityDamper*>(damper);
    hkVector4f& spin = frictionParams.m_chassis.m_angularVel;
    const float damping = spin.lengthSquared<3>().getReal() <= default_damper->m_collisionThreshold * default_damper->m_collisionThreshold
        ? default_damper->m_normalSpinDamping : default_damper->m_collisionSpinDamping;
    const float factor = 1.0f - damping * deltaTime;
    spin.setMul(spin, hkSimdFloat32::fromFloat(0.0f <= factor ? factor : 0.0f));
}

void VEHICLE_SIMULATION_CLASS::applySuspensionForces(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkArray<float,hkContainerHeapAllocator>& suspensionForceAtWheel, hkpRigidBody** groundBody, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* suspensionForcesOnGround)
{
    const hkpVehicleData* data = vehicleInstance.m_data;
    const hkpMotion* chassis_motion = vehicleInstance.m_entity->getMotion();
    for (int w = 0; w < (int)(unsigned char)vehicleInstance.m_data->m_numWheels; w++)
    {
        const float force = suspensionForceAtWheel[w];
        const hkpVehicleInstance::WheelInfo& wheel = vehicleInstance.m_wheelsInfo[w];
        hkpRigidBody* body = wheel.m_contactBody;
        hkVector4f impulse;
        impulse.setMul(hkSimdFloat32::fromFloat(force * deltaTime), wheel.m_contactPoint.getNormal());

        // Applied at the hard point, on both bodies.
        if (0.0f < force)
        {
            frictionParams.m_chassis.m_linearVel.addMul(chassis_motion->m_inertiaAndMassInv.getComponent<3>(), impulse);
            hkVector4f r;
            r.setSub(wheel.m_hardPointWs, chassis_motion->getCenterOfMassInWorld());
            hkVector4f torque;
            torque.setCross(r, impulse);
            vehicle_simulation_apply_angular(chassis_motion, torque, frictionParams.m_chassis.m_angularVel);
        }

        // 5: hkpMotion::MOTION_FIXED
        if (body == NULL || body->getMotion()->m_type.m_storage == 5)
        {
            suspensionForcesOnGround[w].setZero();
            continue;
        }

        const hkpVehicleData::WheelComponentParams& wheel_params = data->m_wheelParams[w];
        hkVector4f ground_impulse;
        ground_impulse.setMul(hkSimdFloat32::fromFloat(0.0f - wheel_params.m_forceFeedbackMultiplier), impulse);
        const float max_impulse = wheel_params.m_maxContactBodyAcceleration * body->getMass() * deltaTime;
        if (max_impulse * max_impulse < ground_impulse.lengthSquared<3>().getReal())
        {
            ground_impulse.setMul(ground_impulse, ground_impulse.lengthInverse<3>());
            ground_impulse.setMul(ground_impulse, hkSimdFloat32::fromFloat(max_impulse));
        }

        const int axle = wheel_params.m_axle;
        if (body == groundBody[axle])
        {
            // The axle's own ground body takes it through the accumulator.
            const hkpMotion* ground_motion = body->getMotion();
            hkpVelocityAccumulator* ground = frictionParams.m_axleParams[axle].m_groundObject;
            ground->m_linearVel.addMul(ground_motion->m_inertiaAndMassInv.getComponent<3>(), ground_impulse);
            hkVector4f r;
            r.setSub(wheel.m_hardPointWs, ground_motion->getCenterOfMassInWorld());
            hkVector4f torque;
            torque.setCross(r, ground_impulse);
            vehicle_simulation_apply_angular(ground_motion, torque, ground->m_angularVel);
            suspensionForcesOnGround[w].setZero();
        }
        else
        {
            suspensionForcesOnGround[w] = ground_impulse;
        }
    }
}

void VEHICLE_SIMULATION_CLASS::applyDampingToAxleAccumulators(const hkStepInfo& stepInfo, hkpRigidBody** groundBodies, hkpVehicleFrictionSolverParams& frictionParams, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels)
{
    hkpRigidBody* ground0 = groundBodies[0];
    // 5: hkpMotion::MOTION_FIXED
    if (ground0 != NULL && ground0->getMotion()->m_type.m_storage != 5)
    {
        vehicle_simulation_damp_accumulator(stepInfo, *frictionParams.m_axleParams[0].m_groundObject, ground0->getMotion(), &originalLinearVels[0], &originalAngularVels[0]);
    }
    hkpRigidBody* ground1 = groundBodies[1];
    if (ground1 != NULL && ground1->getMotion()->m_type.m_storage != 5 && ground1 != groundBodies[0])
    {
        vehicle_simulation_damp_accumulator(stepInfo, *frictionParams.m_axleParams[1].m_groundObject, ground1->getMotion(), &originalLinearVels[1], &originalAngularVels[1]);
    }
}

void VEHICLE_SIMULATION_CLASS::applyDampingToChassisAccumulator(const hkStepInfo& stepInfo, hkpVelocityAccumulator& accumulator, const hkpMotion* motion)
{
    vehicle_simulation_damp_accumulator(stepInfo, accumulator, motion, NULL, NULL);
}

void VEHICLE_SIMULATION_CLASS::getExtraTorqueFactor(const hkpVehicleInstance& vehicleInstance, float deltaTime, hkpVelocityAccumulator& accumulatorForChassis) const
{
    const hkpVehicleData* data = vehicleInstance.m_data;
    if (data->m_extraTorqueFactor != 0.0f)
    {
        const hkSimdFloat32 amount = hkSimdFloat32::fromFloat(vehicleInstance.m_mainSteeringAngleAssumingNoReduction * data->m_extraTorqueFactor * deltaTime);
        hkVector4f torque;
        torque.setMul(amount, data->m_chassisOrientation.getColumn<0>());
        torque.mul(data->m_chassisFrictionInertiaInvDiag);
        accumulatorForChassis.m_angularVel.add(torque);
    }
}

void VEHICLE_SIMULATION_CLASS::applyFrictionSolver(const hkpVehicleInstance& vehicleInstance, const hkStepInfo& stepInfo, hkpVehicleFrictionSolverParams& frictionParams, hkpVehicleFrictionStatus& frictionStatus)
{
    hkpVehicleStepInfo step;
    step.m_deltaTime = stepInfo.m_deltaTime;
    step.m_invDeltaTime = stepInfo.m_invDeltaTime;
    hkVehicleFrictionApplyVehicleFriction(step, *m_frictionDescription, frictionParams, frictionStatus);
}

void VEHICLE_SIMULATION_CLASS::calcChassisVelocities(const hkpVehicleInstance& vehicleInstance, hkpVelocityAccumulator& accumulatorForChassis, hkpVehicleJobResults& vehicleResults)
{
    vehicleResults.m_chassisLinearVel = accumulatorForChassis.m_linearVel;
    vehicleResults.m_chassisAngularVel._setRotatedDir(vehicleInstance.getChassis()->getTransform().getRotation(), accumulatorForChassis.m_angularVel);
}

void VEHICLE_SIMULATION_CLASS::calcGroundBodyVelocities(const hkpVehicleInstance& vehicleInstance, float deltaTime, const hkpVehicleFrictionSolverAxleParams* axleParams, hkpRigidBody** groundBody, hkpVehicleJobResults& vehicleResults, hkVector4f* originalLinearVels, hkVector4f* originalAngularVels)
{
    for (int a = 0; a < vehicleInstance.m_data->m_numWheelsPerAxle.getSize(); a++)
    {
        vehicleResults.m_groundBodyPtr[a] = NULL;
        // 5: hkpMotion::MOTION_FIXED
        if (groundBody[a] == NULL || groundBody[a]->getMotion()->m_type.m_storage == 5 || (a != 0 && groundBody[0] == groundBody[1]))
        {
            continue;
        }

        // 10: the largest velocity change the vehicle gives a ground body per second
        const hkpVelocityAccumulator* accum = axleParams[a].m_groundObject;
        const float limit = deltaTime * 10.0f;
        hkVector4f max_change;
        max_change.set(limit, limit, limit, 0.0f);
        hkVector4f min_change;
        min_change.set(-limit, -limit, -limit, -0.0f);

        hkVector4f angular;
        angular._setRotatedInverseDir(vehicle_simulation_core_from_world(*accum), accum->m_angularVel);
        angular.sub(originalAngularVels[a]);
        angular.setMin(angular, max_change);
        angular.setMax(angular, min_change);
        hkVector4f linear;
        linear.setSub(accum->m_linearVel, originalLinearVels[a]);
        linear.setMin(linear, max_change);
        linear.setMax(linear, min_change);

        vehicleResults.m_groundBodyLinearVel[a].setAdd(linear, originalLinearVels[a]);
        vehicleResults.m_groundBodyAngularVel[a].setAdd(angular, originalAngularVels[a]);
        vehicleResults.m_groundBodyPtr[a] = groundBody[a];
    }
}

void VEHICLE_SIMULATION_CLASS::init(hkpVehicleInstance* instance)
{
    const hkpVehicleData* data = instance->m_data;
    const hkpMotion* motion = instance->m_entity->getMotion();
    const hkRotationf& rotation = motion->getTransform().getRotation();

    hkpVehicleFrictionDescription::Cinfo cinfo;
    cinfo.m_chassisCenterOfMass = motion->m_motionState.getSweptTransform().m_centerOfMassLocal;
    hkMatrix3f inertia_world;
    inertia_world.getColumn<0>().setMul(data->m_chassisFrictionInertiaInvDiag.getComponent<0>(), rotation.getColumn<0>());
    inertia_world.getColumn<1>().setMul(data->m_chassisFrictionInertiaInvDiag.getComponent<1>(), rotation.getColumn<1>());
    inertia_world.getColumn<2>().setMul(data->m_chassisFrictionInertiaInvDiag.getComponent<2>(), rotation.getColumn<2>());
    cinfo.m_chassisFrictionInertiaInv.setMulInverse(inertia_world, rotation);
    cinfo.m_chassisMassInv = motion->m_inertiaAndMassInv(3);
    cinfo.m_directionUp.setAbs(data->m_chassisOrientation.getColumn<0>());
    cinfo.m_directionFront.setAbs(data->m_chassisOrientation.getColumn<1>());
    cinfo.m_directionRight.setAbs(data->m_chassisOrientation.getColumn<2>());
    cinfo.m_frictionEqualizer = data->m_frictionEqualizer;
    cinfo.m_wheelAxleAngularInertia[0] = 0.0f;
    cinfo.m_wheelAxleAngularInertia[1] = 0.0f;
    for (int w = 0; w < data->m_numWheels; w++)
    {
        // The last wheel of an axle gives its radius and position.
        const hkpVehicleData::WheelComponentParams& wheel = data->m_wheelParams[w];
        const hkpVehicleSuspension::SuspensionWheelParameters& suspension = instance->m_suspension->m_wheelParams[w];
        const int axle = wheel.m_axle;
        cinfo.m_wheelRadius[axle] = wheel.m_radius;
        cinfo.m_wheelPosition[axle].setAddMul(suspension.m_hardpointChassisSpace, suspension.m_directionChassisSpace, hkSimdFloat32::fromFloat(suspension.m_length));
        cinfo.m_wheelAxleAngularInertia[axle] = wheel.m_mass * wheel.m_radius + cinfo.m_wheelAxleAngularInertia[axle];
    }

    hkpVehicleFrictionDescription* description = new hkpVehicleFrictionDescription();
    m_frictionDescription.setAndDontIncrementRefCount(description);
    hkVehicleFrictionDescriptionInitValues(cinfo, *description);
}

hkpVehicleSimulation* VEHICLE_SIMULATION_CLASS::clone(hkpVehicleInstance* instance)
{
    VEHICLE_SIMULATION_CLASS* simulation = new VEHICLE_SIMULATION_CLASS();
    simulation->m_frictionDescription = m_frictionDescription;
    return simulation;
}
