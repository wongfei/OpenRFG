// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): the two axle friction solver, from rfg.exe. The side impulses come from a 2x2 mass
// matrix, then are clipped against a friction ellipse (a 16 entry table per axle). The arithmetic keeps the order of
// operations of rfg.exe where the disassembly shows it.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.h"

#include <cmath>
#include <xmmintrin.h>

// 0x34000000: the smallest divisor (2^-23)
static const float FRICTION_MIN_DIVISOR = 1.1920929e-07f;
// __real@5f7ffff0: bounds a relative force before it is squared
static const float FRICTION_MAX_RELATIVE_FORCE = 1.8446726e+19f;
// The ellipse scale may overshoot by this much.
static const float FRICTION_MAX_ELLIPSE_SCALE = 1.1f;

// 0x00B631C0 (hkpvehiclefriction.h:120)
hkpVehicleFrictionDescription::hkpVehicleFrictionDescription()
{
}

// 0x00B631E0 (hkpvehiclefriction.h:136)
hkpVehicleFrictionStatus::AxisStatus::AxisStatus()
{
}

// 0x00B631F0 (hkpvehiclefriction.h:165)
void hkpVehicleFrictionStatus::AxisStatus::init()
{
    m_forward_slip_velocity = 0.0f;
    m_side_slip_velocity = 0.0f;
    m_skid_energy_density = 0.0f;
    m_side_force = 0.0f;
    m_delayed_forward_impulse = 0.0f;
    m_sideRhs = 0.0f;
    m_forwardRhs = 0.0f;
    m_relativeSideForce = 0.0f;
    m_relativeForwardForce = 0.0f;
}

// 0x00B63220 (hkpvehiclefriction.h:183)
hkpVehicleFrictionStatus::hkpVehicleFrictionStatus()
{
    m_axis[0].init();
    m_axis[1].init();
}

// 0x00B63730 (hkpvehiclefrictionsolver.h:76)
void hkpVehicleFrictionSolverAxleParams::initialize()
{
    m_contactPointWs.setZero();
    m_constraintNormalWs.setZero();
    m_forwardDirWs.setZero();
    m_frictionCoefficient = 0.0f;
    m_viscosityFrictionCoefficient = 0.0f;
    m_maxFrictionCoefficient = 0.0f;
    m_wheelDownForce = 0.0f;
    m_forwardForce = 0.0f;
    m_wheelFixed = 0;
    m_groundObject = NULL;
    m_groundObjectAtLastIntegration = NULL;
    m_slipVelocityFactor = 0.0f;
}

// 0x00B896C0
hkpVehicleFrictionSolverParams::hkpVehicleFrictionSolverParams()
{
}

