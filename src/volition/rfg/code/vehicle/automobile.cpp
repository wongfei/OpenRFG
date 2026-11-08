// Original: volition/rfg/code/vehicle/automobile.cpp
//
// Automobile: builds the Havok vehicle kit instance from the vehicle table, steers it, and each frame applies what the
// kit does not (the speed cap, force assist, powersliding, landing friction, wheel and swing arm placement, wheel effects
// and tire tracks).

#include "volition/rfg/code/vehicle/automobile.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/default/hkpvehicledefaultaerodynamics.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/tyremarks/hkptyremarksinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_collide.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_engine.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_suspension.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"

#include <math.h>
#include <stdio.h>

extern bool R_enable_decals; // 0x0165CC74

// ---- file-static globals
static bool Vehicle_wheel_stutter = true; // 0x016BA2C9
static bool Powerslide_straight = true; // 0x016BA2CA
float VEHICLE_MAX_SPEED = 40.2336f; // 0x016BA434, 90 mph in m/s
static float FREE_CAMERA_SWING_RATE_VEHICLE_FORWARD = 0.9f; // 0x016BA464
static float FREE_CAMERA_SWING_RATE_VEHICLE_REVERSE = 0.9f; // 0x016BA468
static float Vehicle_wheel_stutter_minspeed = 10.0f; // 0x016BA46C
static float Vehicle_wheel_stutter_offset = 0.3927f; // 0x016BA470, radians
static float Vehicle_force_assist_max_speed = 1.0f; // 0x016BA474
static float Vehicle_force_assist_ramp = 2.0f; // 0x016BA478
static float Powerslide_min_sideways_velocity = 3.5f; // 0x016BA47C
static float Powerslide_falloff_rate = 1.2f; // 0x016BA480
static float Powerslide_handbrake_axle_friction_amount = 1.0f; // 0x016BA484
static bool Vehicle_continuous_physics = true; // 0x016BA488
// Original bug kept: no tweak item and no write in rfg.exe, so a powerslide never changes the friction of the other axles.
static float Powerslide_other_axle_friction_amount; // 0x03770028

// from the tweak table
static float Automobile_min_side_slip_velocity; // 0x0376F620
static float Automobile_motion_foley_full_volume_wheel_velocity; // 0x0376F624
static int Automobile_landing_friction_full_damp_time; // 0x0376F630
static float Automobile_tire_track_min_direction_dot_prod; // 0x0376F63C
static float Automobile_motion_effect_min_vehicle_velocity; // 0x0376F640
static float Automobile_wheel_projectile_detach_speed; // 0x0376F644
static float Automobile_light_brake_max_slip_velocity; // 0x0376F648
static float Automobile_min_forward_slip_velocity; // 0x0376F64C
static float Automobile_motion_effect_min_wheel_velocity; // 0x0376F67C
static float Automobile_brake_max_volume_velocity; // 0x0376F7B4
static int Automobile_landing_friction_min_airborne_time; // 0x0376F9F4
static int Automobile_landing_friction_ramp_damp_time; // 0x0376FFFC
static float Automobile_landing_friction_full_damp_ratio; // 0x03770014
static float Automobile_tire_track_reuse_track_dist_factor; // 0x03770018, read by nothing
static tweak_table_item Tweak_table_item_Automobile_motion_effect_min_wheel_velocity("Automobile_motion_effect_min_wheel_velocity", &Automobile_motion_effect_min_wheel_velocity);
static tweak_table_item Tweak_table_item_Automobile_motion_effect_min_vehicle_velocity("Automobile_motion_effect_min_vehicle_velocity", &Automobile_motion_effect_min_vehicle_velocity);
static tweak_table_item Tweak_table_item_Automobile_motion_foley_full_volume_wheel_velocity("Automobile_motion_foley_full_volume_wheel_velocity", &Automobile_motion_foley_full_volume_wheel_velocity);
static tweak_table_item Tweak_table_item_Automobile_light_brake_max_slip_velocity("Automobile_light_brake_max_slip_velocity", &Automobile_light_brake_max_slip_velocity);
static tweak_table_item Tweak_table_item_Automobile_brake_max_volume_velocity("Automobile_brake_max_volume_velocity", &Automobile_brake_max_volume_velocity);
static tweak_table_item Tweak_table_item_Automobile_min_side_slip_velocity("Automobile_min_side_slip_velocity", &Automobile_min_side_slip_velocity);
static tweak_table_item Tweak_table_item_Automobile_min_forward_slip_velocity("Automobile_min_forward_slip_velocity", &Automobile_min_forward_slip_velocity);
static tweak_table_item Tweak_table_item_Automobile_tire_track_min_direction_dot_prod("Automobile_tire_track_min_direction_dot_prod", &Automobile_tire_track_min_direction_dot_prod);
static tweak_table_item Tweak_table_item_Automobile_tire_track_reuse_track_dist_factor("Automobile_tire_track_reuse_track_dist_factor", &Automobile_tire_track_reuse_track_dist_factor);
static tweak_table_item Tweak_table_item_Automobile_wheel_projectile_detach_speed("Automobile_wheel_projectile_detach_speed", &Automobile_wheel_projectile_detach_speed);
static tweak_table_item Tweak_table_item_Automobile_landing_friction_full_damp_ratio("Automobile_landing_friction_full_damp_ratio", &Automobile_landing_friction_full_damp_ratio);
static tweak_table_item Tweak_table_item_Automobile_landing_friction_min_airborne_time("Automobile_landing_friction_min_airborne_time", &Automobile_landing_friction_min_airborne_time);
static tweak_table_item Tweak_table_item_Automobile_landing_friction_full_damp_time("Automobile_landing_friction_full_damp_time", &Automobile_landing_friction_full_damp_time);
static tweak_table_item Tweak_table_item_Automobile_landing_friction_ramp_damp_time("Automobile_landing_friction_ramp_damp_time", &Automobile_landing_friction_ramp_damp_time);

static object_pool<automobile,32,1> automobilepool; // 0x0377CC40
static obj_class_instantiator automobile_instantiator("automobile", alloc_automobile, types_automobile, bpo_index_automobile, classname_automobile, automobile::pool_remaining, 0x9D8E0, 32); // 0x0381A538

static const unsigned int NO_HANDLE = 0xFFFFFFFF;
static const float GRAVITY_MS2 = 9.8f;
static const float MPH_TO_MS = 0.44704f;
static const float KMPH_PER_MS = 3.6f;
static const float ONE_OVER_255 = 0.003921569f;
static const int WHEEL_EFFECTS_STOP_DELAY_MS = 5000;
static const int CATCH_FIRE_DELAY_MS = 5000;
static const int WHEEL_FOLEY_FADE_MS = 500;
static const int WHEEL_FOLEY_FADE_REVERSING_MS = 650;
static const int MAX_DECAL_COLLECTIONS = 50; // of a single tire track update
static const float TIRE_TRACK_MAX_DIST_SQ = 1000.0f; // squared metres: never closer than this
static const float TIRE_TRACK_MAX_UNSEEN_DIST_SQ = 500.0f; // squared metres: off screen tracks are closer than this

// The automobile, when a live handle names one.
static automobile* automobile_from_handle(unsigned int a_handle)
{
    if (a_handle == NO_HANDLE) {
        return NULL;
    }
    object* obj = World.object_from_handle(a_handle);
    if (obj == NULL || obj->get_obj_type() != OT_VEHICLE || obj->get_sub_type() != OT_SUB_VEHICLE_AUTO) {
        return NULL;
    }
    return static_cast<automobile*>(obj);
}

// The instance of a live, loaded automobile.
static hk_custom_vehicle_instance* automobile_instance_from_handle(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return NULL;
    }
    return ap->a_rdd.get_data()->m_hk_vehicle_instance;
}

// The most recent driver while still driving.
static human* automobile_current_driver(const automobile* ap)
{
    if (ap->most_recent_driver_exit_time.valid() || ap->most_recent_driver == NO_HANDLE) {
        return NULL;
    }
    object* obj = World.object_from_handle(ap->most_recent_driver);
    if (obj == NULL || obj->get_obj_type() != OT_HUMAN) {
        return NULL;
    }
    return static_cast<human*>(obj);
}

// 0x00B63580 (:89)
void unused_dcf_vehicle_max_speed()
{
    if (Console_command != 0 && console_get_arg(0x10, false)) {
        VEHICLE_MAX_SPEED = Console_arg_float * MPH_TO_MS;
    }
}

// 0x00B635C0 (:261): empty in rfg.exe
static void automobile_attach_trailer_havok_level(automobile* p_tractor)
{
}

// 0x00B71AD0 (:333): looks the tractor up and does nothing with it
static void automobile_attach_to_tractor_havok_level(automobile* p_trailer)
{
    if (p_trailer->m_h_tractor_vehicle != NO_HANDLE) {
        World.object_from_handle(p_trailer->m_h_tractor_vehicle);
    }
}

// 0x00B9B3E0 (:394)
static void automobile_create_havok_vehicle_data(automobile* ap)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    const vehicle_subinfo* sub_info = ap->rdd.get_data()->sub_info;

    hkpVehicleData* data = new hkpVehicleData();
    a_data->m_hk_vehicle_data = data;
    data->m_gravity.set(0.0f, -GRAVITY_MS2, 0.0f, 0.0f);
    data->m_numWheels = (signed char)a_data->m_num_wheels;
    // Up is y, forward z, right -x.
    data->m_chassisOrientation.getColumn(0).set(0.0f, 1.0f, 0.0f, 0.0f);
    data->m_chassisOrientation.getColumn(1).set(0.0f, 0.0f, 1.0f, 0.0f);
    data->m_chassisOrientation.getColumn(2).set(-1.0f, 0.0f, 0.0f, 0.0f);
    data->m_torqueRollFactor = info->m_torque_roll_factor;
    data->m_torquePitchFactor = info->m_torque_pitch_factor;
    data->m_torqueYawFactor = info->m_torque_yaw_factor;
    data->m_extraTorqueFactor = (info->m_extra_torque_factor * info->chassis_mass) / info->max_steering_angle;
    // The diagonal of the collision model's inertia tensor, scaled by the table.
    const matrix& inertia = sub_info->m_cm_dynamics_info.m_inertia_tensor;
    data->m_chassisUnitInertiaPitch = info->m_chassis_unit_inertia_pitch * inertia.rvec.x;
    data->m_chassisUnitInertiaYaw = info->m_chassis_unit_inertia_yaw * inertia.uvec.y;
    data->m_chassisUnitInertiaRoll = info->m_chassis_unit_inertia_roll * inertia.fvec.z;
    data->m_frictionEqualizer = info->m_friction_equalizer;

    data->m_wheelParams.setSize((int)a_data->m_num_wheels);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        hkpVehicleData::WheelComponentParams& wheel = data->m_wheelParams[w];
        const unsigned int axle = a_data->m_wheel_axle_ids[w];
        wheel.m_radius = a_data->m_wheel_radius_per_axle[axle] + info->enlarged_wheel_radius;
        wheel.m_mass = info->m_axle_wheel_infos[axle].mass;
        wheel.m_width = a_data->m_wheel_width_per_axle[axle];
        wheel.m_friction = info->m_axle_wheel_infos[axle].friction;
        wheel.m_viscosityFriction = info->m_viscosity_friction;
        wheel.m_maxFriction = 5.0f;
        wheel.m_slipAngle = 0.0f;
        wheel.m_forceFeedbackMultiplier = 1.0f;
        wheel.m_maxContactBodyAcceleration = GRAVITY_MS2;
        wheel.m_axle = (signed char)axle;
    }

    data->m_numWheelsPerAxle.setSize((int)info->m_num_axles);
    for (unsigned int a = 0; a < info->m_num_axles; a++) {
        data->m_numWheelsPerAxle[a] = (signed char)a_data->m_axle_wheel_counts[a];
    }
}

// 0x00B71AF0 (:475)
static void automobile_create_havok_driver_input(hk_custom_vehicle_instance* p_vehicle_instance)
{
    hk_custom_vehicle_driver_input* driver_input = new hk_custom_vehicle_driver_input();
    driver_input->m_slopeChangePointX = 1.0f;
    driver_input->m_initialSlope = 1.0f;
    p_vehicle_instance->m_driverInput = driver_input;
    driver_input->m_deadZone = 0.0f;
    driver_input->m_autoReverse = true;
}

// 0x00BA4FC0 (:495)
static void automobile_create_havok_steering(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    hk_custom_vehicle_steering* steering = new hk_custom_vehicle_steering();
    p_vehicle_instance->m_steering = steering;
    steering->m_max_turn_speed = info->m_steering_wheel_max_speed;
    steering->m_max_return_speed = info->m_steering_wheel_max_return_speed;
    steering->m_return_damp_angle = info->m_steering_wheel_return_damp_angle;
    steering->m_damp_angle = info->m_steering_wheel_damp_angle;
    steering->m_maxSteeringAngle = info->max_steering_angle;
    steering->m_maxSpeedFullSteeringAngle = info->max_speed_steering_angle;
    steering->m_counter_steer_min_speed = info->counter_steer_min_speed;
    steering->m_counter_steer_max_speed = info->counter_steer_max_speed;
    steering->m_doesWheelSteer.setSize((int)a_data->m_num_wheels);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        steering->m_doesWheelSteer[w] = info->m_axle_wheel_infos[a_data->m_wheel_axle_ids[w]].does_steer;
    }
}

