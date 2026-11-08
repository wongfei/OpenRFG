// Original: volition/rfg/code/objects/object_roadblock_node.h
// Implementation: volition/rfg/code/objects/object_roadblock_node.cpp
//
// Roadblock nodes: roadblocks built on roads from the roadblock tables: a type (roadblock_data_type) lists its squads
// and a formation of barricades and turrets placed relative to the node; spawned roadblocks are cleaned up when they
// stream out.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class matrix;
class player;
class roadblock_construction_info;
struct squad_definition;
struct turret_info;
class vehicle_spawn_group_info;
struct xml_element;

// size 0x1
struct roadblock_squad_flags
{
    bool allow_pursuit : 1; // 0x00:0
    bool stay_in_vehicle : 1; // 0x00:1
};

// size 0x1C
class roadblock_squad_info
{
public:
    roadblock_squad_info(); // 0x00A3D860 (object_roadblock_node_internal.h:123)
    bool load(xml_element* xml_root); // 0x00A92B10 (:388)
    void reset(); // 0x00A1BFA0 (:377)

    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* veh_group; // 0x04
    roadblock_squad_flags squad_flags; // 0x08
    vector offset_pos; // 0x0C
    float rotation; // 0x18
};

// size 0x10
class roadblock_barrier_info
{
public:
    bool load(xml_element* xml_root); // 0x00A1BFE0 (:455)
    void reset(); // 0x00A1BFC0 (:449)
    roadblock_barrier_info(); // 0x00A1C0D0

    vector offset_pos; // 0x00
    float rotation; // 0x0C
};

// size 0x14
class roadblock_turret_info
{
public:
    roadblock_turret_info(); // 0x00A3D840 (object_roadblock_node_internal.h:85)
    bool load(xml_element* xml_root); // 0x00A3D900 (:495)
    void reset(); // 0x00A1C0B0 (:488)

    const turret_info* t_info; // 0x00
    vector offset_pos; // 0x04
    float rotation; // 0x10
};

// size 0xF4
class roadblock_formation_info
{
public:
    roadblock_formation_info(); // 0x00A6A450 (object_roadblock_node_internal.h:103)
    bool load(const char* new_formation_name, xml_element* root); // 0x00A6A620 (:544)
    void reset(); // 0x00A3DA00 (:536)

    const char* formation_name; // 0x00
    farray<roadblock_barrier_info,8> barricade_info_list; // 0x04
    farray<roadblock_turret_info,4> turret_info_list; // 0x94
};

// size 0x3C
class roadblock_data_type
{
public:
    roadblock_data_type(); // 0x00AAB320 (object_roadblock_node_internal.h:149)
    bool load(const char* new_roadblock_name, xml_element* xml_root); // 0x00A92D70 (:607)
    void reset(); // 0x00A92C80 (:588)

    const char* roadblock_name; // 0x00
    unsigned int name_crc; // 0x04
    farray<roadblock_squad_info const*,8> squad_list; // 0x08
    const roadblock_formation_info* formation_info; // 0x38
};

// size 0x10
class roadblock_squad_instance_info
{
public:
    roadblock_squad_instance_info(); // 0x00A3D820 (.h:44)
    void reset(); // 0x00A1BDA0 (.h:52)

    roadblock_squad_flags squad_flags; // 0x00
    unsigned int spawn_node_handle; // 0x04
    unsigned int squad_handle; // 0x08
    unsigned int vehicle_handle; // 0x0C
};

