// Original: volition/rfg/code/objects/object_convoy.h
// Implementation: volition/rfg/code/objects/object_convoy.cpp
//
// object_convoy, a column of squads driving a route (OT_CONVOY): its member squads (convoy_member_info), route and
// state. convoy_do_frame runs once per frame in single player.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class base_activity_data;
class convoy_definition;
struct convoy_squad_definition;
enum convoy_state;
class matrix;
class object_convoy_end_point;
class object_patrol;
class object_squad;
struct squad_definition;
class vehicle;
class vehicle_spawn_group_info;

// size 0x10
struct convoy_member_info
{
    void clear(); // 0x00A094A0 (.h:197)
    bool in_use() const; // 0x00754670 (.h:204)
    convoy_squad_roles get_role() const; // 0x00754680 (.h:206)

    const convoy_squad_definition* squad_info; // 0x00
    unsigned int squad_handle; // 0x04
    int on_foot_follow_index; // 0x08
    struct
    {
        unsigned int left_convoy_formation : 1; // 0x00:0
    } squad_flags; // 0x0C
};

// size 0x1998
class object_convoy : public object
{
    friend class follow_leader_goal; // reads flags (inferred)
    friend float select_convoy_speed(const object_convoy* convoy); // reads flags (inferred)
    friend class convoy_lead_goal; // reads flags (inferred)
    friend class convoy_follow_goal; // reads flags (inferred)
    friend void convoy_do_frame(); // inlines do_frame
public:
    friend class activity_convoy_data; // find_message_points reads the route (inlined)
    // size 0x1
    struct convoy_flags
    {
        bool spawned : 1; // 0x00:0
        bool attacked : 1; // 0x00:1
        bool active_spawn : 1; // 0x00:2
    };

    object_convoy(); // 0x00A61920 (:331)
    static const object_type get_type_static() { return OT_CONVOY; } // 0x005DDB80 (.h:217); a zero would make World.object_from_handle<object_convoy> answer for humans
    static const object_sub_type get_subtype_static(); // 0x005DDB90 (.h:217)
    static const char* get_classname_static(); // 0x00A094C0 (.h:217)
    static int get_bpo_index_static(); // 0x00A094D0 (.h:217)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A61960 (.h:217)
    static unsigned int get_classname_hash(); // 0x00A16120 (:330)
    static int pool_remaining(); // 0x00A39B50 (:330)
    virtual void pool_free(); // vtbl+0x80 0x00A8E030 (:330)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA5910 (:336)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A8E040 (:411)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A8E140 (:425)
    const convoy_definition* get_definition() const; // 0x00A094E0 (.h:232)
    int get_base_convoy_size() const; // 0x00602CF0 (.h:233)
    int get_num_squads() const; // 0x00A39B60 (:620)
    int get_num_vehicles() const; // 0x00A8E4B0 (:636)
    int get_num_target_vehicles() const; // 0x00A8E560 (:737)
    bool spawned() const; // 0x00A094F0 (.h:239)
    bool convoy_assets_ready() const; // 0x00A619C0 (:715)
    void evaluate_squad_spawn(); // 0x00AA5AD0 (:651)
    object_squad* get_primary_target_squad() const; // 0x00A39C00 (:756)
    vehicle* get_primary_target_vehicle() const; // 0x00A61A10 (:772)
    int get_member_index(unsigned int member_squad_handle) const; // 0x00A16130 (:794)
    object_squad* get_member_squad(int index) const; // 0x00A39C60 (:811)
    unsigned int get_member_squad_handle(int) const;
    unsigned int get_lead_squad_handle() const; // 0x00A39CF0 (:861)
    object_squad* get_lead_squad() const; // 0x00A39C90 (:840)
    vehicle* get_lead_vehicle() const; // 0x00A61A90 (:882)
    bool is_convoy_member(unsigned int squad_handle) const; // 0x00A161A0 (:868)
    float get_move_speed() const; // 0x00A16160 (:820)
    float get_dist_sq_to_nearest_following_member(const vector& check_pos, unsigned int exclude_squad) const; // 0x00A39D10 (:897)
    unsigned int join_convoy_formation(object_squad* squad); // 0x00A39D40 (:942)
    bool leave_convoy_formation(unsigned int squad_handle); // 0x00A61AC0 (:1012)
    unsigned int get_main_formation_handle() const; // 0x005DDBA0 (.h:275)
    convoy_squad_roles get_squad_role(unsigned int squad_handle) const; // 0x00A162A0 (:1190)
    object_squad* get_on_foot_follow_squad(unsigned int squad_handle) const; // 0x00A3A000 (:1208)
    bool get_ghost_destination(const vector& my_pos, vector& dest_pos); // 0x00A39F10 (:1119)
    bool get_next_destination(const vector& my_pos, vector& dest_pos); // 0x00A39F70 (:1138)
    bool is_at_final_destination(); // 0x00A39FE0 (:1164)
    const farray<vector,512>& get_route() const; // 0x00754690 (.h:289)
    int get_next_visited_index() const; // 0x00A16250 (:1157)
    bool in_convoy_formation(unsigned int squad_handle) const; // 0x00A16260 (:1170)
    bool attacked() const; // 0x005DDBB0 (.h:298)
    void set_attacked_flag(); // 0x007546A0 (.h:301)
    unsigned int get_patrol_handle() const; // 0x00B1FC60 (.h:304)
    bool spawn_squads(); // 0x00A8E5C0 (:1453)
    const convoy_member_info* get_convoy_member_info(int index) const; // 0x00A16300 (:1442)
    convoy_state do_frame(); // 0x00AA5B50 (:1289)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8E1B0 (:438)
    bool spawn(); // 0x00A61F20 (:1468)
    bool spawn_squad_at_point(int index, const vector& squad_pos, const matrix& squad_orient); // 0x00A61B50 (:1358)
    void do_squad_member_spawning(); // 0x00AA5AF0 (:1255)
    void evaluate_squad_spawn_active(); // 0x00A39BB0 (:662)
    void evaluate_squad_spawn_inactive(); // 0x00A8E500 (:688)
    void remove_squad_from_convoy(int index, object_squad* squad); // 0x00A8E5F0 (:1561)
    bool squad_is_valid(convoy_squad_roles role, const object_squad* const squad) const; // 0x00A8E710 (:1592)
    bool vehicle_squad_is_valid(const object_squad* squad) const; // 0x00A620F0 (:1609)
    bool on_foot_squad_is_valid(const object_squad* squad) const; // 0x00A16320 (:1652)
    bool remove_dead_squads(); // 0x00A8E750 (:1665)
    bool update_formation(); // 0x00A3A080 (:1690)
    bool update_pos_and_orient_from_lead_vehicle(); // 0x00A621A0 (:1751)
    void update_attacked_status(); // 0x00A8E7C0 (:1801)
    void update_break_status(); // 0x00A3A150 (:1778)
    bool build_route(); // 0x00A8E250 (:470)
    int rotate_to_back(int index); // 0x00A161D0 (:976)
    bool set_next_destination(const object_patrol*);
    void update_destination(const vector& new_pos, bool simulated); // 0x00A39E60 (:1083)
    void update_destination_ghost(bool simulated); // 0x00A39DD0 (:1048)
    void find_next_destination(); // 0x00A39E80 (:1093)
    const object_patrol* get_patrol() const; // 0x00A3A1B0 (:1869)
public:
    virtual ~object_convoy(); // vtbl+0x00 0x00A61970

protected:
    const convoy_definition* base_convoy_definition; // 0x9C
    convoy_member_info member_info[12]; // 0xA0
    unsigned int main_formation_handle; // 0x160
    convoy_flags flags; // 0x164
    unsigned int original_patrol_handle; // 0x168
    farray<vector,512> convoy_route; // 0x16C
    vector start_pos; // 0x197C
    vector end_pos; // 0x1988
    int next_visited_index; // 0x1994
};

