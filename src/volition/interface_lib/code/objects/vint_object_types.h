// Original: volition/interface_lib/code/objects/vint_object_types.h
// Implementation: volition/interface_lib/code/objects/vint_object_types.cpp
//
// The object type registry (name, factory, property lookup per type) and the enum property wrappers (name/value tables
// for enum-valued properties). Placement of the info structs inferred.
#pragma once


class vint_object_factory;
class vint_prop_mgr;

// size 0x10
struct vint_object_type_info
{
    const char* name; // 0x00
    unsigned int parent_type; // 0x04
    vint_prop_mgr* prop_mgr; // 0x08
    vint_object_factory* factory; // 0x0C
};

// size 0x8
struct vint_prop_type_enum_info
{
    const char* name; // 0x00
    int value; // 0x04
};

// size 0x8
class vint_prop_type_enum_wrapper
{
public:
    vint_prop_type_enum_wrapper(const vint_prop_type_enum_info* _infos, unsigned short _num_infos); // 0x00460B10 (:191)
    void init(unsigned int _type_handle, const char* _prop_name); // 0x00460B30 (:198)
    int resolve(const char* _enum_name); // 0x00460B60 (:204)
    const char* reverse(int _value); // 0x00460BC0 (:220)
    void get_values(const char** _strings, unsigned int* _num_strings); // 0x00469730 (:232)

private:
    const vint_prop_type_enum_info* m_infos; // 0x00
    unsigned short m_num_infos; // 0x04
};

void vint_object_types_system_init(); // 0x00460950 (:113)
void vint_object_types_system_shutdown(); // 0x004609C0 (:127)
unsigned int vint_object_types_register_type(const char* _name, unsigned int _parent_type, vint_prop_mgr* _prop_mgr); // 0x004609D0 (:132)
void vint_object_types_register_factory(unsigned int _type, vint_object_factory* _factory); // 0x00460A20 (:145)
unsigned int vint_object_types_get_num_types(); // 0x00460A40 (:153)
const vint_object_type_info* vint_object_types_get_type_info(unsigned int _type_index); // 0x00460A50 (:158)
const vint_object_type_info* vint_object_types_get_type_info(const char* _type_name); // 0x00460A70 (:167)
void vint_prop_type_register_enum(unsigned int _object_type, const char* _prop_name, vint_prop_type_enum_wrapper* _wrapper); // 0x00460AD0 (:182)
bool vint_object_types_are_equivalent(unsigned int _left, unsigned int _right); // 0x00460BF0 (:244)
int vint_enum_resolve(unsigned int _object_type, const char* _prop_name, const char* _string); // 0x00460C40 (:271)
const char* vint_enum_reverse(unsigned int _object_type, const char* _prop_name, int _value); // 0x00460CC0 (:285)
void vint_enum_get_values(const char** _strings, unsigned int* _num_strings, unsigned int _object_type, const char* _prop_name); // 0x00469780 (:307)
