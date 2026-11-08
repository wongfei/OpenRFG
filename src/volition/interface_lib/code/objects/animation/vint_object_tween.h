// Original: volition/interface_lib/code/objects/animation/vint_object_tween.h
// Implementation: volition/interface_lib/code/objects/animation/vint_object_tween.cpp
//
// Tweens: a property of a target object interpolated from a start to an end value over a duration, with an algorithm
// (linear, ease in/out, ...), loop mode, state, start and end value types (absolute, relative, current) and the end
// callback; the tween process list.
#pragma once

#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_variant.h"

enum vint_tween_algorithm
{
    VINT_TWEEN_ALG_LINEAR = 0,
    VINT_TWEEN_ALG_EASE_IN_EXP = 1,
    VINT_TWEEN_ALG_EASE_OUT_EXP = 2,
    VINT_TWEEN_ALG_EASE_IN_OUT_EXP = 3,
    VINT_TWEEN_ALG_EASE_IN_QUAD = 4,
    VINT_TWEEN_ALG_EASE_OUT_QUAD = 5,
    VINT_TWEEN_ALG_EASE_IN_OUT_QUAD = 6,
    VINT_TWEEN_ALG_EASE_IN_CUBIC = 7,
    VINT_TWEEN_ALG_EASE_OUT_CUBIC = 8,
    VINT_TWEEN_ALG_EASE_IN_OUT_CUBIC = 9,
    VINT_TWEEN_ALG_EASE_IN_QUART = 10,
    VINT_TWEEN_ALG_EASE_OUT_QUART = 11,
    VINT_TWEEN_ALG_EASE_IN_OUT_QUART = 12,
    VINT_TWEEN_ALG_EASE_IN_QUINT = 13,
    VINT_TWEEN_ALG_EASE_OUT_QUINT = 14,
    VINT_TWEEN_ALG_EASE_IN_OUT_QUINT = 15,
    VINT_TWEEN_ALG_EASE_IN_SIN = 16,
    VINT_TWEEN_ALG_EASE_OUT_SIN = 17,
    VINT_TWEEN_ALG_EASE_IN_OUT_SIN = 18,
    VINT_TWEEN_ALG_EASE_IN_CIRC = 19,
    VINT_TWEEN_ALG_EASE_OUT_CIRC = 20,
    VINT_TWEEN_ALG_EASE_IN_OUT_CIRC = 21,
    VINT_TWEEN_ALG_EASE_IN_BOUNCE = 22,
    VINT_TWEEN_ALG_EASE_OUT_BOUNCE = 23,
    VINT_TWEEN_ALG_EASE_IN_OUT_BOUNCE = 24,
    NUM_VINT_TWEEN_ALGRORITHMS = 25,
};

enum vint_tween_loop
{
    VINT_TWEEN_LOOP_ONCE = 0,
    VINT_TWEEN_LOOP_CYCLE = 1,
    VINT_TWEEN_LOOP_BOUNCE = 2,
    NUM_VINT_TWEEN_LOOPS = 3,
};

enum vint_tween_position_value_type
{
    VINT_TWEEN_POSN_VALUE_TYPE_ABSOLUTE = 0,
    VINT_TWEEN_POSN_VALUE_TYPE_STAGE = 1,
    VINT_TWEEN_POSN_VALUE_TYPE_DYNAMIC = 2,
    NUM_VINT_TWEEN_POSN_VALUE_TYPES = 3,
};

enum vint_tween_state
{
    VINT_TWEEN_STATE_WAITING = 0,
    VINT_TWEEN_STATE_RUNNING = 1,
    VINT_TWEEN_STATE_PAUSED = 2,
    VINT_TWEEN_STATE_DISABLED = 3,
    VINT_TWEEN_STATE_FINISHED = 4,
    NUM_VINT_TWEEN_STATES = 5,
};