// 0x00B71B50 (:533)
static void automobile_create_havok_engine(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const vehicle_info* info = ap->info;
    hk_custom_vehicle_engine* engine = new hk_custom_vehicle_engine();
    p_vehicle_instance->m_engine = engine;
    engine->m_minRPM = info->min_rpm;
    engine->m_optRPM = info->optimal_rpm;
    engine->m_maxRPM = info->max_rpm;
    engine->m_maxTorque = info->engine_torque;
    engine->m_torqueFactorAtMinRPM = info->min_rpm_torque_factor;
    engine->m_torqueFactorAtMaxRPM = info->max_rpm_torque_factor;
    engine->m_resistanceFactorAtMinRPM = info->min_rpm_resistance;
    engine->m_resistanceFactorAtOptRPM = info->opt_rpm_resistance;
    engine->m_resistanceFactorAtMaxRPM = info->max_rpm_resistance;
    engine->m_clutchSlipRPM = info->clutch_slip_rpm;
}

// 0x00B9B740 (:557)
static void automobile_create_havok_transmission(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    const vehicle_info::transmission_info& trans_info = info->trans_info;
    hk_custom_vehicle_transmission* transmission = new hk_custom_vehicle_transmission();
    p_vehicle_instance->m_transmission = transmission;
    transmission->m_downshiftRPM = 0.0f;
    transmission->m_upshiftRPM = 0.0f;
    transmission->m_primaryTransmissionRatio = trans_info.differential_gear_ratio;
    transmission->m_clutchDelayTime = trans_info.clutch_delay;
    transmission->m_clutch_delay_pre_shift = trans_info.clutch_delay_pre_shift;
    transmission->m_reverseGearRatio = trans_info.reverse_gear_ratio;

    transmission->m_gearsRatio.setSize((int)trans_info.num_gears);
    for (unsigned int g = 0; g < trans_info.num_gears; g++) {
        transmission->m_gearsRatio[g] = trans_info.gear_ratios[g];
    }

    transmission->m_wheelsTorqueRatio.setSize((int)a_data->m_num_wheels);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const unsigned int axle = a_data->m_wheel_axle_ids[w];
        transmission->m_wheelsTorqueRatio[w] = info->m_axle_wheel_infos[axle].engine_torque_factor / (float)a_data->m_axle_wheel_counts[axle];
    }

    transmission->m_downshift_rpms.setSize((int)trans_info.num_gears - 1);
    transmission->m_upshift_rpms.setSize((int)trans_info.num_gears - 1);
    for (unsigned int g = 0; g < trans_info.num_gears - 1; g++) {
        transmission->m_downshift_rpms[g] = trans_info.downshift_rpms[g];
        transmission->m_upshift_rpms[g] = trans_info.upshift_rpms[g];
    }
}

// 0x00B9B990 (:627)
static void automobile_create_havok_brake(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    hk_custom_vehicle_brakes* brake = new hk_custom_vehicle_brakes();
    p_vehicle_instance->m_brake = brake;
    brake->m_min_pedal_input_to_block = info->min_pedal_input_to_block;
    brake->m_ai_min_time_to_block = info->ai_min_time_to_block;
    brake->m_min_time_to_block = info->min_time_to_block;
    // Sets the blocking values of the wheels, of which there are none yet (order kept from rfg.exe).
    p_vehicle_instance->set_parking(false);

    brake->m_wheelBrakingProperties.setSize((int)a_data->m_num_wheels);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const vehicle_info::axle_wheel_info& axle = info->m_axle_wheel_infos[a_data->m_wheel_axle_ids[w]];
        brake->m_wheelBrakingProperties[w].m_isConnectedToHandbrake = axle.does_handbrake;
        brake->m_wheelBrakingProperties[w].m_maxBreakingTorque = axle.braking_torque;
    }
}

// 0x00BA5110 (:672)
static void automobile_create_havok_suspension(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    hk_custom_vehicle_suspension* suspension = new hk_custom_vehicle_suspension();
    p_vehicle_instance->m_suspension = suspension;
    const int num_wheels = (int)a_data->m_num_wheels;
    suspension->m_wheelParams.setSize(num_wheels);
    suspension->m_wheelSpringParams.setSize(num_wheels);

    hkVector4f down;
    down.set(0.0f, -1.0f, 0.0f, 0.0f);
    for (int w = 0; w < num_wheels; w++) {
        const vehicle_info::axle_wheel_info& axle = info->m_axle_wheel_infos[a_data->m_wheel_axle_ids[w]];
        hkpVehicleSuspension::SuspensionWheelParameters& params = suspension->m_wheelParams[w];
        params.m_directionChassisSpace = down;
        const vector& position = a_data->m_wheel_components[w]->get_data()->m_position;
        vector hardpoint(position.x, axle.spring_compression + position.y, position.z);
        game_to_havok_vector(params.m_hardpointChassisSpace, hardpoint);
        // Where the spring rests under its share of the vehicle's weight.
        const float rest = (GRAVITY_MS2 / (float)a_data->m_num_wheels) / axle.spring_strength + axle.spring_compression;
        suspension->zero_point.pushBack(rest);
        const float length = axle.spring_extension + axle.spring_compression;
        params.m_length = 0.0f <= length - rest ? rest : length;
        suspension->spring_pow.pushBack(axle.spring_power);
        suspension->m_wheelSpringParams[w].m_dampingCompression = axle.compression_damping;
        suspension->m_wheelSpringParams[w].m_dampingRelaxation = axle.expansion_damping;
        suspension->m_wheelSpringParams[w].m_strength = axle.spring_strength;
    }
}

// 0x00B71BF0 (:759)
static void automobile_create_havok_aerodynamics(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const vehicle_info* info = ap->info;
    hkpVehicleDefaultAerodynamics* aerodynamics = new hkpVehicleDefaultAerodynamics();
    p_vehicle_instance->m_aerodynamics = aerodynamics;
    aerodynamics->m_airDensity = info->air_density;
    aerodynamics->m_frontalArea = info->frontal_area;
    aerodynamics->m_dragCoefficient = info->drag_coefficient;
    aerodynamics->m_extraGravityws.setZero();
    aerodynamics->m_liftCoefficient = info->lift_coefficient;
}

// 0x00BB0AA0 (:778)
static void automobile_create_havok_wheel_collide(hk_custom_vehicle_instance* p_vehicle_instance, automobile* ap)
{
    hk_custom_vehicle_raycast_wheel_collide* wheel_collide = new hk_custom_vehicle_raycast_wheel_collide((int)p_vehicle_instance->m_data->m_numWheels);
    p_vehicle_instance->m_wheelCollide = wheel_collide;
    wheel_collide->m_wheelCollisionFilterInfo = COLLIDE_RAY_VEHICLE_WHEEL;
}

// 0x00B71C70 (:797)
static void automobile_create_havok_tyre_marks(hk_custom_vehicle_instance* p_vehicle_instance)
{
    hkpTyremarksInfo* tyre_marks = new hkpTyremarksInfo(*p_vehicle_instance->m_data, 0x80); // 128 points per wheel
    tyre_marks->m_minTyremarkEnergy = 100.0f;
    p_vehicle_instance->m_tyreMarks = tyre_marks;
    tyre_marks->m_maxTyremarkEnergy = 1000.0f;
}

// 0x00B71CC0 (:815)
static void automobile_create_havok_velocity_damper(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    const vehicle_info* info = ap->info;
    hkpVehicleDefaultVelocityDamper* damper = new hkpVehicleDefaultVelocityDamper();
    p_vehicle_instance->m_velocityDamper = damper;
    damper->m_normalSpinDamping = info->m_normal_spin_damping;
    damper->m_collisionSpinDamping = info->m_collision_spin_damping;
    damper->m_collisionThreshold = info->m_collision_spin_threshold;
}

// 0x00B71D20 (:833)
static void automobile_create_havok_extra_downforce(hk_custom_vehicle_instance* p_vehicle_instance, const automobile* ap)
{
    hk_custom_vehicle_extra_downforce* downforce = new hk_custom_vehicle_extra_downforce(p_vehicle_instance);
    p_vehicle_instance->m_extra_downforce = downforce;
    downforce->m_acceleration = ap->info->extra_gravity;
    Havok_world->addAction(downforce);
    downforce->removeReference();
}

// 0x00BB0AF0 (:855)
static void automobile_create_havok_vehicle_instance(automobile* ap)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_info* info = ap->info;
    hkpRigidBody* chassis = havok_body_get_pointer(ap->havok_handle);

    hk_custom_vehicle_instance* instance = new hk_custom_vehicle_instance();
    a_data->m_hk_vehicle_instance = instance;
    instance->m_data = a_data->m_hk_vehicle_data;
    instance->setChassis(chassis);
    automobile_create_havok_driver_input(instance);
    automobile_create_havok_steering(instance, ap);
    automobile_create_havok_engine(instance, ap);
    automobile_create_havok_transmission(instance, ap);
    automobile_create_havok_brake(instance, ap);
    automobile_create_havok_suspension(instance, ap);
    automobile_create_havok_aerodynamics(instance, ap);
    automobile_create_havok_wheel_collide(instance, ap);
    automobile_create_havok_tyre_marks(instance);
    automobile_create_havok_velocity_damper(instance, ap);
    automobile_create_havok_extra_downforce(instance, ap);
    instance->m_deviceStatus = new hkpVehicleDriverInputAnalogStatus();

    // The chassis AABB grown over the centre of mass and the ground casts of the tread wheels.
    hkVector4f aabb_min;
    hkVector4f aabb_max;
    aabb_min.setZero();
    aabb_max.setZero();
    const hkVector4f& center_of_mass = chassis->getCenterOfMassLocal();
    aabb_min.setMin(aabb_min, center_of_mass);
    aabb_max.setMax(aabb_max, center_of_mass);
    vehicle::resource_dependent_data* v_data = ap->rdd.get_data();
    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        const vehicle_component* comp = &v_data->m_components[c];
        if (comp->get_type() != VCT_TREAD_WHEEL_GROUND) {
            continue;
        }
        vector start;
        vector end;
        float length;
        float radius;
        compute_tread_wheel_ground_cast_positions(ap, comp, start, end, length, radius);
        hkVector4f point;
        game_to_havok_vector(point, start);
        aabb_min.setMin(aabb_min, point);
        aabb_max.setMax(aabb_max, point);
        game_to_havok_vector(point, end);
        aabb_min.setMin(aabb_min, point);
        aabb_max.setMax(aabb_max, point);
    }
    instance->set_extra_aabb_limits(aabb_min, aabb_max);

    instance->set_auto_brake_speed(info->autobrake_speed);
    instance->set_enforced_max_speed(info->enforced_max_speed, info->enforced_max_rspeed);
    instance->set_transition_delays(info->trans_info.forward_to_reverse_delay, info->trans_info.reverse_to_forward_delay);
    instance->set_handbrake_yaw_factor_scalar(info->m_torque_yaw_scalar);
    instance->set_reverse_torque_multiplier(info->reverse_torque_multiplier);
    instance->init();
}

// A wheel against the track decals of every wheel of its width: on a track already made it
// does not lay another. 0x00BA5460 (:943), part.
static bool automobile_wheel_is_on_existing_track(automobile* ap, unsigned int wheel_index, const vector& pos, const matrix& wheel_orient)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const hkpVehicleData* data = a_data->m_hk_vehicle_instance->m_data;
    const float width = data->m_wheelParams[wheel_index].m_width;
    for (unsigned int other = 0; other < a_data->m_num_wheels; other++) {
        if (width != data->m_wheelParams[other].m_width) {
            continue;
        }
        circ_array<automobile::tire_track_decal_data,3>& tracks = ap->m_wheel_effect_infos[other].m_track_decals;
        for (int i = 0; i < tracks.size(); i++) {
            const automobile::tire_track_decal_data track(tracks[i]);
            if (track.m_decal_handle == NO_HANDLE) {
                continue;
            }
            line ray;
            ray.point = track.m_start_pos;
            ray.dir = track.m_move_dir;
            vector closest;
            const float along = ix_closest_point_on_ray(pos, ray, closest);
            if (along < 0.0f || track.m_length < along) {
                continue;
            }
            const double half_width = width * 0.5;
            const vector offset = pos - closest;
            if (half_width * half_width < (double)offset.mag_squared()) {
                continue;
            }
            const float dot = track.m_move_dir.x * wheel_orient.fvec.x + track.m_move_dir.y * wheel_orient.fvec.y + track.m_move_dir.z * wheel_orient.fvec.z;
            if (Automobile_tire_track_min_direction_dot_prod <= fabsf(dot)) {
                return true;
            }
        }
    }
    return false;
}

