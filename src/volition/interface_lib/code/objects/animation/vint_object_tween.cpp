// Original: volition/interface_lib/code/objects/animation/vint_object_tween.cpp
//
// Tweens (UI.md UI2): a property of a target object (by handle, else by name in the tween's document) interpolated
// from the start to the end value over a duration, from a start time on the document's clock or the parent animation's.
// Float, color and vector values are interpolated by one of 25 easing functions; other types jump to the end value.
// Loop modes once, cycle and bounce; start, end and per-frame events go to the tween's callbacks. All tweens are
// processed once a frame ordered by end time (process_all). A pool of 576. Compiled in interface_lib_unity.cpp.
//
// The easing functions are mostly x87 code: intermediates are double (the x87 precision control rfg.exe runs with)
// and every float store in the assembly is a float rounding here.

#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/vlib/code/math/crc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- class statics
unsigned int vint_object_tween::Int_obj_type = 0xFFFFFFFF; // 0x014F5700
const unsigned int& vint_object_tween::Object_type = vint_object_tween::Int_obj_type; // 0x014F5704

// ---- file-private data (rfg.pdb)
static const vint_prop_type_enum_info Internal_vint_object_tween_loop_mode_infos[3] = {{"once", 0}, {"cycle", 1}, {"bounce", 2}}; // 0x014F5708
static const vint_prop_type_enum_info Internal_vint_object_tween_algorithm_infos[25] = { // 0x014F5720
    {"linear", 0},
    {"ease_in_exp", 1},
    {"ease_out_exp", 2},
    {"ease_in_out_exp", 3},
    {"ease_in_quad", 4},
    {"ease_out_quad", 5},
    {"ease_in_out_quad", 6},
    {"ease_in_cubic", 7},
    {"ease_out_cubic", 8},
    {"ease_in_out_cubic", 9},
    {"ease_in_quart", 10},
    {"ease_out_quart", 11},
    {"ease_in_out_quart", 12},
    {"ease_in_quint", 13},
    {"ease_out_quint", 14},
    {"ease_in_out_quint", 15},
    {"ease_in_sin", 16},
    {"ease_out_sin", 17},
    {"ease_in_out_sin", 18},
    {"ease_in_circ", 19},
    {"ease_out_circ", 20},
    {"ease_in_out_circ", 21},
    {"ease_in_bounce", 22},
    {"ease_out_bounce", 23},
    {"ease_in_out_bounce", 24},
};
// "idle" and "waiting" are both 0
static const vint_prop_type_enum_info Internal_vint_object_tween_state_infos[6] = {{"idle", 0}, {"running", 1}, {"paused", 2}, {"disabled", 3}, {"waiting", 0}, {"finished", 4}}; // 0x014F57E8
static const vint_prop_type_enum_info Internal_vint_object_tween_start_value_type_infos[3] = {{"absolute", 0}, {"stage", 1}, {"dynamic", 2}}; // 0x014F5818
static const vint_prop_type_enum_info Internal_vint_object_tween_end_value_type_infos[3] = {{"absolute", 0}, {"stage", 1}, {"dynamic", 2}}; // 0x014F5830
static vint_prop_type_enum_wrapper Internal_vint_object_tween_loop_mode_wrapper(Internal_vint_object_tween_loop_mode_infos, 3); // 0x014F6190
static vint_prop_type_enum_wrapper Internal_vint_object_tween_algorithm_wrapper(Internal_vint_object_tween_algorithm_infos, 25); // 0x014F6198
static vint_prop_type_enum_wrapper Internal_vint_object_tween_state_wrapper(Internal_vint_object_tween_state_infos, 6); // 0x014F61A0
static vint_prop_type_enum_wrapper Internal_vint_object_tween_start_value_type_wrapper(Internal_vint_object_tween_start_value_type_infos, 3); // 0x014F61A8
static vint_prop_type_enum_wrapper Internal_vint_object_tween_end_value_type_wrapper(Internal_vint_object_tween_end_value_type_infos, 3); // 0x014F61B0

// ---- file-static globals
static vint_object_tween** Tween_process_list; // 0x01A48634
static unsigned int Tweens_to_process; // 0x01A48638

// The callbacks' event data: the tween and the event's property name (lua_tween_event_handler reads both words).
struct vint_tween_event_data
{
    vint_object_tween* tween;
    const char* event_name;
};

// ---- fl helpers

// 0x0045C030 (:196)
bool fl_is_zero(float f)
{
    return f == 0.0f;
}

// 0x0045C050 (:202)
bool fl_is_negative(float f)
{
    return f < 0.0f;
}

// 0x0045C070 (:208)
bool fl_is_less_than(float l, float r)
{
    return l < r;
}

// 0x0045C0A0 (:214)
bool fl_is_greater_than(float l, float r)
{
    return l > r;
}

// 0x0045C0D0 (:220)
bool fl_is_equal(float l, float r)
{
    return l == r;
}

// ---- easing functions (Penner's, _time in 0..1)

// 0x0045C100 (:236)
static float interp_linear(float _time, float _start, float _delta)
{
    return (float)((double)_time * _delta + _start);
}

// 0x0045C120 (:241)
static float interp_ease_in_exp(float _time, float _start, float _delta)
{
    if (_time < 0.0f || _time == 0.0f)
    {
        return _start;
    }
    float e = (float)(((double)_time - 1.0) * 10.0);
    float p = (float)pow(2.0, (double)e);
    return (float)((double)p * _delta + _start);
}

