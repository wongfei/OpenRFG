// Original: volition/interface_lib/code/vint_input_internal.cpp
//
// Input (UI.md UI2): documents subscribe callbacks to input events (select, back, navigation, ...; mapped from buttons,
// stick directions and keys by the game's tables) or to raw inputs (a button's or stick axis's value). Subscriptions
// are ordered by document depth and priority; each frame the first subscription of an event that sees it pressed gets
// it (once per frame), with key repeat that accelerates while held, and every subscribed input is masked from the game.
// A pool of 64. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/vint_input_internal.h"

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_input.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/key.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/util/string_comp.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

// ---- file-static globals
static unsigned short Next_handle = 1; // 0x014F5ABC
static bool Events_triggered_this_frame[27]; // 0x01A3E778
static event_state Event_states[27]; // 0x01A484D8
static bool Processing_input; // 0x01A4863D
static const vint_button_map* Button_map; // 0x01A486F4
static unsigned int Num_button_mappings; // 0x01A486F8
static const vint_joy_map* Joy_map; // 0x01A486FC
static unsigned int Num_joy_mappings; // 0x01A48700
static const vint_kb_map* Kb_map; // 0x01A48704
static unsigned int Num_kb_mappings; // 0x01A48708
static vint_input_subscription* Avail_subscriptions; // 0x01A4870C
static vint_input_subscription* Active_subscriptions; // 0x01A48710
static const char** Event_names; // 0x01A48714
static unsigned int Num_raw_mappings; // 0x01A48718
static vint_raw_map* Raw_mappings; // 0x01A4871C
static vint_input_subscription Subscriptions_pool[64]; // 0x01A77390

// The size of a subscription, passed with it to the callbacks.
static const unsigned int VINT_INPUT_SUBSCRIPTION_BYTES = 0x58;

// The callback a subscription holds (rfg.exe inlines the handle lookup).
static vint_callback* vint_input_callback(const vint_variant& _callback)
{
    if (_callback.get_type() != VINT_PROP_TYPE_CALLBACK)
    {
        return nullptr;
    }
    return vint_callback::find_by_handle((unsigned int)_callback);
}

// ---- vint_input_subscription

// 0x0047CDC0
vint_input_subscription::vint_input_subscription()
{
}

// 0x0047CDD0
vint_input_subscription::~vint_input_subscription()
{
}

// 0x00461C40 (.h:62)
vint_input_subscription_type vint_input_subscription::get_type() const
{
    return m_type;
}

// 0x00469C80 (.h:65)
const char* vint_input_subscription::get_name() const
{
    if (m_type == VINT_INPUT_SUBSCRIPTION_EVENT)
    {
        return vint_input_get_event_name(event_props.m_event);
    }
    if (m_type == VINT_INPUT_SUBSCRIPTION_RAW)
    {
        return vint_input_get_raw_map_name(raw_props.m_map_index);
    }
    return nullptr;
}

// 0x00461C50 (.h:78)
float vint_input_subscription::get_acceleration_scale() const
{
    return m_type == VINT_INPUT_SUBSCRIPTION_EVENT ? event_props.m_scale_this_frame : 1.0f;
}

// 0x00461C60 (.h:88)
float vint_input_subscription::get_last_reported_time() const
{
    return m_type == VINT_INPUT_SUBSCRIPTION_RAW ? raw_props.m_last_reported : -1.0f;
}

