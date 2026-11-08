// Original: volition/rfg/code/objects/object_mission_start_node.h
// Implementation: volition/rfg/code/objects/object_mission_start_node.cpp
//
// Mission start nodes: where a mission is started, with its mission info, enable and autostart flags and the marker
// effect.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class mission_info;

// size 0xA8
class object_mission_start_node : public object
{
public:
    object_mission_start_node(); // 0x00A19FA0 (:132)
    static const object_type get_type_static(); // 0x00754EC0 (.h:22)
    static const object_sub_type get_subtype_static(); // 0x00754ED0 (.h:22)
    static const char* get_classname_static(); // 0x00A100C0 (.h:22)
    static int get_bpo_index_static(); // 0x0091D000 (.h:22)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A19FC0 (.h:22)
    static unsigned int get_classname_hash(); // 0x00A19F90 (:131)
    static int pool_remaining(); // 0x00A3CDB0 (:131)
    virtual void pool_free(); // vtbl+0x80 0x00A92080 (:131)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AB41C0 (:144)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A67270 (:198)
    virtual void free_this(); // vtbl+0x28 0x00A92090 (:226)
    mission_info* get_mission_info() const; // 0x00754EE0 (.h:31)
    void enable_markers(); // 0x00AA9E80 (:235)
    void disable_markers(); // 0x00A920B0 (:283)
    bool can_start(); // 0x00A19FE0 (:325)
    void set_enabled(bool set_val); // 0x00AA9FB0 (:336)
    bool should_autostart(); // 0x00A100D0 (.h:41)
private:
    void start_effect(); // 0x00A3CDE0 (:356)
    void stop_effect(); // 0x00A3CE40 (:365)
public:
    virtual ~object_mission_start_node(); // vtbl+0x00 0x00A19FD0

private:
    mission_info* m_info; // 0x9C
    bool enabled; // 0xA0
    bool autostart; // 0xA1
    unsigned int effect_handle; // 0xA4
};

void unused_dcf_mission_set_enabled(); // 0x00AB40A0 (:42)
const char* classname_object_mission_start_node(); // 0x00A19F80 (:131)
object* alloc_object_mission_start_node(); // 0x00A67260 (:131)
int bpo_index_object_mission_start_node(); // 0x00A19F70 (:131)
void types_object_mission_start_node(signed char& type, signed char& subtype); // 0x00A19F60 (:131)
void object_mission_start_node_do_frame(); // 0x00AB42F0 (:384)
void object_mission_start_node_level_init(); // 0x00A1A030 (:400)
void object_mission_start_node_update_markers(); // 0x00AB4310 (:407)
