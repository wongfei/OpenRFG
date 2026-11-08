// Original: volition/rfg/code/objects/object_riding_shotgun_node.h
// Implementation: volition/rfg/code/objects/object_riding_shotgun_node.cpp
//
// Riding shotgun nodes: the start points of the riding shotgun activity (a flare effect marks an available node).
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class object_patrol;
struct riding_shotgun_info;

// size 0xA8
class object_riding_shotgun_node : public object
{
public:
    object_riding_shotgun_node(); // 0x00A1BCC0 (:11)
    static const object_type get_type_static(); // 0x00754040 (.h:24)
    static const object_sub_type get_subtype_static(); // 0x00754050 (.h:24)
    static const char* get_classname_static(); // 0x00A10100 (.h:24)
    static int get_bpo_index_static(); // 0x00A10110 (.h:24)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1BCE0 (.h:24)
    static unsigned int get_classname_hash(); // 0x00A1BCB0 (:10)
    static int pool_remaining(); // 0x00A3D730 (:10)
    virtual void pool_free(); // vtbl+0x80 0x00A92AB0 (:10)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAB210 (:16)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1BD00 (:50)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1BD20 (:65)
    object_patrol* find_patrol() const; // 0x00A92AC0 (:75)
    const riding_shotgun_info* get_riding_shotgun_info() const; // 0x00754060 (.h:38)
    bool is_completed() const; // 0x00754070 (.h:40)
    void set_completed(); // 0x00754080 (.h:41)
    bool flare_started() const; // 0x00A3D760 (:116)
    void start_flare(); // 0x00A3D790 (:123)
    void stop_flare(); // 0x00A3D7D0 (:131)
    bool can_spawn() const; // 0x00A1BD40 (:87)
    virtual ~object_riding_shotgun_node(); // vtbl+0x00 0x00A1BCF0

private:
    friend class cartographer; // get_activity_rewards reads rs_info (inferred)
    const riding_shotgun_info* rs_info; // 0x9C
    unsigned int flare_effect_handle; // 0xA0
    struct
    {
        unsigned int completed : 1; // 0x00:0
    } rs_flags; // 0xA4
};

object* alloc_object_riding_shotgun_node(); // 0x00A6A440 (:10)
const char* classname_object_riding_shotgun_node(); // 0x00A1BCA0 (:10)
void types_object_riding_shotgun_node(signed char& type, signed char& subtype); // 0x00A1BC80 (:10)
int bpo_index_object_riding_shotgun_node(); // 0x00A1BC90 (:10)
char* riding_shotgun_get_name(const object_riding_shotgun_node* const orsn); // 0x00A1BD80 (:139)

// 0x00754040 (.h:24)
inline const object_type object_riding_shotgun_node::get_type_static()
{
    return OT_RIDING_SHOTGUN_NODE;
}

// 0x00754050 (.h:24)
inline const object_sub_type object_riding_shotgun_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754070 (.h:40)
inline bool object_riding_shotgun_node::is_completed() const
{
    return rs_flags.completed != 0;
}