enum convoy_type
{
    CONVOY_TYPE_UNDEFINED = -1,
    CONVOY_TYPE_CAPTURE = 0,
    CONVOY_TYPE_DESTROY = 1,
    CONVOY_TYPE_MISSION = 2,
    NUM_CONVOY_TYPES = 3,
};

// size 0xC
struct convoy_squad_definition
{
    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* vehicle_group; // 0x04
    convoy_squad_roles role; // 0x08
};

// size 0x120
class convoy_definition
{
public:
    convoy_definition(); // 0x00A61830 (:296)
    int get_num_vehicles() const; // 0x00A39B20 (:313)

    const char* name; // 0x00
    const base_activity_data* base_data; // 0x04
    farray<convoy_squad_definition,12> squad_definitions; // 0x08
    int difficulty_level; // 0xA8
    float normal_speed; // 0xAC
    float evade_speed; // 0xB0
    convoy_type type; // 0xB4
    int tech_level_reward; // 0xB8
    activity_standard_rewards standard_rewards; // 0xBC
    int tod_spawn_start_time; // 0xC8
    int tod_spawn_end_time; // 0xCC
    voice_line_handle radio_lines[20]; // 0xD0
};

void types_object_convoy(signed char& type, signed char& subtype); // 0x00A160F0 (:330)
object* alloc_object_convoy(); // 0x00A61910 (:330)
int bpo_index_object_convoy(); // 0x00A16100 (:330)
const char* classname_object_convoy(); // 0x00A16110 (:330)
void convoy_do_frame(); // 0x00AB3C60 (:1231)
void convoy_definitions_init(); // 0x00A8E900 (:1971)
const convoy_definition* convoy_definition_from_name(const char* name); // 0x00A3A1E0 (:2204)
const convoy_definition* convoy_definition_from_index(int index); // 0x00A3A230 (:2225)
int convoy_definition_get_index_from_name(const char* name); // 0x00A3A250 (:2238)
int convoy_definition_get_index(const convoy_definition* convoy_def); // 0x00A3A290 (:2257)
human_teams convoy_definition_get_team(const convoy_definition* convoy_def); // 0x00A3A2C0 (:2275)
object_patrol* convoy_find_patrol(const object_convoy_end_point* objp); // 0x00A8F0B0 (:2281)
object_convoy* convoy_create(const object_convoy_end_point* convoy_objp, const convoy_definition* convoy_def, const char* name); // 0x00AA5BB0 (:2296)
object_convoy* convoy_create(const object_patrol* patrol, const convoy_definition* convoy_def, const char* name); // 0x00A8F100 (:2311)
void console_print_available_convoys(); // 0x00A3A2E0 (:2347)
const farray<convoy_definition const*,20>& convoy_definition_master_list(); // 0x00A16340 (:2358)
bool convoy_is_leader(const vehicle* vp); // 0x00A621F0 (:2364)
object_convoy* convoy_find_convoy(unsigned int squad_handle); // 0x00A62290 (:2377)
object_convoy* convoy_find_convoy(const object_squad* squad); // 0x00A62310 (:2401)