// 0x004654A0 (:159)
void vint_input_subscription::prioritize()
{
    // into the active list: deeper documents first, then higher priority; within a document an "all unassigned"
    // subscription goes behind the others of its priority
    vint_input_subscription* head = Active_subscriptions;
    if (head == nullptr)
    {
        vint_dlist_add(Active_subscriptions, this, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
        return;
    }
    int depth = m_parent_doc->get_depth();
    vint_input_subscription* sub = head;
    for (;;)
    {
        int sub_depth = sub->m_parent_doc->get_depth();
        if (sub_depth > depth)
        {
            break;
        }
        bool catch_all = sub->m_type == VINT_INPUT_SUBSCRIPTION_EVENT && sub->event_props.m_event == VINT_INPUT_EVENT_ALL_UNASSIGNED;
        if (sub_depth == depth && ((sub->m_priority == m_priority && catch_all) || sub->m_priority < m_priority))
        {
            break;
        }
        sub = sub->factory_next;
        if (sub == Active_subscriptions || sub == nullptr)
        {
            // at the tail
            vint_dlist_add(Active_subscriptions, this, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
            return;
        }
    }
    // before sub
    sub->factory_prev->factory_next = this;
    factory_prev = sub->factory_prev;
    sub->factory_prev = this;
    factory_next = sub;
    if (sub == head)
    {
        Active_subscriptions = this;
    }
}

// 0x0046C490 (:195)
vint_input_subscription* vint_input_subscription::create_new(vint_document* _doc, vint_callback* _callback, int _priority)
{
    Vint_thread_id = get_current_thread_id();
    if (_doc == nullptr)
    {
        return nullptr;
    }
    vint_input_subscription* sub = Avail_subscriptions;
    if (sub == nullptr)
    {
        keen::debug::nativePrintArgs("vint out of input subscriptions");
        return nullptr;
    }
    vint_dlist_remove(Avail_subscriptions, sub, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
    // the slot index, and a creation count against stale handles
    sub->m_handle = (unsigned int)(sub - Subscriptions_pool) | ((unsigned int)Next_handle << 16);
    sub->m_priority = _priority;
    sub->m_parent_doc = _doc;
    sub->m_callback = _callback;
    ++Next_handle;
    sub->m_delete = false;
    sub->m_type = VINT_INPUT_SUBSCRIPTION_UNKNOWN;
    sub->prioritize();
    return sub;
}

// 0x00465550 (:231)
void vint_input_subscription::init_raw(unsigned int _map_index)
{
    // nothing reported yet: the first value is always delivered
    m_type = VINT_INPUT_SUBSCRIPTION_RAW;
    raw_props.m_map_index = _map_index;
    raw_props.m_last_reported = -1.0f;
}

// 0x0046C570 (:238)
void vint_input_subscription::init_event(vint_input_event _event)
{
    // repeat after 0.4 s, accelerating by 1.3 a repeat up to 5 times as fast
    event_props.m_accelleration_factor = 1.3f;
    event_props.m_accelleration_limit = 5.0f;
    m_type = VINT_INPUT_SUBSCRIPTION_EVENT;
    event_props.m_event = _event;
    event_props.m_start_new_sequence = true;
    event_props.m_smooth = false;
    event_props.m_repeat_time = 0.4f;
    // placed again now that the event is known
    vint_dlist_remove(Active_subscriptions, this, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
    prioritize();
}

// 0x00465570 (:249)
void vint_input_subscription::event_acceleration_params(bool _smooth, float _accelleration_factor, float _accelleration_limit, float _repeat_time)
{
    if (m_type == VINT_INPUT_SUBSCRIPTION_EVENT)
    {
        event_props.m_accelleration_factor = _accelleration_factor;
        event_props.m_accelleration_limit = _accelleration_limit;
        event_props.m_smooth = _smooth;
        event_props.m_repeat_time = _repeat_time;
    }
}

// 0x004655B0 (:259)
vint_input_subscription* vint_input_subscription::find(unsigned int _handle)
{
    Vint_thread_id = get_current_thread_id();
    unsigned int index = _handle & 0xFFFF;
    if (index >= 64)
    {
        return nullptr;
    }
    vint_input_subscription* sub = &Subscriptions_pool[index];
    return sub->m_handle == _handle && !sub->m_delete ? sub : nullptr;
}

// 0x0047CDF0 (:285)
void vint_input_subscription::release()
{
    // during input processing only marked (removed at the end of it)
    Vint_thread_id = get_current_thread_id();
    if (Processing_input)
    {
        m_delete = true;
        return;
    }
    return_to_factory();
}

// 0x00465600 (:304)
unsigned int vint_input_subscription::get_parent_doc() const
{
    return m_parent_doc != nullptr ? m_parent_doc->get_handle() : 0;
}

// 0x0047CE30 (:313)
void vint_input_subscription::process_all()
{
    vint_input_process();
}

// 0x00476200 (:343)
void vint_input_subscription::return_to_factory()
{
    m_callback.clear();
    m_handle = 0;
    vint_dlist_remove(Active_subscriptions, this, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
    vint_dlist_add(Avail_subscriptions, this, &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
}

// 0x00473050 (:352)
void vint_input_subscription::process()
{
    Vint_thread_id = get_current_thread_id();
    if (m_delete)
    {
        return;
    }
    if (m_type == VINT_INPUT_SUBSCRIPTION_EVENT)
    {
        process_event();
    }
    else if (m_type == VINT_INPUT_SUBSCRIPTION_RAW)
    {
        process_raw();
    }
}

// 0x0046C5F0 (:383)
void vint_input_subscription::process_raw()
{
    // the mapped button's value or stick axis, delivered when it moves by 0.05 or more (always the first time)
    Vint_thread_id = get_current_thread_id();
    if (raw_props.m_map_index >= Num_raw_mappings)
    {
        return;
    }
    const vint_raw_map& map = Raw_mappings[raw_props.m_map_index];
    float value = 0.0f;
    if (map.type == VINT_RAW_INPUT_BUTTON)
    {
        value = input_get_value(map.controller_id, map.button_or_stick);
        input_mask_button(map.controller_id, map.button_or_stick);
    }
    else if (map.type == VINT_RAW_INPUT_JOY)
    {
        float x;
        float y;
        input_get_joy(x, y, map.controller_id, map.button_or_stick);
        input_mask_joy_axis(map.controller_id, map.button_or_stick, map.axis);
        value = map.axis == 0 ? x : map.axis == 1 ? y : 0.0f;
    }
    float last = raw_props.m_last_reported;
    float diff = (float)((double)last - (double)value);
    double change = diff < 0.0f ? (double)diff * -1.0 : (double)diff;
    // 0.05 as a float constant
    if (!(last < 0.0f) && (double)(float)change < (double)0.05f)
    {
        return;
    }
    if (vint_callback* cb = vint_input_callback(m_callback))
    {
        raw_props.m_last_reported = value;
        cb->trigger(m_parent_doc->get_handle(), VINT_EVENT_INPUT, this, VINT_INPUT_SUBSCRIPTION_BYTES);
    }
}

// 0x00465610 (:443)
void vint_input_subscription::adjust_event_accelleration(event_state& es)
{
    // a held event repeats faster each time: the delay is the repeat time over the acceleration (smooth: every frame,
    // scaled by the frame time instead)
    bool new_sequence = event_props.m_start_new_sequence;
    if (new_sequence)
    {
        event_props.m_accelleration = 1.0f;
    }
    else
    {
        double factor = event_props.m_smooth ? ((double)event_props.m_repeat_time * (double)Frametime_real + 1.0) * (double)event_props.m_accelleration_factor : (double)event_props.m_accelleration_factor;
        event_props.m_accelleration = (float)(factor * (double)event_props.m_accelleration);
    }
    float accelleration = event_props.m_accelleration;
    if ((float)((double)accelleration - (double)event_props.m_accelleration_limit) >= 0.0f)
    {
        accelleration = event_props.m_accelleration_limit;
    }
    event_props.m_accelleration = accelleration;
    if (event_props.m_smooth && !new_sequence)
    {
        event_props.m_scale_this_frame = (float)((double)event_props.m_repeat_time * (double)Frametime_real * (double)accelleration);
        es.next_delay = 0;
        event_props.m_start_new_sequence = false;
        return;
    }
    event_props.m_scale_this_frame = 1.0f;
    // x87: milliseconds, truncated
    es.next_delay = (unsigned int)(long long)((double)event_props.m_repeat_time * 1000.0 * (1.0 / (double)accelleration));
    event_props.m_start_new_sequence = false;
}

// 0x0046C750 (:474)
void vint_input_subscription::process_event()
{
    if (input_is_all_masked())
    {
        return;
    }
    vint_input_event event = event_props.m_event;
    if (Events_triggered_this_frame[event])
    {
        return;
    }
    // "all unassigned" swallows every input left for the deeper documents
    if (event == VINT_INPUT_EVENT_ALL_UNASSIGNED)
    {
        input_mask_all();
        Events_triggered_this_frame[event_props.m_event] = true;
        return;
    }
    event_state& es = Event_states[event];
    // the time since the event last fired (Original: one less across the wraparound)
    unsigned int now = Frametime_total_milliseconds_increment_always;
    unsigned int elapsed = es.last_triggered < now ? now - es.last_triggered : now - es.last_triggered - 1;
    bool pressed = false;
    if (Button_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_button_mappings && !pressed; ++i)
        {
            pressed = Button_map[i].event == event_props.m_event && input_is_pressed(Button_map[i].controller_id, Button_map[i].button_id);
        }
    }
    if (!pressed && Joy_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_joy_mappings && !pressed; ++i)
        {
            const vint_joy_map& map = Joy_map[i];
            if (map.event != event_props.m_event)
            {
                continue;
            }
            float x;
            float y;
            input_get_joy(x, y, map.controller_id, map.joy_id);
            switch (map.direction)
            {
            case INPUT_DIR_LEFT:
                if (input_get_invert_x(map.controller_id, map.joy_id))
                {
                    x = (float)((double)x * -1.0);
                }
                pressed = (double)map.threshold > (double)x;
                break;
            case INPUT_DIR_RIGHT:
                if (input_get_invert_x(map.controller_id, map.joy_id))
                {
                    x = (float)((double)x * -1.0);
                }
                pressed = (double)x > (double)map.threshold;
                break;
            case INPUT_DIR_DOWN:
                if (input_get_invert_y(map.controller_id, map.joy_id))
                {
                    y = (float)((double)y * -1.0);
                }
                pressed = (double)map.threshold > (double)y;
                break;
            case INPUT_DIR_UP:
                if (input_get_invert_y(map.controller_id, map.joy_id))
                {
                    y = (float)((double)y * -1.0);
                }
                pressed = (double)y > (double)map.threshold;
                break;
            default:
                break;
            }
        }
    }
    if (!pressed && Kb_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_kb_mappings && !pressed; ++i)
        {
            pressed = Kb_map[i].event == event_props.m_event && key_down(Kb_map[i].scancode);
        }
    }
    if (!pressed)
    {
        // released: the next press starts a new repeat sequence
        if (m_handle == es.last_triggered_by)
        {
            es.next_delay = 0;
        }
        event_props.m_start_new_sequence = true;
        return;
    }
    if (es.next_delay >= elapsed)
    {
        return;
    }
    if (vint_callback* cb = vint_input_callback(m_callback))
    {
        adjust_event_accelleration(es);
        cb->trigger(m_parent_doc->get_handle(), VINT_EVENT_INPUT, this, VINT_INPUT_SUBSCRIPTION_BYTES);
        es.last_triggered = Frametime_total_milliseconds_increment_always;
        Events_triggered_this_frame[event_props.m_event] = true;
        es.last_triggered_by = m_handle;
    }
}

// 0x00465740 (:600)
void vint_input_subscription::mask_input()
{
    // every input mapped to the event, so the game does not see it
    if (m_type != VINT_INPUT_SUBSCRIPTION_EVENT)
    {
        return;
    }
    if (Button_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_button_mappings; ++i)
        {
            if (Button_map[i].event == event_props.m_event)
            {
                input_mask_button(Button_map[i].controller_id, Button_map[i].button_id);
            }
        }
    }
    if (Joy_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_joy_mappings; ++i)
        {
            const vint_joy_map& map = Joy_map[i];
            if (map.event != event_props.m_event)
            {
                continue;
            }
            switch (map.direction)
            {
            case INPUT_DIR_LEFT:
            case INPUT_DIR_RIGHT:
                input_mask_joy_axis(map.controller_id, map.joy_id, 0);
                break;
            case INPUT_DIR_DOWN:
            case INPUT_DIR_UP:
                input_mask_joy_axis(map.controller_id, map.joy_id, 1);
                break;
            default:
                break;
            }
        }
    }
    if (Kb_map != nullptr)
    {
        for (unsigned int i = 0; i < Num_kb_mappings; ++i)
        {
            if (Kb_map[i].event == event_props.m_event)
            {
                key_mask(Kb_map[i].scancode);
            }
        }
    }
}

// ---- input

// 0x00465380 (:116)
void vint_input_system_init()
{
    Button_map = nullptr;
    Num_button_mappings = 0;
    Joy_map = nullptr;
    Num_joy_mappings = 0;
    Kb_map = nullptr;
    Num_kb_mappings = 0;
    // Original: the lists are not reset first (a second init links the pool again)
    for (int i = 0; i < 64; ++i)
    {
        vint_dlist_add(Avail_subscriptions, &Subscriptions_pool[i], &vint_input_subscription::factory_next, &vint_input_subscription::factory_prev);
    }
    for (int i = 0; i < 27; ++i)
    {
        Event_states[i].last_triggered = 0;
        Event_states[i].next_delay = 0;
        Event_states[i].last_triggered_by = 0;
    }
    Event_names = nullptr;
}

// 0x00465490 (:144)
void vint_input_system_shutdown()
{
}

// 0x0047F660 (:149)
void vint_input_remove_doc_subscriptions(unsigned int doc_handle)
{
    vint_input_subscription* sub = Active_subscriptions;
    while (sub != nullptr)
    {
        vint_input_subscription* next = sub->factory_next != Active_subscriptions ? sub->factory_next : nullptr;
        if (sub->get_parent_doc() == doc_handle)
        {
            sub->release();
        }
        sub = next;
    }
}

// 0x0047CF70 (:650)
void vint_input_process()
{
    // deliver, then mask everything subscribed, then remove the subscriptions released meanwhile
    for (int i = 0; i < 27; ++i)
    {
        Events_triggered_this_frame[i] = false;
    }
    Processing_input = true;
    vint_input_subscription* sub = Active_subscriptions;
    while (sub != nullptr)
    {
        sub->process();
        sub = sub->factory_next;
        if (sub == Active_subscriptions)
        {
            break;
        }
    }
    sub = Active_subscriptions;
    while (sub != nullptr)
    {
        sub->mask_input();
        sub = sub->factory_next;
        if (sub == Active_subscriptions)
        {
            break;
        }
    }
    sub = Active_subscriptions;
    while (sub != nullptr)
    {
        vint_input_subscription* next = sub->factory_next != Active_subscriptions ? sub->factory_next : nullptr;
        if (sub->m_delete)
        {
            sub->return_to_factory();
        }
        sub = next;
    }
    Processing_input = false;
}

// 0x00465840 (:656)
bool vint_input_button_map(const vint_button_map* _button_map, unsigned int _num_mappings)
{
    Button_map = _button_map;
    Num_button_mappings = _button_map != nullptr ? _num_mappings : 0;
    return true;
}

// 0x00465860 (:669)
bool vint_input_joy_map(const vint_joy_map* _joy_map, unsigned int _num_mappings)
{
    Joy_map = _joy_map;
    Num_joy_mappings = _joy_map != nullptr ? _num_mappings : 0;
    return true;
}

// 0x00465880 (:682)
bool vint_input_kb_map(const vint_kb_map* _kb_map, unsigned int _num_mappings)
{
    Kb_map = _kb_map;
    Num_kb_mappings = _kb_map != nullptr ? _num_mappings : 0;
    return true;
}

// 0x004658A0 (:695)
const char* vint_input_get_event_name(vint_input_event _event)
{
    if ((int)_event < NUM_VINT_INPUT_EVENTS && Event_names != nullptr)
    {
        return Event_names[_event];
    }
    return nullptr;
}

// 0x004658C0 (:704)
vint_input_event vint_input_get_event_code(const char* _name)
{
    if (_name == nullptr || Event_names == nullptr)
    {
        return VINT_INPUT_EVENT_UNKNOWN;
    }
    for (int i = 0; i < NUM_VINT_INPUT_EVENTS; ++i)
    {
        if (v_stricmp(_name, Event_names[i]) == 0)
        {
            return (vint_input_event)i;
        }
    }
    return VINT_INPUT_EVENT_UNKNOWN;
}

// 0x00465910 (:720)
bool vint_input_define_event_names(const char** _name_table, unsigned int _num_events)
{
    // Original: the count is not checked (the table must hold every event)
    Event_names = _name_table;
    return true;
}

// 0x00465920 (:728)
bool vint_input_raw_map(vint_raw_map* _raw_map, unsigned int _num_mappings)
{
    if (_raw_map == nullptr && _num_mappings != 0)
    {
        return false;
    }
    Raw_mappings = _raw_map;
    Num_raw_mappings = _num_mappings;
    return true;
}

// 0x00465950 (:737)
unsigned int vint_input_get_raw_map_index(const char* _name)
{
    if (_name == nullptr)
    {
        return 0xFFFFFFFF;
    }
    for (unsigned int i = 0; i < Num_raw_mappings; ++i)
    {
        if (v_stricmp(_name, Raw_mappings[i].name) == 0)
        {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

// 0x004659A0 (:753)
const char* vint_input_get_raw_map_name(unsigned int _map_index)
{
    if (_map_index >= Num_raw_mappings)
    {
        return nullptr;
    }
    return Raw_mappings[_map_index].name;
}
