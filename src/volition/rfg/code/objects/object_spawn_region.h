// Original: volition/rfg/code/objects/object_spawn_region.h
// Implementation: volition/rfg/code/objects/object_spawn_region.cpp
//
// Spawn regions: box regions (with a near box) where squads of a team can spawn.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/util/varray.h"

class vector;

// size 0xEC
class object_spawn_region : public object
{
    // human_teams_get_team reads the team directly (friends are not recorded in rfg.pdb)
    friend human_teams human_teams_get_team(const object* obj);
public:
    object_spawn_region(); // 0x00A1D0F0 (:47)
    static const object_type get_type_static(); // 0x00754090 (.h:29)
    static const object_sub_type get_subtype_static(); // 0x007540A0 (.h:29)
    static const char* get_classname_static(); // 0x00A0D210 (.h:29)
    static int get_bpo_index_static(); // 0x00A0D220 (.h:29)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1D110 (.h:29)
    static unsigned int get_classname_hash(); // 0x00A1D0E0 (:46)
    static int pool_remaining(); // 0x00A3ED70 (:46)
    virtual void pool_free(); // vtbl+0x80 0x00A946D0 (:46)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AACB60 (:54)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1D130 (:139)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1D170 (:145)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A1D1B0 (:152)
    human_teams get_team() const; // 0x00A0D230 (.h:53)
    bool team_allowed_to_spawn(human_teams other_team, bool is_vehicle_spawn) const; // 0x00A3EDA0 (:167)
    bool is_inside(const vector& point) const; // 0x00A3EE20 (:187)
    bool is_near_by(const vector& point) const; // 0x00A3EEB0 (:201)
    bool get_spawn_position(float x_dist, float z_dist, vector& spawn_pos) const; // 0x00A1D1D0 (:215)
    bool get_spawn_list(float offset_size, farray<vector,128>& spawn_point_list) const; // 0x00A6DD70 (:254)
    void disable(); // 0x00B1FD00 (.h:71)
    bool is_disabled() const; // 0x00A1D360 (:229)
    void set_continuous_attack_region(bool enabled); // 0x00B1FD10 (.h:74)
    bool is_continuous_attack_region() const;
    void set_vrail_spawn_flag(bool spawn_flag); // 0x007540B0 (.h:78)
    bool get_vrail_spawn_flag() const; // 0x007540D0 (.h:79)
    virtual ~object_spawn_region(); // vtbl+0x00 0x00A1D120

private:
    bbox local_bb; // 0x9C
    bbox bb; // 0xB4
    bbox near_bb; // 0xCC
    human_teams team; // 0xE4
    struct
    {
        bool disabled : 1; // 0x00:0
        bool npc_only : 1; // 0x00:1
        bool vehicle_only : 1; // 0x00:2
        bool cont_attack_region : 1; // 0x00:3
        bool vrail_spawn_region : 1; // 0x00:4
    } region_flags; // 0xE8
};

void types_object_spawn_region(signed char& type, signed char& subtype); // 0x00A1D0B0 (:46)
int bpo_index_object_spawn_region(); // 0x00A1D0C0 (:46)
const char* classname_object_spawn_region(); // 0x00A1D0D0 (:46)
object* alloc_object_spawn_region(); // 0x00A6DD60 (:46)
const object_spawn_region* spawn_region_find_region(human_teams team, bool vehicle_spawn, const vector& point, float min_dist_sqrd); // 0x00A6E040 (:307)
const object_spawn_region* spawn_region_find_near_region(human_teams team, bool vehicle_spawn, const vector& point); // 0x00A6E150 (:334)
