// Original: volition/interface_lib/code/objects/vint_object_base.h
// Implementation: volition/interface_lib/code/objects/vint_object_base.cpp
//
// The base of every document object: name hash, tree links, document, property lookup and reflection (get/set by name
// through vint_prop_lookup tables), process, clone, find, destroy; the name hash buckets and the process stack.
#pragma once

#include "volition/interface_lib/code/vint_variant.h"

class vint_bin_file_strings;
class vint_document;
class vint_load_buffer;
class vint_object_base;
struct xml_element;

// size 0x48
struct vint_prop_lookup
{
    vint_prop_lookup(const char* _name, vint_variant_type _type, bool (*_set_cb)(vint_object_base*, const vint_variant&, int), bool (*_get_cb)(const vint_object_base*, vint_variant&, int), const vint_variant& _default, bool _editor_save); // 0x00476290 (.h:277)
    vint_prop_lookup(const char*, vint_variant_type, bool (*)(vint_object_base*, const vint_variant&, int), bool (*)(const vint_object_base*, vint_variant&, int), const vint_variant&, bool (*)(const vint_object_base*, int&), bool (*)(const vint_object_base*, int&), bool);
    vint_prop_lookup(); // 0x00476280 (.h:202)
    bool set(vint_object_base* _obj, const vint_variant& _value, int _index) const; // 0x00460790 (:1120)
    bool get(const vint_object_base* _obj, vint_variant& _value, int _index) const; // 0x004607E0 (:1137)
    bool is_indexed() const; // 0x0045BC00 (.h:208)
    bool get_first_index(const vint_object_base* _obj, int& _index) const; // 0x0045BC20 (.h:213)
    bool get_next_index(const vint_object_base* _obj, int& _index) const; // 0x0045BC40 (.h:222)
private:
    void init(const char* _name, vint_variant_type _type, bool (*_set_cb)(vint_object_base*, const vint_variant&, int), bool (*_get_cb)(const vint_object_base*, vint_variant&, int), const vint_variant& _default, bool (*_first_index_cb)(const vint_object_base*, int&), bool (*_next_index_cb)(const vint_object_base*, int&), bool _editor_save); // 0x004744B0 (.h:285)
public:
    ~vint_prop_lookup(); // 0x00476390
    vint_prop_lookup& operator=(const vint_prop_lookup& __that); // 0x004757B0

    const char* name; // 0x00
    vint_variant_type type; // 0x04
    bool (*set_value_callback)(vint_object_base*, const vint_variant&, int); // 0x08
    bool (*get_value_callback)(const vint_object_base*, vint_variant&, int); // 0x0C
    bool (*get_first_index_callback)(const vint_object_base*, int&); // 0x10
    bool (*get_next_index_callback)(const vint_object_base*, int&); // 0x14
    unsigned int crc; // 0x18
    unsigned int index; // 0x1C
    bool allow_res_override; // 0x20
    bool editor_save; // 0x21
    vint_variant editor_default_value; // 0x28
};

// size 0x1208
class vint_object_properties_reflect
{
public:
    vint_object_properties_reflect(unsigned int _type); // 0x0047E3F0 (:1097)
    ~vint_object_properties_reflect(); // 0x004790C0

    vint_prop_lookup m_prop_info[64]; // 0x00
    int m_num_props; // 0x1200
};

