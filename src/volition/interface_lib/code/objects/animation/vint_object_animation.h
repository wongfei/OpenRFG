// Original: volition/interface_lib/code/objects/animation/vint_object_animation.h
// Implementation: volition/interface_lib/code/objects/animation/vint_object_animation.cpp
//
// Animations: a set of tweens played together from a start time (clone and retarget, pause, duration).
#pragma once

#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"

class vint_document;
class vint_variant;

// size 0x13C
class vint_object_animation : public vint_object_base
{
public:
    vint_object_animation(); // 0x00468150 (.h:32)
    virtual ~vint_object_animation(); // vtbl+0x00 0x0045BD60 (.h:33)
    static bool system_init(); // 0x004719D0 (:67)
    static void system_shutdown(); // 0x0045BDF0 (:74)
    virtual void process(); // vtbl+0x0C 0x004681B0 (:92)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x00468180 (.h:40)
    virtual unsigned int get_type() const; // vtbl+0x1C 0x004681A0 (.h:40)
    static bool internal_get_prop_start_time(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045BE50 (:133)
    static bool internal_set_prop_start_time(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045BE10 (:125)
    virtual bool get_start_time(vint_variant& _val, int _index) const; // vtbl+0x24 0x0045BE70 (:134)
    virtual bool set_start_time(const vint_variant& _val, int _index); // vtbl+0x28 0x0045BE30 (:126)
    static bool internal_get_prop_is_paused(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045BEE0 (:182)
    static bool internal_set_prop_is_paused(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045BEC0 (:141)
    virtual bool get_is_paused(vint_variant& _val, int _index) const; // vtbl+0x2C 0x0045BF00 (:183)
    void set_is_paused(bool);
    virtual bool set_is_paused(const vint_variant& _val, int _index); // vtbl+0x30 0x00476640 (:142)
    static bool internal_get_prop_target_handle(const vint_object_base* _obj, vint_variant& _val, int _index); // 0x0045BF50 (:227)
    static bool internal_set_prop_target_handle(vint_object_base* _obj, const vint_variant& _val, int _index); // 0x0045BF30 (:190)
    virtual bool get_target_handle(vint_variant& _val, int _index) const; // vtbl+0x34 0x0045BF70 (:228)
    virtual bool set_target_handle(const vint_variant& _val, int _index); // vtbl+0x38 0x004767A0 (:191)
    float get_duration(); // 0x004763B0 (:101)
    float get_time_index() const; // 0x0045BD70 (.h:48)
protected:
    virtual bool init(); // vtbl+0x04 0x00471A70 (:78)
    virtual void clean_up(); // vtbl+0x10 0x0045BE00 (:97)

private:
    static unsigned int Int_obj_type; // 0x014F56F8
public:
    static const unsigned int& Object_type; // 0x014F56FC
protected:
    float m_start_time; // 0x12C
    float m_offset_time; // 0x130
    float m_paused_time; // 0x134
    bool m_is_paused; // 0x138
};

void vint_object_animation_reset_child_tween_objects(unsigned int anim_handle, vint_document* doc); // 0x004768B0 (:236)
void vint_object_animation_set_child_tween_state(unsigned int anim_handle, vint_tween_state state, vint_document* doc); // 0x00476930 (:259)
