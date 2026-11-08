// Original: volition/interface_lib/code/objects/animation/vint_object_animation.cpp
//
// Animations (UI.md UI2): a group of child tweens timed together. The children run on the animation's clock (the
// document's time plus the paused-time offset, minus the start time); pausing records the time and offsets the clock
// when resumed, and pauses or wakes the child tweens. Retargeting points each child tween at the same-named object
// under a new target. A pool of 142. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/vlib/code/math/crc.h"

#include <string.h>

// ---- class statics
unsigned int vint_object_animation::Int_obj_type = 0xFFFFFFFF; // 0x014F56F8
const unsigned int& vint_object_animation::Object_type = vint_object_animation::Int_obj_type; // 0x014F56FC

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_object_animation[3] = { // 0x014F5B78
    VINT_PROP_ENTRY(vint_object_animation, start_time, VINT_PROP_TYPE_FLOAT, 0.0f, true),
    VINT_PROP_ENTRY(vint_object_animation, is_paused, VINT_PROP_TYPE_BOOL, false, true),
    VINT_PROP_ENTRY(vint_object_animation, target_handle, VINT_PROP_TYPE_UINT, 0, false),
};
static vint_prop_mgr Prop_mgr_vint_object_animation;
static vint_object_factory_imp<vint_object_animation> Factory_vint_object_animation;

// A child's property through its type's table (rfg.exe inlines the lookup).
static void vint_object_animation_get_child_prop(vint_object_base* _child, const char* _name, vint_variant& _val)
{
    unsigned int crc = crc_stri(_name, 0);
    const vint_object_type_info* info = vint_object_types_get_type_info(_child->get_type());
    if (info == nullptr || info->prop_mgr == nullptr)
    {
        return;
    }
    if (const vint_prop_lookup* lookup = info->prop_mgr->get_lookup(crc))
    {
        lookup->get(_child, _val, 0);
    }
}

// ---- header-inline members

// 0x00468150 (.h:32)
vint_object_animation::vint_object_animation()
{
}

// 0x0045BD60 (.h:33)
vint_object_animation::~vint_object_animation()
{
}

// 0x00468180 (.h:40)
bool vint_object_animation::is_type(unsigned int _type) const
{
    return _type == Object_type;
}

// 0x004681A0 (.h:40)
unsigned int vint_object_animation::get_type() const
{
    return Object_type;
}

// 0x0045BD70 (.h:48)
float vint_object_animation::get_time_index() const
{
    return (float)(((double)get_parent_document()->get_time_index() + (double)m_offset_time) - (double)m_start_time);
}

// ---- vint_object_animation

// 0x004719D0 (:67)
bool vint_object_animation::system_init()
{
    Int_obj_type = vint_register_object_class("animation", 0xFFFFFFFF, Prop_mgr_vint_object_animation, Prop_lookup_vint_object_animation, 3, Factory_vint_object_animation, 142);
    return true;
}

// 0x0045BDF0 (:74)
void vint_object_animation::system_shutdown()
{
}

// 0x00471A70 (:78)
bool vint_object_animation::init()
{
    // the base init, inlined in rfg.exe
    vint_object_base::init();
    m_is_paused = false;
    m_start_time = 0.0f;
    m_offset_time = 0.0f;
    // Original: m_paused_time is not reset
    return true;
}

// 0x004681B0 (:92)
void vint_object_animation::process()
{
}

// 0x0045BE00 (:97)
void vint_object_animation::clean_up()
{
}

// 0x004763B0 (:101)
float vint_object_animation::get_duration()
{
    // the latest end (start time + duration) of the child tweens
    float duration = 0.0f;
    // Original: both values carry over to the next tween when its lookup fails
    vint_variant start_time;
    vint_variant tween_duration;
    for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() != vint_object_tween::Object_type)
        {
            continue;
        }
        vint_object_animation_get_child_prop(child, "start_time", start_time);
        vint_object_animation_get_child_prop(child, "duration", tween_duration);
        float end = (float)((double)(float)tween_duration + (double)(float)start_time);
        if ((double)end > (double)duration)
        {
            duration = end;
        }
    }
    return duration;
}

// 0x0045BE10 (:125), 0x0045BE50 (:133)
VINT_PROP_WRAPPERS(vint_object_animation, start_time)

// 0x0045BE30 (:126)
bool vint_object_animation::set_start_time(const vint_variant& _val, int _index)
{
    // stored without the pause offset
    m_start_time = (float)((double)(float)_val + (double)m_offset_time);
    return true;
}

// 0x0045BE70 (:134)
bool vint_object_animation::get_start_time(vint_variant& _val, int _index) const
{
    _val = (float)((double)m_offset_time + (double)m_start_time);
    return true;
}