// 0x00D71510
void hkVehicleFrictionDescriptionInitValues(const hkpVehicleFrictionDescription::Cinfo& ci, hkpVehicleFrictionDescription& out)
{
    out.m_chassisMassInv = ci.m_chassisMassInv;
    for (int a = 0; a < 2; a++)
    {
        hkpVehicleFrictionDescription::AxisDescription& axle = out.m_axleDescr[a];
        axle.m_wheelRadius = ci.m_wheelRadius[a];
        axle.m_wheelRadiusInv = 1.0f / ci.m_wheelRadius[a];
        axle.m_wheelSurfaceInertia = ci.m_wheelAxleAngularInertia[a] * axle.m_wheelRadiusInv;
        axle.m_wheelSurfaceInertiaInv = 1.0f / axle.m_wheelSurfaceInertia;
    }

    const hkVector4f& front = ci.m_directionFront;
    const float pos0 = front(1) * ci.m_wheelPosition[0](1) + front(0) * ci.m_wheelPosition[0](0) + front(2) * ci.m_wheelPosition[0](2);
    const float pos1 = ci.m_wheelPosition[1](1) * front(1) + ci.m_wheelPosition[1](0) * front(0) + ci.m_wheelPosition[1](2) * front(2);
    out.m_wheelDistance = fabsf(pos0 - pos1);
    const float distance_inv = 1.0f / out.m_wheelDistance;
    out.m_axleDescr[0].m_wheelDownForceFactor = 1.0f - ci.m_frictionEqualizer;
    out.m_axleDescr[0].m_wheelDownForceSumFactor = ci.m_frictionEqualizer * fabsf(distance_inv * pos1);
    out.m_axleDescr[1].m_wheelDownForceFactor = 1.0f - ci.m_frictionEqualizer;
    out.m_axleDescr[1].m_wheelDownForceSumFactor = fabsf(distance_inv * pos0) * ci.m_frictionEqualizer;

    // The chassis' inverse virtual mass along the right axis at each contact point.
    float virtual_mass_inv[2];
    const hkVector4f& up = ci.m_directionUp;
    const hkVector4f& right = ci.m_directionRight;
    const hkVector4f& com = ci.m_chassisCenterOfMass;
    const hkVector4f& c0 = ci.m_chassisFrictionInertiaInv.getColumn<0>();
    const hkVector4f& c1 = ci.m_chassisFrictionInertiaInv.getColumn<1>();
    const hkVector4f& c2 = ci.m_chassisFrictionInertiaInv.getColumn<2>();
    for (int a = 0; a < 2; a++)
    {
        const float height = -ci.m_wheelRadius[a];
        const float px = height * up(0) + (ci.m_wheelPosition[a](0) - com(0));
        const float py = height * up(1) + (ci.m_wheelPosition[a](1) - com(1));
        const float pz = height * up(2) + (ci.m_wheelPosition[a](2) - com(2));
        const float cz = right(1) * px - py * right(0);
        const float cx = right(2) * py - pz * right(1);
        const float cy = right(0) * pz - px * right(2);
        virtual_mass_inv[a] = (cx * c0(1) + cy * c1(1) + cz * c2(1)) * cy
            + (cx * c0(0) + cy * c1(0) + cz * c2(0)) * cx
            + (cx * c0(2) + cy * c1(2) + cz * c2(2)) * cz
            + out.m_chassisMassInv;
    }

    for (int a = 0; a < 2; a++)
    {
        hkpVehicleFrictionDescription::AxisDescription& axle = out.m_axleDescr[a];
        const float ratio = virtual_mass_inv[a] * axle.m_wheelSurfaceInertia;
        axle.m_wheelChassisMassRatio = ratio;
        // 0.0625: 1/16, one table step
        float x = ratio * 0.0625f;
        axle.m_xStart = x;
        // 0x3FB1111120000000 as a double: 1/15 rounded to a float, one table step of 15
        axle.m_xStep = (float)pow((double)(1.0f / x), 0.06666667014360428);
        const float exponent = 1.0f / ratio;
        for (int i = 0; i < 15; i++)
        {
            axle.m_frictionCircleYtab[i] = 1.0f - powf(1.0f - x, exponent);
            x = axle.m_xStep * x;
        }
        axle.m_frictionCircleYtab[15] = 1.0f;
    }
}

// 0x00D71190
static void hkVehicleFrictionSolverCalcRelativeForces(const hkpVehicleFrictionAxisTmp& axis, hkpVehicleFrictionStatus::AxisStatus& status)
{
    status.m_relativeSideForce = axis.m_invMaxSideImpulse * axis.m_sideImpulse;
    status.m_relativeForwardForce = axis.m_invMaxForwardImpulse * axis.m_forwardImpulse;
}