// 0x00BA5460 (:943): motion, skid, peel and brake effects and foleys per wheel, tire tracks, and the wheel foleys of the car
static void automobile_effects_handle_wheel_effects(automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return;
    }
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();

    vector velocity;
    havok_body_get_linear_velocity(ap->havok_handle, velocity);
    const float speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
    if (speed == 0.0f) {
        if (ap->m_wheel_effect_stopped_timer.valid()) {
            if (ap->m_wheel_effect_stopped_timer.elapsed()) {
                if (!ap->m_wheel_effect_is_stopped) {
                    automobile_effects_stop_wheel_effects(ap);
                    ap->m_wheel_effect_is_stopped = true;
                }
                return;
            }
        }
        else {
            ap->m_wheel_effect_stopped_timer.set(WHEEL_EFFECTS_STOP_DELAY_MS);
        }
    }
    else if (ap->m_wheel_effect_stopped_timer.valid()) {
        ap->m_wheel_effect_stopped_timer.invalidate();
        ap->m_wheel_effect_is_stopped = false;
    }

    bool any_wheel_moving_on_ground = false;
    float max_brake_slip = 0.0f;
    // Original: the automobile keeps this pointer to the stack after returning; only calls made from here read it.
    decal_collection_data decal_collections[MAX_DECAL_COLLECTIONS];
    ap->m_decal_collection_data = decal_collections;
    ap->m_num_decal_collections = MAX_DECAL_COLLECTIONS;
    ap->m_decal_collection_occurred = false;
    ap->m_decal_width = 0.0f;

    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const hkpVehicleInstance::WheelInfo& wheel = instance->m_wheelsInfo[w];
        automobile::wheel_effect_info& effects = ap->m_wheel_effect_infos[w];
        object* contact = object_from_world_obj(wheel.m_contactBody);
        // Surface effects and tracks are for the terrain only.
        const bool on_terrain = contact != NULL && contact->get_obj_type() == OT_ZONE;
        const unsigned char material_index = instance->get_effect_material_in_contact_with_wheel(w);

        bool do_tracks = false;
        bool has_driving_effect = false;
        unsigned int front_effect = NO_HANDLE;
        unsigned int rear_effect = NO_HANDLE;
        if (material_index != 0xFF && on_terrain) {
            const rfg_effect_material* material = rfg_get_effect_material_by_index(material_index);
            do_tracks = material->driving_do_tracks;
            front_effect = material->driving_front_effect;
            rear_effect = material->driving_rear_effect;
            has_driving_effect = front_effect != NO_HANDLE || rear_effect != NO_HANDLE;
        }

        const bool moving_forward = 0.0f <= ap->orient.fvec * velocity;
        const bool trailing_wheel = moving_forward ? a_data->m_back_wheels[w] : a_data->m_front_wheels[w];
        const bool left_wheel = a_data->m_wheel_components[w]->get_data()->m_position.x < 0.0f;

        vector contact_pos;
        vector contact_normal;
        vector spin_axis;
        havok_to_game_vector(contact_pos, wheel.m_contactPoint.getPosition());
        havok_to_game_vector(contact_normal, wheel.m_contactPoint.getNormal());
        havok_to_game_vector(spin_axis, wheel.m_spinAxisWs);
        const vector spin_axis_negated(-spin_axis.x, -spin_axis.y, -spin_axis.z);
        matrix wheel_orient;
        wheel_orient.make_ruvec(spin_axis_negated, contact_normal);
        if (!left_wheel) {
            wheel_orient.rvec.x *= -1.0f;
            wheel_orient.rvec.y *= -1.0f;
            wheel_orient.rvec.z *= -1.0f;
        }

        if (material_index != 0xFF && on_terrain) {
            const rfg_effect_material* material = rfg_get_effect_material_by_index(material_index);
            if (material->effect_tint_enabled) {
                object_effect* effect = World.object_from_handle<object_effect>(effects.m_motion_effect);
                if (effect != NULL && has_driving_effect) {
                    effect->set_material_tint(material->effect_tint);
                }
                effect = World.object_from_handle<object_effect>(effects.m_skid_effect);
                if (effect != NULL) {
                    effect->set_material_tint(material->effect_tint);
                }
                effect = World.object_from_handle<object_effect>(effects.m_peel_effect);
                if (effect != NULL) {
                    effect->set_material_tint(material->effect_tint);
                }
                effect = World.object_from_handle<object_effect>(effects.m_brake_effect);
                if (effect != NULL) {
                    effect->set_material_tint(material->effect_tint);
                }
            }
        }

        const float spin = fabsf(wheel.m_spinVelocity);
        const bool fast_enough_for_motion = Automobile_motion_effect_min_vehicle_velocity < speed || Automobile_motion_effect_min_wheel_velocity < spin;

        // motion
        const bool play_motion = has_driving_effect && fast_enough_for_motion;
        unsigned int motion_info = trailing_wheel ? Automobile_motion_back_effect_info_handle : Automobile_motion_front_effect_info_handle;
        if (play_motion) {
            const unsigned int material_effect = trailing_wheel ? rear_effect : front_effect;
            if (material_effect != NO_HANDLE) {
                motion_info = material_effect;
            }
        }
        automobile_process_wheel_effect(effects.m_motion_effect, play_motion, !play_motion, contact_pos, ap->orient, motion_info, true);
        any_wheel_moving_on_ground = any_wheel_moving_on_ground || (wheel.m_contactBody != NULL && fast_enough_for_motion);

        // skid
        const float side_slip = fabsf(wheel.m_sideSlipVelocity);
        const bool play_skid = on_terrain && Automobile_min_side_slip_velocity <= side_slip;
        const bool play_skid_foley = wheel.m_contactBody != NULL && Automobile_min_side_slip_velocity <= side_slip;
        automobile_process_wheel_effect(effects.m_skid_effect, play_skid, !play_skid, contact_pos, wheel_orient, Automobile_skid_effect_info_handle, true);
        automobile_process_wheel_foley(ap, effects.m_skid_foley, play_skid_foley, wheel.m_contactBody == NULL, Automobile_wheel_skid_cue_id, false, 1.0f, contact_pos, velocity);

        // peel
        const float forward_slip = moving_forward ? wheel.m_forwardSlipVelocity : -wheel.m_forwardSlipVelocity;
        const bool play_peel = on_terrain && Automobile_min_forward_slip_velocity <= forward_slip;
        const bool play_peel_foley = wheel.m_contactBody != NULL && Automobile_min_forward_slip_velocity <= forward_slip;
        automobile_process_wheel_effect(effects.m_peel_effect, play_peel, !play_peel, contact_pos, wheel_orient, Automobile_peel_effect_info_handle, true);
        automobile_process_wheel_foley(ap, effects.m_peel_foley, play_peel_foley, wheel.m_contactBody == NULL, Automobile_wheel_peel_cue_id, false, 1.0f, contact_pos, velocity);

        // brake
        const bool braking_hard = forward_slip <= -Automobile_min_forward_slip_velocity && Automobile_motion_effect_min_vehicle_velocity < speed;
        const bool play_brake = on_terrain && braking_hard;
        const bool play_brake_foley = wheel.m_contactBody != NULL && braking_hard;
        const bool stop_brake = !play_brake && !ap->brake_effect_release_timer.elapsed();
        const bool stop_brake_foley = !play_brake_foley && !ap->brake_effect_release_timer.elapsed();
        const int brake_cue = -Automobile_light_brake_max_slip_velocity < forward_slip ? Automobile_wheel_brake_light_cue_id : Automobile_wheel_brake_cue_id;
        automobile_process_wheel_effect(effects.m_brake_effect, play_brake, stop_brake, contact_pos, wheel_orient, Automobile_brake_effect_info_handle, true);
        automobile_process_wheel_foley(ap, effects.m_brake_foley, play_brake_foley, stop_brake_foley, brake_cue, false, 1.0f, contact_pos, velocity);
        if (play_brake_foley && max_brake_slip < fabsf(wheel.m_forwardSlipVelocity)) {
            max_brake_slip = fabsf(wheel.m_forwardSlipVelocity);
        }

        if (effects.m_motion_effect != NO_HANDLE) {
            object_effect* motion_effect = World.object_from_handle<object_effect>(effects.m_motion_effect);
            if (motion_effect != NULL) {
                motion_effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
            }
        }
        if (audiolib_is_playing(effects.m_brake_foley, true)) {
            audiolib_play_set_volume(effects.m_brake_foley, fl_parameter(-forward_slip, 0.0f, Automobile_brake_max_volume_velocity, true), 100);
        }

        // tire tracks
        const vector to_camera = Rfg_camera.real_pos - contact_pos;
        const float camera_dist_sq = to_camera.mag_squared();
        bool lay_track = false;
        if (R_enable_decals && do_tracks && camera_dist_sq < TIRE_TRACK_MAX_DIST_SQ) {
            lay_track = true;
            vector bmin;
            vector bmax;
            ap->get_bbox(bmin, bmax);
            const float size_x = bmax.x - bmin.x;
            const float size_z = bmax.z - bmin.z;
            ap->m_decal_width = (size_x - size_z < 0.0f ? size_z : size_x) + 3.0f;
            if (!game_render_is_object_visible(bmin, bmax) && TIRE_TRACK_MAX_UNSEEN_DIST_SQ <= camera_dist_sq) {
                lay_track = false;
            }
        }

        circ_array<automobile::tire_track_decal_data,3>& tracks = effects.m_track_decals;
        const bool has_track_object = 0 < tracks.size() && tracks[0].m_obj_handle != NO_HANDLE;
        const unsigned int current_decal = tracks[0].m_decal_handle;
        automobile::tire_track_decal_data new_track;
        if (lay_track) {
            if (automobile_wheel_is_on_existing_track(ap, w, contact_pos, wheel_orient)) {
                if (current_decal != NO_HANDLE) {
                    automobile_tire_track_decal_start_fade(current_decal);
                    tracks.add(new_track);
                }
                else if (has_track_object) {
                    tracks[0].invalidate();
                }
                continue;
            }
            const vector move_dir = velocity.copy_normalize_safe();
            const float width = instance->m_data->m_wheelParams[w].m_width;
            if (!has_track_object) {
                automobile_tire_track_decal_create(ap, new_track, contact_pos, move_dir, contact_normal, contact_pos, contact->get_handle(), width);
                tracks.add(new_track);
            }
            else if (!automobile_tire_track_decal_adjust(ap, tracks[0], contact_pos, contact->get_handle(), width)) {
                const automobile::tire_track_decal_data& current = tracks[0];
                const vector start_pos(current.m_move_dir.x * current.m_length + current.m_start_pos.x, current.m_start_pos.y + current.m_move_dir.y * current.m_length, current.m_start_pos.z + current.m_move_dir.z * current.m_length);
                automobile_tire_track_decal_create(ap, new_track, contact_pos, move_dir, contact_normal, start_pos, contact->get_handle(), width);
                if (tracks[0].m_decal_handle == NO_HANDLE) {
                    tracks[0].invalidate();
                }
                else {
                    automobile_tire_track_decal_start_fade(tracks[0].m_decal_handle);
                }
                tracks.add(new_track);
            }
        }
        else if (current_decal != NO_HANDLE) {
            automobile_tire_track_decal_start_fade(current_decal);
            tracks.add(new_track);
        }
        else if (has_track_object) {
            tracks[0].invalidate();
        }
    }

    const bool play_motion_foley = any_wheel_moving_on_ground && human_in_vehicle_interaction(player_get_local_player(), ap);
    float volume = speed / Automobile_motion_foley_full_volume_wheel_velocity;
    if (volume - 0.0f < 0.0f) {
        volume = 0.0f;
    }
    if (0.0f <= volume - 1.0f) {
        volume = 1.0f;
    }
    automobile_process_wheel_foley(ap, ap->m_motion_foley, play_motion_foley, !play_motion_foley, Automobile_wheel_motion_cue_id, false, volume, ap->pos, velocity);
    if (vehicle_is_tank(ap)) {
        automobile_process_wheel_foley(ap, ap->m_tread_foley, play_motion_foley, !play_motion_foley, Tank_tread_cue_id, false, 1.0f, ap->pos, ZERO_VECTOR);
    }
    if (0.0f < max_brake_slip) {
        vehicle_camera_shake_play_brake(ap, fl_parameter(max_brake_slip, 0.0f, Automobile_brake_max_volume_velocity, true));
    }
}

// One axis of air control: the angular velocity to give, or 0 for none. 0x00B71DA0 (:1305), part.
static float automobile_air_control_axis(float input, float forward_y, float up_y, float current, float max_vel, float max_angle)
{
    if (max_angle <= 0.0f) {
        return 0.0f;
    }
    float angle = -(float)asin(forward_y);
    if (up_y < 0.0f) {
        angle = fl_sign(angle) * 3.1415927f - angle;
    }
    if (fabsf(input) <= 0.2f) { // stick dead zone
        return 0.0f;
    }
    float target = max_vel * input;
    if ((input < 0.0f) == (angle < 0.0f)) {
        float t = fabsf(angle) / max_angle;
        if (t - 0.0f < 0.0f) {
            t = 0.0f;
        }
        if (0.0f <= t - 1.0f) {
            t = 1.0f;
        }
        target = (1.0f - t) * target;
    }
    const float step = Frametime * 6.5f * max_vel; // 6.5: per second, of the maximum
    const float low = current - step;
    const float high = step + current;
    if (target < low) {
        return low;
    }
    return high < target ? high : target;
}

// 0x00B71DA0 (:1305)
static void automobile_steer_air_control(automobile* ap, float steering_input_x, float steering_input_y)
{
    if (!ap->is_airborne.valid() || ap->last_velocity.mag_squared() <= 45.0f) {
        return;
    }
    vector angular;
    angular.zero();
    havok_body_get_angular_velocity(ap->havok_handle, angular);
    const vector& rvec = ap->orient.rvec;
    const vector& fvec = ap->orient.fvec;
    const float roll_vel = angular.x * -fvec.x + angular.y * -fvec.y + angular.z * -fvec.z;
    const float pitch_vel = rvec.y * angular.y + rvec.x * angular.x + rvec.z * angular.z;
    const vehicle_info* info = ap->info;
    const float roll = automobile_air_control_axis(steering_input_x, rvec.y, ap->orient.uvec.y, roll_vel, info->air_steer_roll_max_vel, info->air_steer_roll_max_angle_rad);
    const float pitch = automobile_air_control_axis(steering_input_y, fvec.y, ap->orient.uvec.y, pitch_vel, info->air_steer_pitch_max_vel, info->air_steer_pitch_max_angle_rad);

    // Off the current roll and pitch; on the new ones where control is given.
    vector roll_part(fvec.x * roll_vel, fvec.y * roll_vel, fvec.z * roll_vel);
    vector pitch_part(rvec.x * pitch_vel, rvec.y * pitch_vel, rvec.z * pitch_vel);
    const vector rest((angular.x - pitch_part.x) + roll_part.x, (angular.y - pitch_part.y) + roll_part.y, (angular.z - pitch_part.z) + roll_part.z);
    if (0.001f < fabsf(roll)) {
        roll_part = vector(fvec.x * roll, fvec.y * roll, fvec.z * roll);
    }
    if (0.001f < fabsf(pitch)) {
        pitch_part = vector(rvec.x * pitch, rvec.y * pitch, rvec.z * pitch);
    }
    const vector result(pitch_part.x + (rest.x - roll_part.x), pitch_part.y + (rest.y - roll_part.y), pitch_part.z + (rest.z - roll_part.z));
    havok_body_set_angular_velocity(ap->havok_handle, result);
}