// 0x0045C1A0 (:246)
static float interp_ease_out_exp(float _time, float _start, float _delta)
{
    if (_time > 1.0f || _time == 1.0f)
    {
        return (float)((double)_start + (double)_delta);
    }
    float e = (float)((double)_time * -10.0);
    float p = (float)pow(2.0, (double)e);
    return (float)((1.0 - p) * _delta + _start);
}

// 0x0045C230 (:251)
static float interp_ease_in_out_exp(float _time, float _start, float _delta)
{
    if (_time == 0.0f)
    {
        return _start;
    }
    if (_time == 1.0f)
    {
        return (float)((double)_start + (double)_delta);
    }
    float u = (float)((double)_time * 2.0 - 1.0);
    if (u < 0.0f)
    {
        float p = (float)pow(2.0, (double)(float)((double)u * 10.0));
        return (float)((double)p * ((double)_delta * 0.5) + _start);
    }
    float p = (float)pow(2.0, (double)(float)((double)u * -10.0));
    return (float)((2.0 - p) * ((double)_delta * 0.5) + _start);
}

// 0x0045C310 (:261)
static float interp_ease_in_quad(float _time, float _start, float _delta)
{
    return (float)((double)_time * _delta * _time + _start);
}

// 0x0045C330 (:266)
static float interp_ease_out_quad(float _time, float _start, float _delta)
{
    return (float)(_start - ((double)_time - 2.0) * ((double)_time * _delta));
}

// 0x0045C360 (:271)
static float interp_ease_in_out_quad(float _time, float _start, float _delta)
{
    float u = (float)((double)_time * 2.0);
    double h = (double)_delta * 0.5;
    if (u < 1.0f)
    {
        return (float)(h * u * u + _start);
    }
    return (float)(_start - (((double)u - 1.0) * ((double)u - 3.0) - 1.0) * h);
}

// 0x0045C3E0 (:279)
static float interp_ease_in_cubic(float _time, float _start, float _delta)
{
    return (float)((double)_time * _delta * _time * _time + _start);
}

// 0x0045C400 (:284)
static float interp_ease_out_cubic(float _time, float _start, float _delta)
{
    // Original bug kept: Penner's "+ 1" is missing, so this runs from _start - _delta to _start
    float v = (float)((double)_time - 1.0);
    return (float)((double)v * _delta * v * v + _start);
}

// 0x0045C430 (:290)
static float interp_ease_in_out_cubic(float _time, float _start, float _delta)
{
    float u = (float)((double)_time * 2.0);
    double h = (double)_delta * 0.5;
    if (u < 1.0f)
    {
        return (float)(h * u * u * u + _start);
    }
    float v = (float)((double)u - 2.0);
    return (float)(((double)v * v * v + 2.0) * h + _start);
}

// 0x0045C4D0 (:299)
static float interp_ease_in_quart(float _time, float _start, float _delta)
{
    return (float)((double)_time * _delta * _time * _time * _time + _start);
}

// 0x0045C4F0 (:304)
static float interp_ease_out_quart(float _time, float _start, float _delta)
{
    float v = (float)((double)_time - 1.0);
    return (float)(_start - ((double)v * v * v * v - 1.0) * _delta);
}

// 0x0045C550 (:310)
static float interp_ease_in_out_quart(float _time, float _start, float _delta)
{
    float u = (float)((double)_time * 2.0);
    double h = (double)_delta * 0.5;
    if (u < 1.0f)
    {
        return (float)(h * u * u * u * u + _start);
    }
    float v = (float)((double)u - 2.0);
    return (float)(_start - ((double)v * v * v * v - 2.0) * h);
}

// 0x0045C600 (:318)
static float interp_ease_in_quint(float _time, float _start, float _delta)
{
    return (float)((double)_time * _delta * _time * _time * _time * _time + _start);
}

// 0x0045C620 (:323)
static float interp_ease_out_quint(float _time, float _start, float _delta)
{
    float v = (float)((double)_time - 1.0);
    return (float)(((double)v * v * v * v * v + 1.0) * _delta + _start);
}

// 0x0045C680 (:329)
static float interp_ease_in_out_quint(float _time, float _start, float _delta)
{
    float u = (float)((double)_time * 2.0);
    double h = (double)_delta * 0.5;
    if (u < 1.0f)
    {
        return (float)(h * u * u * u * u * u + _start);
    }
    float v = (float)((double)u - 2.0);
    return (float)(((double)v * v * v * v * v + 2.0) * h + _start);
}

// 0x0045C730 (:337)
static float interp_ease_in_sin(float _time, float _start, float _delta)
{
    // pi/2 as a float constant: 1.5707964f
    float angle = (float)((double)_time * (double)1.5707964f);
    float c = (float)cos((double)angle);
    return (float)(((double)_delta - (double)c * _delta) + _start);
}

// 0x0045C770 (:342)
static float interp_ease_out_sin(float _time, float _start, float _delta)
{
    float angle = (float)((double)_time * (double)1.5707964f);
    float s = (float)sin((double)angle);
    return (float)((double)s * _delta + _start);
}

