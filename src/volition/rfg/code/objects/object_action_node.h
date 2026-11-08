// Original: volition/rfg/code/objects/object_action_node.h
// Implementation: volition/rfg/code/objects/object_action_node.cpp
//
// Action nodes (OT_ACTION_NODE): places where ambient NPCs play an action (smoke, lean, work, sit, ...). An
// action_node_definition (action node table) names the animation sequences (action_node_animation_info: entry, state
// and exit animations with their effects), the cooldown, the prop an NPC picks up and when it is created and removed;
// nodes are collected into sets for an NPC, reserved, used and disabled (by scripting, obstruction, destruction,
// missions, ...).
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_action_node_enum.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

class ambient_prop_info;
class human;
class item;
class matrix;
class npc;
class player;
class rfg_destruction_event;
class rfg_mover;
struct xml_element;

// size 0xC
class animation_effect_info
{
public:
    animation_effect_info(); // 0x00A33C50 (object_action_node_internal.h:82)
    void reset(); // 0x00A151A0 (:809)

    animation_action anim_action; // 0x00
    int min_cooldown_ms; // 0x04
    int max_cooldown_ms; // 0x08
};

// size 0x64
class action_node_animation_info
{
public:
    action_node_animation_info(); // 0x00A5B4E0 (object_action_node_internal.h:96)
    void reset(); // 0x00A38DE0 (:821)
    animation_action get_state_action_anim() const; // 0x00A151B0 (:834)
    bool load(xml_element* xml_root); // 0x00A60380 (:845)

    animation_state state_anim; // 0x00
    animation_action entry_anim; // 0x04
    animation_action exit_anim; // 0x08
    int min_period_ms; // 0x0C
    int max_period_ms; // 0x10
    vector anim_start_offset; // 0x14
    float anim_heading; // 0x20
    farray<animation_effect_info,4> action_list; // 0x24
};

// size 0x80
class action_node_definition
{
public:
    action_node_definition(); // 0x00A5B550 (object_action_node_internal.h:142)
    void reset(); // 0x00A38E20 (:946)
    bool load(xml_element* xml_root); // 0x00A8C590 (:1034)
    bool load_prop_info(xml_element* xml_root); // 0x00A151C0 (:974)

    const char* name; // 0x00
    const char* player_prompt; // 0x04
    farray<action_node_animation_info const*,4> action_anim_list; // 0x08
    int cooldown_period_ms; // 0x28
    action_node_snap_target snap_target; // 0x2C
    const char* action_type; // 0x30
    const action_node_definition* next_node_def; // 0x34
    const action_node_definition* required_node_def; // 0x38
    struct
    {
        unsigned int allow_edf : 1; // 0x00:0
        unsigned int allow_guerrilla : 1; // 0x00:1
        unsigned int allow_marauder : 1; // 0x00:2
        unsigned int allow_player : 1; // 0x00:3
        unsigned int allow_patrols : 1; // 0x00:4
        unsigned int allow_amb_peds : 1; // 0x00:5
        unsigned int disable_by_destruction : 1; // 0x00:6
        unsigned int reusable : 1; // 0x00:7
        unsigned int hide_player_weapon : 1; // 0x00:8
        unsigned int disable_collision : 1; // 0x00:9
        unsigned int script_point_enable : 1; // 0x00:10
        unsigned int prop_required : 1; // 0x00:11
        unsigned int weapon_required : 1; // 0x00:12
        unsigned int weapon_cabinet : 1; // 0x00:13
        unsigned int free_hostage : 1; // 0x00:14
        unsigned int ground_marker_highlight : 1; // 0x00:15
        unsigned int ignore_y : 1; // 0x00:16
        unsigned int male_only : 1; // 0x00:17
        unsigned int melee_weapon_required : 1; // 0x00:18
        unsigned int allow_ambient_lines : 1; // 0x00:19
        unsigned int not_disabled_with_safehouse : 1; // 0x00:20
        unsigned int linked_to_node : 1; // 0x00:21
        unsigned int no_object_interaction : 1; // 0x00:22
    } node_flags; // 0x3C
    const ambient_prop_info* amb_prop_info; // 0x40
    vector prop_offset_pos; // 0x44
    float prop_offset_rot_x; // 0x50
    float prop_offset_rot_y; // 0x54
    float prop_offset_rot_z; // 0x58
    action_node_snap_target prop_offset_target; // 0x5C
    int prop_pickup_trigger_id; // 0x60
    int prop_drop_trigger_id; // 0x64
    action_node_prop_creation_time prop_creation_time; // 0x68
    action_node_prop_removal_time prop_removal_time; // 0x6C
    unsigned int hotspot_effect_info; // 0x70
    vector hotspot_effect_offset; // 0x74
};

