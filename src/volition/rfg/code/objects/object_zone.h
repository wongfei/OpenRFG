// Original: volition/rfg/code/objects/object_zone.h
// Implementation: volition/rfg/code/objects/object_zone.cpp
//
// obj_zone, the object that represents a loaded zone in the world (one per world_zone; OT_ZONE): its terrain, the
// pathfinding layers (bb_pfg_zone) and its zone properties.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class ambient_spawn_info;
class game_aabb_phantom;
class hkpWorldObject;
struct rfg_terrain;
class rl_terrain_occluder;
class spawn_resource_data;
struct terrain_data;
struct weapon_info;

// size 0x1A0
class obj_zone : public object
{
public:
    obj_zone(); // 0x00A715B0 (:151)
    static const object_type get_type_static(); // 0x00743410 (.h:34)
    static const object_sub_type get_subtype_static(); // 0x00743420 (.h:34)
    static const char* get_classname_static(); // 0x00A069F0 (.h:34)
    static int get_bpo_index_static(); // 0x00A06A00 (.h:34)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A715F0 (.h:34)
    static unsigned int get_classname_hash(); // 0x00A1E100 (:150)
    static int pool_remaining(); // 0x00A40E30 (:150)
    virtual void pool_free(); // vtbl+0x80 0x00A95330 (:150)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1E110 (:156)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1E210 (:162)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AADA50 (:176)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1E250 (:424)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1E260 (:429)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00A71600 (.h:47)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00A71780 (:525)
    terrain_data* get_terrain_data() const; // 0x00743430 (.h:99)
    const char* get_terrain_name() const;
    void deinstantiate_resources(); // 0x00A71630 (:464)
    unsigned short get_grid_xy() const; // 0x00A1E2A0 (:448)
    virtual ~obj_zone(); // vtbl+0x00 0x00A71610

    bb_pfg_zone zone_pfg; // 0x9C
    unsigned int heightfield_shape; // 0xB4
    unsigned int stitch_piece_havok_handle; // 0xB8
    unsigned int district_handle; // 0xBC
    const spawn_resource_data* spawn_res_data; // 0xC0
    const ambient_spawn_info* amb_spawn_info; // 0xC4
    float wind_min_speed; // 0xC8
    float wind_max_speed; // 0xCC
private:
    rfg_terrain* terrain; // 0xD0
    farray<rl_terrain_occluder*,40> m_occluders; // 0xD4
    vector local_bmin; // 0x184
    vector local_bmax; // 0x190
    int aabb_phantom_index; // 0x19C
};

// size 0xC
class obj_zone_props : public obj_prop_block
{
public:
    obj_zone_props(const char* terrain_name, const vector& pos, unsigned int hash); // 0x009308B0 (.h:114)
    ~obj_zone_props(); // 0x0092E620

};

// size 0x8
struct obj_zone_phatom
{
    obj_zone_phatom(); // 0x00A1E0A0 (:38)

    game_aabb_phantom* handle; // 0x00
    bool in_use; // 0x04
};

const char* classname_obj_zone(); // 0x00A1E0F0 (:150)
object* alloc_obj_zone(); // 0x00A715A0 (:150)
void types_obj_zone(signed char& type, signed char& subtype); // 0x00A1E0D0 (:150)
int bpo_index_obj_zone(); // 0x00A1E0E0 (:150)
void obj_zone_level_shutdown(); // 0x00A1E2B0 (:565)
bool obj_zone_streamed(const vector& pos); // 0x00A1E2E0 (:581)
bool obj_zone_streamed_grid_id(unsigned short zone_id); // 0x00A1E300 (:596)
bool obj_zone_streamed_xy(unsigned short zone_xy); // 0x00A1E320 (:610)
bool obj_zone_streamed_handle(unsigned int handle); // 0x00A1E350 (:621)
