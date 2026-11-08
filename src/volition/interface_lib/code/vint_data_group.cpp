// Original: volition/interface_lib/code/vint_data_group.cpp
//
// Data groups, items and events (UI.md UI2): the game publishes values to the interface through named data items
// (32 variants each, a pool of 128), optionally in an ordered group (a pool of 32), and fire-and-forget data events (8
// variants, a pool of 4). Documents subscribe a callback to an item (update), an event (update) or a group (update,
// insert, remove) from a pool of 96 subscriptions; a subscription to a name not created yet waits in the pending list
// and is adopted when it appears, and an object destroyed with a name hands its subscriptions back to the pending
// list. Changes are delivered once a frame by the process functions. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/vint_data_group.h"

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <string.h>

// ---- globals
vint_data_group_subscription Subscription_pool[96]; // 0x01A3DB78
vint_data_item* Data_items_used; // 0x01A48684
vint_data_item* Data_items_free; // 0x01A48688
vint_data_event* Data_events_used; // 0x01A48690
vint_data_event* Data_events_free; // 0x01A48694
vint_data_group* Data_groups_free; // 0x01A4869C
vint_data_group* Data_groups_used; // 0x01A486A0
vint_data_group_subscription* Subscriptions_free; // 0x01A486A8
vint_data_group_subscription* Pending_subscriptions; // 0x01A486AC

// ---- file-static globals
static unsigned int Data_item_count; // 0x01A4868C
static unsigned int Data_event_count; // 0x01A48698
static unsigned int Data_group_count; // 0x01A486A4
static vint_variant Vint_variant_empty; // 0x01A48CB8
static vint_data_event Data_events[4]; // 0x01A4EB20
static vint_data_group Data_groups[32]; // 0x01A4F040
static vint_data_item Data_items[128]; // 0x01A55B68

// The data the callbacks receive: the whole object (sizeof in rfg.exe).
static const unsigned int VINT_DATA_ITEM_BYTES = 0x430;
static const unsigned int VINT_DATA_EVENT_BYTES = 0x128;

// ---- subscription lists (rfg.exe inlines these at each use)

// At the tail.
static void vint_sub_add(vint_data_group_subscription*& _head, vint_data_group_subscription* _sub)
{
    if (_head == nullptr)
    {
        _sub->next = _sub;
        _sub->prev = _sub;
        _head = _sub;
        return;
    }
    _sub->prev = _head->prev;
    _sub->next = _head;
    _head->prev->next = _sub;
    _head->prev = _sub;
}

// Unlinks (the links are null-checked) and clears the links.
static void vint_sub_remove(vint_data_group_subscription*& _head, vint_data_group_subscription* _sub)
{
    if (_head == _sub && (_head = _sub->next) == _sub)
    {
        _head = nullptr;
    }
    else
    {
        if (_sub->prev != nullptr)
        {
            _sub->prev->next = _sub->next;
        }
        if (_sub->next != nullptr)
        {
            _sub->next->prev = _sub->prev;
        }
    }
    _sub->next = nullptr;
    _sub->prev = nullptr;
}

// The next node of a walk that may unlink the current one, taken before it does.
static vint_data_group_subscription* vint_sub_next(vint_data_group_subscription* _head, vint_data_group_subscription* _sub)
{
    return _sub->next != _head ? _sub->next : nullptr;
}

// A subscription back to the pool, its callback's reference released.
static void vint_sub_free(vint_data_group_subscription* _sub)
{
    if (vint_callback* cb = vint_callback::find_by_handle(_sub->callback_handle))
    {
        cb->release();
    }
    vint_sub_add(Subscriptions_free, _sub);
}

// A free subscription filled in (or null when the pool is empty).
static vint_data_group_subscription* vint_sub_alloc(unsigned int _cb_handle, unsigned int _group, unsigned int _item, unsigned int _event_object, unsigned int _doc_handle, vint_event_type _event)
{
    vint_data_group_subscription* sub = Subscriptions_free;
    if (sub == nullptr)
    {
        return nullptr;
    }
    vint_sub_remove(Subscriptions_free, sub);
    sub->callback_handle = _cb_handle;
    sub->data_group_handle = _group;
    sub->data_item_handle = _item;
    sub->data_event_handle = _event_object;
    sub->doc_handle = _doc_handle;
    sub->event = _event;
    return sub;
}

// The pending subscriptions with the given name in the given field move to an object's list; the field written is
// the group handle in all three callers (Original bug kept for items and events).
static void vint_sub_adopt_pending(unsigned int vint_data_group_subscription::*_field, unsigned int _name_crc, unsigned int _handle, vint_data_group_subscription*& _subscriptions)
{
    vint_data_group_subscription* sub = Pending_subscriptions;
    while (sub != nullptr)
    {
        vint_data_group_subscription* next = vint_sub_next(Pending_subscriptions, sub);
        if (sub->*_field == _name_crc)
        {
            vint_sub_remove(Pending_subscriptions, sub);
            sub->data_group_handle = _handle;
            vint_sub_add(_subscriptions, sub);
        }
        sub = next;
    }
}