// 0x00D711C0: walks the ellipse table until the point (xs, ys) is outside, then interpolates to the unit distance
static void hkCalculateFrictionalEllipsoidScale(const hkpVehicleFrictionDescription::AxisDescription& adesc, float xs, float ys, float* scaleXOut, float* scaleYOut)
{
    float x = adesc.m_xStart * xs;
    float y = adesc.m_frictionCircleYtab[0] * ys;
    float dist = y * y + x * x;
    float prev_x = 0.0f;
    float prev_y = 0.0f;
    float prev_dist = 0.0f;
    int i = 0;
    while (dist <= 1.0f && ++i < 16)
    {
        prev_x = x;
        prev_y = y;
        prev_dist = dist;
        y = adesc.m_frictionCircleYtab[i] * ys;
        x = x * adesc.m_xStep;
        dist = y * y + x * x;
    }

    const float delta = (sqrtf(dist) - 1.0f) - (sqrtf(prev_dist) - 1.0f);
    const float divisor = FRICTION_MIN_DIVISOR < fabsf(delta - 0.0f) ? delta : FRICTION_MIN_DIVISOR;
    const float weight = -((1.0f / divisor) * (sqrtf(prev_dist) - 1.0f));
    const float prev_weight = (1.0f / divisor) * (sqrtf(dist) - 1.0f);

    float scale_x = weight * x + prev_weight * prev_x;
    if (scale_x <= -FRICTION_MAX_ELLIPSE_SCALE)
    {
        scale_x = -FRICTION_MAX_ELLIPSE_SCALE;
    }
    if (FRICTION_MAX_ELLIPSE_SCALE <= scale_x)
    {
        scale_x = FRICTION_MAX_ELLIPSE_SCALE;
    }
    *scaleXOut = scale_x;

    float scale_y = y * weight + prev_y * prev_weight;
    if (scale_y <= -FRICTION_MAX_ELLIPSE_SCALE)
    {
        scale_y = -FRICTION_MAX_ELLIPSE_SCALE;
    }
    if (FRICTION_MAX_ELLIPSE_SCALE <= scale_y)
    {
        scale_y = FRICTION_MAX_ELLIPSE_SCALE;
    }
    *scaleYOut = scale_y;
}

static float friction_clamp_relative(float value)
{
    if (value <= -FRICTION_MAX_RELATIVE_FORCE)
    {
        return -FRICTION_MAX_RELATIVE_FORCE;
    }
    return FRICTION_MAX_RELATIVE_FORCE <= value ? FRICTION_MAX_RELATIVE_FORCE : value;
}

// 0x00D71350: apar is not read
static void hkVehicleFrictionSolverClipImpulse(const hkpVehicleFrictionSolverAxleParams* apar, hkpVehicleFrictionAxisTmp& atmp, const hkpVehicleFrictionDescription::AxisDescription& adesc, hkpVehicleFrictionStatus::AxisStatus& astatus)
{
    const float relative_side = friction_clamp_relative(atmp.m_invMaxSideImpulse * atmp.m_sideImpulse);
    const float relative_forward = friction_clamp_relative(atmp.m_invMaxForwardImpulse * atmp.m_forwardImpulse);
    astatus.m_relativeForwardForce = relative_forward;
    astatus.m_relativeSideForce = relative_side;
    if (relative_side * relative_side + relative_forward * relative_forward < 1.0f)
    {
        atmp.m_restForwardSlipImpulse = 0.0f;
        return;
    }

    float scale_x;
    float scale_y;
    hkCalculateFrictionalEllipsoidScale(adesc, relative_side, relative_forward, &scale_x, &scale_y);

    const float old_side = atmp.m_sideImpulse;
    atmp.m_sideImpulse = atmp.m_maxSideImpulse * scale_x;
    const float old_forward = atmp.m_forwardImpulse;
    atmp.m_forwardImpulse = atmp.m_maxForwardImpulse * scale_y;
    atmp.m_restForwardSlipImpulse = old_forward - atmp.m_forwardImpulse;

    float speed_sqrd = atmp.m_forward_vel * atmp.m_forward_vel + atmp.m_sideSlipVel * atmp.m_sideSlipVel;
    if (fabsf(speed_sqrd - 0.0f) <= FRICTION_MIN_DIVISOR)
    {
        speed_sqrd = FRICTION_MIN_DIVISOR;
    }
    const float forward = atmp.m_invWheelForwardVirtMass * atmp.m_restForwardSlipImpulse * atmp.m_maxForwardImpulse;
    const float side = atmp.m_invSideVirtMass * (old_side - atmp.m_sideImpulse) * atmp.m_maxSideImpulse;
    astatus.m_skid_energy_density = sqrtf((1.0f / speed_sqrd) * (side * side + forward * forward));
}

