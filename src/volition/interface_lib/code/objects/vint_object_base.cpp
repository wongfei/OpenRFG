// Original: volition/interface_lib/code/objects/vint_object_base.cpp
//
// The object base (UI.md UI2). An object's handle is its address. Objects are chained in the 64 name hash buckets by
// name crc and in their factory's lists; children are a singly linked list (new children go first). Properties are
// reflected through each type's vint_prop_mgr (a table of name, type, get/set callbacks, editor default), searched up
// the parent types. An object remembers the properties set by the current resolution's overrides (m_override_props:
// crcs, zero-terminated). Objects load from the XML (v2) or binary documents: the binary form stores the baseline
// properties and per-resolution overrides; the overrides of the current mode win. Compiled in
// interface_lib_unity.cpp after vint_object_types.cpp and vint_project.cpp, whose tables it reads.

#include "volition/interface_lib/code/objects/vint_object_base.h"

#include "volition/interface_lib/code/objects/vint_object_factory.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- public globals
vint_object_base* Process_stack[256]; // 0x01A3D778

// ---- file-static globals (shared with the unity's other files)
static vint_object_base* Name_hash_buckets[64]; // 0x01A39E20

// The property manager of a type, or null (rfg.exe inlines the type table lookup at each use).
static vint_prop_mgr* vint_object_type_prop_mgr(unsigned int _type)
{
    const vint_object_type_info* info = vint_object_types_get_type_info(_type);
    return info != nullptr ? info->prop_mgr : nullptr;
}

// An enum property given a string: the string's value, or the string itself when it names no value (rfg.exe inlines
// this at each property assignment).
static void vint_object_resolve_enum_string(const vint_prop_lookup* _lookup, const vint_object_base* _obj, vint_variant& _value)
{
    if (_lookup->type != VINT_PROP_TYPE_ENUM || _value.get_type() != VINT_PROP_TYPE_STRING)
    {
        return;
    }
    const char* string = (const char*)_value;
    _value.clear();
    _value = vint_enum_resolve(_obj->get_type(), _lookup->name, string);
    if ((int)_value == 0x7FFFFFFF)
    {
        _value.clear();
        _value = string;
    }
}

// One property value of the binary format: the type byte has been read, then come the value's bytes (rfg.exe inlines
// this in the baseline and override loops).
static void vint_object_read_binary_value(vint_load_buffer& _buffer, unsigned char _type, const vint_bin_file_strings& _strings, vint_variant& _value, const char* _error)
{
    switch (_type)
    {
    case VINT_PROP_TYPE_INT:
    {
        int v = 0;
        _buffer.get_data(v);
        _value = v;
        break;
    }
    case VINT_PROP_TYPE_UINT:
    {
        unsigned int v = 0;
        _buffer.get_data(v);
        _value = v;
        break;
    }
    case VINT_PROP_TYPE_FLOAT:
    {
        float v = 0.0f;
        _buffer.get_data(v);
        _value = v;
        break;
    }
    case VINT_PROP_TYPE_STRING:
    {
        unsigned int v = 0;
        _buffer.get_data(v);
        _value = _strings[v];
        break;
    }
    case VINT_PROP_TYPE_BOOL:
    {
        unsigned char v = 0;
        _buffer.get_data(v);
        _value = v != 0;
        break;
    }
    case VINT_PROP_TYPE_COLOR:
    {
        vint_variant_color v;
        v.b = 0.0f;
        v.g = 0.0f;
        v.r = 0.0f;
        _buffer.get_data(v.r);
        _buffer.get_data(v.g);
        _buffer.get_data(v.b);
        _value = v;
        break;
    }
    case VINT_PROP_TYPE_VECTOR2F:
    {
        vint_variant_vector2f v;
        v.y = 0.0f;
        v.x = 0.0f;
        _buffer.get_data(v.x);
        _buffer.get_data(v.y);
        _value = vint_variant(v);
        break;
    }
    case VINT_PROP_TYPE_UINT64:
    {
        uint64_t v = 0;
        _buffer.get_data(v);
        _value = v;
        break;
    }
    default:
        keen::debug::nativePrintArgs(_error);
        break;
    }
}

// ---- header-inline members (vint_object_base.h)

// 0x00476280 (.h:202)
vint_prop_lookup::vint_prop_lookup()
{
}

// 0x0045BC00 (.h:208)
bool vint_prop_lookup::is_indexed() const
{
    return get_first_index_callback != nullptr && get_next_index_callback != nullptr;
}

// 0x0045BC20 (.h:213)
bool vint_prop_lookup::get_first_index(const vint_object_base* _obj, int& _index) const
{
    if (get_first_index_callback != nullptr)
    {
        return get_first_index_callback(_obj, _index);
    }
    return false;
}

// 0x0045BC40 (.h:222)
bool vint_prop_lookup::get_next_index(const vint_object_base* _obj, int& _index) const
{
    if (get_next_index_callback != nullptr)
    {
        return get_next_index_callback(_obj, _index);
    }
    return false;
}

// 0x00476290 (.h:277)
vint_prop_lookup::vint_prop_lookup(const char* _name, vint_variant_type _type, bool (*_set_cb)(vint_object_base*, const vint_variant&, int), bool (*_get_cb)(const vint_object_base*, vint_variant&, int), const vint_variant& _default, bool _editor_save)
{
    // crc and allow_res_override are left for update_prop_name_crcs and the static zero-initialization
    name = _name;
    type = _type;
    set_value_callback = _set_cb;
    get_value_callback = _get_cb;
    get_first_index_callback = nullptr;
    get_next_index_callback = nullptr;
    editor_save = _editor_save;
    index = 0xFFFFFFFF;
    editor_default_value = _default;
}