// An object's subscriptions as it is destroyed: back to the pool for an unnamed object, else pending again under
// its name (in the given field) for an object of the same name created later.
static void vint_sub_release_all(vint_data_group_subscription*& _subscriptions, unsigned int _name_crc, unsigned int vint_data_group_subscription::*_field)
{
    vint_data_group_subscription* sub = _subscriptions;
    while (sub != nullptr)
    {
        vint_data_group_subscription* next = vint_sub_next(_subscriptions, sub);
        vint_sub_remove(_subscriptions, sub);
        if (_name_crc == 0)
        {
            vint_sub_free(sub);
        }
        else
        {
            sub->*_field = _name_crc;
            vint_sub_add(Pending_subscriptions, sub);
        }
        sub = next;
    }
}

// A document's subscriptions on one object back to the pool.
static void vint_sub_remove_doc(vint_data_group_subscription*& _subscriptions, unsigned int _doc_h)
{
    vint_data_group_subscription* sub = _subscriptions;
    while (sub != nullptr)
    {
        vint_data_group_subscription* next = vint_sub_next(_subscriptions, sub);
        if (sub->doc_handle == _doc_h)
        {
            vint_sub_remove(_subscriptions, sub);
            vint_sub_free(sub);
        }
        sub = next;
    }
}

// The first subscription matching a callback, document and event, unlinked and freed.
static void vint_sub_remove_first(vint_data_group_subscription*& _subscriptions, unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    vint_data_group_subscription* sub = _subscriptions;
    if (sub == nullptr)
    {
        return;
    }
    // Original bug kept: the search never moves past the first subscription, so any other one spins for ever
    while (sub->callback_handle != _cb_handle || sub->event != _event || sub->doc_handle != _doc_handle)
    {
    }
    // the head is always the one removed
    if (sub->next == sub)
    {
        sub->next = nullptr;
        sub->prev = nullptr;
        _subscriptions = nullptr;
    }
    else
    {
        _subscriptions = sub->next;
        if (sub->prev != nullptr)
        {
            sub->prev->next = sub->next;
        }
        if (sub->next != nullptr)
        {
            sub->next->prev = sub->prev;
        }
        sub->next = nullptr;
        sub->prev = nullptr;
    }
    vint_sub_free(sub);
}

// Triggers the subscriptions of one event type (or all of them with VINT_EVENT_UNKNOWN).
static void vint_sub_trigger(vint_data_group_subscription* _subscriptions, vint_event_type _filter, vint_event_type _event, void* _data, unsigned int _bytes)
{
    vint_data_group_subscription* sub = _subscriptions;
    while (sub != nullptr)
    {
        if (_filter == VINT_EVENT_UNKNOWN || sub->event == _filter)
        {
            if (vint_callback* cb = vint_callback::find_by_handle(sub->callback_handle))
            {
                cb->trigger(sub->doc_handle, _event, _data, _bytes);
            }
        }
        sub = sub->next;
        if (sub == _subscriptions)
        {
            break;
        }
    }
}

// A variant template applied to the elements: a typeless element takes its type (zeroed), a typed one keeps it.
static void vint_data_set_template(vint_variant* _elements, const vint_variant_type* _element_types, int _num_elements, int _max, const char* _message)
{
    for (int i = 0; i < _num_elements; ++i)
    {
        // Original bug kept: the bound tested is the count against the wrong limit (127 for the 32 item elements,
        // 3 for the 8 event elements)
        if (_num_elements > _max)
        {
            break;
        }
        if (_elements[i].get_type() == VINT_PROP_TYPE_NONE)
        {
            _elements[i].set_type(_element_types[i]);
        }
        else if (_elements[i].get_type() != _element_types[i])
        {
            keen::debug::nativePrintArgs(_message);
        }
    }
}

// An element's strings freed and the element cleared.
static void vint_data_clear_elements(vint_variant* _elements, int _count)
{
    for (int i = 0; i < _count; ++i)
    {
        if (_elements[i].get_type() == VINT_PROP_TYPE_STRING && (const char*)_elements[i] != nullptr && Memory_callbacks != nullptr && Memory_callbacks->free != nullptr)
        {
            Memory_callbacks->free((void*)(const char*)_elements[i]);
        }
        _elements[i].clear();
    }
}

// ---- vint_data_group

// 0x00461B60 (.h:34)
vint_data_group::vint_data_group()
{
    m_data_items = nullptr;
    m_remove_pending_items = nullptr;
    m_handle = 0;
    m_subscriptions = nullptr;
}

// 0x00461B80 (.h:35)
vint_data_group::~vint_data_group()
{
}

// 0x00461B90 (.h:38)
unsigned int vint_data_group::get_handle() const
{
    return m_handle;
}

// 0x00462570 (:143)
void vint_data_group::init(const char* _name)
{
    m_subscriptions = nullptr;
    m_data_items = nullptr;
    m_name_crc = crc_stri(_name, 0);
    ++Data_group_count;
    // the slot index, and a creation count against stale handles
    m_handle = (unsigned int)(this - Data_groups) + (Data_group_count << 16);
    m_delete_pending = false;
}

