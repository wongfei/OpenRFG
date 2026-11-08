// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.cpp
//
// The game's Havok vehicle instance: hkpVehicleInstance plus the per-vehicle state the custom components read (driver
// input with the pedal and steering inputs, AI torque and speed adjustments, transmission rpm, flags such as tank,
// trailer, articulated or infinite mass, the wheel contact materials) and the extra downforce action. applyAction
// steps the vehicle by applyHk550Action, the Havok 5.5.0 step order the game kept; of the simulation
// (hkpVehicleDefaultSimulation) it reads only the friction description.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefriction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/vlib/code/time/timestamp.h"

class hkStepInfo;
class hkTransformf;
class hk_custom_vehicle_extra_downforce;
class hk_wheel_info_contact_body_listener;

// size 0x190
class hk_custom_vehicle_instance : public hkpVehicleInstance
{
public:
    // size 0x4
    struct steering_instance_data
    {
        float m_turn_velocity; // 0x00
    };

    // size 0x10
    struct engine_instance_data
    {
        float m_ai_torque_multiplier; // 0x00
        float m_ai_top_speed_adjustment; // 0x04
        float m_reverse_torque_multiplier; // 0x08
        float m_powerslide_torque_multiplier; // 0x0C
    };

    // size 0x14
    struct driver_input_instance_data
    {
        driver_input_instance_data(); // 0x00B63B20

        float m_accelerator_pedal_input; // 0x00
        float m_brake_pedal_input; // 0x04
        float m_steering_wheel_input; // 0x08
        bool m_handbrake_on; // 0x0C
        timestamp m_ignore_new_driver_input_timer; // 0x10
    };

    // size 0x4
    struct transmission_instance_data
    {
        float m_rpm; // 0x00
    };

    // size 0x50
    struct misc_instance_data
    {
        misc_instance_data(); // 0x00B63B30

        bool m_is_trailer : 1; // 0x00:0
        bool m_ai_controlled : 1; // 0x00:1
        bool m_is_tank : 1; // 0x00:2
        bool m_is_articulated : 1; // 0x00:3
        bool m_is_infinite_mass : 1; // 0x00:4
        bool m_is_trying_to_recover : 1; // 0x00:5
        bool m_is_engine_running : 1; // 0x00:6
        bool m_is_extra_mass : 1; // 0x00:7
        float m_extra_mass_val; // 0x04
        float m_auto_brake_speed; // 0x08
        float m_enforced_max_speed; // 0x0C
        float m_enforced_max_rspeed; // 0x10
        float m_original_yaw_factor; // 0x14
        float m_original_yaw_factor_scalar; // 0x18
        unsigned int m_forward_to_reverse_delay; // 0x1C
        unsigned int m_reverse_to_forward_delay; // 0x20
        timestamp f2r_ts; // 0x24
        timestamp r2f_ts; // 0x28
        hkVector4f tread_wheel_aabb_min; // 0x30
        hkVector4f tread_wheel_aabb_max; // 0x40
    };

