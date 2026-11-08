// Original: volition/interface_lib/code/vint_data_item.h
//
// A data item: a named record of up to 32 variants the game fills and documents subscribe to, optionally a row of a
// data group. The methods are in vint_data_group.cpp.
#pragma once

#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_variant.h"

class vint_data_group;
struct vint_data_group_subscription;

// size 0x430
class vint_data_item
{
public:
    vint_data_item(); // 0x0047C720 (vint_data_group.cpp:757)
    ~vint_data_item(); // 0x00479D80 (.h:39)
    void destroy(); // 0x004758F0 (vint_data_group.cpp:792)
    void set_template(const vint_variant_type* _element_types, int _num_elements); // 0x0046B6E0 (vint_data_group.cpp:1140)
    vint_data_group* get_data_group() const; // 0x00461BA0 (.h:49)
    unsigned int get_handle() const // 0x00461BB0 (.h:50)
    {
        return m_handle;
    }
    unsigned int get_name_crc() const;
    void set(const vint_variant* _ele_array, int _num_elements); // 0x00475C10 (vint_data_group.cpp:898)
    void get(vint_variant* _ele_array, int _max_elements) const; // 0x00475C50 (vint_data_group.cpp:908)
    int get_num_elements() const; // 0x00461BC0 (.h:56)
    static void add_subscription(unsigned int _di_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle); // 0x00463770 (vint_data_group.cpp:1235)
    void add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x004632A0 (vint_data_group.cpp:918)
    void remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x004633C0 (vint_data_group.cpp:950)
    void set_element(int _index, const vint_variant& _e); // 0x00475B10 (vint_data_group.cpp:845)
    void set_updated() { m_updated = true; } // inlined in rfg.exe (hud_ui_process_item_pickup_info)
    const vint_variant& get_element(int _index) const; // 0x00463280 (vint_data_group.cpp:891)
protected:
    static vint_data_item* create_new(vint_data_group* _group, const char* name); // 0x004635E0 (vint_data_group.cpp:1038)
public:
    static vint_data_item* create_new(const char* _name); // 0x0046B540 (vint_data_group.cpp:992)
    static vint_data_item* find(unsigned int _handle); // 0x004636F0 (vint_data_group.cpp:1103)
    static vint_data_item* find(const char* _name); // 0x004636B0 (vint_data_group.cpp:1096)
    static vint_data_item* find_by_name_crc(unsigned int _crc); // 0x00463680 (vint_data_group.cpp:1070)
    static bool system_init(); // 0x004634A0 (vint_data_group.cpp:969)
    static void process_all(); // 0x00472FC0 (vint_data_group.cpp:1166)
    static void process_item(unsigned int _handle); // 0x00473000 (vint_data_group.cpp:1188)
    static void remove_doc_subscriptions(unsigned int doc_h); // 0x00463880 (vint_data_group.cpp:1271)
    void signal_update(); // 0x00461BD0 (.h:75)
    void force_immediate_update(); // 0x0046B880 (vint_data_group.cpp:1293)
protected:
    void set_data_group(vint_data_group* _group); // 0x00461BE0 (.h:87)
    void init(const char* _name); // 0x00463200 (vint_data_group.cpp:762)
private:
    void process(); // 0x0046B7C0 (vint_data_group.cpp:1209)

public:
    vint_data_item* container_prev; // 0x00
    vint_data_item* container_next; // 0x04
    vint_data_item* factory_prev; // 0x08
    vint_data_item* factory_next; // 0x0C
private:
    // friends are not recorded in rfg.pdb: groups create and detach their items
    friend class vint_data_group;
    unsigned int m_name_crc; // 0x10
    vint_variant m_elements[32]; // 0x18
    int m_num_elements; // 0x418
    int m_index; // 0x41C
    vint_data_group* m_group; // 0x420
    vint_data_group_subscription* m_subscriptions; // 0x424
    unsigned int m_handle; // 0x428
    bool m_updated; // 0x42C
    bool m_inserted; // 0x42D
};
