// Original: volition/rfg/code/objects/object_marauder_ambush_region.h
// Implementation: volition/rfg/code/objects/object_marauder_ambush_region.cpp
//
// Marauder ambush regions: a box region that may spawn an ambush (marauder, EDF and guerrilla squads from their
// spawn nodes) when the player passes, with day and night probabilities, skirmishes between teams and abort checks.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class matrix;
class object_squad;
class object_squad_spawn_node;

// size 0x4
struct ambush_region_flags
{
    unsigned int enabled : 1; // 0x00:0
    unsigned int ambush_is_triggered : 1; // 0x00:1
    unsigned int marauders_used : 1; // 0x00:2
    unsigned int edf_used : 1; // 0x00:3
    unsigned int guerrillas_used : 1; // 0x00:4
    unsigned int teams_checked : 1; // 0x00:5
    unsigned int skirmish_ambush : 1; // 0x00:6
    unsigned int abort_ambush : 1; // 0x00:7
};

// size 0x110
class marauder_ambush_region : public object
{
public:
    marauder_ambush_region(); // 0x00A19CA0 (:59)
    static const object_type get_type_static(); // 0x00A19C00 (.h:48)
    static const object_sub_type get_subtype_static(); // 0x00A19C10 (.h:48)
    static const char* get_classname_static(); // 0x00A19C20 (.h:48)
    static int get_bpo_index_static(); // 0x00A19C30 (.h:48)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A19CD0 (.h:48)
    static unsigned int get_classname_hash(); // 0x00A19C90 (:58)
    static int pool_remaining(); // 0x00A3CC30 (:58)
    virtual void pool_free(); // vtbl+0x80 0x00A91800 (:58)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A91810 (:247)
public:
    void check_teams(); // 0x00A67190 (:635)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA9B40 (:140)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A19CF0 (:216)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A19D10 (:253)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A19D50 (:260)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A19D90 (:270)
    float get_radius() const; // 0x00A19E10 (:281)
    void enable(); // 0x00A19E20 (:329)
    void disable(); // 0x00A19E30 (:335)
    void disable_ambush(); // 0x00A19E70 (:359)
    void trigger_ambush(); // 0x00A91820 (:410)
    void find_skirmish_squads(); // 0x00A3CC90 (:509)
    void update_squads(); // 0x00A91C90 (:680)
    void check_abort_conditions(); // 0x00A19E80 (:736)
    float get_ambush_chance(); // 0x00A19E50 (:348)
    bool is_enabled() const; // 0x00A19E40 (:341)
    bool is_triggered() const; // 0x00A19C40 (.h:122)
    void reset_trigger();
private:
    int count_squad_spawns(); // 0x00A3CC60 (:386)
    void clean_old_vehicle(const object_squad_spawn_node* squad_node) const; // 0x00A67100 (:365)
    bool update_squad_orders(object_squad* squad_ptr); // 0x00A919D0 (:536)
public:
    virtual ~marauder_ambush_region(); // vtbl+0x00 0x00A19CE0

private:
    bbox bb; // 0x9C
    bbox local_bb; // 0xB4
    float radius; // 0xCC
    float night_ambush_prob; // 0xD0
    float day_ambush_prob; // 0xD4
    int min_squad_spawns; // 0xD8
    int max_squad_spawns; // 0xDC
    unsigned int edf_squad_spawn_node; // 0xE0
    unsigned int marauder_squad_spawn_node; // 0xE4
    unsigned int guerrilla_squad_spawn_node; // 0xE8
    object_squad* edf_squad; // 0xEC
    object_squad* marauder_squad; // 0xF0
    object_squad* guerrilla_squad; // 0xF4
public:
    bool is_day; // 0xF8
    vector last_pos; // 0xFC
    timestamp squad_update_timer; // 0x108
    ambush_region_flags flags; // 0x10C
};

const char* classname_marauder_ambush_region(); // 0x00A19C80 (:58)
int bpo_index_marauder_ambush_region(); // 0x00A19C70 (:58)
void types_marauder_ambush_region(signed char& type, signed char& subtype); // 0x00A19C60 (:58)
object* alloc_marauder_ambush_region(); // 0x00A66E90 (:58)
void ambush_region_system_init(); // 0x00A19EE0 (:770)
bool ambush_region_point_inside(const marauder_ambush_region* trigger, const vector& point); // 0x00A19EF0 (:780)
void ambush_region_do_frame(); // 0x00A91E00 (:787)
