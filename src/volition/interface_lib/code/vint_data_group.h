// Original: volition/interface_lib/code/vint_data_group.h
// Implementation: volition/interface_lib/code/vint_data_group.cpp
//
// Data groups (ordered lists of data items: insert, remove, update), the subscriptions documents hold on groups, items
// and events, and the free lists of all three.
#pragma once

#include "volition/interface_lib/code/vint_callback.h"

class vint_data_event;
class vint_data_item;

// size 0x20
struct vint_data_group_subscription
{
    unsigned int data_group_handle; // 0x00
    unsigned int data_item_handle; // 0x04
    unsigned int data_event_handle; // 0x08
    unsigned int doc_handle; // 0x0C
    unsigned int callback_handle; // 0x10
    vint_event_type event; // 0x14
    vint_data_group_subscription* next; // 0x18
    vint_data_group_subscription* prev; // 0x1C
};

// size 0x20
class vint_data_group
{
public:
    vint_data_group(); // 0x00461B60 (.h:34)
    ~vint_data_group(); // 0x00461B80 (.h:35)
    void destroy(); // 0x004625C0 (:170)
    unsigned int get_handle() const; // 0x00461B90 (.h:38)
    void set_name(const char*);
    vint_data_item* add_data_item(const char* name); // 0x0046B2C0 (:491)
    bool remove_data_item(unsigned int _handle); // 0x0046B310 (:507)
    void remove_all_data_items(); // 0x00462F20 (:518)
    bool remove_items_pending(); // 0x00462FA0 (:528)
    unsigned int get_num_data_items(); // 0x00462FB0 (:533)
    void signal_data_item_update(unsigned int _di_handle); // 0x0046B3E0 (:543)
    void signal_data_item_insert(unsigned int _di_handle); // 0x0046B490 (:560)
    vint_data_item* find_data_item(const char* _name); // 0x00462840 (:245)
    vint_data_item* first_data_item(); // 0x00462880 (:273)
    vint_data_item* next_data_item(vint_data_item* _cur_item); // 0x00462890 (:282)
    static void add_subscription(unsigned int _dg_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle); // 0x00462FD0 (:686)
    void add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x004628B0 (:303)
    void remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle); // 0x004629E0 (:354)
    static bool system_init(); // 0x00462AD0 (:371)
    static vint_data_group* create_new(const char* _name); // 0x00462D50 (:401)
    static vint_data_group* find(unsigned int _handle); // 0x00462EF0 (:461)
    static vint_data_group* find(const char* _name); // 0x00462EA0 (:448)
    static vint_data_group* find_by_name_crc(unsigned int _crc); // 0x00462810 (:219)
    static void process_all(); // 0x0047F100 (:577)
    static void process_group(unsigned int _handle); // 0x0047F1F0 (:596)
    static void remove_doc_subscriptions(unsigned int doc_h); // 0x004630E0 (:722)
private:
    void init(const char* _name); // 0x00462570 (:143)
    void process(); // 0x0047C650 (:653)
    void process_pending_delete(); // 0x0047C570 (:618)
    void release_to_factory(); // 0x00462660 (:192)

public:
    vint_data_group* factory_next; // 0x00
    vint_data_group* factory_prev; // 0x04
private:
    unsigned int m_name_crc; // 0x08
    vint_data_item* m_data_items; // 0x0C
    vint_data_item* m_remove_pending_items; // 0x10
    unsigned int m_handle; // 0x14
    vint_data_group_subscription* m_subscriptions; // 0x18
    bool m_delete_pending; // 0x1C
};

extern vint_data_group_subscription Subscription_pool[96]; // 0x01A3DB78
extern vint_data_item* Data_items_used; // 0x01A48684
extern vint_data_item* Data_items_free; // 0x01A48688
extern vint_data_event* Data_events_used; // 0x01A48690
extern vint_data_event* Data_events_free; // 0x01A48694
extern vint_data_group* Data_groups_free; // 0x01A4869C
extern vint_data_group* Data_groups_used; // 0x01A486A0
extern vint_data_group_subscription* Subscriptions_free; // 0x01A486A8
extern vint_data_group_subscription* Pending_subscriptions; // 0x01A486AC

void vint_data_remove_doc_subscriptions(unsigned int doc_h); // 0x00464180 (:1766)