// A jacobian row between the chassis and a ground body. The virtual mass goes into m_angular[0].w, its inverse into
// m_angular[1].w. The two cross products have opposite orders, because the ground takes the impulse negated.
static void friction_build_jacobian(hkp1Lin2AngJacobian& jac, const hkVector4f& dir, const hkVector4f& r_chassis, const hkVector4f& r_ground, const hkpVelocityAccumulator& chassis, const hkpVelocityAccumulator& ground)
{
    jac.m_linear0 = dir;
    hkVector4f cross;
    cross.setCross(r_chassis, dir);
    jac.m_angular[0].setMul(chassis.m_scratch0, cross.getComponent<0>());
    jac.m_angular[0].addMul(chassis.m_scratch1, cross.getComponent<1>());
    jac.m_angular[0].addMul(chassis.m_scratch2, cross.getComponent<2>());
    cross.setCross(dir, r_ground);
    jac.m_angular[1].setMul(ground.m_scratch0, cross.getComponent<0>());
    jac.m_angular[1].addMul(ground.m_scratch1, cross.getComponent<1>());
    jac.m_angular[1].addMul(ground.m_scratch2, cross.getComponent<2>());

    const hkVector4f& a0 = jac.m_angular[0];
    const hkVector4f& a1 = jac.m_angular[1];
    const float x = a0(0) * a0(0) * chassis.m_invMasses(0) + a1(0) * a1(0) * ground.m_invMasses(0);
    const float y = a0(1) * a0(1) * chassis.m_invMasses(1) + a1(1) * a1(1) * ground.m_invMasses(1);
    const float z = a0(2) * a0(2) * chassis.m_invMasses(2) + a1(2) * a1(2) * ground.m_invMasses(2);
    // HK_QUADREAL_EPS_SQRD: never a zero inverse mass
    const float mass_inv = chassis.m_invMasses(3) + ground.m_invMasses(3);
    const float eps = hkSimdFloat32::getConstant<HK_QUADREAL_EPS_SQRD>().getReal();
    const float w = eps < mass_inv ? mass_inv : eps;
    const float sum = (x + z) + (y + w);
    hkSimdFloat32 inv;
    inv.setReciprocal(hkSimdFloat32::fromFloat(sum));
    jac.m_angular[1](3) = sum;
    jac.m_angular[0](3) = inv.getReal();
}

// The velocity along a jacobian row.
static float friction_row_velocity(const hkp1Lin2AngJacobian& jac, const hkVector4f& linear, const hkVector4f& chassis_angular, const hkVector4f& ground_angular)
{
    return jac.m_linear0(1) * linear(1) + jac.m_angular[0](1) * chassis_angular(1) + jac.m_angular[1](1) * ground_angular(1)
        + jac.m_linear0(0) * linear(0) + jac.m_angular[0](0) * chassis_angular(0) + jac.m_angular[1](0) * ground_angular(0)
        + jac.m_linear0(2) * linear(2) + jac.m_angular[0](2) * chassis_angular(2) + jac.m_angular[1](2) * ground_angular(2);
}

static void friction_apply_impulse(const hkp1Lin2AngJacobian& jac, float impulse, hkpVelocityAccumulator& chassis, hkpVelocityAccumulator& ground)
{
    const hkSimdFloat32 simd_impulse = hkSimdFloat32::fromFloat(impulse);
    chassis.m_linearVel.addMul(chassis.m_invMasses.getComponent<3>() * simd_impulse, jac.m_linear0);
    ground.m_linearVel.subMul(ground.m_invMasses.getComponent<3>() * simd_impulse, jac.m_linear0);
    hkVector4f angular;
    angular.setMul(jac.m_angular[0], chassis.m_invMasses);
    chassis.m_angularVel.addMul(angular, simd_impulse);
    angular.setMul(jac.m_angular[1], ground.m_invMasses);
    ground.m_angularVel.addMul(angular, simd_impulse);
}