// 0x00B88510 (:1389)
static void automobile_process_powerslide(automobile* ap)
{
    if (ap == NULL) {
        return;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.is_loaded() ? ap->a_rdd.get_data()->m_hk_vehicle_instance : NULL;
    if (!object_is_player(automobile_current_driver(ap))) {
        ap->powerslide_amount = 0.0f;
        ap->current_powerslide_friction = 1.0f;
        if (ap->a_rdd.is_loaded()) {
            instance->set_powerslide_torque_multiplier(1.0f);
        }
        return;
    }

    vector side(ap->orient.rvec);
    side.normalize_safe();
    vector velocity(ZERO_VECTOR);
    if (ap->get_handle() != NO_HANDLE) {
        object* obj = World.object_from_handle(ap->get_handle());
        if (obj != NULL && obj->get_obj_type() == OT_VEHICLE) {
            vehicle_get_velocity(static_cast<vehicle*>(obj), velocity);
        }
        else {
            velocity.zero();
        }
    }
    else {
        velocity.zero();
    }
    const float sideways = fabsf(velocity.y * side.y + velocity.x * side.x + velocity.z * side.z);
    const float accelerator = automobile_get_accelerator_value(ap->get_handle());
    const bool handbrake_on = ap->a_rdd.is_loaded() && instance->get_handbrake_on();

    if (0.01f < ap->powerslide_amount) {
        if (accelerator <= 0.0f || sideways < Powerslide_min_sideways_velocity) {
            ap->powerslide_amount -= Frametime * Powerslide_falloff_rate;
        }
        if (ap->powerslide_amount <= 0.01f) {
            ap->powerslide_amount = 0.0f;
        }
    }
    else if (handbrake_on && !ap->powerslide_was_handbrake_on && 0.0f < accelerator && (Powerslide_straight || Powerslide_min_sideways_velocity < sideways)) {
        ap->powerslide_amount = 1.0f;
    }
    const float amount = ap->powerslide_amount;
    ap->powerslide_was_handbrake_on = handbrake_on;
    if (amount <= 0.01f) {
        ap->powerslide_amount = 0.0f;
        ap->current_powerslide_friction = 1.0f;
        if (ap->a_rdd.is_loaded()) {
            instance->set_powerslide_torque_multiplier(1.0f);
        }
    }
    else {
        ap->current_powerslide_friction = (ap->info->powerslide_friction - 1.0f) * amount + 1.0f;
        if (ap->a_rdd.is_loaded()) {
            instance->set_powerslide_torque_multiplier((ap->info->powerslide_power - 1.0f) * amount + 1.0f);
        }
    }
}

// 0x00B72550 (:1473)
void automobile_setup_havok_for_corpse(automobile* ap)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    hk_custom_vehicle_suspension* suspension = static_cast<hk_custom_vehicle_suspension*>(instance->m_suspension);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        suspension->m_wheelParams[w].m_length = 0.01f;
        suspension->zero_point[w] = 0.01f;
        suspension->m_wheelSpringParams[w].m_strength = 0.001f;
    }
    hkpVehicleDefaultVelocityDamper* damper = static_cast<hkpVehicleDefaultVelocityDamper*>(instance->m_velocityDamper);
    damper->m_normalSpinDamping = 0.5f;
    damper->m_collisionSpinDamping = 0.5f;
    damper->m_collisionThreshold = ap->info->m_collision_spin_threshold;
    static_cast<hkpVehicleRayCastWheelCollide*>(instance->m_wheelCollide)->m_wheelCollisionFilterInfo = COLLIDE_NO_COLLISION;
    instance->m_extra_downforce->enable(false);
    float mass;
    vector center_of_mass;
    matrix inertia;
    havok_body_get_mass_props(ap->havok_handle, mass, center_of_mass, inertia);
    havok_body_set_mass_props(ap->havok_handle, ap->info->chassis_mass * 0.6f, center_of_mass, inertia); // a burnt out chassis weighs less
}

// 0x00BB0ED0 (:1561)
void automobile_build(automobile* ap)
{
    automobile_create_havok_vehicle_data(ap);
    automobile_create_havok_vehicle_instance(ap);
}

// 0x00B72710 (:1584)
void automobile_calculate_wheelbase(automobile* ap)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    vector front_sum;
    vector rear_sum;
    front_sum.zero();
    rear_sum.zero();
    int num_front = 0;
    int num_rear = 0;
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const vector& position = a_data->m_wheel_components[w]->get_data()->m_position;
        if (a_data->m_wheel_axle_ids[w] == 0) {
            front_sum += position;
            num_front++;
        }
        else if (a_data->m_wheel_axle_ids[w] == 1) {
            rear_sum += position;
            num_rear++;
        }
    }
    if (num_front == 0 || num_rear == 0) {
        a_data->m_wheelbase = 1.0f;
        return;
    }
    const vector front = front_sum * (1.0f / (float)num_front);
    const vector rear = rear_sum * (1.0f / (float)num_rear);
    a_data->m_wheelbase = vec_dist(front, rear);
}

// 0x00B95A90 (:1631)
void automobile_blow_up(automobile* ap)
{
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    instance->set_forward_or_reverse_accelerator(0.0f);
    instance->set_brake_pedal_input(0.0f);
    instance->set_handbrake_on(true);
    automobile_stop_all_scraping_effects(ap);
}

// 0x00B9BBF0 (:1651)
void automobile_handle_human_run_over(automobile* ap)
{
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return;
    }
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    raycast_input input;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.radius = 0.0f;
    input.filter_params = NULL;
    input.filter_callback = NULL;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    raycast_output output;

    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        vehicle_component* wheel = a_data->m_wheel_components[w];
        vector bmin;
        vector bmax;
        ap->rdd.get_data()->sub_info->m_vehicle_mesh->get_subpiece_local_bb(wheel->get_data()->m_submesh_id, bmin, bmax);
        const vector wheel_pos(wheel->get_world_space_position());
        unsigned int ragdolls[8];
        const int num_ragdolls = havok_collect_ragdolls_fast(ragdolls, 8, wheel_pos, (bmax.y - bmin.y) * 0.5f);
        if (num_ragdolls == 0) {
            wheel->m_last_hit_by_handle = NO_HANDLE;
        }
        int num_under_wheel = 0;
        for (int i = num_ragdolls - 1; i >= 0; i--) {
            object* obj = ragdolls[i] != NO_HANDLE ? World.object_from_handle(ragdolls[i]) : NULL;
            // Original bug kept: a handle that is not a human leaves a null pointer that is read below.
            human* hp = obj != NULL && obj->get_obj_type() == OT_HUMAN ? static_cast<human*>(obj) : NULL;
            const vector& human_pos = hp->pos;
            const vector to_wheel = wheel_pos - human_pos;
            if (0.0f <= to_wheel.y * ap->orient.uvec.y + ap->orient.uvec.x * to_wheel.x + ap->orient.uvec.z * to_wheel.z) {
                num_under_wheel++;
            }
            else {
                // Above the wheel: nudged off to the side it is on.
                const vector& rvec = ap->orient.rvec;
                const float side = (human_pos.y - ap->pos.y) * rvec.y + rvec.x * (human_pos.x - ap->pos.x) + (human_pos.z - ap->pos.z) * rvec.z;
                const float sign = 0.0f <= side ? 1.0f : -1.0f;
                const vector push((rvec.x * sign * 2.0f + UP_VECTOR.x) * 0.05f, (rvec.y * sign * 2.0f + UP_VECTOR.y) * 0.05f, (rvec.z * sign * 2.0f + UP_VECTOR.z) * 0.05f);
                havok_ragdoll_apply_velocity(hp->ragdoll_idx, push, true);
            }
        }
        if (num_under_wheel == 0) {
            continue;
        }
        const hkpVehicleInstance::WheelInfo& wheel_info = a_data->m_hk_vehicle_instance->m_wheelsInfo[w];
        havok_to_game_vector(input.start, wheel_info.m_hardPointWs);
        havok_to_game_vector(input.end, wheel_info.m_rayEndPointWs);
        input.m_filter_info = COLLIDE_RAY_RAGDOLL_ONLY;
        input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        if (!havok_raycast(input, output)) {
            wheel->m_last_hit_by_handle = NO_HANDLE;
            continue;
        }
        const raycast_output::hit_info& hit = output.m_hits[0];
        object* obj = hit.hit_object_handle != NO_HANDLE ? World.object_from_handle(hit.hit_object_handle) : NULL;
        if (obj != NULL && obj->get_obj_type() == OT_HUMAN && wheel->m_last_hit_by_handle != obj->get_handle()) {
            const vector down(-UP_VECTOR.x, -UP_VECTOR.y, -UP_VECTOR.z);
            human_collide_vehicle(hit.hit_pos, down, ap->forward_velocity, obj->get_handle(), ap->get_handle(), NO_HANDLE, true);
            wheel->m_last_hit_by_handle = obj->get_handle();
        }
    }
}

// 0x00B63630 (:1724)
void types_automobile(signed char& type, signed char& subtype)
{
    type = OT_VEHICLE;
    subtype = OT_SUB_VEHICLE_AUTO;
}

// 0x00B63640 (:1724)
int bpo_index_automobile()
{
    return 0;
}

// 0x00B63650 (:1724)
const char* classname_automobile()
{
    return "automobile";
}

// 0x00B63040 (.h:59)
const char* automobile::get_classname_static()
{
    return "automobile";
}

// 0x00B63050 (.h:59)
int automobile::get_bpo_index_static()
{
    return 0;
}

// 0x00BA6860 (.h:59)
const char* automobile::get_classname() const
{
    return "automobile";
}

// 0x00B63660 (:1724)
unsigned int automobile::get_classname_hash()
{
    return automobile_instantiator.this_hash;
}

// 0x00B729A0 (:1724)
int automobile::pool_remaining()
{
    return automobilepool.get_free();
}

// 0x00B88850 (:1724)
object* alloc_automobile()
{
    return automobilepool.alloc(0);
}

// 0x00B95B50 (:1724)
void automobile::pool_free()
{
    automobilepool.free(this);
}

// 0x00B63680
mp_collision_prediction_info::mp_collision_prediction_info()
{
}

// 0x00B63670
automobile::collision_effect_info::collision_effect_info()
{
}

// 0x00B63060 (.h:64)
void automobile::collision_effect_info::reset()
{
    m_position.zero();
    m_normal.zero();
    m_other_handle = NO_HANDLE;
    m_foley_info = -1;
    m_effect_obj = NO_HANDLE;
}

// 0x00B635D0
automobile::tire_track_decal_data::tire_track_decal_data(const tire_track_decal_data& __that)
    : m_start_pos(__that.m_start_pos), m_move_dir(__that.m_move_dir), m_normal(__that.m_normal), m_length(__that.m_length), m_decal_handle(__that.m_decal_handle), m_obj_handle(__that.m_obj_handle)
{
}

// 0x00B71380 (.h:108)
automobile::tire_track_decal_data::tire_track_decal_data()
{
    m_decal_handle = NO_HANDLE;
    m_length = 0.0f;
    m_obj_handle = NO_HANDLE;
}

// 0x00B63080 (.h:112)
void automobile::tire_track_decal_data::invalidate()
{
    m_decal_handle = NO_HANDLE;
    m_length = 0.0f;
    m_obj_handle = NO_HANDLE;
}

// 0x00B88860
automobile::wheel_effect_info::wheel_effect_info()
{
}

// 0x00B729B0
automobile::wheel_effect_info::~wheel_effect_info()
{
}

// 0x00BA67B0 (:1725)
automobile::automobile()
{
}

// 0x00BA6870
automobile::~automobile()
{
}

// 0x00B95BB0 (:1737)
bool automobile::serialize(obj_prop_block& pblock, bool load)
{
    if (!vehicle::serialize(pblock, load)) {
        return false;
    }
    if (!load) {
        pblock.set(VEHICLE_PROP_TYPE_NAME, info->name);
        pblock.set<bool>(VEHICLE_PROP_AT_GROUND, true);
        return true;
    }
    user_acc_input = 0.0f;
    prev_chassis_stress = 0.0f;
    max_chassis_stress = 0.0f;
    force_assist_value = 0.0f;
    powerslide_amount = 0.0f;
    honk_alr_id = -1;
    foley_siren = -1;
    foley_megaphone = -1;
    foley_chassis = -1;
    engine_peel = -1;
    gear_shift = -1;
    time_at_max_speed.invalidate();
    play_shift_foley = false;
    is_airborne.invalidate();
    is_landing.invalidate();
    is_skipping.invalidate();
    is_skiprecovery.invalidate();
    current_powerslide_friction = 1.0f;
    powerslide_was_handbrake_on = false;
    brake_effect_release_timer.invalidate();
    catch_fire_timestamp.invalidate();
    horn_timeout.invalidate();
    for (int i = 0; i < 4; i++) {
        m_collision_impact_sounds[i].reset();
        m_collision_scraping_sounds[i].reset();
    }
    m_impact_sound_delay.invalidate();
    m_scraping_sound_delay.invalidate();
    m_chassis_sound_delay.invalidate();
    for (int i = 0; i < 16; i++) {
        wheel_effect_info& effects = m_wheel_effect_infos[i];
        effects.m_motion_effect = NO_HANDLE;
        effects.m_skid_effect = NO_HANDLE;
        effects.m_skid_foley = -1;
        effects.m_peel_effect = NO_HANDLE;
        effects.m_peel_foley = -1;
        effects.m_brake_effect = NO_HANDLE;
        effects.m_brake_foley = -1;
    }
    m_wheel_effect_is_stopped = false;
    mp_sim_state = NULL;
    m_wheel_effect_stopped_timer.invalidate();
    m_h_trailer_vehicle = NO_HANDLE;
    m_h_tractor_vehicle = NO_HANDLE;
    mp_collision_prediction.check_time.invalidate();
    mp_collision_prediction.last_start_pos.zero();
    mp_collision_prediction.last_end_pos.zero();
    mp_collision_prediction.last_pos.zero();
    mp_collision_prediction.last_vel.zero();
    if (Multiplayer && multi_is_server()) {
        remote_obj_flags.remote_object = 1;
        multi_core_object_create_remote_object(4, get_handle()); // 4: the automobile's registered multiplayer type
    }
    return true;
}