// 0x0045C7A0 (:347)
static float interp_ease_in_out_sin(float _time, float _start, float _delta)
{
    // Original bug kept: Penner's -c/2 has its sign lost, so this runs from _start to _start - _delta
    float angle = (float)((double)_time * (double)3.1415927f);
    float c = (float)cos((double)angle);
    return (float)(((double)c - 1.0) * ((double)_delta * 0.5) + _start);
}

// 0x0045C7E0 (:352)
static float interp_ease_in_circ(float _time, float _start, float _delta)
{
    float r = (float)(1.0 - (double)_time * _time);
    float q = (float)sqrt((double)r);
    return (float)((double)_start - ((double)q - 1.0) * _delta);
}

// 0x0045C830 (:357)
static float interp_ease_out_circ(float _time, float _start, float _delta)
{
    float v = (float)((double)_time - 1.0);
    float r = (float)(1.0 - (double)v * v);
    float q = (float)sqrt((double)r);
    return (float)((double)q * _delta + _start);
}

// 0x0045C890 (:363)
static float interp_ease_in_out_circ(float _time, float _start, float _delta)
{
    float u = (float)((double)_time * 2.0);
    double h = (double)_delta * 0.5;
    if (u < 1.0f)
    {
        float r = (float)(1.0 - (double)u * u);
        float q = (float)sqrt((double)r);
        return (float)((double)_start - ((double)q - 1.0) * h);
    }
    // Original bug kept: Penner subtracts 2 here; adding it makes the root negative, so the second half is NaN
    float v = (float)((double)u + 2.0);
    float r = (float)(1.0 - (double)v * v);
    float q = (float)sqrt((double)r);
    return (float)(((double)q + 1.0) * h + _start);
}

// 0x0045C960 (:376)
static float interp_ease_out_bounce(float _time, float _start, float _delta)
{
    // the thresholds and offsets are n/11 as float constants (4/11, 8/11, 6/11, 9/11, 10/11, 10.5/11); 7.5625 = 121/16
    // Original bug kept: the 0.75, 0.9375 and 0.984375 bounce heights are not scaled by _delta
    if (_time < 0.36363637f)
    {
        return (float)(7.5625 * _time * _time * _delta + _start);
    }
    if (_time < 0.72727275f)
    {
        float v = (float)((double)_time - (double)0.54545456f);
        return (float)((double)_delta * 7.5625 * v * v + 0.75 + _start);
    }
    if ((double)_time < (double)0.90909094f)
    {
        float v = (float)((double)_time - (double)0.8181818f);
        return (float)((double)_delta * 7.5625 * v * v + 0.9375 + _start);
    }
    float v = (float)((double)_time - (double)0.95454544f);
    return (float)((double)_delta * 7.5625 * v * v + 0.984375 + _start);
}

// 0x004681C0 (:371)
static float interp_ease_in_bounce(float _time, float _start, float _delta)
{
    float b = interp_ease_out_bounce((float)(1.0 - (double)_time), 0.0f, _delta);
    return (float)(((double)_delta - b) + _start);
}

// 0x00468220 (:392)
static float interp_ease_in_out_bounce(float _time, float _start, float _delta)
{
    if (_time < 0.5f)
    {
        float u = (float)((double)_time * 2.0);
        float b = interp_ease_out_bounce((float)(1.0 - (double)u), 0.0f, _delta);
        float r = (float)(((double)_delta - b) + 0.0);
        return (float)((double)r * 0.5 + _start);
    }
    float b = interp_ease_out_bounce((float)((double)_time * 2.0 - 1.0), 0.0f, _delta);
    return (float)(((double)b * 0.5 + (double)_delta * 0.5) + _start);
}

static float (*Interp_functions[25])(float, float, float) = { // 0x014F5848
    interp_linear,
    interp_ease_in_exp,
    interp_ease_out_exp,
    interp_ease_in_out_exp,
    interp_ease_in_quad,
    interp_ease_out_quad,
    interp_ease_in_out_quad,
    interp_ease_in_cubic,
    interp_ease_out_cubic,
    interp_ease_in_out_cubic,
    interp_ease_in_quart,
    interp_ease_out_quart,
    interp_ease_in_out_quart,
    interp_ease_in_quint,
    interp_ease_out_quint,
    interp_ease_in_out_quint,
    interp_ease_in_sin,
    interp_ease_out_sin,
    interp_ease_in_out_sin,
    interp_ease_in_circ,
    interp_ease_out_circ,
    interp_ease_in_out_circ,
    interp_ease_in_bounce,
    interp_ease_out_bounce,
    interp_ease_in_out_bounce,
};

// OpenRFG: the hkdiff seam for the easing functions (static in rfg.exe).
float vint_tween_interp(int _algorithm, float _time, float _start, float _delta)
{
    return Interp_functions[_algorithm](_time, _start, _delta);
}