// size 0xFC
class object_action_node : public object
{
public:
    object_action_node(); // 0x00A60630 (:1160)
    static const object_type get_type_static(); // 0x005DBF10 (.h:84)
    static const object_sub_type get_subtype_static(); // 0x005DBF20 (.h:84)
    static const char* get_classname_static(); // 0x00A05610 (.h:84)
    static int get_bpo_index_static(); // 0x00A05620 (.h:84)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A60690 (.h:84)
    static unsigned int get_classname_hash(); // 0x00A15380 (:1159)
    static int pool_remaining(); // 0x00A38E90 (:1159)
    virtual void pool_free(); // vtbl+0x80 0x00A8CA70 (:1159)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA3510 (:1165)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AA39A0 (:1298)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A15390 (:1317)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A153A0 (:1327)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A154A0 (:1335)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A15520 (:1341)
    bool disabled() const; // 0x005DBF30 (.h:113)
    bool get_disabled_by(action_node_disabled_reason reason) const; // 0x00A05630 (.h:116)
    void set_disabled_by(action_node_disabled_reason reason); // 0x00A8CA90 (:1533)
    void clear_disabled_by(action_node_disabled_reason reason); // 0x00AA3BA0 (:1558)
    bool is_cooling_down() const; // 0x005DBF40 (.h:121)
    const char* get_ambient_type() const; // 0x00A155F0 (:1484)
    bool is_allowed_to_use(human_teams team) const; // 0x00A15540 (:1374)
    bool is_allowed_to_use(const human* hp) const; // 0x00A155B0 (:1394)
    bool is_allowed_to_use(unsigned int human_handle) const; // 0x00A38EA0 (:1491)
    bool is_occupied() const; // 0x00A8CA80 (:1498)
    human* get_occupant() const; // 0x00A606F0 (:1504)
    void disable_node_collision(human* hp); // 0x00AA3C10 (:1601)
    void enable_node_collision(human* hp); // 0x00A15600 (:1624)
    int pick_action_state() const; // 0x00A38F00 (:1630)
    bool has_entry_anim() const; // 0x00A38F30 (:1645)
    bool has_exit_anim() const; // 0x00A38F50 (:1653)
    bool look_at_parent() const; // 0x00A15620 (:1661)
    bool is_reusable() const; // 0x00A15630 (:1668)
    bool get_entry_point(const human* hp, vector& entry_point) const; // 0x00AA3C50 (:1675)
    bool at_entry_point(const npc* np) const; // 0x00AA4300 (:2242)
    bool at_entry_point(const player* pp) const; // 0x00AA4270 (:2220)
    bool at_entry_point(const human* hp) const; // 0x00AA43E0 (:2286)
    bool get_entry_rotation(const human* hp, matrix& entry_rotation) const; // 0x00A8CDD0 (:2307)
    bool enter_node(human* hp); // 0x00AB9E00 (:2700)
    bool has_entered_node(const human* hp) const; // 0x00A391A0 (:2685)
    void exit_node(human* hp); // 0x00A8D170 (:2604)
    void emergency_exit(human* hp); // 0x00A8CFE0 (:2542)
    void set_cooldown_time(); // 0x00A156F0 (:2423)
    unsigned int get_occupant_handle() const; // 0x00A607A0 (:1519)
    bool allow_player() const; // 0x00A156D0 (:2385)
    bool disabled_by_destruction() const; // 0x00A156E0 (:2391)
    void clear_cooldown(); // 0x005DBF70 (.h:198)
    bool play_action(human* hp); // 0x00AA4430 (:2803)
    bool doing_action(human* hp); // 0x00A608E0 (:2842)
    const char* get_player_action_display_prompt() const; // 0x00A15790 (:2536)
    bool player_hide_weapon() const; // 0x00A15710 (:2429)
    bool is_weapon_cabinet_action_node() const; // 0x00A15720 (:2435)
    bool is_free_hostage_action_node() const; // 0x00A15730 (:2441)
    animation_action get_player_animation(const player* pp) const; // 0x00A60880 (:2519)
    animation_state get_animation_state(int node_idx) const; // 0x00A39170 (:2530)
    bool has_collision_disabled() const; // 0x00AA3BD0 (:1576)
    int get_action_node_type() const; // 0x00A390E0 (:2447)
    int get_next_action_node_type() const; // 0x00A39100 (:2453)
    bool is_linked_to() const; // 0x00A15740 (:2463)
    bool has_pickup_prop_trigger() const; // 0x00A15750 (:2469)
    bool has_drop_prop_trigger() const; // 0x00A15770 (:2480)
    item* get_pickup_object() const; // 0x00A39120 (:2495)
    void create_prop_object(); // 0x00AA40C0 (:2023)
    int get_pickup_trigger_id() const; // 0x00A38F70 (:1703)
    int get_drop_trigger_id() const; // 0x00A38FE0 (:1748)
    bool pickup_action_trigger_elapsed(human* hp) const; // 0x00A38F90 (:1713)
    bool drop_action_trigger_elapsed(human* hp) const; // 0x00A607C0 (:1730)
    const ambient_prop_info* get_ambient_prop_info() const; // 0x00A15640 (:1758)
    bool pickup_trigger_elapsed(const human* hp) const; // 0x00A39000 (:1764)
    bool drop_trigger_elapsed(const human* hp) const; // 0x00A39070 (:1779)
    void destroy_child_relations(); // 0x00AA3CB0 (:1795)
    void destroy_old_children(); // 0x00A8CAF0 (:1991)
    bool is_ped_only() const; // 0x00A15650 (:1815)
    bool is_patrol_only() const; // 0x00A15660 (:1821)
    bool has_ambient_actions(const human* hp) const; // 0x00A60810 (:1827)
    bool play_ambient_action(human* hp); // 0x00AA3D70 (:1889)
    const farray<unsigned int,8>& get_node_links() const; // 0x00A05650 (.h:280)
    bool is_high_priority() const; // 0x005DBF80 (.h:283)
    bool has_infinite_duration() const; // 0x00A05660 (.h:286)
    bool should_run_to() const; // 0x005DBF90 (.h:289)
    bool prop_requirement_met(const human* hp) const; // 0x00A15670 (:1848)
    bool ambient_action_ready() const; // 0x005DBFA0 (.h:295)
    bool weapon_required() const; // 0x00A156A0 (:1859)
    bool melee_weapon_required() const; // 0x00A156B0 (:1864)
    bool allow_ambient_voice_lines() const; // 0x00A156C0 (:1869)
    bool is_object_target(object* test_object); // 0x00AA3D20 (:1876)
    bool is_flyer_sabotage_node(); // 0x00AA3D40 (:1882)
    bool has_ground_marker_highlight() const; // 0x00A157A0 (:2901)
    bool get_entry_point_int(const action_node_animation_info* anim_info, vector& entry_point) const; // 0x00A8CC70 (:2164)
    const object* get_target_object() const; // 0x00A8CC60 (:2158)
    const object* get_prop_target_object() const; // 0x00A8CC50 (:2152)
    void start_hotspot_effect(); // 0x00A8D3C0 (:2859)
    void stop_hotspot_effect(); // 0x00A391E0 (:2912)
    void update_position_disabled_status(); // 0x00AB3AC0 (:1473)
    bool get_position_disabled_status() const; // 0x00AA3A10 (:1412)
    virtual ~object_action_node(); // vtbl+0x00 0x00A606A0

