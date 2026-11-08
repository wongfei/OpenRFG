// Original: volition/rfg/code/vehicle/automobile.h
// Implementation: volition/rfg/code/vehicle/automobile.cpp
//
// automobile, a wheeled or tracked vehicle simulated by the Havok vehicle kit (hk_custom_vehicle_instance). Placement of
// mp_collision_prediction_info inferred.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct decal_collection_data;
class hk_custom_vehicle_instance;
class hkpRigidBody;
class hkpVehicleData;
class matrix;
struct multi_automobile_state;
class vehicle_component;
struct vehicle_controls;
class vehicle_info;

// size 0x34
struct mp_collision_prediction_info
{
    mp_collision_prediction_info(); // 0x00B63680

    timestamp check_time; // 0x00
    vector last_start_pos; // 0x04
    vector last_end_pos; // 0x10
    vector last_pos; // 0x1C
    vector last_vel; // 0x28
};

// size 0x4EC0
class automobile : public vehicle
{
public:
    // size 0x24
    struct collision_effect_info
    {
        void reset(); // 0x00B63060 (.h:64)
        collision_effect_info(); // 0x00B63670

        vector m_position; // 0x00
        vector m_normal; // 0x0C
        unsigned int m_other_handle; // 0x18
        int m_foley_info; // 0x1C
        unsigned int m_effect_obj; // 0x20
    };

    // size 0x30
    struct tire_track_decal_data
    {
        tire_track_decal_data(const tire_track_decal_data& __that); // 0x00B635D0
        tire_track_decal_data(); // 0x00B71380 (.h:108)
        void invalidate(); // 0x00B63080 (.h:112)

        vector m_start_pos; // 0x00
        vector m_move_dir; // 0x0C
        vector m_normal; // 0x18
        float m_length; // 0x24
        unsigned int m_decal_handle; // 0x28
        unsigned int m_obj_handle; // 0x2C
    };

    // size 0xB4
    struct wheel_effect_info
    {
        wheel_effect_info(); // 0x00B88860
        ~wheel_effect_info(); // 0x00B729B0

        unsigned int m_motion_effect; // 0x00
        unsigned int m_skid_effect; // 0x04
        int m_skid_foley; // 0x08
        unsigned int m_peel_effect; // 0x0C
        int m_peel_foley; // 0x10
        unsigned int m_brake_effect; // 0x14
        int m_brake_foley; // 0x18
        circ_array<tire_track_decal_data,3> m_track_decals; // 0x1C
    };

    // size 0xE0
    struct resource_dependent_data
    {
        hkpVehicleData* m_hk_vehicle_data; // 0x00
        hk_custom_vehicle_instance* m_hk_vehicle_instance; // 0x04
        unsigned int m_num_wheels; // 0x08
        vehicle_component* m_wheel_components[16]; // 0x0C
        unsigned int m_wheel_axle_ids[16]; // 0x4C
        float m_wheelbase; // 0x8C
        unsigned int m_axle_wheel_counts[4]; // 0x90
        float m_wheel_radius_per_axle[4]; // 0xA0
        float m_wheel_width_per_axle[4]; // 0xB0
        bool m_front_wheels[16]; // 0xC0
        bool m_back_wheels[16]; // 0xD0
    };

    automobile(); // 0x00BA67B0 (:1725)
    static const object_type get_type_static(); // 0x005DD570 (.h:59)
    static const object_sub_type get_subtype_static(); // 0x005DD580 (.h:59)
    static const char* get_classname_static(); // 0x00B63040 (.h:59)
    static int get_bpo_index_static(); // 0x00B63050 (.h:59)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00BA6860 (.h:59)
    static unsigned int get_classname_hash(); // 0x00B63660 (:1724)
    static int pool_remaining(); // 0x00B729A0 (:1724)
    virtual void pool_free(); // vtbl+0x80 0x00B95B50 (:1724)
    void check_wheel_contact_with_ground(); // 0x00B888C0 (:2160)
    virtual hkpRigidBody* get_supporting_rigid_body() const; // vtbl+0x88 0x00B729D0 (:2228)
    virtual unsigned int find_supporting_rfg_mover() const; // vtbl+0x94 0x00B72A10 (:2250)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00B95BB0 (:1737)
    virtual bool stream(bool load); // vtbl+0x1C 0x00BB0EF0 (:1853)
    virtual void engine_start(bool player); // vtbl+0x8C 0x00B88B50 (:2277)
    virtual void engine_stop(bool player, bool unused); // vtbl+0x90 0x00B88BC0 (:2301)
protected:
    virtual void free_this(); // vtbl+0x28 0x00BB4D40 (:2134)
public:
    virtual ~automobile(); // vtbl+0x00 0x00BA6870