// The two wheel flags of a new wheel against each earlier wheel: the frontmost and rearmost wheels keep theirs.
// Differences within 0.1% of the larger position (or of 1) are the same axle. 0x00BB0EF0 (:1853), part.
static float automobile_wheel_position_tolerance(float a, float b)
{
    float larger = fabsf(a);
    if (fabsf(a) - fabsf(b) < 0.0f) {
        larger = fabsf(b);
    }
    if (0.0f <= 1.0f - larger) {
        larger = 1.0f;
    }
    return larger * 0.001f;
}

// 0x00BB0EF0 (:1853)
bool automobile::stream(bool load)
{
    if (!load) {
        if (a_rdd.is_loaded() && a_rdd.get_data()->m_hk_vehicle_instance != NULL) {
            automobile::resource_dependent_data* a_data = a_rdd.get_data();
            hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
            if (instance->m_extra_downforce != NULL) {
                Havok_world->removeAction(instance->m_extra_downforce);
                instance->m_extra_downforce = NULL;
            }
            if (instance->m_wheelCollide != NULL) {
                hkArray<hkpPhantom*,hkContainerHeapAllocator> phantoms;
                instance->m_wheelCollide->getPhantoms(phantoms);
                for (int i = 0; i < phantoms.getSize(); i++) {
                    havok_remove_phantom(phantoms[i], true);
                }
            }
            Havok_world->removeAction(instance);
            instance->removeReference();
            a_data->m_hk_vehicle_instance = NULL;
        }
        automobile_effects_stop_wheel_effects(this);
        automobile_stop_all_scraping_effects(this);
        a_rdd.set_loaded(false);
        return vehicle::stream(false);
    }

    if (!vehicle::stream(load)) {
        return false;
    }
    a_rdd.set_loaded(true);
    automobile::resource_dependent_data* a_data = a_rdd.get_data();
    vehicle::resource_dependent_data* v_data = rdd.get_data();
    const vehicle_subinfo* sub_info = v_data->sub_info;
    a_data->m_wheelbase = 0.0f;
    a_data->m_num_wheels = 0;
    for (int a = 0; a < 4; a++) {
        a_data->m_axle_wheel_counts[a] = 0;
    }
    for (unsigned int c = 0; c < sub_info->m_num_components; c++) {
        const vehicle_component_data* comp_data = &sub_info->m_components[c];
        const unsigned int type = comp_data->m_type;
        if (type != VCT_FRONT_WHEEL && type != VCT_REAR_WHEEL && type != VCT_AXLE_3_WHEEL && type != VCT_AXLE_4_WHEEL) {
            continue;
        }
        const unsigned int w = a_data->m_num_wheels;
        vehicle_component* comp = &v_data->m_components[c];
        a_data->m_wheel_components[w] = comp;
        const unsigned int axle = vehicle_get_axle_id(comp_data);
        a_data->m_wheel_axle_ids[w] = axle;
        if (a_data->m_axle_wheel_counts[axle] == 0) {
            vector bmin;
            vector bmax;
            sub_info->m_vehicle_mesh->get_subpiece_local_bb(comp->get_data()->m_submesh_id, bmin, bmax);
            a_data->m_wheel_radius_per_axle[axle] = (bmax.y - bmin.y) * 0.5f;
            a_data->m_wheel_width_per_axle[axle] = bmax.x - bmin.x;
        }
        a_data->m_front_wheels[w] = true;
        a_data->m_back_wheels[w] = true;
        for (unsigned int other = 0; other < w; other++) {
            const float z = comp_data->m_position.z;
            const float other_z = a_data->m_wheel_components[other]->get_data()->m_position.z;
            const float tolerance = automobile_wheel_position_tolerance(z, other_z);
            if (a_data->m_back_wheels[w] && a_data->m_back_wheels[other]) {
                if (z < other_z - tolerance) {
                    a_data->m_back_wheels[other] = false;
                }
                else if (tolerance + other_z < z) {
                    a_data->m_back_wheels[w] = false;
                }
            }
            if (a_data->m_front_wheels[w] && a_data->m_front_wheels[other]) {
                if (tolerance + other_z < z) {
                    a_data->m_front_wheels[other] = false;
                }
                else if (z < other_z - tolerance) {
                    a_data->m_front_wheels[w] = false;
                }
            }
        }
        a_data->m_axle_wheel_counts[axle]++;
        a_data->m_num_wheels++;
    }

    if (a_data->m_num_wheels == 0) {
        // rfg.exe formats "Car %s has no wheels, cannot create" (the mesh name) into a per-thread buffer that nothing shows.
        World.destroy_object(this, false);
        return false;
    }

    for (unsigned int c = 0; c < sub_info->m_num_components; c++) {
        v_data->m_components[c].setup(this);
    }
    automobile_create_havok_vehicle_data(this);
    automobile_create_havok_vehicle_instance(this);
    automobile_calculate_wheelbase(this);

    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    {
        hkArray<hkpPhantom*,hkContainerHeapAllocator> phantoms;
        instance->m_wheelCollide->getPhantoms(phantoms);
        for (int i = 0; i < phantoms.getSize(); i++) {
            havok_add_phantom(phantoms[i], true);
        }
    }
    if (info->vehicle_classification == BVT_TANK || info->vehicle_classification == BVT_ART_TANK) {
        instance->set_is_tank(true);
    }
    instance->set_forward_or_reverse_accelerator(0.0f);
    instance->set_steering_wheel_input(0.0f);
    instance->set_handbrake_on(true);
    Havok_world->addAction(instance);

    vai.steer_info.wheelbase = a_data->m_wheelbase;
    const float curvature = (float)sin(info->max_steering_angle) / a_data->m_wheelbase;
    vai.active_control_info.max_steer_curvature = curvature;
    vai.base_control_info.max_steer_curvature = curvature;

    if (!object_is_player(automobile_current_driver(this))) {
        automobile_reset_handling_for_ai(this, true, false);
    }
    automobile_attach_to_tractor_havok_level(this);

    if (fabsf(vai.initial_forward_speed) <= 0.0f || get_handle() == NO_HANDLE) {
        return true;
    }
    object* self = World.object_from_handle(get_handle());
    if (self == NULL || self->get_obj_type() != OT_VEHICLE) {
        return true;
    }
    hkpRigidBody* body = havok_body_get_pointer(self->havok_handle);
    if (body == NULL) {
        return true;
    }
    hkVector4f initial;
    initial.set(orient.fvec.x * vai.initial_forward_speed, orient.fvec.y * vai.initial_forward_speed, orient.fvec.z * vai.initial_forward_speed, 0.0f);
    body->activate();
    body->getMotion()->setLinearVelocity(initial);
    return true;
}

// 0x00BB4D40 (:2134)
void automobile::free_this()
{
    if (Multiplayer) {
        multi_core_object_process_object_free(this);
    }
    if (m_h_trailer_vehicle != NO_HANDLE) {
        object* trailer = World.object_from_handle(m_h_trailer_vehicle);
        if (trailer != NULL && trailer->get_obj_type() == OT_VEHICLE) {
            World.destroy_object(trailer, false);
        }
    }
    vehicle::free_this();
}

// 0x00B63690 (:2156): empty in rfg.exe
void automobile_system_init()
{
}

// 0x00B888C0 (:2160)
void automobile::check_wheel_contact_with_ground()
{
    if (!a_rdd.is_loaded()) {
        return;
    }
    automobile::resource_dependent_data* a_data = a_rdd.get_data();
    const unsigned int num_wheels = a_data->m_num_wheels;
    unsigned int num_on_ground = 0;
    for (unsigned int w = 0; w < num_wheels; w++) {
        hkpRigidBody* contact = a_data->m_hk_vehicle_instance->m_wheelsInfo[w].m_contactBody;
        if (contact == NULL) {
            continue;
        }
        num_on_ground++;
        // Tanks destroy the light movers (flag 4) they drive on. 100: kilograms
        if (!vehicle_is_tank(this) || 100.0f <= contact->getMass()) {
            continue;
        }
        object* contact_obj = object_from_world_obj(contact);
        if (contact_obj == NULL || contact_obj->get_obj_type() != OT_MOVER) {
            continue;
        }
        object_mover* mover = static_cast<object_mover*>(contact_obj);
        if ((mover->mover_flags & 4) == 0) {
            continue;
        }
        if (!Multiplayer || mover->get_sub_type() != OT_SUB_MOVER_RFG) {
            World.destroy_object(mover, false);
            vector bmin;
            vector bmax;
            mover->get_local_bbox(bmin, bmax);
            float size = bmax.x - bmin.x;
            if (size - (bmax.y - bmin.y) < 0.0f) {
                size = bmax.y - bmin.y;
            }
            if (size - (bmax.z - bmin.z) < 0.0f) {
                size = bmax.z - bmin.z;
            }
            const unsigned char material = mover->get_effect_material(NO_HANDLE, NULL, NULL);
            rfg_effect_candidate_create(material, mover->pos, MATERIAL_EVENT_LOAD_BALANCE, rfg_effect_candidate_get_size(material, MATERIAL_EVENT_LOAD_BALANCE, size), INVALID_MATERIAL_EVENT_SHAPE_TYPE, false, 0, NULL, NULL, NO_HANDLE, 0xFF, 0xFF);
        }
        else if (Multiplayer && multi_is_server()) {
            rfg_dyn_add_pre_collide_mp_destroy(static_cast<rfg_mover*>(mover), automobile_current_driver(this), false);
        }
    }
    m_flags.m_wheels_on_ground = (float)(int)num_wheels * 0.6f <= (float)(int)num_on_ground; // 60% of the wheels
    m_flags.m_all_wheels_on_ground = num_on_ground == num_wheels;
}

// 0x00B729D0 (:2228)
hkpRigidBody* automobile::get_supporting_rigid_body() const
{
    if (!a_rdd.is_loaded()) {
        return NULL;
    }
    const automobile::resource_dependent_data* a_data = a_rdd.get_data();
    for (int w = 0; w < (int)a_data->m_num_wheels; w++) {
        hkpRigidBody* contact = a_data->m_hk_vehicle_instance->m_wheelsInfo[w].m_contactBody;
        if (contact != NULL) {
            return contact;
        }
    }
    return NULL;
}

// 0x00B72A10 (:2250)
unsigned int automobile::find_supporting_rfg_mover() const
{
    if (!a_rdd.is_loaded()) {
        return NO_HANDLE;
    }
    const automobile::resource_dependent_data* a_data = a_rdd.get_data();
    for (int w = 0; w < (int)a_data->m_num_wheels; w++) {
        hkpRigidBody* contact = a_data->m_hk_vehicle_instance->m_wheelsInfo[w].m_contactBody;
        if (contact != NULL) {
            object* obj = object_from_world_obj(contact);
            // Original: only the subtype is tested, which other object types can share.
            if (obj != NULL && obj->get_sub_type() == OT_SUB_MOVER_RFG) {
                return obj->get_handle();
            }
        }
    }
    return NO_HANDLE;
}

// 0x00B88B50 (:2277)
void automobile::engine_start(bool player)
{
    if (this == NULL || m_flags.m_engine_running == 0) {
        vehicle::engine_start(player);
        if (a_rdd.is_loaded()) {
            a_rdd.get_data()->m_hk_vehicle_instance->set_is_engine_running(true);
        }
    }
    else if (player && audiolib_vehicle_engine_is_playing(engine_inst)) {
        audiolib_vehicle_engine_audio_to_2d(engine_inst);
    }
}

// 0x00B88BC0 (:2301)
void automobile::engine_stop(bool player, bool unused)
{
    automobile_release_horn(get_handle());
    if (m_flags.m_engine_running != 0) {
        vehicle::engine_stop(player, false);
        if (a_rdd.is_loaded()) {
            a_rdd.get_data()->m_hk_vehicle_instance->set_is_engine_running(false);
        }
    }
}

// 0x00B95DC0 (:2330)
void automobile_steer(automobile* ap, float steering_input_x, float steering_input_y)
{
    if (!ap->a_rdd.is_loaded()) {
        return;
    }
    // Squared, keeping the sign.
    const float sign = signbit(steering_input_x) ? -1.0f : 1.0f;
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(ap->get_handle());
    if (instance != NULL) {
        instance->set_steering_wheel_input(steering_input_x * steering_input_x * sign);
    }
    automobile_steer_air_control(ap, steering_input_x, steering_input_y);
}

// 0x00B95E80 (:2360)
void automobile_clear_accelerator(automobile* ap)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_forward_or_reverse_accelerator(0.0f);
        ap->user_acc_input = 0.0f;
    }
}

// 0x00B95EB0 (:2381)
void automobile_set_accelerator(automobile* ap)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_forward_or_reverse_accelerator(1.0f);
        ap->user_acc_input = 1.0f;
    }
}

// 0x00B95EF0 (:2401)
void automobile_set_brake_reverse(automobile* ap)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_forward_or_reverse_accelerator(-1.0f);
        ap->user_acc_input = -1.0f;
    }
}