    const action_node_definition* base_node_info; // 0x9C
    timestamp cooldown_period; // 0xA0
    int selected_cooldown_time; // 0xA4
    struct
    {
        unsigned short high_priority : 1; // 0x00:0
        unsigned short infinite_duration : 1; // 0x00:1
        unsigned short run_to : 1; // 0x00:2
        unsigned short parented : 1; // 0x00:3
    } node_flags; // 0xA8
    int disabled_by; // 0xAC
    timestamp position_disabled_check_timer; // 0xB0
    unsigned int hotspot_effect_handle; // 0xB4
    int action_node_script_index; // 0xB8
    unsigned int last_target_object_handle; // 0xBC
    unsigned int child_prop_handle; // 0xC0
    farray<unsigned int,8> node_links; // 0xC4
    timestamp action_cooldown_timer; // 0xF4
    timestamp voice_timeout; // 0xF8
};

const object_action_node* linked_occupied_node_of_type(const farray<unsigned int,8>& linked_node_handles, const action_node_definition* required_node_def); // 0x00A5FED0 (:315)
// Also used by downloadable_content.cpp.
extern string_pool Action_node_string_pool; // 0x030F7E10

void load_action_node_definitions(int dlc_id); // 0x00AA3190 (:731)
int bpo_index_object_action_node(); // 0x00A15360 (:1159)
object* alloc_object_action_node(); // 0x00A60620 (:1159)
const char* classname_object_action_node(); // 0x00A15370 (:1159)
void types_object_action_node(signed char& type, signed char& subtype); // 0x00A15350 (:1159)
void action_node_do_frame(); // 0x00AB3B20 (:2983)
void action_node_system_init(); // 0x00AA45C0 (:3019)
bool action_node_collect_nodes(const npc* np, float radius, farray<object_action_node const*,608>& node_list, bool ped_only, bool patrol_only); // 0x00A60910 (:3049)
bool action_node_collect_nodes(const player* pp, float radius, farray<object_action_node const*,608>& node_list); // 0x00A60AD0 (:3154)
void action_node_disable_node(unsigned int node_handle, action_node_disabled_reason reason); // 0x00A8D5A0 (:3186)
bool action_node_reserve_node(unsigned int human_handle, const farray<unsigned int,4>& node_set); // 0x00AA4700 (:3196)
bool action_node_reserve_node(human* hp, const farray<unsigned int,4>& node_set); // 0x00A8D5D0 (:3210)
bool action_node_reserve_node(player* pp, const object_action_node* anp); // 0x00A8D5F0 (:3219)
bool action_node_build_set(unsigned int human_handle, unsigned int root_node_handle, farray<unsigned int,4>& node_set); // 0x00AA4730 (:3234)
bool action_node_build_set(const human* hp, const object_action_node* root_anp, farray<unsigned int,4>& node_set); // 0x00A8D870 (:3346)
bool action_node_has_entered_node(const human* hp, const object_action_node* anp); // 0x00A39240 (:3358)
bool action_node_is_entering(const human* hp); // 0x00A157D0 (:3364)
bool action_node_is_exiting(const human* hp); // 0x00A157F0 (:3371)
void action_node_process_destruction_event(const rfg_destruction_event* eventp); // 0x00A8D8A0 (:3382)
void enable_all_action_nodes_in_a_building(const rfg_mover* building); // 0x00AA47A0 (:3421)
void disable_all_action_nodes_in_a_building(const rfg_mover* building); // 0x00A8DAA0 (:3439)