// size 0xC
class vint_prop_mgr
{
public:
    vint_prop_mgr(); // 0x0045BC60 (.h:303)
    ~vint_prop_mgr(); // 0x0045BC70 (.h:304)
    bool init(unsigned int _parent_type, const char* _friendly_name, vint_prop_lookup* _table, unsigned int _num_entries); // 0x004696E0 (:1279)
    void update_prop_name_crcs(); // 0x00460840 (:1296)
    unsigned int get_prop_list(vint_prop_lookup* _table, unsigned int _max_entries); // 0x004791F0 (:1187)
    const vint_prop_lookup* get_lookup(unsigned int _name_crc); // 0x00469640 (:1161)
    const vint_prop_lookup* get_lookup(const char* _name); // 0x00472850 (:1153)
    bool set_prop_value(vint_object_base* _obj, unsigned int _name_crc, const vint_variant* _value, int _index); // 0x004792F0 (:1232)
    bool set_prop_value(vint_object_base* _obj, const char* _name, const vint_variant* _value, int _index); // 0x0047E440 (:1224)
    bool get_prop_value(const vint_object_base* _obj, unsigned int _name_crc, vint_variant* _value, int _index); // 0x004696A0 (:1268)
    bool get_prop_value(const vint_object_base* _obj, const char* _name, vint_variant* _value, int _index); // 0x00472880 (:1260)
    unsigned int get_num_entries();
    unsigned int get_parent_type();

private:
    vint_prop_lookup* m_lookup_table; // 0x00
    unsigned int m_num_entries; // 0x04
    unsigned int m_parent_type; // 0x08
};

// size 0x12C
class vint_object_base
{
public:
    vint_object_base(); // 0x00468120 (.h:342)
    virtual ~vint_object_base(); // vtbl+0x00 0x0045BC80 (.h:344)
protected:
    virtual bool init() = 0; // vtbl+0x04 0x00469240 (:314)
public:
    bool init(unsigned int _name_crc, vint_document* _parent_document); // 0x00469190 (:268)
    bool init(const char* _name, vint_document* _document); // 0x004691E0 (:283)
    virtual void destroy(); // vtbl+0x08 0x00469270 (:349)
    virtual void process(); // vtbl+0x0C 0x004604F0 (:465)
    virtual void clean_up(); // vtbl+0x10 0x0045BC90 (.h:351)
    virtual vint_object_base* clone(bool _clone_children, vint_object_base* _parent); // vtbl+0x14 0x0047F6D0 (:390)
    virtual bool is_type(unsigned int _type) const; // vtbl+0x18 0x0045BCA0 (.h:355)
    virtual unsigned int get_type() const = 0; // vtbl+0x1C
    unsigned int get_handle() const; // 0x0045BCB0 (.h:359)
    unsigned int get_name_crc() const; // 0x0045BCC0 (.h:360)
    static vint_object_base* create_new(vint_object_base* _parent, const char* _name, unsigned int _type); // 0x00469470 (:1002)
    static vint_object_base* create_new(vint_object_base* _parent, const char* _name, const char* _type_name); // 0x00469380 (:959)
    static vint_object_base* create_new(vint_object_base* _parent, unsigned int _name_crc, unsigned int _type); // 0x00469570 (:1051)
    void set_name(const char* _new_name); // 0x004605B0 (:491)
    vint_document* get_parent_document() const; // 0x0045BCD0 (.h:371)
    virtual bool is_rendered() const; // vtbl+0x20 0x0045BCE0 (.h:373)
    bool add_child(vint_object_base* _child); // 0x00469340 (:717)
    bool remove_from_parent(); // 0x00460700 (:761)
    bool is_descendant_of(vint_object_base* _ancestor_test) const; // 0x00460760 (:803)
    bool get_property_value(unsigned int _name_crc, vint_variant& _value, int _index) const; // 0x004727C0 (:527)
    bool get_property_value(const char* _name, vint_variant& _value, int _index) const; // 0x00475710 (:521)
    bool set_property_value(unsigned int _name_crc, const char* _value, int _index); // 0x004790F0 (:540)
    bool set_property_value(const char* _name, const char* _value, int _index); // 0x0047E360 (:534)
    bool set_property_value(unsigned int _name_crc, const vint_variant& _value, int _index); // 0x0047E3A0 (:571)
    bool set_property_value(const char* _name, const vint_variant& _value, int _index); // 0x0047F9B0 (:565)
    static void system_init(); // 0x00460880 (:1309)
    static vint_object_base* find_object(unsigned int _handle, const vint_document* _document); // 0x00460690 (:656)
    static vint_object_base* find_object(const char* _name, const vint_document* _document); // 0x00469310 (:682)
    static vint_object_base* find_object_by_name_crc(unsigned int _crc, const vint_document* _document); // 0x004606C0 (:691)
    vint_object_base* find_child(unsigned int _name_crc); // 0x004608A0 (:1318)
    vint_object_base* get_first_child() const; // 0x0045BCF0 (.h:397)
    vint_object_base* get_next_sibling() const; // 0x0045BD00 (.h:398)
    vint_object_base* get_parent_object() const; // 0x0045BD10 (.h:399)
    const vint_prop_lookup* get_property_lookup(unsigned int _name_crc) const; // 0x00472820 (:584)
    const vint_prop_lookup* get_property_lookup(const char* _name) const; // 0x00475750 (:578)
    static int get_num_properties(unsigned int);
    void set_is_override_property(unsigned int prop_name_crc, bool val); // 0x004605E0 (:591)
    bool get_is_override_property(unsigned int prop_name_crc) const; // 0x00460660 (:639)
protected:
    static vint_object_base* load_from_binary(vint_load_buffer& _buffer, vint_object_base* _parent, const vint_bin_file_strings& strings); // 0x00479C30 (:1767)
    bool load_props_from_binary(vint_load_buffer& _buffer, const vint_bin_file_strings& _strings); // 0x00479470 (:1479)
    void xml_parse_properties_v2(xml_element* _root); // 0x0047FA10 (:816)
    void xml_parse_children_v2(xml_element* _root); // 0x004608F0 (:1463)
    void xml_parse_children(xml_element* _root); // 0x0047FE10 (:2406)
    static vint_object_base* xml_parse_object_v2(xml_element* _root, vint_object_base* _parent); // 0x0047FD80 (:1402)
    void set_name_crc(unsigned int _new_name_crc); // 0x00460500 (:470)
    static vint_object_base* reload_overrides_from_binary(vint_load_buffer& _buffer, vint_object_base* _parent, const vint_bin_file_strings& strings); // 0x0047EEE0 (:2839)
    bool reload_overrides_props_from_binary(vint_load_buffer& _buffer, const vint_bin_file_strings& _strings); // 0x0047E550 (:2479)