// 0x004744B0 (.h:285)
void vint_prop_lookup::init(const char* _name, vint_variant_type _type, bool (*_set_cb)(vint_object_base*, const vint_variant&, int), bool (*_get_cb)(const vint_object_base*, vint_variant&, int), const vint_variant& _default, bool (*_first_index_cb)(const vint_object_base*, int&), bool (*_next_index_cb)(const vint_object_base*, int&), bool _editor_save)
{
    name = _name;
    type = _type;
    set_value_callback = _set_cb;
    get_value_callback = _get_cb;
    get_first_index_callback = _first_index_cb;
    get_next_index_callback = _next_index_cb;
    editor_save = _editor_save;
    index = 0xFFFFFFFF;
    editor_default_value = _default;
}

// 0x00476390
vint_prop_lookup::~vint_prop_lookup()
{
}

// 0x004757B0
vint_prop_lookup& vint_prop_lookup::operator=(const vint_prop_lookup& __that)
{
    name = __that.name;
    type = __that.type;
    set_value_callback = __that.set_value_callback;
    get_value_callback = __that.get_value_callback;
    get_first_index_callback = __that.get_first_index_callback;
    get_next_index_callback = __that.get_next_index_callback;
    crc = __that.crc;
    index = __that.index;
    allow_res_override = __that.allow_res_override;
    editor_save = __that.editor_save;
    editor_default_value = __that.editor_default_value;
    return *this;
}

// 0x0045BC60 (.h:303)
vint_prop_mgr::vint_prop_mgr()
{
    m_lookup_table = nullptr;
}

// 0x0045BC70 (.h:304)
vint_prop_mgr::~vint_prop_mgr()
{
}

// 0x00468120 (.h:342)
vint_object_base::vint_object_base()
{
    m_handle = 0;
    m_name_crc = 0;
    memset(m_override_props, 0, sizeof(m_override_props));
}

// 0x0045BC80 (.h:344)
vint_object_base::~vint_object_base()
{
}

// 0x0045BC90 (.h:351)
void vint_object_base::clean_up()
{
}

// 0x0045BCA0 (.h:355)
bool vint_object_base::is_type(unsigned int _type) const
{
    return false;
}

// 0x0045BCB0 (.h:359)
unsigned int vint_object_base::get_handle() const
{
    return m_handle;
}

// 0x0045BCC0 (.h:360)
unsigned int vint_object_base::get_name_crc() const
{
    return m_name_crc;
}

// 0x0045BCD0 (.h:371)
vint_document* vint_object_base::get_parent_document() const
{
    return m_document;
}

// 0x0045BCE0 (.h:373)
bool vint_object_base::is_rendered() const
{
    return false;
}

// 0x0045BCF0 (.h:397)
vint_object_base* vint_object_base::get_first_child() const
{
    return first_child;
}

// 0x0045BD00 (.h:398)
vint_object_base* vint_object_base::get_next_sibling() const
{
    return next_sibling;
}

// 0x0045BD10 (.h:399)
vint_object_base* vint_object_base::get_parent_object() const
{
    return parent_object;
}

// ---- vint_object_base.cpp

// 0x00469190 (:268)
bool vint_object_base::init(unsigned int _name_crc, vint_document* _parent_document)
{
    if (_parent_document == nullptr)
    {
        return false;
    }
    bool result = init();
    set_name_crc(_name_crc);
    m_document = _parent_document;
    memset(m_override_props, 0, sizeof(m_override_props));
    return result;
}

// 0x004691E0 (:283)
bool vint_object_base::init(const char* _name, vint_document* _document)
{
    if (_name == nullptr || _document == nullptr)
    {
        return false;
    }
    bool result = init();
    set_name_crc(crc_stri(_name, 0));
    m_document = _document;
    memset(m_override_props, 0, sizeof(m_override_props));
    return result;
}

// 0x00469240 (:314)
bool vint_object_base::init()
{
    m_name_crc = 0;
    m_document = nullptr;
    // the handle is the object's address
    m_handle = (unsigned int)this;
    parent_object = nullptr;
    first_child = nullptr;
    next_sibling = nullptr;
    memset(m_override_props, 0, sizeof(m_override_props));
    return true;
}

// 0x00469270 (:349)
void vint_object_base::destroy()
{
    vint_object_base* child = first_child;
    while (child != nullptr)
    {
        vint_object_base* next = child->next_sibling;
        child->destroy();
        child = next;
    }
    m_handle = 0;
    vint_dlist_remove(Name_hash_buckets[m_name_crc & 0x3F], this, &vint_object_base::name_hash_next, &vint_object_base::name_hash_prev);
    m_name_crc = 0;
    const vint_object_type_info* info = vint_object_types_get_type_info(get_type());
    if (info != nullptr && info->factory != nullptr)
    {
        // Original: the object is not unlinked from its parent (the parent is being destroyed too, or the caller
        // removed it first).
        info->factory->release(this);
    }
}