// 0x00D718C0
void hkVehicleFrictionApplyVehicleFriction(const hkpVehicleStepInfo& stepInfo, const hkpVehicleFrictionDescription& descr, hkpVehicleFrictionSolverParams& params, hkpVehicleFrictionStatus& status)
{
    hkpVehicleFrictionAxisTmp axis[2];
    hkpVelocityAccumulator& chassis = params.m_chassis;

    for (int a = 0; a < 2; a++)
    {
        const hkpVehicleFrictionSolverAxleParams& axle = params.m_axleParams[a];
        hkpVehicleFrictionAxisTmp& tmp = axis[a];
        hkpVelocityAccumulator& ground = *axle.m_groundObject;
        hkVector4f r_chassis;
        r_chassis.setSub(axle.m_contactPointWs, chassis.m_scratch3);
        hkVector4f r_ground;
        r_ground.setSub(axle.m_contactPointWs, ground.m_scratch3);

        friction_build_jacobian(tmp.m_forwardJac, axle.m_forwardDirWs, r_chassis, r_ground, chassis, ground);
        tmp.m_invWheelForwardVirtMass = tmp.m_forwardJac.m_angular[1](3) + descr.m_axleDescr[a].m_wheelSurfaceInertiaInv;
        tmp.m_wheelForwardVirtMass = 1.0f / tmp.m_invWheelForwardVirtMass;

        friction_build_jacobian(tmp.m_sideJac, axle.m_constraintNormalWs, r_chassis, r_ground, chassis, ground);
        tmp.m_sideVirtMass = tmp.m_sideJac.m_angular[0](3);
        tmp.m_invSideVirtMass = tmp.m_sideJac.m_angular[1](3);

        // The drift since the last integration feeds the positional friction.
        const hkpVelocityAccumulator& ground_last = *axle.m_groundObjectAtLastIntegration;
        hkVector4f linear;
        linear.setSub(params.m_chassisAtLastIntegration.m_linearVel, ground_last.m_linearVel);
        status.m_axis[a].m_sideRhs = friction_row_velocity(tmp.m_sideJac, linear, params.m_chassisAtLastIntegration.m_angularVel, ground_last.m_angularVel) + status.m_axis[a].m_sideRhs;
        status.m_axis[a].m_forwardRhs = friction_row_velocity(tmp.m_forwardJac, linear, params.m_chassisAtLastIntegration.m_angularVel, ground_last.m_angularVel) + status.m_axis[a].m_forwardRhs;
    }

    // The side mass matrix: k00 and k11 are the diagonal, k01 couples the axles.
    float k00 = axis[0].m_invSideVirtMass;
    float k11 = axis[1].m_invSideVirtMass;
    float k01;
    {
        const hkp1Lin2AngJacobian& j0 = axis[0].m_sideJac;
        const hkp1Lin2AngJacobian& j1 = axis[1].m_sideJac;
        const hkVector4f& inv = chassis.m_invMasses;
        k01 = inv(3) * j1.m_linear0(1) * j0.m_linear0(1) + j1.m_angular[0](1) * j0.m_angular[0](1) * inv(1)
            + inv(3) * j1.m_linear0(0) * j0.m_linear0(0) + j1.m_angular[0](0) * j0.m_angular[0](0) * inv(0)
            + inv(3) * j1.m_linear0(2) * j0.m_linear0(2) + j1.m_angular[0](2) * j0.m_angular[0](2) * inv(2);
        if (params.m_axleParams[0].m_groundObject == params.m_axleParams[1].m_groundObject)
        {
            const hkVector4f& g = params.m_axleParams[0].m_groundObject->m_invMasses;
            k01 += g(3) * j1.m_linear0(0) * j0.m_linear0(0) + j1.m_angular[1](0) * j0.m_angular[1](0) * g(0)
                + g(3) * j1.m_linear0(1) * j0.m_linear0(1) + j1.m_angular[1](1) * j0.m_angular[1](1) * g(1)
                + g(3) * j1.m_linear0(2) * j0.m_linear0(2) + j1.m_angular[1](2) * j0.m_angular[1](2) * g(2);
        }
    }

    // The forward impulse left over from the last step.
    for (int a = 0; a < 2; a++)
    {
        const float impulse = status.m_axis[a].m_delayed_forward_impulse;
        if (impulse != 0.0f)
        {
            friction_apply_impulse(axis[a].m_forwardJac, impulse, chassis, *params.m_axleParams[a].m_groundObject);
        }
    }

    const float down_force0 = params.m_axleParams[0].m_wheelDownForce;
    const float down_force1 = params.m_axleParams[1].m_wheelDownForce;
    const float down_force_sum = down_force1 + down_force0;
    axis[0].m_wheel_down_force = descr.m_axleDescr[0].m_wheelDownForceSumFactor * down_force_sum + descr.m_axleDescr[0].m_wheelDownForceFactor * down_force0;
    axis[1].m_wheel_down_force = descr.m_axleDescr[1].m_wheelDownForceSumFactor * down_force_sum + descr.m_axleDescr[1].m_wheelDownForceFactor * down_force1;
    status.m_axis[0].m_skid_energy_density = 0.0f;
    status.m_axis[1].m_skid_energy_density = 0.0f;

    for (int a = 0; a < 2; a++)
    {
        const hkpVehicleFrictionSolverAxleParams& axle = params.m_axleParams[a];
        hkpVehicleFrictionAxisTmp& tmp = axis[a];
        hkpVehicleFrictionStatus::AxisStatus& axis_status = status.m_axis[a];
        const hkpVelocityAccumulator& ground = *axle.m_groundObject;
        hkVector4f linear;
        linear.setSub(chassis.m_linearVel, ground.m_linearVel);

        tmp.m_forward_vel = friction_row_velocity(tmp.m_forwardJac, linear, chassis.m_angularVel, ground.m_angularVel);
        // 0.25: a quarter of the positional drift each step
        tmp.m_sideSlipVel = axis_status.m_sideRhs * 0.25f + friction_row_velocity(tmp.m_sideJac, linear, chassis.m_angularVel, ground.m_angularVel);

        const float down_impulse = fabsf(tmp.m_wheel_down_force) * stepInfo.m_deltaTime;
        float forward_force = axle.m_forwardForce;
        float forward_slip;
        if (axle.m_wheelFixed == 0)
        {
            axis_status.m_forwardRhs = 0.0f;
            forward_slip = axis_status.m_forward_slip_velocity;
            tmp.m_forwardSlipVel = forward_slip;
        }
        else
        {
            // A blocked wheel: the forward velocity and drift become slip, bounded by the static friction.
            forward_slip = -tmp.m_forward_vel;
            const float slip = (axis_status.m_forwardRhs * 0.5f + tmp.m_forward_vel) * tmp.m_sideVirtMass * tmp.m_invWheelForwardVirtMass * 0.5f;
            const float limit = axle.m_frictionCoefficient * down_impulse * tmp.m_invWheelForwardVirtMass;
            float clamped = slip;
            if (limit < fabsf(slip))
            {
                clamped = slip <= 0.0f ? -limit : limit;
            }
            tmp.m_forwardSlipVel = forward_slip - clamped;
            forward_force = 0.0f;
        }

        float friction = axle.m_frictionCoefficient;
        if (axle.m_viscosityFrictionCoefficient != 0.0f)
        {
            float viscous = sqrtf(tmp.m_sideSlipVel * tmp.m_sideSlipVel + forward_slip * forward_slip) * axle.m_viscosityFrictionCoefficient + axle.m_frictionCoefficient;
            if (axle.m_maxFrictionCoefficient <= viscous)
            {
                viscous = axle.m_maxFrictionCoefficient;
            }
            friction = 0.0f <= viscous ? viscous : 0.0f;
        }
        const float max_impulse = friction * down_impulse;
        tmp.m_maxForwardImpulse = max_impulse;
        tmp.m_maxSideImpulse = max_impulse;
        // 1.4210855e-14: 2^-46, no division by zero with no down force
        const float max_impulse_inv = 1.0f / (max_impulse + 1.4210855e-14f);
        tmp.m_invMaxForwardImpulse = max_impulse_inv;
        tmp.m_invMaxSideImpulse = max_impulse_inv;
        tmp.m_forwardImpulse = tmp.m_wheelForwardVirtMass * tmp.m_forwardSlipVel + stepInfo.m_deltaTime * forward_force;
    }

    // Heavy ground contact relative to the chassis: stiffens the side response.
    if (0.0f < params.m_axleParams[1].m_slipVelocityFactor + params.m_axleParams[0].m_slipVelocityFactor)
    {
        const float reference = stepInfo.m_deltaTime * (fabsf(down_force1) + fabsf(down_force0)) * descr.m_chassisMassInv;
        float scale[2];
        for (int a = 0; a < 2; a++)
        {
            // 10000: the largest scale
            if (reference == 0.0f)
            {
                scale[a] = 10000.0f;
            }
            else
            {
                const float factor = params.m_axleParams[a].m_slipVelocityFactor <= 0.0f ? 0.0f : params.m_axleParams[a].m_slipVelocityFactor;
                const float value = reference < factor ? factor / reference : 1.0f;
                scale[a] = value <= 10000.0f ? value : 10000.0f;
            }
        }
        k00 = scale[0] * k00;
        k11 = scale[1] * k11;
        k01 = sqrtf(scale[1] * scale[0]) * k01;
        axis[0].m_sideVirtMass = axis[0].m_sideVirtMass / scale[0];
        axis[0].m_invSideVirtMass = scale[0] * axis[0].m_invSideVirtMass;
        axis[1].m_sideVirtMass = axis[1].m_sideVirtMass / scale[1];
        axis[1].m_invSideVirtMass = scale[1] * axis[1].m_invSideVirtMass;
    }

    // The inverse of the 2x2 matrix; the identity (0x01352AA0) if it is singular.
    float inv00 = 1.0f;
    float inv01 = 0.0f;
    float inv10 = 0.0f;
    float inv11 = 1.0f;
    const float det = k11 * k00 - k01 * k01;
    if (0.0f < fabsf(det))
    {
        hkSimdFloat32 det_inv;
        det_inv.setReciprocal(hkSimdFloat32::fromFloat(det));
        const float d = det_inv.getReal();
        inv00 = k11 * d;
        inv01 = -k01 * d;
        inv10 = -k01 * d;
        inv11 = k00 * d;
    }
    axis[1].m_sideImpulse = -(inv11 * axis[1].m_sideSlipVel + inv10 * axis[0].m_sideSlipVel);
    axis[0].m_sideImpulse = -(axis[1].m_sideSlipVel * inv01 + axis[0].m_sideSlipVel * inv00);

    const float relative1 = axis[1].m_sideImpulse * axis[1].m_invMaxSideImpulse;
    const float relative0 = axis[0].m_invMaxSideImpulse * axis[0].m_sideImpulse;
    const float force1 = relative1 * relative1 + axis[1].m_invMaxForwardImpulse * axis[1].m_forwardImpulse * axis[1].m_invMaxForwardImpulse * axis[1].m_forwardImpulse;
    const float force0 = relative0 * relative0 + axis[0].m_invMaxForwardImpulse * axis[0].m_forwardImpulse * axis[0].m_invMaxForwardImpulse * axis[0].m_forwardImpulse;

    // No positional friction at speed.
    if (params.m_maxVelocityForPositionalFriction <= fabsf(axis[0].m_forward_vel))
    {
        status.m_axis[0].m_sideRhs = 0.0f;
        status.m_axis[0].m_forwardRhs = 0.0f;
        status.m_axis[1].m_sideRhs = 0.0f;
        status.m_axis[1].m_forwardRhs = 0.0f;
    }

    if (force0 <= 1.0f && force1 <= 1.0f)
    {
        // Both axles grip. Original quirk kept: the coupling always goes into axis 1's side slip, also from axis 1
        // itself, where it has no further reader.
        for (int a = 0; a < 2; a++)
        {
            hkpVehicleFrictionAxisTmp& tmp = axis[a];
            const float relative_side = tmp.m_invMaxSideImpulse * tmp.m_sideImpulse;
            const float relative_forward = tmp.m_forwardImpulse * tmp.m_invMaxForwardImpulse;
            if (relative_forward * relative_forward + relative_side * relative_side < 1.0f)
            {
                tmp.m_sideImpulse = -(tmp.m_sideSlipVel * tmp.m_sideVirtMass);
            }
            else
            {
                float scale_x;
                float scale_y;
                hkCalculateFrictionalEllipsoidScale(descr.m_axleDescr[a], relative_side, relative_forward, &scale_x, &scale_y);
                const float limit = fabsf(tmp.m_maxSideImpulse * scale_x);
                float impulse = tmp.m_sideImpulse;
                if (impulse <= -limit)
                {
                    impulse = -limit;
                }
                if (limit <= impulse)
                {
                    impulse = limit;
                }
                tmp.m_sideImpulse = impulse;
            }
            axis[1].m_sideSlipVel = tmp.m_sideImpulse * k01 + axis[1].m_sideSlipVel;
        }
        hkVehicleFrictionSolverCalcRelativeForces(axis[0], status.m_axis[0]);
        hkVehicleFrictionSolverCalcRelativeForces(axis[1], status.m_axis[1]);
        status.m_axis[0].m_forward_slip_velocity = 0.0f;
        status.m_axis[1].m_forward_slip_velocity = 0.0f;
        status.m_axis[0].m_side_slip_velocity = 0.0f;
        status.m_axis[1].m_side_slip_velocity = 0.0f;
    }
    else
    {
        if (1.0f < force0)
        {
            const float scale = 1.0f / force0;
            status.m_axis[0].m_sideRhs = scale * status.m_axis[0].m_sideRhs;
            status.m_axis[0].m_forwardRhs = status.m_axis[0].m_forwardRhs * scale;
        }
        if (1.0f < force1)
        {
            const float scale = 1.0f / force1;
            status.m_axis[1].m_sideRhs = scale * status.m_axis[1].m_sideRhs;
            status.m_axis[1].m_forwardRhs = status.m_axis[1].m_forwardRhs * scale;
        }

        // The axle further over the limit goes first. Original quirk kept: both passes add the coupling to the side slip
        // of the second axle, so the second axle's own impulse is added to it after it was used.
        const int first = force0 > force1 ? 0 : 1;
        const int second = 1 - first;
        const int order[2] = { first, second };
        for (int i = 0; i < 2; i++)
        {
            const int a = order[i];
            axis[a].m_sideImpulse = -(axis[a].m_sideVirtMass * axis[a].m_sideSlipVel);
            hkVehicleFrictionSolverClipImpulse(&params.m_axleParams[a], axis[a], descr.m_axleDescr[a], status.m_axis[a]);
            axis[second].m_sideSlipVel = axis[a].m_sideImpulse * k01 + axis[second].m_sideSlipVel;
        }
        status.m_axis[0].m_forward_slip_velocity = axis[0].m_invWheelForwardVirtMass * axis[0].m_restForwardSlipImpulse;
        status.m_axis[1].m_forward_slip_velocity = axis[1].m_invWheelForwardVirtMass * axis[1].m_restForwardSlipImpulse;
        status.m_axis[0].m_side_slip_velocity = axis[0].m_sideSlipVel;
        status.m_axis[1].m_side_slip_velocity = axis[1].m_sideSlipVel;
    }

    status.m_axis[0].m_delayed_forward_impulse = axis[0].m_forwardImpulse;
    status.m_axis[1].m_delayed_forward_impulse = axis[1].m_forwardImpulse;
    status.m_axis[0].m_side_force = axis[0].m_sideImpulse * stepInfo.m_invDeltaTime;
    status.m_axis[1].m_side_force = axis[1].m_sideImpulse * stepInfo.m_invDeltaTime;

    for (int a = 0; a < 2; a++)
    {
        friction_apply_impulse(axis[a].m_sideJac, axis[a].m_sideImpulse, chassis, *params.m_axleParams[a].m_groundObject);
    }
}

// 0x00B631D0
hkpVehicleFrictionDescription::~hkpVehicleFrictionDescription()
{
}
