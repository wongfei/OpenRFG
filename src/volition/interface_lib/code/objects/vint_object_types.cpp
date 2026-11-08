// Original: volition/interface_lib/code/objects/vint_object_types.cpp
//
// The object type registry (UI.md UI2): up to 32 types, each with its name, parent type, property manager and
// factory; a type's index is its handle. Enum-valued properties register a wrapper (name/value table) per type and
// property name; lookups accept a type related to the registered one in either direction. Compiled early in
// interface_lib_unity.cpp: the type table is read by the objects' files.

#include "volition/interface_lib/code/objects/vint_object_types.h"

#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/objects/vint_object_factory.h"
#include "volition/vlib/code/util/string_comp.h"

// ---- file-private types (rfg.pdb)
// size 0x10
struct vint_registered_enum
{
    unsigned int object_type; // 0x00
    const char* property_name; // 0x04
    unsigned int prop_name_crc; // 0x08
    vint_prop_type_enum_wrapper* wrapper; // 0x0C
};
static_assert(sizeof(vint_registered_enum) == 0x10, "vint_registered_enum");

// ---- file-static globals (shared with the unity's other files)
static vint_registered_enum Registered_enums[32]; // 0x01A39A20
static vint_object_type_info Object_type_info[32]; // 0x01A39C20
static unsigned int Num_object_types; // 0x01A48640
static unsigned int Num_register_enums; // 0x01A48644

// 0x004673E0
vint_object_factory::vint_object_factory()
{
}

// 0x0045BD90 (vint_object_factory.h:19)
vint_object_factory::~vint_object_factory()
{
}

// 0x00460950 (:113)
void vint_object_types_system_init()
{
    // the property name crcs and indices of every registered type
    for (unsigned int i = 0; i < Num_object_types; ++i)
    {
        if (Object_type_info[i].prop_mgr != nullptr)
        {
            Object_type_info[i].prop_mgr->update_prop_name_crcs();
        }
    }
}

// 0x004609C0 (:127)
void vint_object_types_system_shutdown()
{
}

// 0x004609D0 (:132)
unsigned int vint_object_types_register_type(const char* _name, unsigned int _parent_type, vint_prop_mgr* _prop_mgr)
{
    if (_name == nullptr || _prop_mgr == nullptr || Num_object_types >= 32)
    {
        return 0xFFFFFFFF;
    }
    Object_type_info[Num_object_types].name = _name;
    Object_type_info[Num_object_types].parent_type = _parent_type;
    Object_type_info[Num_object_types].prop_mgr = _prop_mgr;
    return Num_object_types++;
}

// 0x00460A20 (:145)
void vint_object_types_register_factory(unsigned int _type, vint_object_factory* _factory)
{
    if (_factory != nullptr && _type < Num_object_types)
    {
        Object_type_info[_type].factory = _factory;
    }
}

// 0x00460A40 (:153)
unsigned int vint_object_types_get_num_types()
{
    return Num_object_types;
}

// 0x00460A50 (:158)
const vint_object_type_info* vint_object_types_get_type_info(unsigned int _type_index)
{
    if (_type_index < Num_object_types)
    {
        return &Object_type_info[_type_index];
    }
    return nullptr;
}

// 0x00460A70 (:167)
const vint_object_type_info* vint_object_types_get_type_info(const char* _type_name)
{
    for (unsigned int i = 0; i < Num_object_types; ++i)
    {
        if (Object_type_info[i].name != nullptr && v_stricmp(_type_name, Object_type_info[i].name) == 0)
        {
            return &Object_type_info[i];
        }
    }
    return nullptr;
}

// 0x00460AD0 (:182)
void vint_prop_type_register_enum(unsigned int _object_type, const char* _prop_name, vint_prop_type_enum_wrapper* _wrapper)
{
    // prop_name_crc is not set
    if (Num_register_enums < 32)
    {
        Registered_enums[Num_register_enums].object_type = _object_type;
        Registered_enums[Num_register_enums].property_name = _prop_name;
        Registered_enums[Num_register_enums].wrapper = _wrapper;
        ++Num_register_enums;
    }
}