    object_stream_resource<resource_dependent_data> a_rdd; // 0x40B8
    wheel_effect_info m_wheel_effect_infos[16]; // 0x419C
    int m_motion_foley; // 0x4CDC
    int m_tread_foley; // 0x4CE0
    decal_collection_data* m_decal_collection_data; // 0x4CE4
    unsigned int m_num_decal_collections; // 0x4CE8
    float m_decal_width; // 0x4CEC
    bool m_decal_collection_occurred; // 0x4CF0
    timestamp m_wheel_effect_stopped_timer; // 0x4CF4
    bool m_wheel_effect_is_stopped; // 0x4CF8
    int honk_alr_id; // 0x4CFC
    int foley_siren; // 0x4D00
    int foley_megaphone; // 0x4D04
    int foley_chassis; // 0x4D08
    int engine_peel; // 0x4D0C
    int gear_shift; // 0x4D10
    float user_acc_input; // 0x4D14
    timestamp time_at_max_speed; // 0x4D18
    bool play_shift_foley; // 0x4D1C
    timestamp horn_timeout; // 0x4D20
    collision_effect_info m_collision_impact_sounds[4]; // 0x4D24
    collision_effect_info m_collision_scraping_sounds[4]; // 0x4DB4
    timestamp m_impact_sound_delay; // 0x4E44
    timestamp m_scraping_sound_delay; // 0x4E48
    timestamp m_chassis_sound_delay; // 0x4E4C
    timestamp brake_effect_release_timer; // 0x4E50
    timestamp catch_fire_timestamp; // 0x4E54
    unsigned int m_h_trailer_vehicle; // 0x4E58
    unsigned int m_h_tractor_vehicle; // 0x4E5C
    float prev_chassis_stress; // 0x4E60
    float max_chassis_stress; // 0x4E64
    timestamp is_airborne; // 0x4E68
    timestamp is_landing; // 0x4E6C
    timestamp is_skipping; // 0x4E70
    timestamp is_skiprecovery; // 0x4E74
    float force_assist_value; // 0x4E78
    float powerslide_amount; // 0x4E7C
    float current_powerslide_friction; // 0x4E80
    bool powerslide_was_handbrake_on; // 0x4E84
    multi_sim_state<multi_automobile_state>* mp_sim_state; // 0x4E88
    mp_collision_prediction_info mp_collision_prediction; // 0x4E8C
};

// size 0xC
class obj_auto_props : public obj_prop_block
{
public:
    obj_auto_props(int type_index, const vector& pos, const matrix& orient, bool pos_at_ground); // 0x00768CC0 (.h:285)
    ~obj_auto_props(); // 0x00761CC0

};