static vint_prop_lookup Prop_lookup_vint_object_tween[18] = { // 0x014F5C68
    VINT_PROP_ENTRY(vint_object_tween, target_handle, VINT_PROP_TYPE_UINT64, 0, false),
    VINT_PROP_ENTRY(vint_object_tween, target_name, VINT_PROP_TYPE_STRING, (const char*)nullptr, true),
    VINT_PROP_ENTRY(vint_object_tween, target_name_crc, VINT_PROP_TYPE_UINT, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, target_property, VINT_PROP_TYPE_STRING, (const char*)nullptr, true),
    VINT_PROP_ENTRY(vint_object_tween, state, VINT_PROP_TYPE_ENUM, 0, false),
    VINT_PROP_ENTRY(vint_object_tween, start_time, VINT_PROP_TYPE_FLOAT, 0.0f, true),
    VINT_PROP_ENTRY(vint_object_tween, duration, VINT_PROP_TYPE_FLOAT, 0.0f, true),
    VINT_PROP_ENTRY(vint_object_tween, start_value, VINT_PROP_TYPE_VARIABLE, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, end_value, VINT_PROP_TYPE_VARIABLE, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, loop_mode, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, algorithm, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, max_loops, VINT_PROP_TYPE_FLOAT, 1.0f, true),
    VINT_PROP_ENTRY(vint_object_tween, start_event, VINT_PROP_TYPE_CALLBACK, false, false),
    VINT_PROP_ENTRY(vint_object_tween, end_event, VINT_PROP_TYPE_CALLBACK, false, false),
    VINT_PROP_ENTRY(vint_object_tween, per_frame_event, VINT_PROP_TYPE_CALLBACK, false, false),
    VINT_PROP_ENTRY(vint_object_tween, start_value_type, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, end_value_type, VINT_PROP_TYPE_ENUM, 0, true),
    VINT_PROP_ENTRY(vint_object_tween, reverse, VINT_PROP_TYPE_BOOL, false, true),
};
static vint_prop_mgr Prop_mgr_vint_object_tween;
static vint_object_factory_imp<vint_object_tween> Factory_vint_object_tween;

// ---- header-inline members

// 0x004762E0 (.h:92)
vint_object_tween::~vint_object_tween()
{
    m_per_frame_callback.clear();
    m_end_callback.clear();
    m_start_callback.clear();
    m_end_value.clear();
    m_start_value.clear();
}

// 0x00476360 (.h:99)
bool vint_object_tween::is_type(unsigned int _type) const
{
    return _type == Object_type;
}

// 0x00476380 (.h:99)
unsigned int vint_object_tween::get_type() const
{
    return Object_type;
}

// 0x008940B0 (.h:119)
void vint_object_tween::zero_start_callback()
{
    m_start_callback.zero();
}

// 0x008940C0 (.h:120)
void vint_object_tween::zero_end_callback()
{
    m_end_callback.zero();
}

// 0x0045BD40 (.h:126)
vint_tween_state vint_object_tween::get_state()
{
    return m_state;
}

// 0x0045BD50 (.h:127)
unsigned int vint_object_tween::get_target_name_crc()
{
    return m_target_obj_name_crc;
}

// ---- vint_object_tween

// 0x004769D0 (:404)
vint_object_tween::vint_object_tween()
{
    // the callbacks are callback-typed from the start
    m_start_callback = (vint_callback*)nullptr;
    m_end_callback = (vint_callback*)nullptr;
    m_per_frame_callback = (vint_callback*)nullptr;
}

// 0x0047D690 (:411)
bool vint_object_tween::system_init()
{
    Int_obj_type = vint_register_object_class("tween", 0xFFFFFFFF, Prop_mgr_vint_object_tween, Prop_lookup_vint_object_tween, 18, Factory_vint_object_tween, 576);
    Internal_vint_object_tween_state_wrapper.init(Object_type, "state");
    Internal_vint_object_tween_loop_mode_wrapper.init(Object_type, "loop_mode");
    Internal_vint_object_tween_algorithm_wrapper.init(Object_type, "algorithm");
    Internal_vint_object_tween_start_value_type_wrapper.init(Object_type, "start_value_type");
    Internal_vint_object_tween_end_value_type_wrapper.init(Object_type, "end_value_type");
    return true;
}

// 0x0045CA50 (:424)
void vint_object_tween::system_shutdown()
{
}

// 0x00474500 (:429)
bool vint_object_tween::init()
{
    // the base init, inlined in rfg.exe
    vint_object_base::init();
    m_start_value.clear();
    m_end_value.clear();
    m_start_callback.zero();
    m_end_callback.zero();
    m_per_frame_callback.zero();
    m_target = nullptr;
    m_target_obj_handle = 0;
    m_target_obj_name_crc = 0;
    m_target_prop_name_crc = 0;
    m_target_prop_lookup = nullptr;
    m_loop_mode = VINT_TWEEN_LOOP_ONCE;
    m_algorithm = VINT_TWEEN_ALG_LINEAR;
    m_max_loops = 0;
    m_state = VINT_TWEEN_STATE_WAITING;
    m_priority = 0;
    m_reverse = false;
    m_duration = 0.0f;
    m_start_time = 0.0f;
    m_last_interp = 0.0f;
    // Original: both default to stage, not to the property table's absolute
    m_start_value_type = VINT_TWEEN_POSN_VALUE_TYPE_STAGE;
    m_end_value_type = VINT_TWEEN_POSN_VALUE_TYPE_STAGE;
    return true;
}