// size 0x168
class object_roadblock_node : public object
{
public:
    object_roadblock_node(); // 0x00A6A750 (:664)
    static const object_type get_type_static(); // 0x00755020 (.h:66)
    static const object_sub_type get_subtype_static(); // 0x00755030 (.h:66)
    static const char* get_classname_static(); // 0x00A1BDB0 (.h:66)
    static int get_bpo_index_static(); // 0x00A1BDC0 (.h:66)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A6A780 (.h:66)
    static unsigned int get_classname_hash(); // 0x00A1C120 (:663)
    static int pool_remaining(); // 0x00A3DA20 (:663)
    virtual void pool_free(); // vtbl+0x80 0x00A92E80 (:663)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAB350 (:717)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AB4320 (:690)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A92EC0 (:706)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A92ED0 (:764)
public:
    bool is_active() const; // 0x00A1BDD0 (.h:89)
    void do_frame(); // 0x00A92EF0 (:842)
    bool start(const vector& target_pos); // 0x00ABA980 (:1073)
    void cleanup_roadblock(); // 0x00A6AAB0 (:870)
    unsigned int get_road_handle() const; // 0x00A1BDE0 (.h:101)
    float get_road_spline_dist() const;
    void destroy_all_objects(); // 0x00A6AEF0 (:1055)
private:
    bool build_road_data(); // 0x00A6ADE0 (:964)
    void place_bbox(roadblock_construction_info& construction_info); // 0x00A3DC40 (:1241)
    void place_barricades(roadblock_construction_info& construction_info); // 0x00AB4360 (:1323)
    void place_turrets(roadblock_construction_info& construction_info); // 0x00A3DFC0 (:1276)
    void place_guard_nodes(roadblock_construction_info& construction_info); // 0x00A3DAA0 (:1184)
    void place_squads(roadblock_construction_info& construction_info); // 0x00A6B0C0 (:1125)
    void place_single_squad_spawn(const vector& squad_pos, const matrix& squad_orient, const squad_definition* squad_def, const vehicle_spawn_group_info* veh_group, const roadblock_squad_info* squad_info, unsigned int guard_obj_handle); // 0x00A6AF60 (:1084)
    void reset_for_reuse(); // 0x00A3DA30 (:669)
    void reset_all(); // 0x00A3DA40 (:676)
    bool spawn_roadblock_dynamic(const vector& target_pos); // 0x00ABA590 (:995)
    void spawn_roadblock_objects(); // 0x00AAB430 (:1377)
    bool release_squads(const player* pp); // 0x00A6A7B0 (:775)
    bool on_managed_layer() const; // 0x00A1C130 (:754)
public:
    virtual ~object_roadblock_node(); // vtbl+0x00 0x00A6A790

private:
    const roadblock_data_type* roadblock_type; // 0x9C
    bool use_original_orient; // 0xA0
    unsigned int road_handle; // 0xA4
    vector road_center; // 0xA8
    float road_dist; // 0xB4
    vector road_fvec; // 0xB8
    vector road_rvec; // 0xC4
    float road_width; // 0xD0
    farray<roadblock_squad_instance_info,8> spawn_node_list; // 0xD4
    struct
    {
        unsigned int active : 1; // 0x00:0
        unsigned int reverse : 1; // 0x00:1
        unsigned int squads_released : 1; // 0x00:2
    } roadblock_flags; // 0x164
};

roadblock_formation_info* find_roadblock_formation(const char* roadblock_name); // 0x00A6A510 (:324)
roadblock_data_type* find_roadblock_info(const char* roadblock_name); // 0x00A6A580 (:341)
object* alloc_object_roadblock_node(); // 0x00A6A740 (:663)
const char* classname_object_roadblock_node(); // 0x00A1C110 (:663)
int bpo_index_object_roadblock_node(); // 0x00A1C100 (:663)
void types_object_roadblock_node(signed char& type, signed char& subtype); // 0x00A1C0F0 (:663)
void roadblock_system_init(); // 0x00AAB490 (:1532)
void roadblock_level_init_pre(); // 0x00A3E250 (:1565)
void roadblock_do_frame(); // 0x00A93130 (:1572)
bool roadblock_spawn(unsigned int roadblock_handle); // 0x00ABA9A0 (:1597)
void roadblock_destroy_all_created_objects(); // 0x00A6B2B0 (:1613)
void roadblock_destroy_all(); // 0x00A6B370 (:1633)
const object_roadblock_node* roadblock_nearest_block(unsigned int road_handle, const vector& pos); // 0x00A6B3C0 (:1645)

// 0x00755020 (.h:66)
inline const object_type object_roadblock_node::get_type_static()
{
    return OT_ROADBLOCK_NODE;
}

// 0x00755030 (.h:66)
inline const object_sub_type object_roadblock_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}