// 0x0045BEC0 (:141), 0x0045BEE0 (:182)
VINT_PROP_WRAPPERS(vint_object_animation, is_paused)

// 0x00476640 (:142)
bool vint_object_animation::set_is_paused(const vint_variant& _val, int _index)
{
    if (_val.get_type() != VINT_PROP_TYPE_BOOL)
    {
        return false;
    }
    bool paused = (bool)_val;
    if (paused != m_is_paused)
    {
        if (paused)
        {
            m_paused_time = get_parent_document()->get_time_index();
        }
        else
        {
            // the paused span no longer counts
            m_offset_time = (float)((double)m_offset_time - ((double)get_parent_document()->get_time_index() - (double)m_paused_time));
        }
        m_is_paused = paused;
    }
    // the child tweens pause, or wait to run again; disabled tweens stay so
    vint_tween_state state = paused ? VINT_TWEEN_STATE_PAUSED : VINT_TWEEN_STATE_WAITING;
    vint_variant current;
    for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() != vint_object_tween::Object_type)
        {
            continue;
        }
        vint_object_tween* tween = (vint_object_tween*)child;
        current.clear();
        tween->get_state(current, 0);
        if ((int)current == VINT_TWEEN_STATE_DISABLED)
        {
            continue;
        }
        vint_variant value;
        value = (int)state;
        tween->set_state(value, 0);
    }
    return true;
}

// 0x0045BF00 (:183)
bool vint_object_animation::get_is_paused(vint_variant& _val, int _index) const
{
    _val = m_is_paused;
    return true;
}

// 0x0045BF30 (:190), 0x0045BF50 (:227)
VINT_PROP_WRAPPERS(vint_object_animation, target_handle)

// 0x004767A0 (:191)
bool vint_object_animation::set_target_handle(const vint_variant& _val, int _index)
{
    // each child tween targets the object of its target's name under the new target
    unsigned int handle = (unsigned int)_val;
    vint_object_base* target = (vint_object_base*)handle;
    if (handle == 0 || handle == 0xFFFFFFFF || target->get_handle() != handle)
    {
        return false;
    }
    for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
    {
        if (!child->is_type(vint_object_tween::Object_type))
        {
            continue;
        }
        vint_object_tween* tween = (vint_object_tween*)child;
        unsigned int name_crc = tween->get_target_name_crc();
        if (name_crc == 0)
        {
            continue;
        }
        vint_object_base* found = nullptr;
        for (vint_object_base* c = target->first_child; c != nullptr; c = c->next_sibling)
        {
            if (c->get_name_crc() == name_crc)
            {
                found = c;
                break;
            }
            found = c->find_child(name_crc);
            if (found != nullptr)
            {
                break;
            }
        }
        if (found != nullptr)
        {
            vint_variant value;
            value = found->get_handle();
            tween->set_target_handle(value, 0);
        }
    }
    return true;
}

// 0x0045BF70 (:228)
bool vint_object_animation::get_target_handle(vint_variant& _val, int _index) const
{
    return false;
}

// 0x004768B0 (:236)
void vint_object_animation_reset_child_tween_objects(unsigned int anim_handle, vint_document* doc)
{
    // every child tween's target back to its start value
    vint_object_base* anim = (vint_object_base*)anim_handle;
    if (doc == nullptr || anim_handle == 0 || anim_handle == 0xFFFFFFFF || anim->get_handle() != anim_handle || anim->get_parent_document() != doc || anim->get_type() != vint_object_animation::Object_type)
    {
        return;
    }
    for (vint_object_base* child = anim->get_first_child(); child != nullptr; child = child->get_next_sibling())
    {
        if (child->get_type() == vint_object_tween::Object_type)
        {
            ((vint_object_tween*)child)->reset_tween_object();
        }
    }
}

// 0x00476930 (:259)
void vint_object_animation_set_child_tween_state(unsigned int anim_handle, vint_tween_state state, vint_document* doc)
{
    vint_object_base* anim = (vint_object_base*)anim_handle;
    if (doc == nullptr || anim_handle == 0 || anim_handle == 0xFFFFFFFF || anim->get_handle() != anim_handle || anim->get_parent_document() != doc || anim->get_type() != vint_object_animation::Object_type || (int)state >= NUM_VINT_TWEEN_STATES)
    {
        return;
    }
    for (vint_object_base* child = anim->get_first_child(); child != nullptr; child = child->get_next_sibling())
    {
        if (child->get_type() == vint_object_tween::Object_type)
        {
            vint_variant value;
            value = (int)state;
            ((vint_object_tween*)child)->set_state(value, 0);
        }
    }
}
