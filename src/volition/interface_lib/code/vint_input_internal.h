// Original: volition/interface_lib/code/vint_input_internal.h
// Implementation: volition/interface_lib/code/vint_input_internal.cpp
//
// Input subscriptions (a document's callback on an input event or raw input, with priority) and the per-event repeat
// state; input processing each frame. Placement of event_state inferred.
#pragma once

#include "volition/interface_lib/code/vint_input.h"
#include "volition/interface_lib/code/vint_variant.h"

struct event_state;
class vint_callback;
class vint_document;

enum vint_input_subscription_type
{
    VINT_INPUT_SUBSCRIPTION_UNKNOWN = -1,
    VINT_INPUT_SUBSCRIPTION_EVENT = 0,
    VINT_INPUT_SUBSCRIPTION_RAW = 1,
    VINT_NUM_INPUT_SUBSCRIPTION_TYPES = 2,
};

// size 0x58
class vint_input_subscription
{
public:
    static vint_input_subscription* find(unsigned int _handle); // 0x004655B0 (:259)
    static vint_input_subscription* create_new(vint_document* _doc, vint_callback* _callback, int _priority); // 0x0046C490 (:195)
    static void process_all(); // 0x0047CE30 (:313)
    void init_event(vint_input_event _event); // 0x0046C570 (:238)
    void event_acceleration_params(bool _smooth, float _accelleration_factor, float _accelleration_limit, float _repeat_time); // 0x00465570 (:249)
    void init_raw(unsigned int _map_index); // 0x00465550 (:231)
    void process(); // 0x00473050 (:352)
    void mask_input(); // 0x00465740 (:600)
    void release(); // 0x0047CDF0 (:285)
    unsigned int get_parent_doc() const; // 0x00465600 (:304)
    vint_input_subscription_type get_type() const; // 0x00461C40 (.h:62)
    unsigned int get_handle() const;
    const char* get_name() const; // 0x00469C80 (.h:65)
    float get_acceleration_scale() const; // 0x00461C50 (.h:78)
    float get_last_reported_time() const; // 0x00461C60 (.h:88)
private:
    void adjust_event_accelleration(event_state& es); // 0x00465610 (:443)
    void process_event(); // 0x0046C750 (:474)
    void process_raw(); // 0x0046C5F0 (:383)
    void return_to_factory(); // 0x00476200 (:343)
    void prioritize(); // 0x004654A0 (:159)
public:
    vint_input_subscription(); // 0x0047CDC0
    ~vint_input_subscription(); // 0x0047CDD0

    vint_input_subscription* factory_next; // 0x00
    vint_input_subscription* factory_prev; // 0x04
private:
    // friends are not recorded in rfg.pdb: the frame's processing removes the released subscriptions
    friend void vint_input_process();
    vint_variant m_callback; // 0x08
    int m_priority; // 0x28
    vint_document* m_parent_doc; // 0x2C
    unsigned int m_handle; // 0x30
    vint_input_subscription_type m_type; // 0x34
    bool m_delete; // 0x38
    union
    {
        struct
        {
            vint_input_event m_event; // 0x00
            float m_accelleration; // 0x04
            float m_scale_this_frame; // 0x08
            bool m_start_new_sequence; // 0x0C
            bool m_smooth; // 0x0D
            float m_accelleration_factor; // 0x10
            float m_accelleration_limit; // 0x14
            float m_repeat_time; // 0x18
        } event_props;
        struct
        {
            float m_last_reported; // 0x00
            unsigned int m_map_index; // 0x04
        } raw_props;
    }; // 0x3C
};

// size 0xC
struct event_state
{
    unsigned int last_triggered; // 0x00
    unsigned int next_delay; // 0x04
    unsigned int last_triggered_by; // 0x08
};

void vint_input_system_init(); // 0x00465380 (:116)
void vint_input_system_shutdown(); // 0x00465490 (:144)
void vint_input_remove_doc_subscriptions(unsigned int doc_handle); // 0x0047F660 (:149)
void vint_input_process(); // 0x0047CF70 (:650)
bool vint_input_button_map(const vint_button_map* _button_map, unsigned int _num_mappings); // 0x00465840 (:656)
bool vint_input_joy_map(const vint_joy_map* _joy_map, unsigned int _num_mappings); // 0x00465860 (:669)
bool vint_input_kb_map(const vint_kb_map* _kb_map, unsigned int _num_mappings); // 0x00465880 (:682)
const char* vint_input_get_event_name(vint_input_event _event); // 0x004658A0 (:695)
vint_input_event vint_input_get_event_code(const char* _name); // 0x004658C0 (:704)
bool vint_input_define_event_names(const char** _name_table, unsigned int _num_events); // 0x00465910 (:720)
bool vint_input_raw_map(vint_raw_map* _raw_map, unsigned int _num_mappings); // 0x00465920 (:728)
unsigned int vint_input_get_raw_map_index(const char* _name); // 0x00465950 (:737)
const char* vint_input_get_raw_map_name(unsigned int _map_index); // 0x004659A0 (:753)