// 0x00B95F30 (:2423)
void automobile_set_forward_or_reverse_accelerator(unsigned int a_handle, float input)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    instance->set_forward_or_reverse_accelerator(input);
    if ((ap->m_flags.m_on_fire != 0 && ap->m_flags.m_invulnerable == 0) || ap->m_flags.disable_accelerator != 0) {
        input = 0.0f;
        instance->set_accelerator_pedal_input(0.0f);
    }
    ap->user_acc_input = input;
}

// 0x00B72A80 (:2459)
void automobile_set_brake_pedal(automobile* ap)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_brake_pedal_input(1.0f);
    }
}

// 0x00B72B20 (:2475)
void automobile_set_brake_pedal(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap != NULL) {
        automobile_set_brake_pedal(ap);
    }
}

// 0x00B72B50 (:2491)
void automobile_set_handbrake(automobile* ap, bool applied)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_handbrake_on(applied);
    }
}

// 0x00B72BA0 (:2509)
void automobile_set_handbrake(unsigned int a_handle, bool applied)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    if (instance != NULL) {
        instance->set_handbrake_on(applied);
    }
}

// 0x00B88C10 (:2534)
void automobile_set_parking(automobile* ap, bool applied)
{
    // Original bug kept: not checked for being loaded.
    ap->a_rdd.get_data()->m_hk_vehicle_instance->set_parking(applied);
}

// 0x00B88C40 (:2551)
void automobile_set_steering(unsigned int a_handle, float input)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    if (instance != NULL) {
        instance->set_steering_wheel_input(input);
    }
}

// 0x00B72BE0 (:2574)
void automobile_set_brake_pedal_input(unsigned int a_handle, float input)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    if (instance != NULL) {
        instance->set_brake_pedal_input(input);
    }
}

// 0x00B72C30 (:2598)
float automobile_get_accelerator_value(unsigned int a_handle)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    return instance != NULL ? instance->get_accelerator_pedal_input() : 0.0f;
}

// 0x00B72C70 (:2622)
float automobile_get_brake_value(unsigned int a_handle)
{
    // Original bug kept: a handle that is no automobile reads through a null pointer.
    automobile* ap = automobile_from_handle(a_handle);
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    return ap->a_rdd.get_data()->m_hk_vehicle_instance->get_brake_pedal_input();
}

// 0x00B88C90 (:2636)
bool automobile_has_handbrake(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    return ap != NULL && automobile_has_handbrake(ap);
}

// 0x00B72CB0 (:2644)
bool automobile_has_handbrake(const automobile* ap)
{
    if (ap == NULL) {
        return false;
    }
    if (!ap->a_rdd.is_loaded()) {
        return automobile_has_handbrake(ap->info);
    }
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const hkpVehicleDefaultBrake* brake = static_cast<const hkpVehicleDefaultBrake*>(a_data->m_hk_vehicle_instance->m_brake);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        if (brake->m_wheelBrakingProperties[w].m_isConnectedToHandbrake) {
            return true;
        }
    }
    return false;
}

// 0x00B636A0 (:2669)
bool automobile_has_handbrake(const vehicle_info* info)
{
    if (info != NULL) {
        for (unsigned int a = 0; a < info->m_num_axles; a++) {
            if (info->m_axle_wheel_infos[a].does_handbrake) {
                return true;
            }
        }
    }
    return false;
}

// 0x00B72D10 (:2689)
bool automobile_get_handbrake_on(automobile* ap)
{
    if (ap != NULL && ap->a_rdd.is_loaded()) {
        return ap->a_rdd.get_data()->m_hk_vehicle_instance->get_handbrake_on();
    }
    return false;
}

// 0x00B88CC0 (:2709)
bool automobile_get_parking(automobile* ap)
{
    // Original bug kept: not checked for being loaded.
    return ap->a_rdd.get_data()->m_hk_vehicle_instance->get_parking();
}

// 0x00B72D40 (:2726)
void automobile_get_steering_info(unsigned int a_handle, float& max_angle, float& speed_for_max_angle)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap != NULL) {
        max_angle = ap->info->max_steering_angle;
        speed_for_max_angle = ap->info->max_speed_steering_angle;
    }
}

// 0x00B72D90 (:2744)
void automobile_set_trying_to_reverse(unsigned int a_handle, bool trying_to_revese)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    if (instance != NULL) {
        instance->set_trying_to_reverse(trying_to_revese);
    }
}

// 0x00B72E00 (:2768)
int automobile_get_mph(const automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0;
    }
    return (int)ap->a_rdd.get_data()->m_hk_vehicle_instance->calcMPH();
}

// 0x00B72E30 (:2786)
int automobile_get_mph(unsigned int a_handle)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    return instance != NULL ? (int)instance->calcMPH() : 0;
}

// 0x00B72E80 (:2803)
float automobile_get_speed_ms(const automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    return ap->a_rdd.get_data()->m_hk_vehicle_instance->calcKMPH() / KMPH_PER_MS;
}

// 0x00B72EB0 (:2820)
float automobile_get_speed_ms(unsigned int a_handle)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    return instance != NULL ? instance->calcKMPH() / KMPH_PER_MS : 0.0f;
}

// 0x00B88D40 (:2830)
float automobile_get_speed_squared(const automobile* ap)
{
    if (ap->a_rdd.is_loaded()) {
        hkpRigidBody* body = havok_body_get_pointer(ap->havok_handle);
        if (body != NULL) {
            vector velocity;
            havok_body_get_linear_velocity(body, velocity);
            return velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z;
        }
    }
    return 0.0f;
}

// 0x00B88D90 (:2845)
float automobile_get_speed_squared(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    return ap != NULL ? automobile_get_speed_squared(ap) : 0.0f;
}

// 0x00B72F10 (:2863)
float automobile_get_steering_t(const automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    const hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    const float max_angle = static_cast<const hkpVehicleDefaultSteering*>(instance->m_steering)->m_maxSteeringAngle;
    const float t = fl_parameter(instance->m_mainSteeringAngle, max_angle, -max_angle, true);
    return (t + t) - 1.0f;
}

// 0x00B72F70 (:2883)
float automobile_get_steering_t(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    return ap != NULL ? automobile_get_steering_t(ap) : 0.0f;
}

// 0x00B73000 (:2901)
bool automobile_moving_in_reverse(const automobile* ap)
{
    if (ap->a_rdd.is_loaded() && ap->a_rdd.get_data()->m_hk_vehicle_instance != NULL) {
        return ap->a_rdd.get_data()->m_hk_vehicle_instance->m_tryingToReverse;
    }
    return false;
}

// 0x00B73030 (:2925)
bool automobile_moving_in_reverse(unsigned int a_handle)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    return instance != NULL && instance->m_tryingToReverse;
}

// 0x00B73080 (:2943)
unsigned int automobile_get_current_gear(const automobile* ap)
{
    if (ap->a_rdd.is_loaded() && !ap->a_rdd.get_data()->m_hk_vehicle_instance->m_tryingToReverse) {
        return ap->a_rdd.get_data()->m_hk_vehicle_instance->m_currentGear + 1;
    }
    return 0;
}

// 0x00B730B0 (:2965)
unsigned int automobile_get_desired_gear(const automobile* ap)
{
    if (ap->a_rdd.is_loaded() && !ap->a_rdd.get_data()->m_hk_vehicle_instance->m_tryingToReverse) {
        return static_cast<const hk_custom_vehicle_transmission*>(ap->a_rdd.get_data()->m_hk_vehicle_instance->m_transmission)->getDesiredGear() + 1;
    }
    return 0;
}

// 0x00B730E0 (:2988)
float automobile_get_engine_rpm(const automobile* ap)
{
    return ap->a_rdd.is_loaded() ? ap->a_rdd.get_data()->m_hk_vehicle_instance->m_rpm : 0.0f;
}

// 0x00B73100 (:2999)
float automobile_get_current_max_rpm(const automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    const hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    const float max_rpm = static_cast<const hkpVehicleDefaultEngine*>(instance->m_engine)->m_maxRPM;
    if (!instance->m_tryingToReverse) {
        const hk_custom_vehicle_transmission* transmission = static_cast<const hk_custom_vehicle_transmission*>(instance->m_transmission);
        if (instance->m_currentGear < transmission->m_upshift_rpms.getSize()) {
            const float upshift = transmission->m_upshift_rpms[instance->m_currentGear];
            return 0.0f <= upshift - max_rpm ? max_rpm : upshift;
        }
    }
    return max_rpm;
}

// 0x00B73170 (:3029)
float automobile_get_gear_ratio(const automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    const hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    const hkpVehicleDefaultTransmission* transmission = static_cast<const hkpVehicleDefaultTransmission*>(instance->m_transmission);
    return instance->m_tryingToReverse ? transmission->m_reverseGearRatio : transmission->m_gearsRatio[instance->m_currentGear];
}

// 0x00B731B0 (:3050)
float automobile_get_engine_torque(const automobile* ap)
{
    return ap->a_rdd.is_loaded() ? ap->a_rdd.get_data()->m_hk_vehicle_instance->m_torque : 0.0f;
}

// 0x00B731D0 (:3067)
float automobile_get_engine_horsepower(const automobile* ap)
{
    const float torque = automobile_get_engine_torque(ap);
    const float rpm = automobile_get_engine_rpm(ap);
    return (rpm * torque * 0.7375621f) / 5252.0f; // newton metres to pound feet; hp = lb-ft * rpm / 5252
}

// 0x00B73250 (:3077)
void automobile_set_torque_multiplier(automobile* ap, float torque_multiplier)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_ai_torque_multiplier(torque_multiplier);
    }
}

// 0x00B73280 (:3088)
void automobile_set_powerslide_torque_multiplier(automobile* ap, float torque_multiplier)
{
    if (ap != NULL && ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_powerslide_torque_multiplier(torque_multiplier);
    }
}

// 0x00B732B0 (:3101)
void automobile_set_top_speed_increase(automobile* ap, float speed_increase)
{
    if (ap->a_rdd.is_loaded()) {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_ai_top_speed_adjustment(speed_increase);
    }
}

// 0x00B732E0 (:3118)
float automobile_get_wheel_radius(const automobile* ap, unsigned int wheel_index)
{
    if (!ap->a_rdd.is_loaded()) {
        return 0.0f;
    }
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    return a_data->m_wheel_radius_per_axle[a_data->m_wheel_axle_ids[wheel_index]];
}

// 0x00B636E0 (:3142)
bool automobile_are_wheels_on_ground(const automobile* ap)
{
    return ap->m_flags.m_wheels_on_ground != 0;
}

// 0x00B73310 (:3153)
bool automobile_are_wheels_on_ground(int a_handle)
{
    // Original bug kept: a handle that is no automobile reads through a null pointer.
    return automobile_from_handle((unsigned int)a_handle)->m_flags.m_wheels_on_ground != 0;
}

// 0x00B73350 (:3166)
bool automobile_any_wheel_on_ground(const automobile* ap)
{
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return false;
    }
    const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        // 0x20000: a component flag, read here as the wheel being gone (meaning inferred)
        if ((a_data->m_wheel_components[w]->get_flags() & 0x20000) == 0 && a_data->m_hk_vehicle_instance->get_physical_material_in_contact_with_wheel(w) != 0xFF) {
            return true;
        }
    }
    return false;
}

// 0x00B733C0 (:3204)
bool automobile_any_wheel_on_ground(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    return ap != NULL && automobile_any_wheel_on_ground(ap);
}

// 0x00B733F0 (:3223)
void automobile_reset_handling_for_ai(automobile* ap, bool ai_vehicle, bool is_driver)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    hk_custom_vehicle_raycast_wheel_collide* wheel_collide = static_cast<hk_custom_vehicle_raycast_wheel_collide*>(instance->m_wheelCollide);
    if (ai_vehicle) {
        wheel_collide->setup_for_nonplayer();
    }
    else {
        wheel_collide->setup_for_player();
    }
    const bool ai_handling = ai_vehicle || !is_driver;
    if (instance->get_ai_controlled() == ai_handling) {
        return;
    }
    instance->set_ai_controlled(ai_handling);
    const vehicle_info* info = ap->info;
    hkpVehicleData* data = instance->m_data;
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const vehicle_info::axle_wheel_info& axle = info->m_axle_wheel_infos[a_data->m_wheel_axle_ids[w]];
        data->m_wheelParams[w].m_friction = ai_handling ? axle.ai_friction : axle.friction;
    }
    hkpVehicleDefaultSteering* steering = static_cast<hkpVehicleDefaultSteering*>(instance->m_steering);
    hkpVehicleDefaultBrake* brake = static_cast<hkpVehicleDefaultBrake*>(instance->m_brake);
    hkpVehicleDefaultVelocityDamper* damper = static_cast<hkpVehicleDefaultVelocityDamper*>(instance->m_velocityDamper);
    hkpVehicleDefaultEngine* engine = static_cast<hkpVehicleDefaultEngine*>(instance->m_engine);
    // m_objectQualityType of the chassis' broad phase handle, as a byte at 0x2A.
    unsigned char* quality_type = reinterpret_cast<unsigned char*>(instance->m_entity) + 0x2A;
    float enforced_max_speed;
    if (ai_handling) {
        data->m_chassisUnitInertiaYaw = info->m_ai_chassis_unit_inertia_yaw;
        data->m_frictionEqualizer = info->m_ai_friction_equalizer;
        steering->m_maxSpeedFullSteeringAngle = info->ai_max_speed_steering_angle;
        brake->m_wheelsMinTimeToBlock = info->ai_min_time_to_block;
        damper->m_normalSpinDamping = info->m_normal_spin_damping_ai;
        if (Vehicle_continuous_physics) {
            *quality_type = 2;
        }
        ap->set_extra_mass(false);
        ap->set_extra_mass_value(1.0f);
        enforced_max_speed = info->ai_enforced_max_speed;
        engine->m_maxTorque = info->ai_engine_torque;
    }
    else {
        data->m_chassisUnitInertiaYaw = info->m_chassis_unit_inertia_yaw;
        data->m_frictionEqualizer = info->m_friction_equalizer;
        steering->m_maxSpeedFullSteeringAngle = info->max_speed_steering_angle;
        brake->m_wheelsMinTimeToBlock = info->min_time_to_block;
        damper->m_normalSpinDamping = info->m_normal_spin_damping;
        if (Vehicle_continuous_physics) {
            *quality_type = 4;
        }
        ap->set_extra_mass(true);
        ap->set_extra_mass_value(0.0f <= 1.0f - info->m_collision_mass_scalar ? 1.0f : info->m_collision_mass_scalar);
        enforced_max_speed = info->enforced_max_speed;
        engine->m_maxTorque = info->engine_torque;
    }
    instance->set_enforced_max_speed(enforced_max_speed, info->enforced_max_rspeed);
    instance->reinitialize_vehicle_data();
}

