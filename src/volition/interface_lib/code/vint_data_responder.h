// Original: volition/interface_lib/code/vint_data_responder.h
// Implementation: volition/interface_lib/code/vint_data_responder.cpp
//
// Data responders: the objects that apply a data subscription's values to a document (a callback per update).
#pragma once

#include "volition/interface_lib/code/vint_variant.h"

// size 0x268
class vint_data_responder
{
public:
    vint_data_responder(); // 0x00479DE0 (.h:37)
    ~vint_data_responder(); // 0x00479E10 (.h:38)
    void destroy(); // 0x00476010 (:98)
    unsigned int get_handle() const; // 0x00461C10 (.h:41)
    void set_template(vint_variant_type* _element_types, int _num_elements); // 0x0046BBF0 (:296)
    void set_element(int _index, const vint_variant& _e); // 0x004760E0 (:310)
    void get_elements(vint_variant* _elements, unsigned int _num_elements); // 0x004761D0 (:341)
    unsigned int get_num_elements() const;
    void retrieve_data(const char* _response_func_name, int _max_num_records, vint_variant* _parameters, int _num_parameters); // 0x00464310 (:127)
    void submit_record(); // 0x0046BA50 (:137)
    void set_data_fulfillment_callback(bool (*)(int, vint_variant*, int));
    bool is_submission_complete(); // 0x00461C20 (.h:50)
    void set_submission_complete(bool);
    void set_parent_context(unsigned int context); // 0x00461C30 (.h:52)
    unsigned int get_parent_context();
    static bool system_init(); // 0x00464370 (:183)
    static vint_data_responder* create_new(const char* _name); // 0x004643A0 (:206)
    static vint_data_responder* find(unsigned int _handle); // 0x004644B0 (:268)
    static vint_data_responder* find(const char* _name); // 0x00464470 (:260)
    static vint_data_responder* find_by_name_crc(unsigned int _crc); // 0x00464440 (:234)
private:
    void init(const char* _name); // 0x004642B0 (:79)

public:
    vint_data_responder* factory_next; // 0x00
    vint_data_responder* factory_prev; // 0x04
private:
    unsigned int m_name_crc; // 0x08
    vint_variant m_elements[16]; // 0x10
    unsigned int m_num_elements; // 0x210
    unsigned int m_handle; // 0x214
    bool m_submission_complete; // 0x218
    unsigned int m_parent_context; // 0x21C
    char m_response_lua_func_name[64]; // 0x220
    bool (*m_fulfillment_func)(int, vint_variant*, int); // 0x260
};

extern vint_data_responder* Data_responders_free; // 0x01A486B0
extern vint_data_responder* Data_responders_used; // 0x01A486B4