// 0x004625C0 (:170)
void vint_data_group::destroy()
{
    // the items go to the removal list (their remove callbacks fire when processed), the group with them
    Vint_thread_id = get_current_thread_id();
    vint_data_item* item = m_data_items;
    while (item != nullptr)
    {
        vint_data_item* next = item->container_next != m_data_items ? item->container_next : nullptr;
        vint_dlist_remove(m_data_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
        // Original: each item becomes the head of the removal list, reversing their order
        vint_dlist_add(m_remove_pending_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
        m_remove_pending_items = item;
        item = next;
    }
    m_delete_pending = true;
}

// 0x00462660 (:192)
void vint_data_group::release_to_factory()
{
    vint_sub_release_all(m_subscriptions, m_name_crc, &vint_data_group_subscription::data_group_handle);
    m_handle = 0;
    vint_dlist_remove(Data_groups_used, this, &vint_data_group::factory_next, &vint_data_group::factory_prev);
    vint_dlist_add(Data_groups_free, this, &vint_data_group::factory_next, &vint_data_group::factory_prev);
}

// 0x00462810 (:219)
vint_data_group* vint_data_group::find_by_name_crc(unsigned int _crc)
{
    if (_crc == 0)
    {
        return nullptr;
    }
    vint_data_group* group = Data_groups_used;
    while (group != nullptr)
    {
        if (group->m_name_crc == _crc && !group->m_delete_pending)
        {
            return group;
        }
        group = group->factory_next;
        if (group == Data_groups_used)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00462840 (:245)
vint_data_item* vint_data_group::find_data_item(const char* _name)
{
    if (_name == nullptr || m_delete_pending)
    {
        return nullptr;
    }
    unsigned int name_crc = crc_stri(_name, 0);
    vint_data_item* item = m_data_items;
    while (item != nullptr)
    {
        if (item->get_name_crc() == name_crc)
        {
            return item;
        }
        item = item->container_next;
        if (item == m_data_items)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00462880 (:273)
vint_data_item* vint_data_group::first_data_item()
{
    return m_delete_pending ? nullptr : m_data_items;
}

// 0x00462890 (:282)
vint_data_item* vint_data_group::next_data_item(vint_data_item* _cur_item)
{
    if (m_delete_pending)
    {
        return nullptr;
    }
    return _cur_item->container_next != m_data_items ? _cur_item->container_next : nullptr;
}

// 0x004628B0 (:303)
void vint_data_group::add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    // update, insert or remove; an insert subscription gets every item already there
    if (_event < VINT_EVENT_DATA_GROUP_UPDATE || _event > VINT_EVENT_DATA_GROUP_REMOVE)
    {
        keen::debug::nativePrintArgs("vint_data_group::add_callback() - This card not taken here.");
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_cb_handle);
    if (cb == nullptr || Subscriptions_free == nullptr)
    {
        return;
    }
    vint_data_group_subscription* sub = vint_sub_alloc(_cb_handle, m_handle, 0, 0, _doc_handle, _event);
    vint_sub_add(m_subscriptions, sub);
    cb->add_ref();
    if (_event == VINT_EVENT_DATA_GROUP_INSERT)
    {
        if (vint_callback* insert_cb = vint_callback::find_by_handle(sub->callback_handle))
        {
            vint_data_item* item = m_data_items;
            while (item != nullptr)
            {
                insert_cb->trigger(_doc_handle, VINT_EVENT_DATA_GROUP_INSERT, item, VINT_DATA_ITEM_BYTES);
                item = item->container_next;
                if (item == m_data_items)
                {
                    break;
                }
            }
        }
    }
}

// 0x004629E0 (:354)
void vint_data_group::remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    vint_sub_remove_first(m_subscriptions, _cb_handle, _event, _doc_handle);
}

// 0x00462AD0 (:371)
bool vint_data_group::system_init()
{
    Data_groups_free = nullptr;
    Data_groups_used = nullptr;
    for (int i = 0; i < 32; ++i)
    {
        vint_dlist_add(Data_groups_free, &Data_groups[i], &vint_data_group::factory_next, &vint_data_group::factory_prev);
    }
    Subscriptions_free = nullptr;
    Pending_subscriptions = nullptr;
    for (int i = 0; i < 96; ++i)
    {
        vint_sub_add(Subscriptions_free, &Subscription_pool[i]);
    }
    return true;
}

// 0x00462D50 (:401)
vint_data_group* vint_data_group::create_new(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    vint_data_group* group = Data_groups_free;
    if (group == nullptr)
    {
        keen::debug::nativePrintArgs("[vint_data_group.cpp] <create_new> dg is a nullptr! \n");
        return nullptr;
    }
    vint_dlist_remove(Data_groups_free, group, &vint_data_group::factory_next, &vint_data_group::factory_prev);
    vint_dlist_add(Data_groups_used, group, &vint_data_group::factory_next, &vint_data_group::factory_prev);
    group->init(_name);
    vint_sub_adopt_pending(&vint_data_group_subscription::data_group_handle, group->m_name_crc, group->m_handle, group->m_subscriptions);
    return group;
}

// 0x00462EA0 (:448)
vint_data_group* vint_data_group::find(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return find_by_name_crc(crc_stri(_name, 0));
}

// 0x00462EF0 (:461)
vint_data_group* vint_data_group::find(unsigned int _handle)
{
    unsigned int index = _handle & 0xFFFF;
    if (_handle == 0 || index >= 32)
    {
        return nullptr;
    }
    vint_data_group* group = &Data_groups[index];
    return group->m_handle == _handle && !group->m_delete_pending ? group : nullptr;
}

// 0x0046B2C0 (:491)
vint_data_item* vint_data_group::add_data_item(const char* name)
{
    if (m_delete_pending)
    {
        return nullptr;
    }
    vint_data_item* item = vint_data_item::create_new(this, name);
    if (item != nullptr)
    {
        vint_dlist_add(m_data_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
    }
    return item;
}

// 0x0046B310 (:507)
bool vint_data_group::remove_data_item(unsigned int _handle)
{
    // to the removal list: the remove callbacks fire and the item is destroyed when processed
    unsigned int index = _handle & 0xFFFF;
    if (_handle == 0 || _handle == 0xFFFFFFFF || index >= 128)
    {
        return false;
    }
    vint_data_item* item = &Data_items[index];
    if (item->get_handle() != _handle)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    if (item->get_data_group() != this)
    {
        return false;
    }
    vint_dlist_remove(m_data_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
    vint_dlist_add(m_remove_pending_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
    return true;
}

// 0x00462F20 (:518)
void vint_data_group::remove_all_data_items()
{
    vint_data_item* item = m_data_items;
    while (item != nullptr)
    {
        vint_data_item* next = item->container_next != m_data_items ? item->container_next : nullptr;
        vint_dlist_remove(m_data_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
        vint_dlist_add(m_remove_pending_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
        item = next;
    }
}

// 0x00462FA0 (:528)
bool vint_data_group::remove_items_pending()
{
    // Original: reports the group's pending deletion, not pending item removals
    return m_delete_pending;
}

// 0x00462FB0 (:533)
unsigned int vint_data_group::get_num_data_items()
{
    unsigned int count = 0;
    vint_data_item* item = m_data_items;
    while (item != nullptr)
    {
        item = item->container_next;
        ++count;
        if (item == m_data_items)
        {
            break;
        }
    }
    return count;
}

// 0x0046B3E0 (:543)
void vint_data_group::signal_data_item_update(unsigned int _di_handle)
{
    unsigned int index = _di_handle & 0xFFFF;
    if (_di_handle == 0 || _di_handle == 0xFFFFFFFF || index >= 128)
    {
        return;
    }
    if (Data_items[index].get_handle() != _di_handle)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    if (Data_items[index].get_data_group() == this)
    {
        vint_sub_trigger(m_subscriptions, VINT_EVENT_DATA_GROUP_UPDATE, VINT_EVENT_DATA_GROUP_UPDATE, &Data_items[index], VINT_DATA_ITEM_BYTES);
    }
}

// 0x0046B490 (:560)
void vint_data_group::signal_data_item_insert(unsigned int _di_handle)
{
    unsigned int index = _di_handle & 0xFFFF;
    if (_di_handle == 0 || _di_handle == 0xFFFFFFFF || index >= 128)
    {
        return;
    }
    if (Data_items[index].get_handle() != _di_handle)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    if (Data_items[index].get_data_group() == this)
    {
        vint_sub_trigger(m_subscriptions, VINT_EVENT_DATA_GROUP_INSERT, VINT_EVENT_DATA_GROUP_INSERT, &Data_items[index], VINT_DATA_ITEM_BYTES);
    }
}

// 0x0047F100 (:577)
void vint_data_group::process_all()
{
    vint_data_group* group = Data_groups_used;
    while (group != nullptr)
    {
        vint_data_group* next = group->factory_next != Data_groups_used ? group->factory_next : nullptr;
        group->process();
        group = next;
    }
}

// 0x0047F1F0 (:596)
void vint_data_group::process_group(unsigned int _handle)
{
    if (vint_data_group* group = find(_handle))
    {
        group->process();
    }
}

// 0x0047C570 (:618)
void vint_data_group::process_pending_delete()
{
    // each removed item's remove callbacks, then the item is destroyed
    vint_data_item* item = m_remove_pending_items;
    while (item != nullptr)
    {
        vint_data_item* next = item->container_next != m_remove_pending_items ? item->container_next : nullptr;
        vint_sub_trigger(m_subscriptions, VINT_EVENT_DATA_GROUP_REMOVE, VINT_EVENT_DATA_GROUP_REMOVE, item, VINT_DATA_ITEM_BYTES);
        vint_dlist_remove(m_remove_pending_items, item, &vint_data_item::container_next, &vint_data_item::container_prev);
        item->set_data_group(nullptr);
        item->destroy();
        item = next;
    }
}

// 0x0047C650 (:653)
void vint_data_group::process()
{
    vint_sub_adopt_pending(&vint_data_group_subscription::data_group_handle, m_name_crc, m_handle, m_subscriptions);
    process_pending_delete();
    if (m_delete_pending)
    {
        release_to_factory();
    }
}

// 0x00462FD0 (:686)
void vint_data_group::add_subscription(unsigned int _dg_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle)
{
    // by name: pending until the group is created
    if (_dg_name_crc == 0 || _callback_handle == 0 || _doc_handle == 0)
    {
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_callback_handle);
    if (cb == nullptr)
    {
        return;
    }
    if (vint_data_group* group = find_by_name_crc(_dg_name_crc))
    {
        group->add_subscription(_callback_handle, _event, _doc_handle);
        return;
    }
    if (vint_data_group_subscription* sub = vint_sub_alloc(_callback_handle, _dg_name_crc, 0, 0, _doc_handle, _event))
    {
        vint_sub_add(Pending_subscriptions, sub);
        cb->add_ref();
    }
}

// 0x004630E0 (:722)
void vint_data_group::remove_doc_subscriptions(unsigned int doc_h)
{
    vint_data_group* group = Data_groups_used;
    while (group != nullptr)
    {
        vint_sub_remove_doc(group->m_subscriptions, doc_h);
        group = group->factory_next;
        if (group == Data_groups_used)
        {
            break;
        }
    }
}

// ---- vint_data_item

// 0x0047C720 (:757)
vint_data_item::vint_data_item()
{
    m_subscriptions = nullptr;
}

// 0x00479D80 (.h:39)
vint_data_item::~vint_data_item()
{
}

// 0x00461BA0 (.h:49)
vint_data_group* vint_data_item::get_data_group() const
{
    return m_group;
}

// 0x00461BC0 (.h:56)
int vint_data_item::get_num_elements() const
{
    return m_num_elements;
}

// 0x00461BD0 (.h:75)
void vint_data_item::signal_update()
{
    m_updated = true;
}

// 0x00461BE0 (.h:87)
void vint_data_item::set_data_group(vint_data_group* _group)
{
    m_group = _group;
}

// OpenRFG: declared in the header without an rfg.exe body (the name lookups read the member).
unsigned int vint_data_item::get_name_crc() const
{
    return m_name_crc;
}

// 0x00463200 (:762)
void vint_data_item::init(const char* _name)
{
    m_num_elements = 0;
    m_name_crc = _name != nullptr ? crc_stri(_name, 0) : 0;
    m_group = nullptr;
    m_subscriptions = nullptr;
    // a new item reports itself inserted (in a group) or updated on the next process
    m_updated = true;
    m_inserted = true;
    ++Data_item_count;
    m_handle = (unsigned int)(this - Data_items) + (Data_item_count << 16);
}

// 0x004758F0 (:792)
void vint_data_item::destroy()
{
    // an item in a group is removed through the group (destroyed when the group processes)
    if (m_group != nullptr)
    {
        m_group->remove_data_item(m_handle);
        return;
    }
    vint_sub_release_all(m_subscriptions, m_name_crc, &vint_data_group_subscription::data_item_handle);
    vint_data_clear_elements(m_elements, 32);
    m_name_crc = 0;
    m_handle = 0;
    vint_dlist_remove(Data_items_used, this, &vint_data_item::factory_next, &vint_data_item::factory_prev);
    vint_dlist_add(Data_items_free, this, &vint_data_item::factory_next, &vint_data_item::factory_prev);
}

// 0x00475B10 (:845)
void vint_data_item::set_element(int _index, const vint_variant& _e)
{
    // a changed value marks the item updated; strings are copied
    if (_index >= 32)
    {
        return;
    }
    vint_variant& element = m_elements[_index];
    if (_e == element)
    {
        return;
    }
    if (_e.get_type() == VINT_PROP_TYPE_STRING)
    {
        const char* string = (const char*)_e;
        if (string == nullptr)
        {
            if (element.get_type() == VINT_PROP_TYPE_STRING && (const char*)element != nullptr)
            {
                vint_free((void*)(const char*)element);
            }
            element = (const char*)nullptr;
        }
        else
        {
            element.set_type(VINT_PROP_TYPE_STRING);
            if (element.get_type() != VINT_PROP_TYPE_STRING)
            {
                // Original: a string into a non-string element is dropped (and not reported updated)
                return;
            }
            char* copy = (char*)vint_realloc((void*)(const char*)element, (unsigned int)strlen(string) + 1);
            if (copy != nullptr)
            {
                strcpy(copy, string);
                element = (const char*)copy;
            }
        }
    }
    else
    {
        element = _e;
    }
    if (m_num_elements < _index + 1)
    {
        m_num_elements = _index + 1;
    }
    m_updated = true;
}

// 0x00463280 (:891)
const vint_variant& vint_data_item::get_element(int _index) const
{
    if (_index >= 32)
    {
        return Vint_variant_empty;
    }
    return m_elements[_index];
}

// 0x00475C10 (:898)
void vint_data_item::set(const vint_variant* _ele_array, int _num_elements)
{
    if (_ele_array == nullptr)
    {
        return;
    }
    for (int i = 0; i < _num_elements && i < 32; ++i)
    {
        set_element(i, _ele_array[i]);
    }
}

// 0x00475C50 (:908)
void vint_data_item::get(vint_variant* _ele_array, int _max_elements) const
{
    if (_ele_array == nullptr)
    {
        return;
    }
    for (int i = 0; i < m_num_elements && i < _max_elements; ++i)
    {
        _ele_array[i] = get_element(i);
    }
}

// 0x004632A0 (:918)
void vint_data_item::add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    // update only; the callback gets the current value at once
    if (_event != VINT_EVENT_DATA_ITEM_UPDATE)
    {
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_cb_handle);
    if (cb == nullptr || Subscriptions_free == nullptr)
    {
        return;
    }
    cb->add_ref();
    vint_data_group_subscription* sub = vint_sub_alloc(_cb_handle, 0, m_handle, 0, _doc_handle, VINT_EVENT_DATA_ITEM_UPDATE);
    vint_sub_add(m_subscriptions, sub);
    cb->trigger(sub->doc_handle, VINT_EVENT_DATA_ITEM_UPDATE, this, VINT_DATA_ITEM_BYTES);
}

// 0x004633C0 (:950)
void vint_data_item::remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    if (_event == VINT_EVENT_DATA_ITEM_UPDATE)
    {
        vint_sub_remove_first(m_subscriptions, _cb_handle, VINT_EVENT_DATA_ITEM_UPDATE, _doc_handle);
    }
}

// 0x004634A0 (:969)
bool vint_data_item::system_init()
{
    Data_items_used = nullptr;
    Data_items_free = nullptr;
    for (int i = 0; i < 128; ++i)
    {
        vint_dlist_add(Data_items_free, &Data_items[i], &vint_data_item::factory_next, &vint_data_item::factory_prev);
    }
    return true;
}

// 0x0046B540 (:992)
vint_data_item* vint_data_item::create_new(const char* _name)
{
    // a name already in use fails
    if (_name == nullptr)
    {
        return nullptr;
    }
    if (find_by_name_crc(crc_stri(_name, 0)) != nullptr)
    {
        return nullptr;
    }
    vint_data_item* item = Data_items_free;
    if (item == nullptr)
    {
        return nullptr;
    }
    vint_dlist_remove(Data_items_free, item, &vint_data_item::factory_next, &vint_data_item::factory_prev);
    vint_dlist_add(Data_items_used, item, &vint_data_item::factory_next, &vint_data_item::factory_prev);
    item->init(_name);
    vint_sub_adopt_pending(&vint_data_group_subscription::data_item_handle, item->m_name_crc, item->m_handle, item->m_subscriptions);
    return item;
}

// 0x004635E0 (:1038)
vint_data_item* vint_data_item::create_new(vint_data_group* _group, const char* name)
{
    // a group's item: unnamed allowed, no pending subscriptions adopted
    if (_group == nullptr)
    {
        return nullptr;
    }
    vint_data_item* item = Data_items_free;
    if (item == nullptr)
    {
        return nullptr;
    }
    vint_dlist_remove(Data_items_free, item, &vint_data_item::factory_next, &vint_data_item::factory_prev);
    vint_dlist_add(Data_items_used, item, &vint_data_item::factory_next, &vint_data_item::factory_prev);
    item->init(name);
    item->m_group = _group;
    return item;
}

// 0x00463680 (:1070)
vint_data_item* vint_data_item::find_by_name_crc(unsigned int _crc)
{
    if (_crc == 0)
    {
        return nullptr;
    }
    vint_data_item* item = Data_items_used;
    while (item != nullptr)
    {
        if (item->m_name_crc == _crc)
        {
            return item;
        }
        item = item->factory_next;
        if (item == Data_items_used)
        {
            break;
        }
    }
    return nullptr;
}

// 0x004636B0 (:1096)
vint_data_item* vint_data_item::find(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return find_by_name_crc(crc_stri(_name, 0));
}

// 0x004636F0 (:1103)
vint_data_item* vint_data_item::find(unsigned int _handle)
{
    unsigned int index = _handle & 0xFFFF;
    if (_handle == 0 || _handle == 0xFFFFFFFF || index >= 128)
    {
        return nullptr;
    }
    if (Data_items[index].m_handle != _handle)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    return &Data_items[index];
}

// 0x0046B6E0 (:1140)
void vint_data_item::set_template(const vint_variant_type* _element_types, int _num_elements)
{
    vint_data_set_template(m_elements, _element_types, _num_elements, 127, "vint_data_item::set_template() - Attempt to change element type after it's been established.");
    m_num_elements = _num_elements;
}

// 0x00463740 (:1154)
// LTCG: elem in EDX.
static bool used_factory_contains(vint_data_item* elem)
{
    vint_data_item* item = Data_items_used;
    while (item != nullptr)
    {
        if (item == elem)
        {
            return true;
        }
        item = item->factory_next;
        if (item == Data_items_used)
        {
            break;
        }
    }
    return false;
}

// 0x00472FC0 (:1166)
void vint_data_item::process_all()
{
    vint_data_item* item = Data_items_used;
    while (item != nullptr)
    {
        item->process();
        // Original: the result of this check is unused
        used_factory_contains(item);
        item = item->factory_next;
        if (item == Data_items_used)
        {
            break;
        }
    }
}

// 0x00473000 (:1188)
void vint_data_item::process_item(unsigned int _handle)
{
    if (vint_data_item* item = find(_handle))
    {
        item->process();
    }
}

// 0x0046B7C0 (:1209)
void vint_data_item::process()
{
    // a new group item is announced as an insert (not as an update)
    if (m_inserted && m_group != nullptr)
    {
        m_group->signal_data_item_insert(m_handle);
        m_updated = false;
        m_inserted = false;
        return;
    }
    if (m_updated)
    {
        vint_sub_trigger(m_subscriptions, VINT_EVENT_UNKNOWN, VINT_EVENT_DATA_ITEM_UPDATE, this, VINT_DATA_ITEM_BYTES);
        if (m_group != nullptr)
        {
            m_group->signal_data_item_update(m_handle);
        }
    }
    m_updated = false;
    m_inserted = false;
}

// 0x00463770 (:1235)
void vint_data_item::add_subscription(unsigned int _di_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle)
{
    if (_di_name_crc == 0 || _doc_handle == 0)
    {
        return;
    }
    if (vint_data_item* item = find_by_name_crc(_di_name_crc))
    {
        item->add_subscription(_callback_handle, _event, _doc_handle);
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_callback_handle);
    if (cb == nullptr)
    {
        return;
    }
    if (vint_data_group_subscription* sub = vint_sub_alloc(_callback_handle, 0, _di_name_crc, 0, _doc_handle, _event))
    {
        vint_sub_add(Pending_subscriptions, sub);
        cb->add_ref();
    }
}

// 0x00463880 (:1271)
void vint_data_item::remove_doc_subscriptions(unsigned int doc_h)
{
    vint_data_item* item = Data_items_used;
    while (item != nullptr)
    {
        vint_sub_remove_doc(item->m_subscriptions, doc_h);
        item = item->factory_next;
        if (item == Data_items_used)
        {
            break;
        }
    }
}

// 0x0046B880 (:1293)
void vint_data_item::force_immediate_update()
{
    m_updated = true;
    process();
}

// ---- vint_data_event

// 0x0047C750 (:1314)
vint_data_event::vint_data_event()
{
    m_subscriptions = nullptr;
}

// 0x00479DB0 (.h:37)
vint_data_event::~vint_data_event()
{
}

// 0x00461BF0 (.h:47)
unsigned int vint_data_event::get_handle() const
{
    return m_handle;
}

// 0x00461C00 (.h:53)
int vint_data_event::get_num_elements() const
{
    return m_num_elements;
}

// OpenRFG: declared in the header without an rfg.exe body.
unsigned int vint_data_event::get_name_crc() const
{
    return m_name_crc;
}

// 0x004639B0 (:1319)
void vint_data_event::init(const char* _name)
{
    m_num_elements = 0;
    m_name_crc = _name != nullptr ? crc_stri(_name, 0) : 0;
    m_subscriptions = nullptr;
    m_updated = false;
    ++Data_event_count;
    m_handle = (unsigned int)(this - Data_events) + (Data_event_count << 16);
}

// 0x00475CA0 (:1348)
void vint_data_event::destroy()
{
    vint_sub_release_all(m_subscriptions, m_name_crc, &vint_data_group_subscription::data_event_handle);
    vint_data_clear_elements(m_elements, 8);
    m_name_crc = 0;
    m_handle = 0;
    vint_dlist_remove(Data_events_used, this, &vint_data_event::factory_next, &vint_data_event::factory_prev);
    vint_dlist_add(Data_events_free, this, &vint_data_event::factory_next, &vint_data_event::factory_prev);
}

// 0x00475EA0 (:1396)
void vint_data_event::set_element(int _index, const vint_variant& _e)
{
    // every set is an update (no change test); strings are copied
    // Original bug kept: the bound is the item's 32, past the event's 8 elements
    if (_index >= 32)
    {
        return;
    }
    m_updated = true;
    vint_variant& element = m_elements[_index];
    if (_e.get_type() == VINT_PROP_TYPE_STRING)
    {
        const char* string = (const char*)_e;
        if (string == nullptr)
        {
            // Original: the old string is not freed
            element = (const char*)nullptr;
        }
        else
        {
            const char* old = element.get_type() == VINT_PROP_TYPE_STRING ? (const char*)element : nullptr;
            if (Memory_callbacks != nullptr && Memory_callbacks->realloc != nullptr)
            {
                char* copy = (char*)Memory_callbacks->realloc((void*)old, (unsigned int)strlen(string) + 1);
                if (copy != nullptr)
                {
                    strcpy(copy, string);
                    element = (const char*)copy;
                }
            }
        }
    }
    else
    {
        element = _e;
    }
    if (m_num_elements < _index + 1)
    {
        m_num_elements = _index + 1;
    }
    m_updated = true;
}

// 0x00463A20 (:1429)
const vint_variant& vint_data_event::get_element(int _index) const
{
    // Original bug kept: the bound is 32, not 8
    if (_index >= 32)
    {
        return Vint_variant_empty;
    }
    return m_elements[_index];
}

// 0x00463A40 (:1436)
void vint_data_event::signal_update()
{
    m_updated = true;
}

// 0x00475F80 (:1441)
void vint_data_event::set(const vint_variant* _ele_array, int _num_elements)
{
    if (_ele_array == nullptr)
    {
        return;
    }
    for (int i = 0; i < _num_elements && i < 32; ++i)
    {
        set_element(i, _ele_array[i]);
    }
}

// 0x00475FC0 (:1451)
void vint_data_event::get(vint_variant* _ele_array, int _max_elements) const
{
    if (_ele_array == nullptr)
    {
        return;
    }
    for (int i = 0; i < m_num_elements && i < _max_elements; ++i)
    {
        _ele_array[i] = get_element(i);
    }
}

// 0x00463A50 (:1461)
void vint_data_event::add_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    if (_event != VINT_EVENT_DATA_EVENT_UPDATE)
    {
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_cb_handle);
    if (cb == nullptr || Subscriptions_free == nullptr)
    {
        return;
    }
    cb->add_ref();
    vint_data_group_subscription* sub = vint_sub_alloc(_cb_handle, 0, 0, m_handle, _doc_handle, VINT_EVENT_DATA_EVENT_UPDATE);
    vint_sub_add(m_subscriptions, sub);
}

// 0x00463B60 (:1490)
void vint_data_event::remove_subscription(unsigned int _cb_handle, vint_event_type _event, unsigned int _doc_handle)
{
    if (_event == VINT_EVENT_DATA_EVENT_UPDATE)
    {
        vint_sub_remove_first(m_subscriptions, _cb_handle, VINT_EVENT_DATA_EVENT_UPDATE, _doc_handle);
    }
}

// 0x00463C40 (:1509)
bool vint_data_event::system_init()
{
    Data_events_used = nullptr;
    Data_events_free = nullptr;
    for (int i = 0; i < 4; ++i)
    {
        vint_dlist_add(Data_events_free, &Data_events[i], &vint_data_event::factory_next, &vint_data_event::factory_prev);
    }
    return true;
}

// 0x00463CA0 (:1532)
vint_data_event* vint_data_event::create_new(const char* _name)
{
    // Original: no duplicate name test (unlike items)
    if (_name == nullptr)
    {
        return nullptr;
    }
    vint_data_event* event = Data_events_free;
    if (event == nullptr)
    {
        return nullptr;
    }
    vint_dlist_remove(Data_events_free, event, &vint_data_event::factory_next, &vint_data_event::factory_prev);
    vint_dlist_add(Data_events_used, event, &vint_data_event::factory_next, &vint_data_event::factory_prev);
    event->init(_name);
    vint_sub_adopt_pending(&vint_data_group_subscription::data_event_handle, event->m_name_crc, event->m_handle, event->m_subscriptions);
    return event;
}

// 0x00463E00 (:1575)
vint_data_event* vint_data_event::find_by_name_crc(unsigned int _crc)
{
    if (_crc == 0)
    {
        return nullptr;
    }
    vint_data_event* event = Data_events_used;
    while (event != nullptr)
    {
        if (event->m_name_crc == _crc)
        {
            return event;
        }
        event = event->factory_next;
        if (event == Data_events_used)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00463E30 (:1601)
vint_data_event* vint_data_event::find(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return find_by_name_crc(crc_stri(_name, 0));
}

// 0x00463E70 (:1608)
vint_data_event* vint_data_event::find(unsigned int _handle)
{
    unsigned int index = _handle & 0xFFFF;
    if (_handle == 0 || index >= 4)
    {
        return nullptr;
    }
    if (Data_events[index].m_handle != _handle)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    return &Data_events[index];
}

// 0x0046B890 (:1637)
void vint_data_event::set_template(const vint_variant_type* _element_types, int _num_elements)
{
    vint_data_set_template(m_elements, _element_types, _num_elements, 3, "vint_data_event::set_template() - Attempt to change element type after it's been established.");
    m_num_elements = _num_elements;
}

// 0x0046B970 (:1651)
void vint_data_event::process_all()
{
    vint_data_event* event = Data_events_used;
    while (event != nullptr)
    {
        if (event->m_updated)
        {
            vint_sub_trigger(event->m_subscriptions, VINT_EVENT_UNKNOWN, VINT_EVENT_DATA_EVENT_UPDATE, event, VINT_DATA_EVENT_BYTES);
        }
        event->m_updated = false;
        event = event->factory_next;
        if (event == Data_events_used)
        {
            break;
        }
    }
}

// 0x0046BA00 (:1671)
void vint_data_event::process_item(unsigned int _handle)
{
    if (vint_data_event* event = find(_handle))
    {
        event->process();
    }
}

// 0x00463EB0 (:1692)
void vint_data_event::process()
{
    if (m_updated)
    {
        vint_sub_trigger(m_subscriptions, VINT_EVENT_UNKNOWN, VINT_EVENT_DATA_EVENT_UPDATE, this, VINT_DATA_EVENT_BYTES);
    }
    m_updated = false;
}

// 0x00463F40 (:1709)
void vint_data_event::add_subscription(unsigned int _de_name_crc, vint_event_type _event, unsigned int _callback_handle, unsigned int _doc_handle)
{
    if (_de_name_crc == 0 || _doc_handle == 0)
    {
        return;
    }
    if (vint_data_event* event = find_by_name_crc(_de_name_crc))
    {
        event->add_subscription(_callback_handle, _event, _doc_handle);
        return;
    }
    vint_callback* cb = vint_callback::find_by_handle(_callback_handle);
    if (cb == nullptr)
    {
        return;
    }
    if (vint_data_group_subscription* sub = vint_sub_alloc(_callback_handle, 0, 0, _de_name_crc, _doc_handle, _event))
    {
        vint_sub_add(Pending_subscriptions, sub);
        cb->add_ref();
    }
}

// 0x00464050 (:1745)
void vint_data_event::remove_doc_subscriptions(unsigned int doc_h)
{
    vint_data_event* event = Data_events_used;
    while (event != nullptr)
    {
        vint_sub_remove_doc(event->m_subscriptions, doc_h);
        event = event->factory_next;
        if (event == Data_events_used)
        {
            break;
        }
    }
}

// 0x00464180 (:1766)
void vint_data_remove_doc_subscriptions(unsigned int doc_h)
{
    // on the objects, then the pending ones
    if (doc_h == 0)
    {
        return;
    }
    vint_data_group::remove_doc_subscriptions(doc_h);
    vint_data_item::remove_doc_subscriptions(doc_h);
    vint_data_event::remove_doc_subscriptions(doc_h);
    vint_sub_remove_doc(Pending_subscriptions, doc_h);
}