// 0x00B88DC0 (:3349)
void automobile_calculate_wheel_hardpoint(vector* wheel_hardpoint, const vehicle_component* p_component, const automobile* ap)
{
    unsigned int wheel_index = 0xFFFFFFFF;
    if (ap->a_rdd.is_loaded()) {
        const automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
        for (unsigned int w = 0; w < 16; w++) {
            if (p_component == a_data->m_wheel_components[w]) {
                wheel_index = w;
                break;
            }
        }
    }
    const vehicle_component_data* data = p_component->get_data();
    *wheel_hardpoint = data->m_position;
    float compression = 0.125f; // not loaded
    if (ap->a_rdd.is_loaded()) {
        // Original bug kept: a component that is no wheel indexes with -1.
        compression = ap->info->m_axle_wheel_infos[ap->a_rdd.get_data()->m_wheel_axle_ids[wheel_index]].spring_compression;
    }
    wheel_hardpoint->y += compression;
    if ((p_component->get_flags() & 8) != 0) {
        float t = (float)p_component->m_pos_orient_deform_weight * ONE_OVER_255;
        if (t - 0.0f < 0.0f) {
            t = 0.0f;
        }
        if (0.0f <= t - 1.0f) {
            t = 1.0f;
        }
        wheel_hardpoint->x += data->m_deformed_position_offset.x * t;
        wheel_hardpoint->y += data->m_deformed_position_offset.y * t;
        wheel_hardpoint->z += data->m_deformed_position_offset.z * t;
    }
}

// 0x00B88F60 (:3382)
void automobile_set_controls_when_abandoned(automobile* ap)
{
    if (!ap->a_rdd.is_loaded()) {
        return;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    if (ap->m_flags.m_scripted_bail != 0) {
        // Rolls on with the horn blaring.
        instance->set_accelerator_pedal_input(1.0f);
        instance->set_steering_wheel_input(0.0f);
        instance->set_handbrake_on(false);
        instance->set_brake_pedal_input(0.0f);
        instance->set_parking(false);
        automobile_honk_horn(ap->get_handle());
        return;
    }
    if (object_is_player(ap->most_recent_driver)) {
        instance->set_accelerator_pedal_input(0.0f);
        instance->set_steering_wheel_input(0.0f);
        if (ap->m_flags.m_engine_running != 0 && ap->m_flags.m_hijacked == 0 && 0.01f < fabsf(ap->forward_velocity) && 0.01f < fabsf(ap->last_forward_velocity)) {
            instance->set_handbrake_on(false);
            instance->set_brake_pedal_input(0.0f);
            instance->set_parking(false);
            return;
        }
        instance->set_handbrake_on(true);
        instance->set_brake_pedal_input(1.0f);
        instance->set_parking(true);
    }
    else {
        instance->set_handbrake_on(true);
        instance->set_steering_wheel_input(0.0f);
        instance->set_brake_pedal_input(1.0f);
        instance->set_accelerator_pedal_input(0.0f);
        instance->set_parking(true);
        // A living driver still in the seat keeps the horn and the engine.
        human* driver = automobile_current_driver(ap);
        if (driver != NULL && !driver->is_killed()) {
            return;
        }
        automobile_release_horn(ap->get_handle());
    }
    if (ap->m_flags.m_engine_running != 0) {
        ap->engine_stop(false, false);
    }
}

// 0x00B89250 (:3470)
void automobile_set_controls_when_abandoned(unsigned int a_handle)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap != NULL) {
        automobile_set_controls_when_abandoned(ap);
    }
}

// 0x00B73690 (:3484)
hk_custom_vehicle_instance* automobile_get_raycast_vehicle(unsigned int a_handle)
{
    // Original bug kept: a handle that is no automobile reads through a null pointer.
    automobile* ap = automobile_from_handle(a_handle);
    return ap->a_rdd.is_loaded() ? ap->a_rdd.get_data()->m_hk_vehicle_instance : NULL;
}

// 0x00B95FD0 (:3504)
void automobile_set_vcontrols(unsigned int a_handle, const vehicle_controls& v_controls)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return;
    }
    if (v_controls.honk && !vehicle_get_honked_horn(a_handle)) {
        automobile_honk_horn(a_handle);
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    instance->set_handbrake_on(v_controls.handbrake);
    instance->set_forward_or_reverse_accelerator(v_controls.brake_reverse ? -v_controls.accelerate : v_controls.accelerate);
    automobile_steer(ap, v_controls.steering, 0.0f);
}

// 0x00B89280 (:3534)
void automobile_get_vcontrols(unsigned int a_handle, vehicle_controls* v_controls)
{
    automobile* ap = automobile_from_handle(a_handle);
    if (ap == NULL || !ap->a_rdd.is_loaded()) {
        return;
    }
    v_controls->honk = vehicle_get_honked_horn(a_handle);
    v_controls->steering = automobile_get_steering_t(a_handle);
    v_controls->accelerate = automobile_get_accelerator_value(a_handle);
    const hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    v_controls->brake_reverse = instance->m_tryingToReverse;
    v_controls->handbrake = instance->get_handbrake_on();
}

// 0x00B89340 (:3556)
void automobile_update_orient_pos_from_havok(automobile* ap)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    const hkpVehicleSuspension* suspension = instance->m_suspension;
    const hkpRigidBody* chassis = havok_body_get_pointer(ap->havok_handle);
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        vehicle_component* component = a_data->m_wheel_components[w];
        hkVector4f hk_position;
        hkQuaternionf hk_rotation;
        calcHk550CurrentPositionAndRotation(instance, chassis, suspension, w, hk_position, hk_rotation);
        vector position;
        quaternion rotation;
        havok_to_game_vector(position, hk_position);
        havok_to_game_quaternion(rotation, hk_rotation);
        if (Vehicle_wheel_stutter && Vehicle_wheel_stutter_minspeed <= fabsf(instance->m_wheelsInfo[w].m_spinVelocity)) {
            // Fast wheels get a random spin, so that they do not strobe.
            matrix stutter_matrix(IDENTITY_MATRIX);
            stutter_matrix.set(stutter_matrix.rvec, fl_rand_range(-Vehicle_wheel_stutter_offset, Vehicle_wheel_stutter_offset));
            quaternion stutter;
            stutter.set(stutter_matrix);
            rotation = stutter * rotation;
        }
        if ((component->get_flags() & 0x10) != 0 && component->m_pos_orient_deform_weight != 0) {
            float t = (float)component->m_pos_orient_deform_weight * ONE_OVER_255;
            if (t - 0.0f < 0.0f) {
                t = 0.0f;
            }
            if (0.0f <= t - 1.0f) {
                t = 1.0f;
            }
            quaternion deform;
            quat_lerp(deform, IDENTITY_QUATERNION, component->get_data()->m_deformed_rotation_offset, t);
            rotation = deform * rotation;
        }
        matrix orient;
        rotation.extract_matrix(orient);
        component->set_orient_pos_in_world_space(ap, orient, position);
    }
}

// 0x00B9C190 (:3622)
void automobile_do_frame_presim(automobile* ap)
{
    if (Multiplayer) {
        multi_game_auto_predict_collision(ap);
        if (Multiplayer && multi_is_server() && multi_game_object_vehicle_automobile_is_client_authoritative(ap)) {
            multi_automobile_state state;
            bool exceeded_extrap_time;
            if (ap->mp_sim_state->obtain_state(multi_core_clock_get_time(), &state, exceeded_extrap_time)) {
                multi_game_object_vehicle_automobile_apply_sim_state(&state, ap);
            }
        }
    }
    if (!ap->a_rdd.is_loaded()) {
        return;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;

    // The speed cap.
    hkpRigidBody* body = havok_body_get_pointer(ap->havok_handle);
    const hkVector4f& hk_velocity = body->getLinearVelocity();
    const float max_speed = instance->get_ai_top_speed_adjustment() + VEHICLE_MAX_SPEED;
    const float vx = hk_velocity(0);
    const float vy = hk_velocity(1);
    const float vz = hk_velocity(2);
    const float speed_squared = vy * vy + vx * vx + vz * vz;
    const float speed = 0.0f < speed_squared ? sqrtf(speed_squared) : 0.0f;
    if (max_speed < speed) {
        const float scale = max_speed / speed;
        hkVector4f capped;
        capped.set(vx * scale, vy * scale, vz * scale, hk_velocity(3) * scale);
        body->activate();
        body->getMotion()->setLinearVelocity(capped);
    }

    if (Vehicle_door_collision_threshold_mph < automobile_get_mph(ap)) {
        if (ap->m_flags.m_door_collision_enabled == 0) {
            vehicle_enable_human_door_collisions(ap, true);
            ap->m_flags.m_door_collision_enabled = 1;
        }
    }
    else if (ap->m_flags.m_door_collision_enabled != 0) {
        vehicle_enable_human_door_collisions(ap, false);
        ap->m_flags.m_door_collision_enabled = 0;
    }

    human* driver = automobile_current_driver(ap);
    if (driver == NULL) {
        driver = vehicle_passenger_entering_seat(ap, VSI_DRIVER);
    }

    // Force assist: a push to get a heavy vehicle moving, ramping towards its target.
    const float forward_speed = fabsf(ap->forward_velocity);
    float target = -0.35f; // falls away quickly when not needed
    if (0.9f < automobile_get_accelerator_value(ap->get_handle()) && forward_speed < Vehicle_force_assist_max_speed) {
        target = (Vehicle_force_assist_max_speed - forward_speed) / Vehicle_force_assist_max_speed;
    }
    const float difference = target - ap->force_assist_value;
    const float magnitude = fabsf(difference);
    const float ramp = Frametime * Vehicle_force_assist_ramp;
    const float change = 0.0f <= ramp - magnitude ? magnitude : ramp;
    ap->force_assist_value = (difference < 0.0f ? -1.0f : 1.0f) * change + ap->force_assist_value;
    if (0.05f < ap->force_assist_value) {
        const float direction = instance->m_tryingToReverse ? -1.0f : 1.0f;
        const float impulse = ap->info->force_assist * ap->info->chassis_mass * direction * Frametime * ap->force_assist_value;
        const vector flat_forward = ap->orient.fvec.copy_flatten_normalize_safe();
        const vector push(flat_forward.x * impulse, flat_forward.y * impulse, flat_forward.z * impulse);
        havok_body_apply_linear_impulse(ap->havok_handle, push);
    }

    if (driver == NULL || (!object_is_local_player(driver->get_handle()) && !object_is_player(driver->get_handle()))) {
        vehicle_ai_do_frame(ap);
    }
    automobile_audio_horn_update(ap);
    automobile_audio_siren_update(ap, driver);
    if (audiolib_is_playing(ap->foley_chassis, true)) {
        audiolib_play_set_3d_position(ap->foley_chassis, &ap->pos, NULL, NULL);
    }
}

// Swing arms follow their wheel. 0x00BA68A0 (:3783), part.
static void automobile_update_swing_arm(automobile* ap, vehicle_component* arm, bool update_wheels)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_component::suspension_attr& attr = arm->m_type_attribute.suspension;
    vehicle_component* wheel = a_data->m_wheel_components[attr.wheel_index];
    const vector bias(attr.wheel_bias[0], attr.wheel_bias[1], attr.wheel_bias[2]);
    matrix rotation(arm->get_data()->m_rotation);

    vector wheel_local = wheel->get_world_space_position() - ap->pos;
    wheel_local.rotate(ap->orient);
    wheel_local.x = (wheel_local.x + bias.x) - arm->get_data()->m_position.x;
    wheel_local.y = (bias.y + wheel_local.y) - arm->get_data()->m_position.y;
    wheel_local.z = (wheel_local.z + bias.z) - arm->get_data()->m_position.z;
    wheel_local.rotate(rotation);

    const float length = attr.pivot_pos_mag;
    const float rest_angle = (float)asin(attr.pivot_pos[1] / length);
    float height = -length;
    if (0.0f <= wheel_local.y - -length) {
        height = wheel_local.y;
    }
    if (0.0f <= height - length) {
        height = length;
    }
    const float swing = (float)asin(height / length) - rest_angle;
    rotation.rotate_around_local_x(-swing);
    matrix world_rotation(rotation);
    world_rotation.unrotate(ap->orient);
    arm->set_orient_pos_in_world_space(ap, world_rotation, arm->get_world_space_position());

    if (update_wheels) {
        hkVector4f& hk_hardpoint = a_data->m_hk_vehicle_instance->m_suspension->m_wheelParams[attr.wheel_index].m_hardpointChassisSpace;
        vector hardpoint;
        havok_to_game_vector(hardpoint, hk_hardpoint);
        vector pivot(attr.pivot_pos[0], attr.pivot_pos[1], attr.pivot_pos[2]);
        pivot.unrotate(rotation);
        pivot.x -= bias.x;
        pivot.y -= bias.y;
        pivot.z -= bias.z;
        const vector new_hardpoint(arm->get_chassis_space_position().x + pivot.x, hardpoint.y, arm->get_chassis_space_position().z + pivot.z);
        game_to_havok_vector(hk_hardpoint, new_hardpoint);
        vector moved = new_hardpoint - hardpoint;
        moved.unrotate(ap->orient);
        const vector wheel_pos(moved.x + wheel->get_world_space_position().x, wheel->get_world_space_position().y + moved.y, wheel->get_world_space_position().z + moved.z);
        wheel->set_orient_pos_in_world_space(ap, wheel->get_world_space_rotation(), wheel_pos);
    }
}

