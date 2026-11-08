// Original: volition/interface_lib/code/vint_data_event.h
//
// A data event: a named record the game signals (the HUD's kill feed, messages); subscribers get each signal once. The
// methods are in vint_data_group.cpp.
#pragma once

#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_variant.h"

struct vint_data_group_subscription;

// size 0x128
class vint_data_event
{
public:
    vint_data_event(); // 0x0047C750 (vint_data_group.cpp:1314)
    ~vint_data_event(); // 0x00479DB0 (.h:37)
    void destroy(); // 0x00475CA0 (vint_data_group.cpp:1348)
    void set_template(const vint_variant_type* _element_types, int _num_elements); // 0x0046B890 (vint_data_group.cpp:1637)
    unsigned int get_handle() const; // 0x00461BF0 (.h:47)
    unsigned int get_name_crc() const;
    void set(const vint_variant* _ele_array, int _num_elements); // 0x00475F80 (vint_data_group.cpp:1441)
    void get(vint_variant* _ele_array, int _max_elements) const; // 0x00475FC0 (vint_data_group.cpp:1451)
    int get_num_elements() const; // 0x00461C00 (.h:53)
    static void add_subscription(unsigned int _de_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle); // 0x00463F40 (vint_data_group.cpp:1709)
    void add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x00463A50 (vint_data_group.cpp:1461)
    void remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x00463B60 (vint_data_group.cpp:1490)
    void set_element(int _index, const vint_variant& _e); // 0x00475EA0 (vint_data_group.cpp:1396)
    const vint_variant& get_element(int _index) const; // 0x00463A20 (vint_data_group.cpp:1429)
    static vint_data_event* create_new(const char* _name); // 0x00463CA0 (vint_data_group.cpp:1532)
    static vint_data_event* find(unsigned int _handle); // 0x00463E70 (vint_data_group.cpp:1608)
    static vint_data_event* find(const char* _name); // 0x00463E30 (vint_data_group.cpp:1601)
    static vint_data_event* find_by_name_crc(unsigned int _crc); // 0x00463E00 (vint_data_group.cpp:1575)
    static bool system_init(); // 0x00463C40 (vint_data_group.cpp:1509)
    static void process_all(); // 0x0046B970 (vint_data_group.cpp:1651)
    static void process_item(unsigned int _handle); // 0x0046BA00 (vint_data_group.cpp:1671)
    static void remove_doc_subscriptions(unsigned int doc_h); // 0x00464050 (vint_data_group.cpp:1745)
    void signal_data_item_update();
protected:
    void signal_update(); // 0x00463A40 (vint_data_group.cpp:1436)
    void init(const char* _name); // 0x004639B0 (vint_data_group.cpp:1319)
private:
    void process(); // 0x00463EB0 (vint_data_group.cpp:1692)

public:
    vint_data_event* container_prev; // 0x00
    vint_data_event* container_next; // 0x04
    vint_data_event* factory_prev; // 0x08
    vint_data_event* factory_next; // 0x0C
private:
    unsigned int m_name_crc; // 0x10
    vint_variant m_elements[8]; // 0x18
    int m_num_elements; // 0x118
    vint_data_group_subscription* m_subscriptions; // 0x11C
    unsigned int m_handle; // 0x120
    bool m_updated; // 0x124
};