// 0x0047F6D0 (:390)
vint_object_base* vint_object_base::clone(bool _clone_children, vint_object_base* _parent)
{
    if (_parent == nullptr)
    {
        _parent = parent_object;
    }
    vint_object_base* copy = create_new(_parent, m_name_crc, get_type());
    if (copy == nullptr)
    {
        return nullptr;
    }
    // every editor-saved property, each index of an indexed one
    vint_object_properties_reflect props(get_type());
    for (int i = 0; i < props.m_num_props; ++i)
    {
        const vint_prop_lookup& prop = props.m_prop_info[i];
        if (!prop.editor_save)
        {
            continue;
        }
        vint_variant value;
        if ((int)prop.type < VINT_PROP_TYPE_ENUM)
        {
            value.set_type(prop.type);
        }
        if (!prop.is_indexed())
        {
            if (prop.get_value_callback != nullptr && prop.get_value_callback(this, value, 0))
            {
                bool is_override = get_is_override_property(prop.crc);
                value.set_is_override_property(is_override);
                if (prop.set_value_callback != nullptr)
                {
                    if (is_override)
                    {
                        copy->set_is_override_property(prop.crc, true);
                    }
                    prop.set_value_callback(copy, value, 0);
                }
            }
        }
        else
        {
            int index;
            if (prop.get_first_index_callback(this, index))
            {
                do
                {
                    if (prop.get_value_callback != nullptr)
                    {
                        bool got = prop.get_value_callback(this, value, index);
                        if (got)
                        {
                            value.set_is_override_property(get_is_override_property(prop.crc));
                        }
                        if (got && prop.set_value_callback != nullptr)
                        {
                            if (value.get_is_override_property())
                            {
                                copy->set_is_override_property(prop.crc, true);
                            }
                            prop.set_value_callback(copy, value, index);
                        }
                    }
                } while (prop.get_next_index_callback(this, index));
            }
        }
    }
    if (_clone_children)
    {
        for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
        {
            vint_object_base* child_copy = child->clone(true, copy);
            if (child_copy != nullptr)
            {
                // create_new added it already; re-added at the front (the same place)
                if (child_copy->parent_object != nullptr)
                {
                    child_copy->remove_from_parent();
                }
                child_copy->next_sibling = copy->first_child;
                child_copy->parent_object = copy;
                child_copy->m_document = copy->m_document;
                copy->first_child = child_copy;
            }
        }
    }
    return copy;
}

// 0x004604F0 (:465)
void vint_object_base::process()
{
}

// 0x00460500 (:470)
void vint_object_base::set_name_crc(unsigned int _new_name_crc)
{
    if (m_name_crc == _new_name_crc)
    {
        return;
    }
    if (m_name_crc != 0)
    {
        vint_dlist_remove(Name_hash_buckets[m_name_crc & 0x3F], this, &vint_object_base::name_hash_next, &vint_object_base::name_hash_prev);
    }
    m_name_crc = _new_name_crc;
    if (_new_name_crc != 0)
    {
        vint_dlist_add(Name_hash_buckets[_new_name_crc & 0x3F], this, &vint_object_base::name_hash_next, &vint_object_base::name_hash_prev);
    }
}

// 0x004605B0 (:491)
void vint_object_base::set_name(const char* _new_name)
{
    if (_new_name != nullptr)
    {
        set_name_crc(crc_stri(_new_name, 0));
    }
}

// 0x00475710 (:521)
bool vint_object_base::get_property_value(const char* _name, vint_variant& _value, int _index) const
{
    if (_name == nullptr)
    {
        return false;
    }
    return get_property_value(crc_stri(_name, 0), _value, _index);
}

// 0x004727C0 (:527)
bool vint_object_base::get_property_value(unsigned int _name_crc, vint_variant& _value, int _index) const
{
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(get_type());
    if (mgr == nullptr || &_value == nullptr)
    {
        return false;
    }
    const vint_prop_lookup* lookup = mgr->get_lookup(_name_crc);
    if (lookup == nullptr)
    {
        return false;
    }
    return lookup->get(this, _value, _index);
}

// 0x0047E360 (:534)
bool vint_object_base::set_property_value(const char* _name, const char* _value, int _index)
{
    if (_name == nullptr)
    {
        return false;
    }
    return set_property_value(crc_stri(_name, 0), _value, _index);
}

// 0x004790F0 (:540)
bool vint_object_base::set_property_value(unsigned int _name_crc, const char* _value, int _index)
{
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(get_type());
    if (mgr == nullptr)
    {
        return false;
    }
    const vint_prop_lookup* lookup = mgr->get_lookup(_name_crc);
    if (lookup == nullptr)
    {
        return false;
    }
    vint_variant value;
    if (lookup->type == VINT_PROP_TYPE_ENUM)
    {
        value = vint_enum_resolve(get_type(), lookup->name, _value);
        if ((int)value == 0x7FFFFFFF)
        {
            value.clear();
            value = _value;
        }
    }
    else
    {
        value.from_string(_value, lookup->type);
    }
    return lookup->set(this, value, _index);
}

// 0x0047F9B0 (:565)
bool vint_object_base::set_property_value(const char* _name, const vint_variant& _value, int _index)
{
    if (_name == nullptr)
    {
        return false;
    }
    unsigned int name_crc = crc_stri(_name, 0);
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(get_type());
    if (mgr == nullptr)
    {
        return false;
    }
    return mgr->set_prop_value(this, name_crc, &_value, _index);
}

// 0x0047E3A0 (:571)
bool vint_object_base::set_property_value(unsigned int _name_crc, const vint_variant& _value, int _index)
{
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(get_type());
    if (mgr == nullptr)
    {
        return false;
    }
    return mgr->set_prop_value(this, _name_crc, &_value, _index);
}

// 0x00475750 (:578)
const vint_prop_lookup* vint_object_base::get_property_lookup(const char* _name) const
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return get_property_lookup(crc_stri(_name, 0));
}

// 0x00472820 (:584)
const vint_prop_lookup* vint_object_base::get_property_lookup(unsigned int _name_crc) const
{
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(get_type());
    if (mgr == nullptr)
    {
        return nullptr;
    }
    return mgr->get_lookup(_name_crc);
}

// 0x004605E0 (:591)
void vint_object_base::set_is_override_property(unsigned int prop_name_crc, bool val)
{
    if (prop_name_crc == 0)
    {
        return;
    }
    if (val)
    {
        // into its slot or the first free one (a full list ignores it)
        for (unsigned int i = 0; i < 64; ++i)
        {
            if (m_override_props[i] == prop_name_crc || m_override_props[i] == 0)
            {
                m_override_props[i] = prop_name_crc;
                return;
            }
        }
        return;
    }
    // removed: the entries after it move down one
    bool found = false;
    for (unsigned int i = 0; i < 64; ++i)
    {
        if (m_override_props[i] == prop_name_crc)
        {
            found = true;
        }
        if (found)
        {
            if (i > 62)
            {
                m_override_props[i] = 0;
                return;
            }
            m_override_props[i] = m_override_props[i + 1];
        }
        if (m_override_props[i] == 0)
        {
            return;
        }
    }
}