    hk_custom_vehicle_instance(); // 0x00B9CF30 (:59)
    virtual ~hk_custom_vehicle_instance(); // vtbl+0x00 0x00B9D140 (:125)
    virtual void init(); // vtbl+0x24 0x00B9D240 (:143)
    virtual void reinitialize_vehicle_data(); // vtbl+0x44 0x00B63B40 (:226)
    virtual unsigned char get_physical_material_in_contact_with_wheel(unsigned int wheel_index); // vtbl+0x48 0x00B746C0 (:241)
    virtual unsigned char get_effect_material_in_contact_with_wheel(unsigned int wheel_index); // vtbl+0x4C 0x00B746D0 (:250)
    virtual void applyAction(const hkStepInfo& step_info); // vtbl+0x10 0x00BB1800 (:263)
    void set_accelerator_pedal_input(float input); // 0x00B71630 (.h:61)
    float get_accelerator_pedal_input() const; // 0x00B632F0 (.h:88)
    void set_brake_pedal_input(float input); // 0x00B716F0 (.h:97)
    float get_brake_pedal_input() const; // 0x00B63300 (.h:116)
    void set_forward_or_reverse_accelerator(float input); // 0x00B89EA0 (:951)
    void set_steering_wheel_input(float input); // 0x00B746E0 (:1155)
    float get_steering_wheel_input() const; // 0x00B63310 (.h:141)
    bool get_trying_to_reverse() const; // 0x00B63320 (.h:148)
    void set_handbrake_on(bool handbrake_on); // 0x00B71780 (.h:157)
    bool get_handbrake_on() const; // 0x00B63330 (.h:176)
    void set_parking(bool parking_on); // 0x00B749B0 (:1243)
    bool get_parking(); // 0x00B74A40 (:1270)
    void set_trying_to_reverse(bool trying_to_reverse); // 0x00B717D0 (.h:188)
    float get_wheel_spin_velocity(int wheel); // 0x00B71820 (.h:203)
    void set_inputs_due_to_crash(float steering_param); // 0x00B8A310 (:1295)
    void set_ai_controlled(bool ai_controlled); // 0x00B63BB0 (:1312)
    bool get_ai_controlled(); // 0x00B63BD0 (:1321)
    // Read directly by the custom components in rfg.exe (they are friends or inline the access; name inferred).
    float get_ai_torque_multiplier() const { return m_engine_instance_data.m_ai_torque_multiplier; }
    float get_reverse_torque_multiplier() const { return m_engine_instance_data.m_reverse_torque_multiplier; }
    float get_powerslide_torque_multiplier() const { return m_engine_instance_data.m_powerslide_torque_multiplier; }
    float get_original_yaw_factor() const { return m_misc_instance_data.m_original_yaw_factor; }
    float get_handbrake_yaw_factor_scalar_value() const { return m_misc_instance_data.m_original_yaw_factor_scalar; }
    float& get_turn_velocity() { return m_steering_instance_data.m_turn_velocity; }
    void set_transmission_rpm(float rpm) { m_transmission_instance_data.m_rpm = rpm; }
    void set_wheel_contact_materials(int wheel, unsigned char physical, unsigned char effect) { m_wheel_contact_p_material[wheel] = physical; m_wheel_contact_e_material[wheel] = effect; }
    void set_ai_torque_multiplier(float ai_torque_multiplier); // 0x00B63BE0 (:1331)
    void set_powerslide_torque_multiplier(float powerslide_torque_multiplier); // 0x00B63C00 (:1341)
    void set_ai_top_speed_adjustment(float ai_top_speed_adjustment); // 0x00B63C20 (:1350)
    float get_ai_top_speed_adjustment(); // 0x00B63C40 (:1359)
    void set_reverse_torque_multiplier(float reverse_torque_multiplier); // 0x00B63C50 (:1368)
    float get_steering_t() const; // 0x00B63C70 (:1377)
    void set_is_trailer(bool);
    bool get_is_trailer() const; // 0x00B63340 (.h:237)
    void set_is_tank(bool is_tank); // 0x00B63350 (.h:239)
    bool get_is_tank() const; // 0x00B63370 (.h:240)
    void set_is_articulated(bool);
    bool get_is_articulated() const;
    void set_is_infinite_mass(bool is_inf); // 0x00B63380 (.h:245)
    bool get_is_infinite_mass() const; // 0x00B633A0 (.h:246)
    void set_is_extra_mass(bool is_ext); // 0x00B633B0 (.h:248)
    bool get_is_extra_mass() const;
    void set_extra_mass_val(float ext_val); // 0x00B633D0 (.h:250)
    float get_extra_mass_val() const;
    void set_is_trying_to_recover(bool is_rec); // 0x00B633F0 (.h:253)
    bool get_is_trying_to_recover() const;
    void set_is_engine_running(bool is_run); // 0x00B63410 (.h:256)
    bool get_is_engine_running() const; // 0x00B63430 (.h:257)
    void set_auto_brake_speed(float speed); // 0x00B63440 (.h:259)
    float get_auto_brake_speed() const; // 0x00B63460 (.h:260)
    void set_enforced_max_speed(float ems, float emrs); // 0x00B63CC0 (:1386)
    float get_enforced_max_speed() const; // 0x00B63470 (.h:263)
    float get_enforced_max_reverse_speed() const; // 0x00B63480 (.h:264)
    void set_handbrake_yaw_factor_scalar(float scalar); // 0x00B63490 (.h:266)
    float get_handbrake_yaw_factor_scalar() const;
    unsigned int get_transition_delay_f2r();
    unsigned int get_transition_delay_r2f();
    void set_transition_delays(unsigned int f2r, unsigned int r2f); // 0x00B634B0 (.h:271)
    void set_extra_aabb_limits(hkVector4f& min, hkVector4f& max); // 0x00B63D00 (:1398)
    void get_extra_aabb_limits(hkVector4f& min, hkVector4f& max); // 0x00B63D20 (:1404)
    void set_npc_ray_override(bool ride); // 0x00B74AA0 (:1411)
    void clear_wheel_caches(); // 0x00B8A380 (:1418)
protected:
    bool ignore_new_driver_input() const; // 0x00B63D40 (:1430)
    void updateWheelsHardPoints(const hkTransformf& car_transform); // 0x00B745E0 (:200)
    void applyHk550Action(const hkStepInfo& stepInfo); // 0x00BAE410 (:303)

public:
    hk_custom_vehicle_extra_downforce* m_extra_downforce; // 0x94
protected:
    steering_instance_data m_steering_instance_data; // 0x98
    hkArray<unsigned char,hkContainerHeapAllocator> m_wheel_contact_p_material; // 0x9C
    hkArray<unsigned char,hkContainerHeapAllocator> m_wheel_contact_e_material; // 0xA8
    engine_instance_data m_engine_instance_data; // 0xB4
    driver_input_instance_data m_driver_input_instance_data; // 0xC4
    transmission_instance_data m_transmission_instance_data; // 0xD8
    misc_instance_data m_misc_instance_data; // 0xE0
    hkArray<hk_wheel_info_contact_body_listener*,hkContainerHeapAllocator> m_wheel_info_contact_body_listeners; // 0x130
    hkpVehicleFrictionStatus m_frictionStatus; // 0x13C
};

// 0x00B63470 (.h:263)
inline float hk_custom_vehicle_instance::get_enforced_max_speed() const
{
    return m_misc_instance_data.m_enforced_max_speed;
}

// 0x00B63480 (.h:264)
inline float hk_custom_vehicle_instance::get_enforced_max_reverse_speed() const
{
    return m_misc_instance_data.m_enforced_max_rspeed;
}