void unused_dcf_vehicle_max_speed(); // 0x00B63580 (:89)
void automobile_setup_havok_for_corpse(automobile* ap); // 0x00B72550 (:1473)
void automobile_build(automobile* ap); // 0x00BB0ED0 (:1561)
void automobile_calculate_wheelbase(automobile* ap); // 0x00B72710 (:1584)
void automobile_blow_up(automobile* ap); // 0x00B95A90 (:1631)
void automobile_handle_human_run_over(automobile* ap); // 0x00B9BBF0 (:1651)
object* alloc_automobile(); // 0x00B88850 (:1724)
void types_automobile(signed char& type, signed char& subtype); // 0x00B63630 (:1724)
const char* classname_automobile(); // 0x00B63650 (:1724)
int bpo_index_automobile(); // 0x00B63640 (:1724)
void automobile_system_init(); // 0x00B63690 (:2156)
void automobile_steer(automobile* ap, float steering_input_x, float steering_input_y); // 0x00B95DC0 (:2330)
void automobile_clear_accelerator(automobile* ap); // 0x00B95E80 (:2360)
void automobile_set_accelerator(automobile* ap); // 0x00B95EB0 (:2381)
void automobile_set_brake_reverse(automobile* ap); // 0x00B95EF0 (:2401)
void automobile_set_forward_or_reverse_accelerator(unsigned int a_handle, float input); // 0x00B95F30 (:2423)
void automobile_set_brake_pedal(automobile* ap); // 0x00B72A80 (:2459)
void automobile_set_brake_pedal(unsigned int a_handle); // 0x00B72B20 (:2475)
void automobile_set_handbrake(automobile* ap, bool applied); // 0x00B72B50 (:2491)
void automobile_set_handbrake(unsigned int a_handle, bool applied); // 0x00B72BA0 (:2509)
void automobile_set_parking(automobile* ap, bool applied); // 0x00B88C10 (:2534)
void automobile_set_steering(unsigned int a_handle, float input); // 0x00B88C40 (:2551)
void automobile_set_brake_pedal_input(unsigned int a_handle, float input); // 0x00B72BE0 (:2574)
float automobile_get_accelerator_value(unsigned int a_handle); // 0x00B72C30 (:2598)
float automobile_get_brake_value(unsigned int a_handle); // 0x00B72C70 (:2622)
bool automobile_has_handbrake(unsigned int a_handle); // 0x00B88C90 (:2636)
bool automobile_has_handbrake(const automobile* ap); // 0x00B72CB0 (:2644)
bool automobile_has_handbrake(const vehicle_info* info); // 0x00B636A0 (:2669)
bool automobile_get_handbrake_on(automobile* ap); // 0x00B72D10 (:2689)
bool automobile_get_parking(automobile* ap); // 0x00B88CC0 (:2709)
void automobile_get_steering_info(unsigned int a_handle, float& max_angle, float& speed_for_max_angle); // 0x00B72D40 (:2726)
void automobile_set_trying_to_reverse(unsigned int a_handle, bool trying_to_revese); // 0x00B72D90 (:2744)
int automobile_get_mph(const automobile* ap); // 0x00B72E00 (:2768)
int automobile_get_mph(unsigned int a_handle); // 0x00B72E30 (:2786)
float automobile_get_speed_ms(const automobile* ap); // 0x00B72E80 (:2803)
float automobile_get_speed_ms(unsigned int a_handle); // 0x00B72EB0 (:2820)
float automobile_get_speed_squared(const automobile* ap); // 0x00B88D40 (:2830)
float automobile_get_speed_squared(unsigned int a_handle); // 0x00B88D90 (:2845)
float automobile_get_steering_t(const automobile* ap); // 0x00B72F10 (:2863)
float automobile_get_steering_t(unsigned int a_handle); // 0x00B72F70 (:2883)
bool automobile_moving_in_reverse(const automobile* ap); // 0x00B73000 (:2901)
bool automobile_moving_in_reverse(unsigned int a_handle); // 0x00B73030 (:2925)
unsigned int automobile_get_current_gear(const automobile* ap); // 0x00B73080 (:2943)
unsigned int automobile_get_desired_gear(const automobile* ap); // 0x00B730B0 (:2965)
float automobile_get_engine_rpm(const automobile* ap); // 0x00B730E0 (:2988)
float automobile_get_current_max_rpm(const automobile* ap); // 0x00B73100 (:2999)
float automobile_get_gear_ratio(const automobile* ap); // 0x00B73170 (:3029)
float automobile_get_engine_torque(const automobile* ap); // 0x00B731B0 (:3050)
float automobile_get_engine_horsepower(const automobile* ap); // 0x00B731D0 (:3067)
void automobile_set_torque_multiplier(automobile* ap, float torque_multiplier); // 0x00B73250 (:3077)
void automobile_set_powerslide_torque_multiplier(automobile* ap, float torque_multiplier); // 0x00B73280 (:3088)
void automobile_set_top_speed_increase(automobile* ap, float speed_increase); // 0x00B732B0 (:3101)
float automobile_get_wheel_radius(const automobile* ap, unsigned int wheel_index); // 0x00B732E0 (:3118)
bool automobile_are_wheels_on_ground(const automobile* ap); // 0x00B636E0 (:3142)
bool automobile_are_wheels_on_ground(int a_handle); // 0x00B73310 (:3153)
bool automobile_any_wheel_on_ground(const automobile* ap); // 0x00B73350 (:3166)
bool automobile_any_wheel_on_ground(unsigned int a_handle); // 0x00B733C0 (:3204)
void automobile_reset_handling_for_ai(automobile* ap, bool ai_vehicle, bool is_driver); // 0x00B733F0 (:3223)
void automobile_calculate_wheel_hardpoint(vector* wheel_hardpoint, const vehicle_component* p_component, const automobile* ap); // 0x00B88DC0 (:3349)
void automobile_set_controls_when_abandoned(automobile* ap); // 0x00B88F60 (:3382)
void automobile_set_controls_when_abandoned(unsigned int a_handle); // 0x00B89250 (:3470)
hk_custom_vehicle_instance* automobile_get_raycast_vehicle(unsigned int a_handle); // 0x00B73690 (:3484)
void automobile_set_vcontrols(unsigned int a_handle, const vehicle_controls& v_controls); // 0x00B95FD0 (:3504)
void automobile_get_vcontrols(unsigned int a_handle, vehicle_controls* v_controls); // 0x00B89280 (:3534)
void automobile_update_orient_pos_from_havok(automobile* ap); // 0x00B89340 (:3556)
void automobile_do_frame_presim(automobile* ap); // 0x00B9C190 (:3622)
void automobile_do_frame_postsim(automobile* ap, bool update_wheels); // 0x00BA68A0 (:3783)
float automobile_get_game_max_vehicle_speed(); // 0x00B63700 (:4235)
bool automobile_is_stopped_and_not_upright(const automobile* ap); // 0x00B736D0 (:4246)
float automobile_get_vehicle_steering_input_raw(unsigned int a_handle); // 0x00B737B0 (:4262)
void automobile_effects_stop_wheel_effects(automobile* ap); // 0x00B9C6A0 (:4281)
bool automobile_is_powersliding(automobile* ap); // 0x00B63710 (:4314)

// 0x005DD570 (.h:59)
inline const object_type automobile::get_type_static()
{
    return OT_VEHICLE;
}

// 0x005DD580 (.h:59)
inline const object_sub_type automobile::get_subtype_static()
{
    return OT_SUB_VEHICLE_AUTO;
}
