// OpenRFG helper (not in rfg.pdb): the property reflection boilerplate interface_lib expands per property with macros.
// Each property has a static internal_get_prop_<name>/internal_set_prop_<name> pair (the vint_prop_lookup callbacks)
// that null-check the object and call the class's virtual get_<name>/set_<name>. rfg.exe emits each pair as two
// functions; their VAs are noted at each use.
#pragma once

#define VINT_PROP_WRAPPERS(cls, prop) \
    bool cls::internal_get_prop_##prop(const vint_object_base* _obj, vint_variant& _val, int _index) \
    { \
        if (_obj == nullptr) \
        { \
            return false; \
        } \
        return static_cast<const cls*>(_obj)->get_##prop(_val, _index); \
    } \
    bool cls::internal_set_prop_##prop(vint_object_base* _obj, const vint_variant& _val, int _index) \
    { \
        if (_obj == nullptr) \
        { \
            return false; \
        } \
        return static_cast<cls*>(_obj)->set_##prop(_val, _index); \
    }

// A property table entry (the vint_prop_lookup constructor with its editor default).
#define VINT_PROP_ENTRY(cls, prop, type, default_value, editor_save) \
    vint_prop_lookup(#prop, type, cls::internal_set_prop_##prop, cls::internal_get_prop_##prop, vint_variant(default_value), editor_save)

// A concrete object class's system_init: its property manager, its type (child of the parent type), its factory's pool
// (max_objects) and the factory's registration (rfg.exe inlines vint_prop_mgr::init and the type registration).
#include "volition/interface_lib/code/objects/vint_object_factory.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"

template<typename T>
inline unsigned int vint_register_object_class(const char* _name, unsigned int _parent_type, vint_prop_mgr& _mgr, vint_prop_lookup* _table, unsigned int _num_props, vint_object_factory_imp<T>& _factory, unsigned int _max_objects)
{
    _mgr.init(_parent_type, _name, _table, _num_props);
    unsigned int type = vint_object_types_register_type(_name, _parent_type, &_mgr);
    _factory.init(_max_objects);
    vint_object_types_register_factory(type, &_factory);
    return type;
}