    // 0x00 vfptr
public:
    static const unsigned int Object_type; // 0x012B94C8
    vint_object_base* name_hash_next; // 0x04
    vint_object_base* name_hash_prev; // 0x08
    vint_object_base* factory_next; // 0x0C
    vint_object_base* factory_prev; // 0x10
    vint_object_base* first_child; // 0x14
    vint_object_base* next_sibling; // 0x18
    vint_object_base* parent_object; // 0x1C
private:
    // friends are not recorded in rfg.pdb: documents link and load their objects directly
    friend class vint_document;
    unsigned int m_handle; // 0x20
    unsigned int m_name_crc; // 0x24
    vint_document* m_document; // 0x28
    unsigned int m_override_props[64]; // 0x2C
};

extern vint_object_base* Process_stack[256]; // 0x01A3D778

bool vint_object_set_property(unsigned int _obj_handle, unsigned int _prop_name_crc, const vint_variant& _value); // 0x0047E490 (:1342)
bool vint_object_set_property(unsigned int _obj_handle, const char* _prop_name, const vint_variant& _value); // 0x0047E4E0 (:1357)
bool vint_object_get_property(unsigned int _obj_handle, unsigned int _prop_name_crc, vint_variant _value); // 0x004793C0 (:1366)
bool vint_object_get_property(unsigned int _obj_handle, const char* _prop_name, vint_variant _value); // 0x00479400 (:1381)
vint_object_base* vint_object_find(unsigned int _obj_handle); // 0x004608E0 (:1390)