// 0x00460B10 (:191)
vint_prop_type_enum_wrapper::vint_prop_type_enum_wrapper(const vint_prop_type_enum_info* _infos, unsigned short _num_infos)
{
    // Members are left as they are for an empty table (static wrappers: zero).
    if (_infos != nullptr && _num_infos != 0)
    {
        m_num_infos = _num_infos;
        m_infos = _infos;
    }
}

// 0x00460B30 (:198)
void vint_prop_type_enum_wrapper::init(unsigned int _type_handle, const char* _prop_name)
{
    if (_type_handle != 0xFFFFFFFF && _prop_name != nullptr && m_infos != nullptr)
    {
        vint_prop_type_register_enum(_type_handle, _prop_name, this);
    }
}

// 0x00460B60 (:204)
int vint_prop_type_enum_wrapper::resolve(const char* _enum_name)
{
    if (_enum_name == nullptr)
    {
        return 0x7FFFFFFF;
    }
    for (unsigned int i = 0; i < m_num_infos; ++i)
    {
        if (v_stricmp(m_infos[i].name, _enum_name) == 0)
        {
            return m_infos[i].value;
        }
    }
    return 0x7FFFFFFF;
}

// 0x00460BC0 (:220)
const char* vint_prop_type_enum_wrapper::reverse(int _value)
{
    for (unsigned int i = 0; i < m_num_infos; ++i)
    {
        if (m_infos[i].value == _value)
        {
            return m_infos[i].name;
        }
    }
    return nullptr;
}

// 0x00469730 (:232)
void vint_prop_type_enum_wrapper::get_values(const char** _strings, unsigned int* _num_strings)
{
    if (_strings == nullptr || _num_strings == nullptr)
    {
        return;
    }
    for (unsigned int i = 0; i < m_num_infos && i < *_num_strings; ++i)
    {
        _strings[i] = m_infos[i].name;
    }
    if (m_num_infos < *_num_strings)
    {
        *_num_strings = m_num_infos;
    }
}

// 0x00460BF0 (:244)
bool vint_object_types_are_equivalent(unsigned int _left, unsigned int _right)
{
    // either type derives from the other
    for (unsigned int t = _left; t != 0xFFFFFFFF; t = Object_type_info[t].parent_type)
    {
        if (t == _right)
        {
            return true;
        }
    }
    for (unsigned int t = _right; t != 0xFFFFFFFF; t = Object_type_info[t].parent_type)
    {
        if (t == _left)
        {
            return true;
        }
    }
    return false;
}

// 0x00460C40 (:271)
int vint_enum_resolve(unsigned int _object_type, const char* _prop_name, const char* _string)
{
    if (_prop_name == nullptr || _string == nullptr)
    {
        return 0x7FFFFFFF;
    }
    for (unsigned int i = 0; i < Num_register_enums; ++i)
    {
        if (vint_object_types_are_equivalent(Registered_enums[i].object_type, _object_type) && v_stricmp(_prop_name, Registered_enums[i].property_name) == 0)
        {
            return Registered_enums[i].wrapper->resolve(_string);
        }
    }
    return 0x7FFFFFFF;
}

// 0x00460CC0 (:285)
const char* vint_enum_reverse(unsigned int _object_type, const char* _prop_name, int _value)
{
    if (_prop_name == nullptr)
    {
        return nullptr;
    }
    for (unsigned int i = 0; i < Num_register_enums; ++i)
    {
        if (vint_object_types_are_equivalent(Registered_enums[i].object_type, _object_type) && v_stricmp(_prop_name, Registered_enums[i].property_name) == 0)
        {
            return Registered_enums[i].wrapper->reverse(_value);
        }
    }
    return nullptr;
}

// 0x00469780 (:307)
void vint_enum_get_values(const char** _strings, unsigned int* _num_strings, unsigned int _object_type, const char* _prop_name)
{
    for (unsigned int i = 0; i < Num_register_enums; ++i)
    {
        if (vint_object_types_are_equivalent(Registered_enums[i].object_type, _object_type) && v_stricmp(_prop_name, Registered_enums[i].property_name) == 0)
        {
            Registered_enums[i].wrapper->get_values(_strings, _num_strings);
            return;
        }
    }
    // Original: no match writes 0 through _num_strings without a null check.
    *_num_strings = 0;
}