// Wheel hubs and steering joints hang off their wheel. 0x00BA68A0 (:3783), part.
static void automobile_update_wheel_attachment(automobile* ap, vehicle_component* comp)
{
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    const vehicle_component::suspension_attr& attr = comp->m_type_attribute.suspension;
    vector offset(attr.pivot_pos[0], attr.pivot_pos[1], attr.pivot_pos[2]);
    offset.unrotate(comp->get_chassis_space_rotation());
    vector pos(a_data->m_wheel_components[attr.wheel_index]->get_world_space_position());
    matrix rotation;
    if (comp->get_type() == VCT_WHEEL_HUB) {
        // The chassis rotation times the wheel's steering, without the spin.
        const hkQuaternionf& steering = a_data->m_hk_vehicle_instance->m_wheelsInfo[attr.wheel_index].m_steeringOrientationChassisSpace;
        hkpRigidBody* body = havok_body_get_pointer(ap->havok_handle);
        const hkQuaternionf& body_rotation = body->getRotation();
        hkQuaternionf combined;
        combined.setMul(body_rotation, steering);
        quaternion game_rotation;
        havok_to_game_quaternion(game_rotation, combined);
        game_rotation.extract_matrix(rotation);
    }
    else {
        rotation = comp->get_world_space_rotation();
    }
    offset.unrotate(rotation);
    pos.y -= offset.y;
    pos.x -= offset.x;
    pos.z -= offset.z;
    comp->set_orient_pos_in_world_space(ap, rotation, pos);
}

// 0x00BA68A0 (:3783)
void automobile_do_frame_postsim(automobile* ap, bool update_wheels)
{
    if (!ap->a_rdd.is_loaded() || !ap->rdd.is_loaded()) {
        return;
    }
    automobile::resource_dependent_data* a_data = ap->a_rdd.get_data();
    vehicle::resource_dependent_data* v_data = ap->rdd.get_data();
    hk_custom_vehicle_instance* instance = a_data->m_hk_vehicle_instance;
    instance->set_is_infinite_mass(ap->get_infinite_mass());
    instance->set_is_extra_mass(ap->get_extra_mass());
    instance->set_extra_mass_val(ap->get_extra_mass_value());
    if (instance->get_accelerator_pedal_input() != 0.0f || instance->get_steering_wheel_input() != 0.0f) {
        havok_body_get_pointer(ap->havok_handle)->activate();
    }

    player* local_player = player_get_local_player();
    if (ap->get_handle() == local_player->vehicle_handle) {
        switch (camera_free_get_submode()) {
        case FREE_CAMERA_SUBMODE_INTERIOR:
        case FREE_CAMERA_SUBMODE_INTERIOR_COMBAT:
        case FREE_CAMERA_SUBMODE_INTERIOR_CLOSE:
        case FREE_CAMERA_SUBMODE_EXTERIOR:
        case FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH:
        case FREE_CAMERA_SUBMODE_EXTERIOR_STAND:
        case FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG:
        case FREE_CAMERA_SUBMODE_ZOOM:
        case FREE_CAMERA_SUBMODE_SPECTATOR:
            break;
        default:
            camera_free_set_swing_rate(automobile_moving_in_reverse(ap) ? FREE_CAMERA_SWING_RATE_VEHICLE_REVERSE : FREE_CAMERA_SWING_RATE_VEHICLE_FORWARD);
            break;
        }
        ap->m_flags.m_reverse_last_frame = automobile_moving_in_reverse(ap) ? 1 : 0;
    }

    // A stopped vehicle left tipped over catches fire after a while.
    vector velocity;
    vehicle_get_velocity(ap, velocity);
    unsigned int patrol_vehicle = NO_HANDLE;
    vrail_get_patrol_vehicle(patrol_vehicle);
    if (ap->m_flags.m_on_fire != 0
        || (human_in_vehicle(local_player, ap) && ap->get_handle() != patrol_vehicle)
        || 0.25f <= velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z
        || 0.25f <= ap->orient.uvec.y
        || ap->m_flags.m_invulnerable != 0
        || local_player->last_vehicle_driven == ap->get_handle()
        || ap->m_flags.convoy_vehicle != 0) {
        ap->catch_fire_timestamp.invalidate();
    }
    else if (!ap->catch_fire_timestamp.valid()) {
        ap->catch_fire_timestamp.set(CATCH_FIRE_DELAY_MS);
    }
    if (ap->catch_fire_timestamp.elapsed()) {
        vehicle_catch_fire(ap, false);
    }

    if (0.2f < automobile_get_brake_value(ap->get_handle())) {
        ap->brake_effect_release_timer.set(object_is_player(automobile_current_driver(ap)) ? 100 : 500); // milliseconds
    }
    if (ap->brake_effect_release_timer.elapsed()) {
        ap->brake_effect_release_timer.invalidate();
    }

    if (!havok_body_get_pointer(ap->havok_handle)->isActive() && ap->m_flags.fixed_motion == 0) {
        automobile_stop_all_scraping_effects(ap);
        return;
    }

    // Projectiles stuck in the wheels fly off at speed.
    float speed;
    vehicle_get_speed(ap, speed);
    if (Automobile_wheel_projectile_detach_speed < fabsf(speed)) {
        constraint* cp = ap->host_constraint_ptr;
        while (cp != NULL) {
            constraint* next = cp->host_constraint_next != ap->host_constraint_ptr ? cp->host_constraint_next : NULL;
            projectile* proj = World.object_from_handle<projectile>(cp->child_handle);
            // 0x2F: the wheel collision layer
            if (proj != NULL && cp->host_attach_index != NO_HANDLE && (vehicle_get_collision_filter(ap, cp->host_alt_hk_body_index, cp->host_attach_index) & 0xFFFF) == 0x2F) {
                World.destroy_object(cp, false);
                proj->detach();
            }
            cp = next;
        }
    }

    if (update_wheels) {
        automobile_update_orient_pos_from_havok(ap);
        ap->check_wheel_contact_with_ground();
    }

    // Landing friction: the chassis is slippery just after a jump.
    float friction_scale = 1.0f;
    if (automobile_any_wheel_on_ground(ap)) {
        if (ap->is_airborne.elapsed()) {
            ap->is_landing.set(Automobile_landing_friction_full_damp_time);
        }
        ap->is_airborne.invalidate();
    }
    else {
        if (!ap->is_airborne.valid()) {
            ap->is_airborne.set(Automobile_landing_friction_min_airborne_time);
        }
        ap->is_landing.invalidate();
    }
    if (ap->is_landing.valid()) {
        friction_scale = Automobile_landing_friction_full_damp_ratio;
        if (ap->is_landing.elapsed()) {
            if (Automobile_landing_friction_ramp_damp_time < ap->is_landing.time_since()) {
                ap->is_landing.invalidate();
                friction_scale = 1.0f;
            }
            else {
                friction_scale = (1.0f - Automobile_landing_friction_full_damp_ratio) * ((float)ap->is_landing.time_since() / (float)Automobile_landing_friction_ramp_damp_time) + Automobile_landing_friction_full_damp_ratio;
            }
        }
    }
    // And less on a steep slope. Up vector y: 0.342 is cos 70 degrees of tilt, 0.5 is cos 60.
    float slope_scale = 1.0f;
    if (ap->m_flags.m_wheels_on_ground != 0) {
        slope_scale = fl_parameter(ap->orient.uvec.y, 0.342f, 0.5f, 0.1f, 1.0f, true);
    }
    if (0.0f <= friction_scale - slope_scale) {
        friction_scale = slope_scale;
    }
    havok_body_get_pointer(ap->havok_handle)->setFriction(Vehicle_body_friction * friction_scale);

    automobile_process_powerslide(ap);

    hkpVehicleData* data = instance->m_data;
    for (unsigned int w = 0; w < a_data->m_num_wheels; w++) {
        const vehicle_info::axle_wheel_info& axle = ap->info->m_axle_wheel_infos[a_data->m_wheel_axle_ids[w]];
        if (!instance->get_ai_controlled()) {
            const float base = axle.friction * friction_scale;
            const float amount = axle.does_handbrake ? Powerslide_handbrake_axle_friction_amount : Powerslide_other_axle_friction_amount;
            data->m_wheelParams[w].m_friction = ((ap->current_powerslide_friction - 1.0f) * amount + 1.0f) * base;
        }
        else {
            data->m_wheelParams[w].m_friction = axle.ai_friction * friction_scale;
        }
    }

    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        vehicle_component* comp = &v_data->m_components[c];
        if (comp->get_type() == VCT_SWING_ARM && comp->m_type_attribute_valid) {
            automobile_update_swing_arm(ap, comp, update_wheels);
        }
    }
    if (update_wheels) {
        for (unsigned int c = 0; c < v_data->m_num_components; c++) {
            vehicle_component* comp = &v_data->m_components[c];
            if ((comp->get_type() == VCT_WHEEL_HUB || comp->get_type() == VCT_STEERING_JOINT) && comp->m_type_attribute_valid) {
                automobile_update_wheel_attachment(ap, comp);
            }
        }
    }

    automobile_collision_effects_update_pos_orient(ap);
    automobile_effects_handle_wheel_effects(ap);
    automobile_handle_chassis_sounds(ap);
}

// 0x00B63700 (:4235)
float automobile_get_game_max_vehicle_speed()
{
    return VEHICLE_MAX_SPEED;
}

// 0x00B736D0 (:4246)
bool automobile_is_stopped_and_not_upright(const automobile* ap)
{
    if (0.1f < automobile_get_speed_ms(ap)) {
        return false;
    }
    const vector& uvec = ap->orient.uvec;
    if (0.0f <= uvec.y * UP_VECTOR.y + uvec.x * UP_VECTOR.x + uvec.z * UP_VECTOR.z) {
        return false;
    }
    const automobile* other = automobile_from_handle(ap->vai.vehicle_handle);
    return other == NULL || !automobile_any_wheel_on_ground(other);
}

// 0x00B737B0 (:4262)
float automobile_get_vehicle_steering_input_raw(unsigned int a_handle)
{
    hk_custom_vehicle_instance* instance = automobile_instance_from_handle(a_handle);
    return instance != NULL ? instance->get_steering_wheel_input() : 0.0f;
}

// Stops one looping wheel foley, more slowly when reversing. 0x00B9C6A0 (:4281), part.
static void automobile_fade_wheel_foley(const automobile* ap, int& foley)
{
    if (foley == -1) {
        return;
    }
    audiolib_play_fade_out(foley, automobile_moving_in_reverse(ap) ? WHEEL_FOLEY_FADE_REVERSING_MS : WHEEL_FOLEY_FADE_MS);
    foley = -1;
}

// Stops one wheel effect. 0x00B9C6A0 (:4281), part.
static void automobile_stop_wheel_effect(unsigned int& effect)
{
    if (effect == NO_HANDLE) {
        return;
    }
    object* obj = World.object_from_handle(effect);
    if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
        static_cast<object_effect*>(obj)->stop(object_effect::SS_STOP_EMITTING);
    }
    effect = NO_HANDLE;
}

// 0x00B9C6A0 (:4281)
void automobile_effects_stop_wheel_effects(automobile* ap)
{
    if (ap->get_sub_type() != OT_SUB_VEHICLE_AUTO) {
        return;
    }
    for (int w = 0; w < 16; w++) {
        automobile::wheel_effect_info& effects = ap->m_wheel_effect_infos[w];
        automobile_stop_wheel_effect(effects.m_motion_effect);
        automobile_stop_wheel_effect(effects.m_skid_effect);
        automobile_fade_wheel_foley(ap, effects.m_skid_foley);
        automobile_stop_wheel_effect(effects.m_peel_effect);
        automobile_fade_wheel_foley(ap, effects.m_peel_foley);
        automobile_stop_wheel_effect(effects.m_brake_effect);
        automobile_fade_wheel_foley(ap, effects.m_brake_foley);
        if (0 < effects.m_track_decals.size() && effects.m_track_decals[0].m_decal_handle != NO_HANDLE) {
            automobile_tire_track_decal_start_fade(effects.m_track_decals[0].m_decal_handle);
        }
        effects.m_track_decals.remove_all();
    }
    automobile_fade_wheel_foley(ap, ap->m_motion_foley);
    const base_vehicle_types classification = ap->info->vehicle_classification;
    if (classification == BVT_TANK || classification == BVT_ART_TANK) {
        automobile_fade_wheel_foley(ap, ap->m_tread_foley);
    }
}

// 0x00B63710 (:4314)
bool automobile_is_powersliding(automobile* ap)
{
    return ap != NULL && 0.01f < ap->powerslide_amount;
}

// 0x00768CC0 (.h:285)
obj_auto_props::obj_auto_props(int type_index, const vector& pos, const matrix& orient, bool pos_at_ground)
    : obj_prop_block(pos, orient, NULL)
{
    set(VEHICLE_PROP_TYPE, type_index);
    set(VEHICLE_PROP_AT_GROUND, pos_at_ground);
}

// 0x00761CC0
obj_auto_props::~obj_auto_props()
{
}