// 0x00474770 (:470)
vint_object_base* vint_object_tween::acquire_target() const
{
    vint_object_tween* self = const_cast<vint_object_tween*>(this);
    // a target destroyed since is dropped
    if (m_target != nullptr && m_target_obj_handle != m_target->get_handle())
    {
        self->m_target = nullptr;
        self->m_target_prop_lookup = nullptr;
    }
    if (m_target == nullptr)
    {
        // by handle (in the tween's document, if it has one), else by name
        if (m_target_obj_handle != 0)
        {
            vint_object_base* target = (vint_object_base*)m_target_obj_handle;
            if (m_target_obj_handle == 0xFFFFFFFF || target->get_handle() != m_target_obj_handle || (get_parent_document() != nullptr && target->get_parent_document() != get_parent_document()))
            {
                target = nullptr;
            }
            self->m_target = target;
            self->m_target_prop_lookup = nullptr;
        }
        if (m_target == nullptr && m_target_obj_name_crc != 0)
        {
            self->m_target = vint_object_base::find_object_by_name_crc(m_target_obj_name_crc, get_parent_document());
            if (m_target != nullptr)
            {
                self->m_target_obj_handle = m_target->get_handle();
                self->m_target_prop_lookup = nullptr;
            }
        }
    }
    if (m_target != nullptr && m_target_prop_lookup == nullptr && m_target_prop_name_crc != 0)
    {
        const vint_object_type_info* info = vint_object_types_get_type_info(m_target->get_type());
        self->m_target_prop_lookup = info != nullptr && info->prop_mgr != nullptr ? info->prop_mgr->get_lookup(m_target_prop_name_crc) : nullptr;
    }
    if (m_target != nullptr)
    {
        self->m_target_obj_name_crc = m_target->get_name_crc();
    }
    return m_target;
}

// 0x004682F0 (:507)
void vint_object_tween::process()
{
}

// 0x0045CA60 (:515)
static bool add_tween_for_processing(vint_object_base* _t)
{
    if (Tween_process_list == nullptr)
    {
        return false;
    }
    if (!_t->is_type(vint_object_tween::Object_type) || _t->get_handle() == 0)
    {
        // Original: stops the factory walk
        return false;
    }
    vint_object_tween* tween = (vint_object_tween*)_t;
    vint_tween_state state = tween->get_state();
    if (state != VINT_TWEEN_STATE_DISABLED && state != VINT_TWEEN_STATE_PAUSED)
    {
        Tween_process_list[Tweens_to_process++] = tween;
    }
    return true;
}

// 0x0045CAC0 (:531)
int vint_object_tween::compare_tweens_for_process(const void* _left, const void* _right)
{
    return (*(vint_object_tween* const*)_left)->m_priority - (*(vint_object_tween* const*)_right)->m_priority;
}

// 0x0047D7E0 (:539)
void vint_object_tween::process_all()
{
    // a list on the stack for the whole pool
    vint_object_tween* list[576];
    Tween_process_list = list;
    Tweens_to_process = 0;
    const vint_object_type_info* info = vint_object_types_get_type_info(Object_type);
    info->factory->for_each_active(add_tween_for_processing);
    if (Tweens_to_process != 0)
    {
        // by end time
        qsort(Tween_process_list, Tweens_to_process, sizeof(vint_object_tween*), compare_tweens_for_process);
        for (unsigned int i = 0; i < Tweens_to_process; ++i)
        {
            Tween_process_list[i]->process_internal();
        }
    }
    Tweens_to_process = 0;
    Tween_process_list = nullptr;
}

// 0x0045CAE0 (:564)
void vint_object_tween::update_priority()
{
    // the end time in 1e-8 s units
    m_priority = (int)(((double)m_duration + (double)m_start_time) * 1e8);
}

