// Original: volition/interface_lib/code/vint_callback.h
// Implementation: volition/interface_lib/code/vint_callback.cpp
//
// Callbacks: the base class (handle, name hash, event type), native callbacks (a C function), and the event types data
// groups, items, events, tweens and input deliver. Lua callbacks are in scripting/vint_lua_actions.h. Placement of
// vint_event_type inferred.
#pragma once

enum vint_event_type
{
    VINT_EVENT_UNKNOWN = -1,
    VINT_EVENT_INPUT = 0,
    VINT_EVENT_TWEEN = 1,
    VINT_EVENT_DATA_ITEM_UPDATE = 2,
    VINT_EVENT_DATA_EVENT_UPDATE = 3,
    VINT_EVENT_DATA_GROUP_UPDATE = 4,
    VINT_EVENT_DATA_GROUP_INSERT = 5,
    VINT_EVENT_DATA_GROUP_REMOVE = 6,
    NUM_VINT_EVENT_TYPES = 7,
};

// size 0x20
class vint_callback
{
public:
    vint_callback(); // 0x0045BF80 (.h:53)
    virtual ~vint_callback(); // vtbl+0x00 0x0045BF90 (.h:54)
    void add_ref(); // 0x0045BFA0 (.h:56)
    void release(); // 0x004621E0 (:95)
    virtual bool trigger(unsigned int, vint_event_type, void*, unsigned int) = 0; // vtbl+0x04
    unsigned int get_handle() const; // 0x0045BFB0 (.h:60)
    static void system_init(); // 0x0046B1C0 (:124)
    static vint_callback* find_by_name(unsigned int _name_crc); // 0x004622B0 (:145)
    static vint_callback* find_by_handle(unsigned int _handle); // 0x004622E0 (:168)
protected:
    virtual bool init(const char* _name); // vtbl+0x08 0x00462110 (:73)
    virtual void clean_up(); // vtbl+0x0C 0x0045BFC0 (.h:78)
    virtual void return_to_factory() = 0; // vtbl+0x10
    virtual void destroy(); // vtbl+0x14 0x00462200 (:114)

    // 0x00 vfptr
public:
    vint_callback* handle_hash_prev; // 0x04
    vint_callback* handle_hash_next; // 0x08
    vint_callback* name_hash_prev; // 0x0C
    vint_callback* name_hash_next; // 0x10
protected:
    unsigned int m_name_crc; // 0x14
    unsigned int m_handle; // 0x18
    unsigned int m_ref_count; // 0x1C
};

// size 0x2C
class vint_callback_native : public vint_callback
{
public:
    vint_callback_native(); // 0x0045BFF0 (.h:94)
    virtual ~vint_callback_native(); // vtbl+0x00 0x0045C000 (.h:95)
    virtual bool trigger(unsigned int _doc_handle, vint_event_type _type, void* _data, unsigned int _bytes); // vtbl+0x04 0x004623C0 (:222)
    static vint_callback_native* create_new(const char* _name, bool (*_func)(unsigned int, vint_event_type, void*, unsigned int)); // 0x00462320 (:195)
    static void system_init(); // 0x00462490 (:258)
protected:
    virtual void return_to_factory(); // vtbl+0x10 0x00462420 (:252)
    virtual bool init_native(const char* _name, bool (*_func)(unsigned int, vint_event_type, void*, unsigned int)); // vtbl+0x18 0x004623F0 (:240)

public:
    vint_callback_native* factory_next; // 0x20
    vint_callback_native* factory_prev; // 0x24
private:
    bool (*m_callback_function)(unsigned int, vint_event_type, void*, unsigned int); // 0x28
};
