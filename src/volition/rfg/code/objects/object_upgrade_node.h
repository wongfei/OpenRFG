// Original: volition/rfg/code/objects/object_upgrade_node.h
// Implementation: volition/rfg/code/objects/object_upgrade_node.cpp
//
// Upgrade nodes: the upgrade shop markers.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

// size 0xA4
class object_upgrade_node : public object
{
public:
    object_upgrade_node(); // 0x00A1E020 (:39)
    static const object_type get_type_static() { return OT_UPGRADE_NODE; } // 0x00A10120 (.h:29)
    static const object_sub_type get_subtype_static() { return OT_SUB_UNDEFINED; } // 0x00A10130 (.h:29)
    static const char* get_classname_static(); // 0x00A10140 (.h:29)
    static int get_bpo_index_static(); // 0x00A10150 (.h:29)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1E040 (.h:29)
    static unsigned int get_classname_hash(); // 0x00A1E010 (:38)
    static int pool_remaining(); // 0x00A40CE0 (:38)
    virtual void pool_free(); // vtbl+0x80 0x00A95310 (:38)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAD990 (:51)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A40D10 (:88)
    virtual void free_this(); // vtbl+0x28 0x00A95320 (:116)
    void enable_markers(); // 0x00A40D80 (:123)
    void disable_markers(); // 0x00A40DC0 (:134)
    bool is_enabled() { return enabled; } // 0x00A10160 (.h:41)
    void set_enabled(bool set_val); // 0x00A40DD0 (:140)
private:
    void start_effect(); // 0x00A1E060 (:160)
    void stop_effect(); // 0x00A1E090 (:166)
public:
    virtual ~object_upgrade_node(); // vtbl+0x00 0x00A1E050

private:
    bool enabled; // 0x9C
    int upgrade_node_type; // 0xA0
};

const char* classname_object_upgrade_node(); // 0x00A1E000 (:38)
object* alloc_object_upgrade_node(); // 0x00A71150 (:38)
void types_object_upgrade_node(signed char& type, signed char& subtype); // 0x00A1DFE0 (:38)
int bpo_index_object_upgrade_node(); // 0x00A1DFF0 (:38)
void object_upgrade_node_enable_markers(); // 0x00A71160 (:171)
void object_upgrade_node_disable_markers(); // 0x00A711E0 (:179)