// 0x00476A80 (:569)
void vint_object_tween::process_internal()
{
    if (get_handle() == 0 || m_state == VINT_TWEEN_STATE_DISABLED || m_state == VINT_TWEEN_STATE_PAUSED)
    {
        return;
    }
    vint_variant_type type = m_start_value.get_type();
    if (type == VINT_PROP_TYPE_NONE || m_end_value.get_type() != type)
    {
        return;
    }
    vint_object_base* target = acquire_target();
    if (target == nullptr || m_target_prop_lookup == nullptr)
    {
        return;
    }
    if (m_target_prop_lookup->type != type && m_target_prop_lookup->type != VINT_PROP_TYPE_VARIABLE)
    {
        return;
    }
    // the parent animation's clock, else the document's
    float time_index;
    vint_object_animation* anim = (vint_object_animation*)parent_object;
    if (anim != nullptr && anim->is_type(vint_object_animation::Object_type))
    {
        time_index = anim->get_time_index();
    }
    else
    {
        time_index = get_parent_document()->get_time_index();
    }
    float t = (float)((double)time_index - (double)m_start_time);
    vint_tween_loop loop = m_loop_mode;
    vint_tween_state old_state = m_state;
    bool started = false;
    bool ended = false;
    if (loop == VINT_TWEEN_LOOP_ONCE)
    {
        if (old_state != VINT_TWEEN_STATE_RUNNING && !(t < 0.0f) && (float)((double)t - (double)m_duration) < 0.0f)
        {
            m_state = VINT_TWEEN_STATE_RUNNING;
            started = true;
        }
        else if (old_state != VINT_TWEEN_STATE_FINISHED && (float)((double)m_duration - (double)t) < 0.0f)
        {
            ended = true;
            m_state = VINT_TWEEN_STATE_FINISHED;
        }
    }
    else if (loop == VINT_TWEEN_LOOP_CYCLE || loop == VINT_TWEEN_LOOP_BOUNCE)
    {
        float max_loops = (float)m_max_loops;
        if (max_loops > 0.0f && (double)t > (double)m_duration * (double)max_loops)
        {
            ended = true;
            m_state = VINT_TWEEN_STATE_FINISHED;
        }
        else if (old_state != VINT_TWEEN_STATE_RUNNING && t > 0.0f)
        {
            m_state = VINT_TWEEN_STATE_RUNNING;
            started = true;
        }
    }
    if (t < 0.0f)
    {
        return;
    }
    float interp = 1.0f;
    if (m_state == VINT_TWEEN_STATE_FINISHED)
    {
        // the end value once
        if (old_state == VINT_TWEEN_STATE_FINISHED)
        {
            return;
        }
    }
    else
    {
        if (m_state == VINT_TWEEN_STATE_WAITING)
        {
            return;
        }
        float duration = m_duration;
        if (duration != 0.0f)
        {
            switch (loop)
            {
            case VINT_TWEEN_LOOP_ONCE:
                if ((double)duration >= (double)t)
                {
                    interp = (float)((double)t / (double)duration);
                }
                break;
            case VINT_TWEEN_LOOP_CYCLE:
            {
                // x87 fmod and divide
                float m = (float)fmod((double)t, (double)duration);
                interp = (float)((double)m / (double)duration);
                m_last_interp = interp;
                // Original: the start event fires every frame of a cycling tween
                started = true;
                break;
            }
            case VINT_TWEEN_LOOP_BOUNCE:
            {
                float m = (float)fmod((double)t, (double)duration);
                float f = (float)((double)m / (double)duration);
                float q = (float)((double)t / (double)duration);
                // odd passes run backwards
                int pass = (int)(float)floor((double)q);
                bool was_backwards = m_last_interp < 0.0f;
                if ((pass & 1) == 1)
                {
                    if (!was_backwards)
                    {
                        ended = true;
                    }
                    float v = (float)(1.0 - (double)f);
                    // the sign marks a backwards pass
                    m_last_interp = -v;
                    interp = v;
                }
                else
                {
                    if (was_backwards)
                    {
                        started = true;
                    }
                    m_last_interp = f;
                    interp = f;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    if (m_reverse)
    {
        interp = (float)(1.0 - (double)interp);
    }
    vint_variant value;
    float (*fn)(float, float, float) = Interp_functions[m_algorithm];
    if (fn != nullptr && type == VINT_PROP_TYPE_FLOAT)
    {
        float delta = (float)((double)(float)m_end_value - (double)(float)m_start_value);
        value = fn(interp, (float)m_start_value, delta);
    }
    else if (fn != nullptr && type == VINT_PROP_TYPE_COLOR)
    {
        vint_variant_color s = (vint_variant_color)m_start_value;
        vint_variant_color e = (vint_variant_color)m_end_value;
        float b = fn(interp, s.b, (float)((double)e.b - (double)s.b));
        float g = Interp_functions[m_algorithm](interp, s.g, (float)((double)e.g - (double)s.g));
        float r = Interp_functions[m_algorithm](interp, s.r, (float)((double)e.r - (double)s.r));
        value.set_color(r, g, b);
    }
    else if (fn != nullptr && type == VINT_PROP_TYPE_VECTOR2F)
    {
        vector2 s = (vector2)m_start_value;
        vector2 e = (vector2)m_end_value;
        float x = fn(interp, s.x, (float)((double)e.x - (double)s.x));
        float y = Interp_functions[m_algorithm](interp, s.y, (float)((double)e.y - (double)s.y));
        value.set_vector2f(x, y);
    }
    else
    {
        // the other types jump to the end value
        value = m_end_value;
    }
    m_target_prop_lookup->set(target, value, 0);
    vint_tween_event_data data;
    if (started)
    {
        if (vint_callback* cb = (vint_callback*)m_start_callback)
        {
            data.tween = this;
            data.event_name = "start_event";
            cb->trigger(get_parent_document()->get_handle(), VINT_EVENT_TWEEN, &data, sizeof(data));
        }
    }
    // a callback may destroy the tween
    if (get_handle() != 0)
    {
        if (ended)
        {
            if (vint_callback* cb = (vint_callback*)m_end_callback)
            {
                data.tween = this;
                data.event_name = "end_event";
                cb->trigger(get_parent_document()->get_handle(), VINT_EVENT_TWEEN, &data, sizeof(data));
            }
        }
        if (get_handle() != 0)
        {
            if (vint_callback* cb = (vint_callback*)m_per_frame_callback)
            {
                data.tween = this;
                data.event_name = "per_frame_event";
                cb->trigger(get_parent_document()->get_handle(), VINT_EVENT_TWEEN, &data, sizeof(data));
            }
        }
    }
}

// 0x00474870 (:789)
void vint_object_tween::reset_tween_object()
{
    vint_object_base* target = acquire_target();
    if (target != nullptr && m_target_prop_lookup != nullptr)
    {
        m_target_prop_lookup->set(target, m_start_value, 0);
    }
}

// 0x004748A0 (:801)
void vint_object_tween::clean_up()
{
    m_start_callback.clear();
    m_end_callback.clear();
    m_per_frame_callback.clear();
}

// 0x0045CB10 (:807), 0x0045CB30 (:817)
VINT_PROP_WRAPPERS(vint_object_tween, target_handle)

// 0x004748F0 (:808)
bool vint_object_tween::get_target_handle(vint_variant& _val, int _index) const
{
    acquire_target();
    if (_val.get_type() == VINT_PROP_TYPE_NONE)
    {
        _val = (uint64_t)0;
    }
    _val = m_target_obj_handle;
    return m_target_obj_handle != 0;
}

// 0x0045CB50 (:818)
bool vint_object_tween::set_target_handle(const vint_variant& _val, int _index)
{
    m_target_obj_handle = (unsigned int)_val;
    m_target = nullptr;
    m_target_obj_name_crc = 0;
    return true;
}

// 0x0045CBE0 (:828), 0x0045CC10 (:849)
VINT_PROP_WRAPPERS(vint_object_tween, target_name)

// 0x0045CC00 (:829)
bool vint_object_tween::get_target_name(vint_variant& _val, int _index) const
{
    return false;
}

// 0x0045CC30 (:850)
bool vint_object_tween::set_target_name(const vint_variant& _val, int _index)
{
    m_target = nullptr;
    m_target_obj_handle = 0;
    m_target_obj_name_crc = crc_stri(_val.get_type() == VINT_PROP_TYPE_STRING ? (const char*)_val : nullptr, 0);
    return true;
}

// 0x0045CC70 (:860), 0x0045CC90 (:869)
VINT_PROP_WRAPPERS(vint_object_tween, target_name_crc)

// 0x00471AC0 (:861)
bool vint_object_tween::get_target_name_crc(vint_variant& _val, int _index) const
{
    _val = m_target_obj_name_crc;
    return true;
}

// 0x0045CCB0 (:870)
bool vint_object_tween::set_target_name_crc(const vint_variant& _val, int _index)
{
    m_target = nullptr;
    m_target_obj_handle = 0;
    m_target_obj_name_crc = (unsigned int)_val;
    return true;
}

// 0x0045CD50 (:880), 0x0045CD70 (:895)
VINT_PROP_WRAPPERS(vint_object_tween, target_property)

// 0x00474930 (:881)
bool vint_object_tween::get_target_property(vint_variant& _val, int _index) const
{
    acquire_target();
    if (m_target_prop_lookup != nullptr)
    {
        _val = m_target_prop_lookup->name;
    }
    return m_target_prop_lookup != nullptr;
}

// 0x0045CD90 (:896)
bool vint_object_tween::set_target_property(const vint_variant& _val, int _index)
{
    m_target_prop_lookup = nullptr;
    m_target_prop_name_crc = crc_stri(_val.get_type() == VINT_PROP_TYPE_STRING ? (const char*)_val : nullptr, 0);
    return true;
}

// 0x0045CDD0 (:905), 0x0045CDF0 (:913)
VINT_PROP_WRAPPERS(vint_object_tween, state)

// 0x00468300 (:906)
bool vint_object_tween::get_state(vint_variant& _val, int _index) const
{
    _val = (int)m_state;
    return true;
}

// 0x0045CE10 (:914)
bool vint_object_tween::set_state(const vint_variant& _val, int _index)
{
    int state = (int)_val;
    if (state < 0 || state >= NUM_VINT_TWEEN_STATES)
    {
        return false;
    }
    m_state = (vint_tween_state)state;
    return true;
}

// 0x0045CF60 (:949), 0x0045CFC0 (:957)
VINT_PROP_WRAPPERS(vint_object_tween, start_time)

// 0x0045CF80 (:950)
bool vint_object_tween::get_start_time(vint_variant& _val, int _index) const
{
    _val = m_start_time;
    return true;
}

// 0x0045CFE0 (:958)
bool vint_object_tween::set_start_time(const vint_variant& _val, int _index)
{
    m_start_time = (float)_val;
    update_priority();
    return true;
}

// 0x0045CE80 (:928), 0x0045CEE0 (:936)
VINT_PROP_WRAPPERS(vint_object_tween, duration)

// 0x0045CEA0 (:929)
bool vint_object_tween::get_duration(vint_variant& _val, int _index) const
{
    _val = m_duration;
    return true;
}

// 0x0045CF00 (:937)
bool vint_object_tween::set_duration(const vint_variant& _val, int _index)
{
    // not negative
    m_duration = (float)_val;
    if (m_duration < 0.0f)
    {
        m_duration = 0.0f;
    }
    update_priority();
    return true;
}

// 0x0045D030 (:966), 0x0045D050 (:974)
VINT_PROP_WRAPPERS(vint_object_tween, start_value)

// 0x00474960 (:967)
bool vint_object_tween::get_start_value(vint_variant& _val, int _index) const
{
    _val = m_start_value;
    return m_start_value.get_type() != VINT_PROP_TYPE_NONE;
}

// 0x00474980 (:975)
bool vint_object_tween::set_start_value(const vint_variant& _val, int _index)
{
    // takes the value's type
    m_start_value.clear();
    m_start_value = _val;
    return true;
}

// 0x0045D070 (:983), 0x0045D090 (:991)
VINT_PROP_WRAPPERS(vint_object_tween, end_value)

// 0x004749B0 (:984)
bool vint_object_tween::get_end_value(vint_variant& _val, int _index) const
{
    _val = m_end_value;
    return m_end_value.get_type() != VINT_PROP_TYPE_NONE;
}

// 0x004749D0 (:992)
bool vint_object_tween::set_end_value(const vint_variant& _val, int _index)
{
    m_end_value.clear();
    m_end_value = _val;
    return true;
}

// 0x0045D0B0 (:1000), 0x0045D0D0 (:1008)
VINT_PROP_WRAPPERS(vint_object_tween, loop_mode)

// 0x00468320 (:1001)
bool vint_object_tween::get_loop_mode(vint_variant& _val, int _index) const
{
    _val = (int)m_loop_mode;
    return true;
}

// 0x0045D0F0 (:1009)
bool vint_object_tween::set_loop_mode(const vint_variant& _val, int _index)
{
    int mode = (int)_val;
    if (mode < 0 || mode >= NUM_VINT_TWEEN_LOOPS)
    {
        return false;
    }
    m_loop_mode = (vint_tween_loop)mode;
    return true;
}

// 0x0045D160 (:1023), 0x0045D180 (:1031)
VINT_PROP_WRAPPERS(vint_object_tween, algorithm)

// 0x00468340 (:1024)
bool vint_object_tween::get_algorithm(vint_variant& _val, int _index) const
{
    _val = (int)m_algorithm;
    return true;
}

// 0x0045D1A0 (:1032)
bool vint_object_tween::set_algorithm(const vint_variant& _val, int _index)
{
    int algorithm = (int)_val;
    if (algorithm < 0 || algorithm >= NUM_VINT_TWEEN_ALGRORITHMS)
    {
        return false;
    }
    m_algorithm = (vint_tween_algorithm)algorithm;
    return true;
}

// 0x0045D210 (:1046), 0x0045D230 (:1054)
VINT_PROP_WRAPPERS(vint_object_tween, max_loops)

// 0x00468360 (:1047)
bool vint_object_tween::get_max_loops(vint_variant& _val, int _index) const
{
    _val = m_max_loops;
    return true;
}

// 0x0045D250 (:1055)
bool vint_object_tween::set_max_loops(const vint_variant& _val, int _index)
{
    m_max_loops = (int)_val;
    return true;
}

// 0x0045D2F0 (:1070), 0x0045D2D0 (:1062)
VINT_PROP_WRAPPERS(vint_object_tween, start_event)

// 0x00474A00 (:1063)
bool vint_object_tween::set_start_event(const vint_variant& _val, int _index)
{
    m_start_callback = _val;
    return true;
}

// 0x00474A20 (:1071)
bool vint_object_tween::get_start_event(vint_variant& _val, int _index) const
{
    _val = m_start_callback;
    return true;
}

// 0x0045D330 (:1086), 0x0045D310 (:1078)
VINT_PROP_WRAPPERS(vint_object_tween, end_event)

// 0x00474A40 (:1079)
bool vint_object_tween::set_end_event(const vint_variant& _val, int _index)
{
    m_end_callback = _val;
    return true;
}

// 0x00474A60 (:1087)
bool vint_object_tween::get_end_event(vint_variant& _val, int _index) const
{
    _val = m_end_callback;
    return true;
}

// 0x0045D370 (:1102), 0x0045D350 (:1094)
VINT_PROP_WRAPPERS(vint_object_tween, per_frame_event)

// 0x00474A80 (:1095)
bool vint_object_tween::set_per_frame_event(const vint_variant& _val, int _index)
{
    m_per_frame_callback = _val;
    return true;
}

// 0x00474AA0 (:1103)
bool vint_object_tween::get_per_frame_event(vint_variant& _val, int _index) const
{
    _val = m_per_frame_callback;
    return true;
}

// 0x0045D390 (:1110), 0x0045D3B0 (:1118)
VINT_PROP_WRAPPERS(vint_object_tween, start_value_type)

// 0x00468380 (:1111)
bool vint_object_tween::get_start_value_type(vint_variant& _val, int _index) const
{
    _val = (int)m_start_value_type;
    return true;
}

// 0x0045D3D0 (:1119)
bool vint_object_tween::set_start_value_type(const vint_variant& _val, int _index)
{
    int value_type = (int)_val;
    if (value_type < 0 || value_type >= NUM_VINT_TWEEN_POSN_VALUE_TYPES)
    {
        return false;
    }
    m_start_value_type = (vint_tween_position_value_type)value_type;
    return true;
}

// 0x0045D440 (:1133), 0x0045D460 (:1141)
VINT_PROP_WRAPPERS(vint_object_tween, end_value_type)

// 0x004683A0 (:1134)
bool vint_object_tween::get_end_value_type(vint_variant& _val, int _index) const
{
    _val = (int)m_end_value_type;
    return true;
}

// 0x0045D480 (:1142)
bool vint_object_tween::set_end_value_type(const vint_variant& _val, int _index)
{
    int value_type = (int)_val;
    if (value_type < 0 || value_type >= NUM_VINT_TWEEN_POSN_VALUE_TYPES)
    {
        return false;
    }
    m_end_value_type = (vint_tween_position_value_type)value_type;
    return true;
}

// 0x0045D4F0 (:1156), 0x0045D540 (:1163)
VINT_PROP_WRAPPERS(vint_object_tween, reverse)

// 0x0045D510 (:1157)
bool vint_object_tween::get_reverse(vint_variant& _val, int _index) const
{
    _val = m_reverse;
    return true;
}

// 0x0045D560 (:1164)
bool vint_object_tween::set_reverse(const vint_variant& _val, int _index)
{
    m_reverse = (bool)_val;
    return true;
}