// size 0x210
class vint_object_tween : public vint_object_base
{
public:
    vint_object_tween(); // 0x004769D0 (:404)
    virtual ~vint_object_tween(); // vtbl+0x00 0x004762E0 (.h:92)
    static bool system_init(); // 0x0047D690 (:411)
    static void system_shutdown(); // 0x0045CA50 (:424)
    virtual void process(); // vtbl+0x0C 0x004682F0 (:507)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x00476360 (.h:99)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x00476380 (.h:99)
    static bool internal_get_prop_target_handle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CB10 (:807)
    static bool internal_set_prop_target_handle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CB30 (:817)
    virtual bool get_target_handle(vint_variant& _val, int _index) const; // vtbl+0x24 0x004748F0 (:808)
    virtual bool set_target_handle(const vint_variant& _val, int _index); // vtbl+0x28 0x0045CB50 (:818)
    static bool internal_get_prop_target_name(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CBE0 (:828)
    static bool internal_set_prop_target_name(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CC10 (:849)
    virtual bool get_target_name(vint_variant& _val, int _index) const; // vtbl+0x2C 0x0045CC00 (:829)
    virtual bool set_target_name(const vint_variant& _val, int _index); // vtbl+0x30 0x0045CC30 (:850)
    static bool internal_get_prop_target_name_crc(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CC70 (:860)
    static bool internal_set_prop_target_name_crc(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CC90 (:869)
    unsigned int get_target_name_crc(); // 0x0045BD50 (.h:127)
    virtual bool get_target_name_crc(vint_variant& _val, int _index) const; // vtbl+0x34 0x00471AC0 (:861)
    virtual bool set_target_name_crc(const vint_variant& _val, int _index); // vtbl+0x38 0x0045CCB0 (:870)
    static bool internal_get_prop_target_property(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CD50 (:880)
    static bool internal_set_prop_target_property(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CD70 (:895)
    virtual bool get_target_property(vint_variant& _val, int _index) const; // vtbl+0x3C 0x00474930 (:881)
    virtual bool set_target_property(const vint_variant& _val, int _index); // vtbl+0x40 0x0045CD90 (:896)
    static bool internal_get_prop_state(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CDD0 (:905)
    static bool internal_set_prop_state(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CDF0 (:913)
    vint_tween_state get_state(); // 0x0045BD40 (.h:126)
    virtual bool get_state(vint_variant& _val, int _index) const; // vtbl+0x44 0x00468300 (:906)
    virtual bool set_state(const vint_variant& _val, int _index); // vtbl+0x48 0x0045CE10 (:914)
    static bool internal_get_prop_start_time(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CF60 (:949)
    static bool internal_set_prop_start_time(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CFC0 (:957)
    virtual bool get_start_time(vint_variant& _val, int _index) const; // vtbl+0x4C 0x0045CF80 (:950)
    virtual bool set_start_time(const vint_variant& _val, int _index); // vtbl+0x50 0x0045CFE0 (:958)
    static bool internal_get_prop_duration(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045CE80 (:928)
    static bool internal_set_prop_duration(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045CEE0 (:936)
    virtual bool get_duration(vint_variant& _val, int _index) const; // vtbl+0x54 0x0045CEA0 (:929)
    virtual bool set_duration(const vint_variant& _val, int _index); // vtbl+0x58 0x0045CF00 (:937)
    static bool internal_get_prop_start_value(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D030 (:966)
    static bool internal_set_prop_start_value(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D050 (:974)
    virtual bool get_start_value(vint_variant& _val, int _index) const; // vtbl+0x5C 0x00474960 (:967)
    virtual bool set_start_value(const vint_variant& _val, int _index); // vtbl+0x60 0x00474980 (:975)
    static bool internal_get_prop_end_value(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D070 (:983)
    static bool internal_set_prop_end_value(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D090 (:991)
    virtual bool get_end_value(vint_variant& _val, int _index) const; // vtbl+0x64 0x004749B0 (:984)
    virtual bool set_end_value(const vint_variant& _val, int _index); // vtbl+0x68 0x004749D0 (:992)
    static bool internal_get_prop_loop_mode(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D0B0 (:1000)
    static bool internal_set_prop_loop_mode(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D0D0 (:1008)
    virtual bool get_loop_mode(vint_variant& _val, int _index) const; // vtbl+0x6C 0x00468320 (:1001)
    virtual bool set_loop_mode(const vint_variant& _val, int _index); // vtbl+0x70 0x0045D0F0 (:1009)
    static bool internal_get_prop_algorithm(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D160 (:1023)
    static bool internal_set_prop_algorithm(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D180 (:1031)
    virtual bool get_algorithm(vint_variant& _val, int _index) const; // vtbl+0x74 0x00468340 (:1024)
    virtual bool set_algorithm(const vint_variant& _val, int _index); // vtbl+0x78 0x0045D1A0 (:1032)
    static bool internal_get_prop_max_loops(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D210 (:1046)
    static bool internal_set_prop_max_loops(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D230 (:1054)
    virtual bool get_max_loops(vint_variant& _val, int _index) const; // vtbl+0x7C 0x00468360 (:1047)
    virtual bool set_max_loops(const vint_variant& _val, int _index); // vtbl+0x80 0x0045D250 (:1055)
    static bool internal_get_prop_start_event(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D2F0 (:1070)
    static bool internal_set_prop_start_event(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D2D0 (:1062)
    virtual bool get_start_event(vint_variant& _val, int _index) const; // vtbl+0x84 0x00474A20 (:1071)
    virtual bool set_start_event(const vint_variant& _val, int _index); // vtbl+0x88 0x00474A00 (:1063)
    static bool internal_get_prop_end_event(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D330 (:1086)
    static bool internal_set_prop_end_event(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D310 (:1078)
    virtual bool get_end_event(vint_variant& _val, int _index) const; // vtbl+0x8C 0x00474A60 (:1087)
    virtual bool set_end_event(const vint_variant& _val, int _index); // vtbl+0x90 0x00474A40 (:1079)
    static bool internal_get_prop_per_frame_event(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D370 (:1102)
    static bool internal_set_prop_per_frame_event(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D350 (:1094)
    virtual bool get_per_frame_event(vint_variant& _val, int _index) const; // vtbl+0x94 0x00474AA0 (:1103)
    virtual bool set_per_frame_event(const vint_variant& _val, int _index); // vtbl+0x98 0x00474A80 (:1095)
    static bool internal_get_prop_start_value_type(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D390 (:1110)
    static bool internal_set_prop_start_value_type(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D3B0 (:1118)
    virtual bool get_start_value_type(vint_variant& _val, int _index) const; // vtbl+0x9C 0x00468380 (:1111)
    virtual bool set_start_value_type(const vint_variant& _val, int _index); // vtbl+0xA0 0x0045D3D0 (:1119)
    static bool internal_get_prop_end_value_type(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D440 (:1133)
    static bool internal_set_prop_end_value_type(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D460 (:1141)
    virtual bool get_end_value_type(vint_variant& _val, int _index) const; // vtbl+0xA4 0x004683A0 (:1134)
    virtual bool set_end_value_type(const vint_variant& _val, int _index); // vtbl+0xA8 0x0045D480 (:1142)
    static bool internal_get_prop_reverse(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045D4F0 (:1156)
    static bool internal_set_prop_reverse(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045D540 (:1163)
    virtual bool get_reverse(vint_variant& _val, int _index) const; // vtbl+0xAC 0x0045D510 (:1157)
    virtual bool set_reverse(const vint_variant& _val, int _index); // vtbl+0xB0 0x0045D560 (:1164)
    void zero_start_callback(); // 0x008940B0 (.h:119)
    void zero_end_callback(); // 0x008940C0 (.h:120)
    void reset_tween_object(); // 0x00474870 (:789)
    static void process_all(); // 0x0047D7E0 (:539)
protected:
    virtual void clean_up(); // vtbl+0x10 0x004748A0 (:801)
    virtual bool init(); // vtbl+0x04 0x00474500 (:429)
    void update_priority(); // 0x0045CAE0 (:564)
    vint_object_base* acquire_target() const; // 0x00474770 (:470)
    void process_internal(); // 0x00476A80 (:569)
    static int compare_tweens_for_process(const void* _left, const void* _right); // 0x0045CAC0 (:531)

private:
    static unsigned int Int_obj_type; // 0x014F5700
public:
    static const unsigned int& Object_type; // 0x014F5704
protected:
    vint_variant m_start_value; // 0x130
    vint_variant m_end_value; // 0x150
    vint_variant m_start_callback; // 0x170
    vint_variant m_end_callback; // 0x190
    vint_variant m_per_frame_callback; // 0x1B0
    float m_start_time; // 0x1D0
    float m_duration; // 0x1D4
    float m_last_interp; // 0x1D8
    vint_object_base* m_target; // 0x1DC
    const vint_prop_lookup* m_target_prop_lookup; // 0x1E0
    unsigned int m_target_obj_handle; // 0x1E4
    unsigned int m_target_obj_name_crc; // 0x1E8
    unsigned int m_target_prop_name_crc; // 0x1EC
    int m_priority; // 0x1F0
    int m_max_loops; // 0x1F4
    vint_tween_loop m_loop_mode; // 0x1F8
    vint_tween_algorithm m_algorithm; // 0x1FC
    vint_tween_state m_state; // 0x200
    vint_tween_position_value_type m_start_value_type; // 0x204
    vint_tween_position_value_type m_end_value_type; // 0x208
    bool m_reverse; // 0x20C
};

bool fl_is_zero(float f); // 0x0045C030 (:196)
bool fl_is_negative(float f); // 0x0045C050 (:202)
bool fl_is_less_than(float l, float r); // 0x0045C070 (:208)
bool fl_is_greater_than(float l, float r); // 0x0045C0A0 (:214)
bool fl_is_equal(float l, float r); // 0x0045C0D0 (:220)