// 0x00460660 (:639)
bool vint_object_base::get_is_override_property(unsigned int prop_name_crc) const
{
    for (unsigned int i = 0; i < 64; ++i)
    {
        if (m_override_props[i] == prop_name_crc)
        {
            return true;
        }
        if (m_override_props[i] == 0)
        {
            break;
        }
    }
    return false;
}

// 0x00460690 (:656)
vint_object_base* vint_object_base::find_object(unsigned int _handle, const vint_document* _document)
{
    // the handle is the address; it is valid while the object holds it
    if (_handle == 0 || _handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    vint_object_base* obj = (vint_object_base*)_handle;
    if (obj->m_handle != _handle || (_document != nullptr && obj->m_document != _document))
    {
        return nullptr;
    }
    return obj;
}

// 0x00469310 (:682)
vint_object_base* vint_object_base::find_object(const char* _name, const vint_document* _document)
{
    if (_name == nullptr || _document == nullptr)
    {
        return nullptr;
    }
    return find_object_by_name_crc(crc_stri(_name, 0), _document);
}

// 0x004606C0 (:691)
vint_object_base* vint_object_base::find_object_by_name_crc(unsigned int _crc, const vint_document* _document)
{
    if (_crc == 0 || _document == nullptr)
    {
        return nullptr;
    }
    vint_object_base* head = Name_hash_buckets[_crc & 0x3F];
    vint_object_base* obj = head;
    while (obj != nullptr)
    {
        if (obj->m_document == _document && obj->m_name_crc == _crc)
        {
            return obj;
        }
        obj = obj->name_hash_next;
        if (obj == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00469340 (:717)
bool vint_object_base::add_child(vint_object_base* _child)
{
    if (_child == nullptr)
    {
        return false;
    }
    if (_child->parent_object != nullptr)
    {
        _child->remove_from_parent();
    }
    _child->next_sibling = first_child;
    _child->parent_object = this;
    _child->m_document = m_document;
    first_child = _child;
    return true;
}

// 0x00460700 (:761)
bool vint_object_base::remove_from_parent()
{
    vint_object_base* parent = parent_object;
    if (parent == nullptr)
    {
        next_sibling = nullptr;
        return true;
    }
    vint_object_base* prev = parent->first_child;
    if (prev == nullptr)
    {
        return false;
    }
    if (prev == this)
    {
        parent->first_child = next_sibling;
        parent_object = nullptr;
        next_sibling = nullptr;
        return true;
    }
    while (prev->next_sibling != this && prev->next_sibling != nullptr)
    {
        prev = prev->next_sibling;
    }
    if (prev->next_sibling == nullptr)
    {
        return false;
    }
    prev->next_sibling = next_sibling;
    parent_object = nullptr;
    next_sibling = nullptr;
    return true;
}

// 0x00460760 (:803)
bool vint_object_base::is_descendant_of(vint_object_base* _ancestor_test) const
{
    if (_ancestor_test == nullptr)
    {
        return false;
    }
    for (const vint_object_base* obj = parent_object; obj != nullptr; obj = obj->parent_object)
    {
        if (obj == _ancestor_test)
        {
            return true;
        }
    }
    return false;
}

// 0x0047FA10 (:816)
void vint_object_base::xml_parse_properties_v2(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    // per property: the XML element that sets it (the current resolution's override, else the baseline)
    xml_element* prop_xml[64];
    memset(prop_xml, 0, sizeof(prop_xml));
    vint_object_properties_reflect props(get_type());
    xml_element* overrides = xml_find(_root, "overrides");
    if (overrides != nullptr)
    {
        for (xml_element* res = xml_find(overrides, "resolution"); res != nullptr; res = xml_find_next(overrides, res, "resolution"))
        {
            xml_element* name = xml_find(res, "name");
            const char* name_text = name != nullptr ? xml_get_string(name, nullptr) : nullptr;
            if (name_text == nullptr || Vint_current_resolution_mode != vint_project_find_resolution_mode(name_text))
            {
                continue;
            }
            for (xml_element* prop = xml_find(res, "property"); prop != nullptr; prop = xml_find_next(res, prop, "property"))
            {
                xml_element* prop_name = xml_find(prop, "name");
                const char* prop_name_text = prop_name != nullptr ? xml_get_string(prop_name, nullptr) : nullptr;
                if (prop_name_text == nullptr)
                {
                    continue;
                }
                unsigned int crc = crc_stri(prop_name_text, 0);
                for (int i = 0; i < props.m_num_props; ++i)
                {
                    if (props.m_prop_info[i].crc == crc)
                    {
                        prop_xml[i] = prop;
                        break;
                    }
                }
            }
            break;
        }
    }
    xml_element* baseline = xml_find(_root, "baseline");
    if (baseline != nullptr)
    {
        for (xml_element* prop = xml_find(baseline, "property"); prop != nullptr; prop = xml_find_next(baseline, prop, "property"))
        {
            xml_element* prop_name = xml_find(prop, "name");
            const char* prop_name_text = prop_name != nullptr ? xml_get_string(prop_name, nullptr) : nullptr;
            if (prop_name_text == nullptr)
            {
                continue;
            }
            unsigned int crc = crc_stri(prop_name_text, 0);
            for (int i = 0; i < props.m_num_props; ++i)
            {
                if (props.m_prop_info[i].crc == crc)
                {
                    if (prop_xml[i] == nullptr)
                    {
                        prop_xml[i] = prop;
                    }
                    break;
                }
            }
        }
    }
    for (int i = 0; i < props.m_num_props; ++i)
    {
        xml_element* prop = prop_xml[i];
        const vint_prop_lookup& lookup = props.m_prop_info[i];
        if (prop == nullptr || !lookup.editor_save)
        {
            continue;
        }
        xml_element* type = xml_find(prop, "type");
        const char* type_text = type != nullptr ? xml_get_string(type, nullptr) : nullptr;
        if (type_text == nullptr)
        {
            continue;
        }
        xml_element* value_xml = xml_find(prop, "value");
        const char* value_text = value_xml != nullptr ? xml_get_string(value_xml, nullptr) : nullptr;
        if (value_text == nullptr)
        {
            continue;
        }
        vint_variant_type value_type = vint_variant::get_type_from_name(type_text);
        if (value_type == VINT_PROP_TYPE_NONE)
        {
            continue;
        }
        vint_variant value;
        if (lookup.type == VINT_PROP_TYPE_ENUM && value_type == VINT_PROP_TYPE_STRING)
        {
            value = vint_enum_resolve(get_type(), lookup.name, value_text);
            if ((int)value == 0x7FFFFFFF)
            {
                value.clear();
                value = value_text;
            }
        }
        else
        {
            value.from_string(value_text, value_type);
        }
        lookup.set(this, value, 0);
    }
}

// 0x00469380 (:959)
vint_object_base* vint_object_base::create_new(vint_object_base* _parent, const char* _name, const char* _type_name)
{
    if (_type_name == nullptr || _parent == nullptr)
    {
        return nullptr;
    }
    const vint_object_type_info* info = vint_object_types_get_type_info(_type_name);
    if (info == nullptr || info->factory == nullptr)
    {
        return nullptr;
    }
    vint_object_base* obj = info->factory->allocate();
    if (obj == nullptr)
    {
        // formatted and dropped (rfg.exe: an assert's message)
        char msg[256];
        sprintf(msg, "VINT: Out of object_type %s.. please send this msg to Matt Gawalek with a description of where you were in the game.  K-thanks", _type_name);
        return nullptr;
    }
    // Original: the init result is ignored here (a null name leaves the object uninitialized but added).
    obj->init(_name, _parent->m_document);
    _parent->add_child(obj);
    obj->is_rendered();
    _parent->is_rendered();
    return obj;
}

// 0x00469470 (:1002)
vint_object_base* vint_object_base::create_new(vint_object_base* _parent, const char* _name, unsigned int _type)
{
    if (_name == nullptr || _parent == nullptr)
    {
        return nullptr;
    }
    const vint_object_type_info* info = vint_object_types_get_type_info(_type);
    if (info == nullptr || info->factory == nullptr)
    {
        return nullptr;
    }
    vint_object_base* obj = info->factory->allocate();
    if (obj == nullptr)
    {
        char msg[256];
        sprintf(msg, "VINT: Out of object_type %s.. please send this msg to Matt Gawalek with a description of where you were in the game.  K-thanks", info->name);
        return nullptr;
    }
    if (!obj->init(_name, _parent->m_document))
    {
        obj->m_handle = 0;
        info->factory->release(obj);
        return nullptr;
    }
    _parent->add_child(obj);
    obj->is_rendered();
    _parent->is_rendered();
    return obj;
}

// 0x00469570 (:1051)
vint_object_base* vint_object_base::create_new(vint_object_base* _parent, unsigned int _name_crc, unsigned int _type)
{
    if (_parent == nullptr)
    {
        return nullptr;
    }
    const vint_object_type_info* info = vint_object_types_get_type_info(_type);
    if (info == nullptr || info->factory == nullptr)
    {
        return nullptr;
    }
    vint_object_base* obj = info->factory->allocate();
    if (obj == nullptr)
    {
        char msg[256];
        sprintf(msg, "VINT: Out of object_type %s.. please send this msg to Matt Gawalek with a description of where you were in the game.  K-thanks", info->name);
        return nullptr;
    }
    obj->init(_name_crc, _parent->m_document);
    _parent->add_child(obj);
    obj->is_rendered();
    _parent->is_rendered();
    return obj;
}

// 0x0047E3F0 (:1097)
vint_object_properties_reflect::vint_object_properties_reflect(unsigned int _type)
{
    // m_num_props is left as it is when the type has no properties (rfg.exe: uninitialized)
    m_num_props = 0;
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(_type);
    if (mgr != nullptr)
    {
        m_num_props = mgr->get_prop_list(m_prop_info, 64);
    }
}

// 0x004790C0
vint_object_properties_reflect::~vint_object_properties_reflect()
{
}

// 0x00460790 (:1120)
bool vint_prop_lookup::set(vint_object_base* _obj, const vint_variant& _value, int _index) const
{
    if (set_value_callback == nullptr || _obj == nullptr)
    {
        return false;
    }
    if (_value.get_is_override_property())
    {
        _obj->set_is_override_property(crc, true);
    }
    return set_value_callback(_obj, _value, _index);
}

// 0x004607E0 (:1137)
bool vint_prop_lookup::get(const vint_object_base* _obj, vint_variant& _value, int _index) const
{
    if (get_value_callback == nullptr)
    {
        return false;
    }
    bool result = get_value_callback(_obj, _value, _index);
    if (result)
    {
        if (_obj == nullptr)
        {
            return false;
        }
        _value.set_is_override_property(_obj->get_is_override_property(crc));
    }
    return result;
}

// 0x00472850 (:1153)
const vint_prop_lookup* vint_prop_mgr::get_lookup(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return get_lookup(crc_stri(_name, 0));
}

// 0x00469640 (:1161)
const vint_prop_lookup* vint_prop_mgr::get_lookup(unsigned int _name_crc)
{
    // this type's table, then the parent types'
    for (vint_prop_mgr* mgr = this; mgr != nullptr; mgr = vint_object_type_prop_mgr(mgr->m_parent_type))
    {
        for (unsigned int i = 0; i < mgr->m_num_entries; ++i)
        {
            if (mgr->m_lookup_table[i].crc == _name_crc)
            {
                return &mgr->m_lookup_table[i];
            }
        }
        if (vint_object_types_get_type_info(mgr->m_parent_type) == nullptr)
        {
            break;
        }
    }
    return nullptr;
}

// 0x004791F0 (:1187)
unsigned int vint_prop_mgr::get_prop_list(vint_prop_lookup* _table, unsigned int _max_entries)
{
    // this type's properties, then the parent types' that are not overridden (by crc)
    unsigned int count = 0;
    vint_prop_mgr* mgr = this;
    while (mgr != nullptr && count < _max_entries)
    {
        for (unsigned int i = 0; i < mgr->m_num_entries && count < _max_entries; ++i)
        {
            bool present = false;
            for (unsigned int j = 0; j < count; ++j)
            {
                if (_table[j].crc == mgr->m_lookup_table[i].crc)
                {
                    present = true;
                    break;
                }
            }
            if (!present)
            {
                _table[count++] = mgr->m_lookup_table[i];
            }
        }
        if (vint_object_types_get_type_info(mgr->m_parent_type) == nullptr)
        {
            break;
        }
        mgr = vint_object_type_prop_mgr(mgr->m_parent_type);
    }
    return count;
}

// 0x0047E440 (:1224)
bool vint_prop_mgr::set_prop_value(vint_object_base* _obj, const char* _name, const vint_variant* _value, int _index)
{
    if (_name == nullptr || _obj == nullptr || _value == nullptr)
    {
        return false;
    }
    return set_prop_value(_obj, crc_stri(_name, 0), _value, _index);
}

// 0x004792F0 (:1232)
bool vint_prop_mgr::set_prop_value(vint_object_base* _obj, unsigned int _name_crc, const vint_variant* _value, int _index)
{
    if (_obj == nullptr || _value == nullptr)
    {
        return false;
    }
    const vint_prop_lookup* lookup = get_lookup(_name_crc);
    if (lookup == nullptr)
    {
        return false;
    }
    vint_variant value;
    value = *_value;
    vint_object_resolve_enum_string(lookup, _obj, value);
    return lookup->set(_obj, value, _index);
}

// 0x00472880 (:1260)
bool vint_prop_mgr::get_prop_value(const vint_object_base* _obj, const char* _name, vint_variant* _value, int _index)
{
    if (_obj == nullptr || _name == nullptr || _value == nullptr)
    {
        return false;
    }
    return get_prop_value(_obj, crc_stri(_name, 0), _value, _index);
}

// 0x004696A0 (:1268)
bool vint_prop_mgr::get_prop_value(const vint_object_base* _obj, unsigned int _name_crc, vint_variant* _value, int _index)
{
    if (_obj == nullptr || _value == nullptr)
    {
        return false;
    }
    const vint_prop_lookup* lookup = get_lookup(_name_crc);
    if (lookup == nullptr)
    {
        return false;
    }
    return lookup->get(_obj, *_value, _index);
}

// 0x004696E0 (:1279)
bool vint_prop_mgr::init(unsigned int _parent_type, const char* _friendly_name, vint_prop_lookup* _table, unsigned int _num_entries)
{
    m_parent_type = 0xFFFFFFFF;
    m_lookup_table = nullptr;
    m_num_entries = 0;
    if (_friendly_name == nullptr || _table == nullptr || _num_entries == 0)
    {
        return false;
    }
    m_parent_type = _parent_type;
    m_lookup_table = _table;
    m_num_entries = _num_entries;
    update_prop_name_crcs();
    return true;
}

// 0x00460840 (:1296)
void vint_prop_mgr::update_prop_name_crcs()
{
    if (m_lookup_table == nullptr)
    {
        return;
    }
    for (unsigned int i = 0; i < m_num_entries; ++i)
    {
        m_lookup_table[i].crc = crc_stri(m_lookup_table[i].name, 0);
        m_lookup_table[i].index = i;
    }
}

// 0x00460880 (:1309)
void vint_object_base::system_init()
{
    for (int i = 0; i < 64; ++i)
    {
        Name_hash_buckets[i] = nullptr;
    }
}

// 0x004608A0 (:1318)
vint_object_base* vint_object_base::find_child(unsigned int _name_crc)
{
    // depth first
    if (_name_crc == 0)
    {
        return nullptr;
    }
    for (vint_object_base* child = first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->m_name_crc == _name_crc)
        {
            return child;
        }
        if (vint_object_base* found = child->find_child(_name_crc))
        {
            return found;
        }
    }
    return nullptr;
}

// 0x0047E490 (:1342)
bool vint_object_set_property(unsigned int _obj_handle, unsigned int _prop_name_crc, const vint_variant& _value)
{
    if (_prop_name_crc == 0 || _obj_handle == 0)
    {
        return false;
    }
    vint_object_base* obj = (vint_object_base*)_obj_handle;
    vint_prop_mgr* mgr = vint_object_type_prop_mgr(obj->get_type());
    if (mgr == nullptr)
    {
        return false;
    }
    return mgr->set_prop_value(obj, _prop_name_crc, &_value, 0);
}

// 0x0047E4E0 (:1357)
bool vint_object_set_property(unsigned int _obj_handle, const char* _prop_name, const vint_variant& _value)
{
    if (_prop_name == nullptr)
    {
        return false;
    }
    return vint_object_set_property(_obj_handle, crc_stri(_prop_name, 0), _value);
}

// 0x004793C0 (:1366)
bool vint_object_get_property(unsigned int _obj_handle, unsigned int _prop_name_crc, vint_variant _value)
{
    // Original bug kept: the value is taken by value, so the caller never sees it.
    if (_prop_name_crc == 0 || _obj_handle == 0)
    {
        return false;
    }
    return ((vint_object_base*)_obj_handle)->get_property_value(_prop_name_crc, _value, 0);
}

// 0x00479400 (:1381)
bool vint_object_get_property(unsigned int _obj_handle, const char* _prop_name, vint_variant _value)
{
    // Original bug kept: the value is taken by value (and copied again), so the caller never sees it.
    if (_prop_name == nullptr)
    {
        return false;
    }
    vint_variant value;
    value = _value;
    unsigned int name_crc = crc_stri(_prop_name, 0);
    if (name_crc == 0 || _obj_handle == 0)
    {
        return false;
    }
    return ((vint_object_base*)_obj_handle)->get_property_value(name_crc, value, 0);
}

// 0x004608E0 (:1390)
vint_object_base* vint_object_find(unsigned int _obj_handle)
{
    return (vint_object_base*)_obj_handle;
}

// 0x0047FD80 (:1402)
vint_object_base* vint_object_base::xml_parse_object_v2(xml_element* _root, vint_object_base* _parent)
{
    if (_root == nullptr || _parent == nullptr)
    {
        return nullptr;
    }
    xml_element* e = xml_find(_root, "name");
    const char* name = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
    if (name == nullptr)
    {
        return nullptr;
    }
    e = xml_find(_root, "type");
    const char* type_name = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
    if (type_name == nullptr)
    {
        return nullptr;
    }
    vint_object_base* obj = create_new(_parent, name, type_name);
    if (obj != nullptr)
    {
        obj->xml_parse_properties_v2(_root);
        obj->xml_parse_children_v2(_root);
    }
    return obj;
}

// 0x004608F0 (:1463)
void vint_object_base::xml_parse_children_v2(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    xml_element* children = xml_find(_root, "children");
    if (children == nullptr)
    {
        return;
    }
    // Original: _root is passed as the parent (xml_find_next only checks it for null).
    for (xml_element* obj = xml_find(children, "object"); obj != nullptr; obj = xml_find_next(_root, obj, "object"))
    {
        xml_parse_object_v2(obj, this);
    }
}

// 0x00479470 (:1479)
bool vint_object_base::load_props_from_binary(vint_load_buffer& _buffer, const vint_bin_file_strings& _strings)
{
    // The overrides of the current resolution mode first (remembered as overrides), then the baseline properties
    // they did not set.
    unsigned int override_crcs[64];
    int num_override_crcs = 0;
    const vint_resolution_mode* mode = get_project_get_resolution_mode_info(Vint_current_resolution_mode);
    unsigned int baseline_offset = 0;
    _buffer.get_data(baseline_offset);
    if (baseline_offset >= _buffer.get_size())
    {
        return false;
    }
    unsigned char num_resolutions = 0;
    _buffer.get_data(num_resolutions);
    for (unsigned int r = 0; r < num_resolutions; ++r)
    {
        unsigned int name = 0;
        unsigned int offset = 0;
        _buffer.get_data(name);
        _buffer.get_data(offset);
        // Original: no check for a null mode.
        if (strcmp(_strings[name], mode->id) != 0)
        {
            continue;
        }
        if (offset >= _buffer.get_size())
        {
            return false;
        }
        _buffer.set_pos(offset);
        unsigned char type = 0;
        _buffer.get_data(type);
        while (type != 0)
        {
            unsigned int crc = 0;
            _buffer.get_data(crc);
            const vint_prop_lookup* lookup = get_property_lookup(crc);
            vint_variant value;
            vint_object_read_binary_value(_buffer, type, _strings, value, "unsupported value type on load");
            if (lookup != nullptr && value.get_type() != VINT_PROP_TYPE_NONE)
            {
                vint_object_resolve_enum_string(lookup, this, value);
                if (num_override_crcs < 64)
                {
                    override_crcs[num_override_crcs++] = crc;
                }
                value.set_is_override_property(true);
                lookup->set(this, value, 0);
            }
            type = 0;
            _buffer.get_data(type);
        }
        break;
    }
    _buffer.set_pos(baseline_offset);
    unsigned char type = 0;
    _buffer.get_data(type);
    while (type != 0)
    {
        unsigned int crc = 0;
        _buffer.get_data(crc);
        const vint_prop_lookup* lookup = get_property_lookup(crc);
        vint_variant value;
        vint_object_read_binary_value(_buffer, type, _strings, value, "unsupported value type on load");
        bool overridden = false;
        for (int i = 0; i < num_override_crcs; ++i)
        {
            if (override_crcs[i] == crc)
            {
                overridden = true;
                break;
            }
        }
        if (lookup != nullptr && !overridden && value.get_type() != VINT_PROP_TYPE_NONE)
        {
            vint_object_resolve_enum_string(lookup, this, value);
            lookup->set(this, value, 0);
        }
        type = 0;
        _buffer.get_data(type);
    }
    return true;
}

// 0x00479C30 (:1767)
vint_object_base* vint_object_base::load_from_binary(vint_load_buffer& _buffer, vint_object_base* _parent, const vint_bin_file_strings& strings)
{
    if (_parent == nullptr)
    {
        return nullptr;
    }
    // name string, type string, child count, one unused byte
    unsigned int name = 0;
    unsigned int type_name = 0;
    unsigned short num_children = 0;
    unsigned char unused;
    _buffer.get_data(name);
    _buffer.get_data(type_name);
    _buffer.get_data(num_children);
    _buffer.get_data(unused);
    vint_object_base* obj = create_new(_parent, strings[name], strings[type_name]);
    if (obj == nullptr)
    {
        return nullptr;
    }
    // create_new added it at the front already (add_child, inlined)
    if (obj->parent_object != nullptr)
    {
        obj->remove_from_parent();
    }
    obj->next_sibling = _parent->first_child;
    obj->parent_object = _parent;
    obj->m_document = _parent->m_document;
    _parent->first_child = obj;
    obj->load_props_from_binary(_buffer, strings);
    for (unsigned short i = 0; i < num_children; ++i)
    {
        load_from_binary(_buffer, obj, strings);
    }
    return obj;
}

// 0x0047FE10 (:2406)
void vint_object_base::xml_parse_children(xml_element* _root)
{
    // the version 1 format: names, types and the editor lock; the properties come from the document's property sets
    if (_root == nullptr)
    {
        return;
    }
    xml_element* children = xml_find(_root, "children");
    if (children == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(children, "object"); cur != nullptr; cur = xml_find_next(_root, cur, "object"))
    {
        xml_element* e = xml_find(cur, "name");
        const char* name = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        if (name == nullptr)
        {
            continue;
        }
        e = xml_find(cur, "type");
        const char* type_name = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
        if (type_name == nullptr)
        {
            continue;
        }
        xml_element* locked = xml_find(cur, "locked");
        vint_object_base* obj = create_new(this, name, type_name);
        if (obj == nullptr)
        {
            continue;
        }
        if (locked != nullptr)
        {
            vint_variant value(v_stricmp(locked->text, "true") == 0);
            obj->set_property_value("editor_locked", value, 0);
        }
        obj->xml_parse_children(cur);
    }
}

// 0x0047E550 (:2479)
bool vint_object_base::reload_overrides_props_from_binary(vint_load_buffer& _buffer, const vint_bin_file_strings& _strings)
{
    // Like load_props_from_binary, for a resolution change: the new mode's overrides are applied; the baseline is
    // reapplied only to properties that were overrides before; then the properties still flagged as overrides that
    // neither set return to their editor default.
    unsigned int set_crcs[64];
    int num_set_crcs = 0;
    const vint_resolution_mode* mode = get_project_get_resolution_mode_info(Vint_current_resolution_mode);
    unsigned int baseline_offset = 0;
    _buffer.get_data(baseline_offset);
    if (baseline_offset >= _buffer.get_size())
    {
        return false;
    }
    unsigned char num_resolutions = 0;
    _buffer.get_data(num_resolutions);
    for (unsigned int r = 0; r < num_resolutions; ++r)
    {
        unsigned int name = 0;
        unsigned int offset = 0;
        _buffer.get_data(name);
        _buffer.get_data(offset);
        if (strcmp(_strings[name], mode->id) != 0)
        {
            continue;
        }
        if (offset >= _buffer.get_size())
        {
            return false;
        }
        _buffer.set_pos(offset);
        unsigned char type = 0;
        _buffer.get_data(type);
        while (type != 0)
        {
            unsigned int crc = 0;
            _buffer.get_data(crc);
            const vint_prop_lookup* lookup = get_property_lookup(crc);
            vint_variant value;
            vint_object_read_binary_value(_buffer, type, _strings, value, "unsupported value type on reload_overrides");
            if (lookup != nullptr && value.get_type() != VINT_PROP_TYPE_NONE)
            {
                vint_object_resolve_enum_string(lookup, this, value);
                if (num_set_crcs < 64)
                {
                    set_crcs[num_set_crcs++] = crc;
                }
                value.set_is_override_property(true);
                lookup->set(this, value, 0);
            }
            type = 0;
            _buffer.get_data(type);
        }
        break;
    }
    _buffer.set_pos(baseline_offset);
    unsigned char type = 0;
    _buffer.get_data(type);
    while (type != 0)
    {
        unsigned int crc = 0;
        _buffer.get_data(crc);
        const vint_prop_lookup* lookup = get_property_lookup(crc);
        vint_variant value;
        vint_object_read_binary_value(_buffer, type, _strings, value, "unsupported value type on reload_overrides");
        bool already_set = false;
        for (int i = 0; i < num_set_crcs; ++i)
        {
            if (set_crcs[i] == crc)
            {
                already_set = true;
                break;
            }
        }
        vint_variant current;
        if (lookup != nullptr && lookup->get(this, current, 0) && current.get_is_override_property() && !already_set
            && value.get_type() != VINT_PROP_TYPE_NONE)
        {
            vint_object_resolve_enum_string(lookup, this, value);
            if (num_set_crcs < 64)
            {
                set_crcs[num_set_crcs++] = crc;
            }
            // Original: the override flag is kept (the baseline value is set without clearing it).
            lookup->set(this, value, 0);
        }
        type = 0;
        _buffer.get_data(type);
    }
    vint_object_properties_reflect props(get_type());
    for (int i = 0; i < props.m_num_props; ++i)
    {
        const vint_prop_lookup& lookup = props.m_prop_info[i];
        vint_variant value;
        value = lookup.editor_default_value;
        bool already_set = false;
        for (int j = 0; j < num_set_crcs; ++j)
        {
            if (set_crcs[j] == lookup.crc)
            {
                already_set = true;
                break;
            }
        }
        vint_variant current;
        if (lookup.get(this, current, 0) && current.get_is_override_property() && !already_set && value.get_type() != VINT_PROP_TYPE_NONE)
        {
            vint_object_resolve_enum_string(&lookup, this, value);
            lookup.set(this, value, 0);
        }
    }
    return true;
}

// 0x0047EEE0 (:2839)
vint_object_base* vint_object_base::reload_overrides_from_binary(vint_load_buffer& _buffer, vint_object_base* _parent, const vint_bin_file_strings& strings)
{
    if (_parent == nullptr)
    {
        return nullptr;
    }
    unsigned int name = 0;
    unsigned int type_name;
    unsigned short num_children = 0;
    unsigned char unused;
    _buffer.get_data(name);
    _buffer.get_data(type_name);
    _buffer.get_data(num_children);
    _buffer.get_data(unused);
    // the object is found by name under the parent (depth first)
    vint_object_base* obj = _parent->find_child(crc_stri(strings[name], 0));
    if (obj == nullptr)
    {
        return nullptr;
    }
    obj->reload_overrides_props_from_binary(_buffer, strings);
    for (unsigned short i = 0; i < num_children; ++i)
    {
        reload_overrides_from_binary(_buffer, obj, strings);
    }
    return obj;
}
