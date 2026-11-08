// Original: volition/rfg/code/gameplay/activities/activity_internal.h
//
// activity_base, the base of the activities (raids, house arrest, demolitions master, guerrilla express...): mode and
// properties, the layer objects it owns, its state and interface state with their timers, the district and its data
// (base_activity_data). activity_base_free runs in the open world; activity_base_modal takes over the game while
// active. The concrete activities are deferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_disable_reason;
enum activity_load_state;
class base_activity_data;
struct cfile;
class district;
struct explosion_info;
class npc;
class object;
class object_squad;
class patrol_node_handle;
class rfg_mover;
class vector;

// size 0x1
class base_activity_properties
{
public:
    base_activity_properties(bool modal_, bool can_always_start_); // 0x00753B50 (.h:82)

    bool can_always_start : 1; // 0x00:0
    bool modal : 1; // 0x00:1
};

// size 0x444
class activity_base
{
    friend class mission_info; // mission_completed clears disable flag 2 of every activity
public:
    virtual ~activity_base(); // vtbl+0x00 0x00768B90 (.h:96)
    const char* get_name() const; // 0x007694A0 (activity.cpp:925)
    activity_base(activity_modes new_mode, bool modal_, bool can_always_start_); // 0x0077DB90 (.h:110)
    const char* get_instance_name() const; // 0x00755A10 (activity.cpp:2308)
    virtual void activity_reset(); // vtbl+0x04 0x00755820 (activity.cpp:1656)
    virtual bool start_activity_spawn(const object* spawn_objp, const district* dp); // vtbl+0x08 0x007A0410 (activity.cpp:1502)
    bool is_disabled_for_reason(activity_disable_reason disable_reason); // 0x007558C0 (activity.cpp:1804)
    void disable(activity_disable_reason disable_reason); // 0x00755910 (activity.cpp:1818)
    bool enable(activity_disable_reason disable_reason); // 0x00755950 (activity.cpp:1832)
    void suspend(activity_disable_reason disable_reason); // 0x00755990 (activity.cpp:1849)
    virtual void reinstate(activity_disable_reason disable_reason); // vtbl+0x0C 0x007559D0 (activity.cpp:1862)
    bool is_disabled() const; // 0x00753B80 (.h:142)
    void release_activity_slot(); // 0x007555E0 (activity.cpp:1179)
    virtual bool can_initialize() const; // vtbl+0x10 0x007A03A0 (activity.cpp:1191)
    virtual bool can_start() const; // vtbl+0x14 0x00755640 (activity.cpp:1221)
    virtual bool was_initialized() const; // vtbl+0x18 0x00755740 (activity.cpp:1578)
    bool was_initialized_or_active() const; // 0x00753BA0 (.h:157)
    bool get_suspended_flag() const;
    bool get_disabled_flag() const;
    virtual void system_init(); // vtbl+0x1C 0x00755300 (activity.cpp:919)
    virtual void system_shutdown(); // vtbl+0x20 0x00755760 (activity.cpp:1584)
    virtual void level_init(); // vtbl+0x24 0x00755870 (activity.cpp:1669)
    virtual void level_shutdown(); // vtbl+0x28 0x00755890 (activity.cpp:1676)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x00795C70 (activity.cpp:1435)
    virtual bool accept_new_squad(object_squad* squad); // vtbl+0x30 0x00755650 (activity.cpp:1228)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x0077EF00 (activity.cpp:1694)
    virtual bool activity_start(); // vtbl+0x38 0x007AC9A0 (activity.cpp:1743)
    virtual void activity_end(); // vtbl+0x3C 0x00796030 (activity.cpp:2062)
    virtual void activity_kill(); // vtbl+0x40 0x00796020 (activity.cpp:2057)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x00755A00 (activity.cpp:2068)
    virtual bool activity_starting_completed() const; // vtbl+0x48 0x0077EFE0 (activity.cpp:2075)
    virtual void activity_complete(); // vtbl+0x4C 0x0077F0D0 (activity.cpp:2118)
    void activity_do_frame(bool game_paused); // 0x00769500 (activity.cpp:2137)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x00755A70 (activity.cpp:2409)
    virtual activity_load_state activity_do_frame_loading(); // vtbl+0x54 0x0077F210 (activity.cpp:2416)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007697D0 (activity.cpp:2489)
    virtual void activity_do_frame_starting(); // vtbl+0x5C 0x00755A80 (activity.cpp:2532)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x00755A90 (activity.cpp:2539)
    virtual void activity_do_frame_clean_up(); // vtbl+0x64 0x00755AB0 (activity.cpp:2558)
    virtual void activity_do_frame_paused(); // vtbl+0x68 0x00755AD0 (activity.cpp:2572)
    virtual void activity_clean_up_once(); // vtbl+0x6C 0x00755AA0 (activity.cpp:2547)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x00755AE0 (activity.cpp:2578)
    virtual void render_icons() const; // vtbl+0x74 0x00753BD0 (.h:219)
    virtual void render_regions() const; // vtbl+0x78 0x00753BE0 (.h:222)
    virtual bool is_active() const = 0; // vtbl+0x7C
    virtual const char* get_title() const; // vtbl+0x80 0x00768BA0 (.h:228)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00768BB0 (.h:232)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00768BC0 (.h:235)
    virtual void report_recruitment(npc* np); // vtbl+0x8C 0x00755AF0 (activity.cpp:2584)
    virtual void report_patrol_reached(unsigned int squad_handle, const patrol_node_handle* patrol_node); // vtbl+0x90 0x00755B00 (activity.cpp:2590)
    virtual void report_action_node_start(unsigned int action_node_handle); // vtbl+0x94 0x00755B10 (activity.cpp:2596)
    virtual void report_action_node_end(unsigned int action_node_handle); // vtbl+0x98 0x00755B20 (activity.cpp:2602)
    virtual bool is_player_cover_blown() const; // vtbl+0x9C 0x00755B30 (activity.cpp:2610)
    virtual bool lock_alert_level() const; // vtbl+0xA0 0x00755B40 (activity.cpp:2616)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x00755B50 (activity.cpp:2622)
    virtual bool outside_defensive_perimeter(const npc* np, const vector& pos, bool& result, const float* buffer_dist) const; // vtbl+0xA8 0x00755B60 (activity.cpp:2628)
    bool is_modal() const; // 0x00753BF0 (.h:259)
    base_activity_states get_activity_state() const; // 0x00753C00 (.h:262)
    activity_interface_states get_interface_state() const; // 0x00753C10 (.h:265)
    int get_interface_state_duration() const; // 0x00753C20 (.h:268)
    void show_status() const;
    bool layers_loaded();
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00769920 (activity.cpp:2636)
    virtual void record_victory(); // vtbl+0xB0 0x00755B70 (activity.cpp:2644)
    virtual void record_loss(); // vtbl+0xB4 0x00755BD0 (activity.cpp:2660)
    unsigned int get_district_handle() const; // 0x00753C30 (.h:285)
    bool get_victor_record_flag() const; // 0x00753C40 (.h:288)
    virtual void register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints); // vtbl+0xB8 0x00768BD0 (.h:291)
    virtual void register_death(const object* target_obj, const object* attacker_obj); // vtbl+0xBC 0x00768BE0 (.h:292)
    virtual void register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent); // vtbl+0xC0 0x00768BF0 (.h:293)
    virtual void register_building_destroyed(const rfg_mover* objp); // vtbl+0xC4 0x00768C00 (.h:294)
    virtual void register_explosion(const object* owner, const explosion_info* p_explosion_info); // vtbl+0xC8 0x00768C10 (.h:295)
    virtual bool get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const; // vtbl+0xCC 0x00768C20 (.h:299)
    virtual void save_activity_state(cfile* save_file); // vtbl+0xD0 0x00755C10 (activity.cpp:2671)
    virtual void load_activity_state(cfile* save_file); // vtbl+0xD4 0x00755C40 (activity.cpp:2678)
    virtual bool allow_guerrilla_reinforcements() const; // vtbl+0xD8 0x007558A0 (activity.cpp:1681)
    int get_state_duration() const; // 0x00753C50 (.h:314)
    base_activity_states get_state() const { return state; } // inlined in activity_raid.cpp
    unsigned int get_spawn_object_handle() const; // 0x00753C60 (.h:317)
    void collect_layer_objects(base_array<object const*>& obj_list); // 0x0077F360 (activity.cpp:2700)
    void collect_layer_objects(base_array<object*>& obj_list); // 0x0077F2F0 (activity.cpp:2685)
    template<typename T> void collect_layer_objects(base_array<T*>& obj_list); // (.h:508)
    void play_activity_music(); // 0x00755520 (activity.cpp:1137)
    void change_interface_state(activity_interface_states new_interface_state); // 0x00755770 (activity.cpp:1589)
private:
    void reset_flags(); // 0x00753C70 (.h:392)
    void change_activity_state(base_activity_states new_state); // 0x00755660 (activity.cpp:1234)
    void abort_spawn(); // 0x00796040 (activity.cpp:2292)
    void activity_end_common(); // 0x00795E10 (activity.cpp:1877)
    bool reserve_voice(); // 0x007696E0 (activity.cpp:2325)
    bool voice_loaded() const; // 0x00769720 (activity.cpp:2341)
    bool release_voice(); // 0x00769760 (activity.cpp:2352)
    bool can_reserve_voice() const; // 0x007697A0 (activity.cpp:2369)
    bool should_reserve_voice_at_init() const; // 0x00755A30 (activity.cpp:2380)
    bool should_reserve_voice_at_start() const; // 0x00755A50 (activity.cpp:2393)
    bool start_managed_layer_loading(); // 0x007A03F0 (activity.cpp:1345)
    void start_managed_layer_unloading(); // 0x00795BC0 (activity.cpp:1332)
    bool create_managed_layer(); // 0x00795BE0 (activity.cpp:1389)
    bool resources_ready() const; // 0x007694E0 (activity.cpp:1598)
    bool resources_loading() const; // 0x0077EED0 (activity.cpp:1645)
    bool resource_slot_free() const; // 0x00795DF0 (activity.cpp:1634)
    bool should_create_layer_at_init() const; // 0x00755700 (activity.cpp:1360)
    bool should_create_layer_at_start() const; // 0x00755720 (activity.cpp:1373)
    bool resources_streaming_complete() const; // 0x007557A0 (activity.cpp:1609)
    void start_object_restoration(); // 0x0077EEB0 (activity.cpp:1321)
    bool object_restoration_completed() const; // 0x0077EE80 (activity.cpp:1307)
    bool should_restore_objects_at_init() const; // 0x007556C0 (activity.cpp:1282)
    bool should_restore_objects_at_start() const; // 0x007556E0 (activity.cpp:1294)
    void load_managed_script_group(); // 0x0077F1A0 (activity.cpp:2247)
    void unload_managed_script_group(); // 0x007696A0 (activity.cpp:2277)
    bool should_reserve_primary_vehicle_at_init() const; // 0x007553F0 (activity.cpp:1018)
    bool should_reserve_primary_vehicle_at_start() const; // 0x00755410 (activity.cpp:1030)
    bool should_reserve_secondary_vehicle_at_start() const; // 0x00755430 (activity.cpp:1043)
    bool primary_vehicle_slot_is_free() const; // 0x00755310 (activity.cpp:940)
    bool reserve_primary_vehicle_slot(); // 0x00755330 (activity.cpp:954)
    bool reserve_secondary_vehicle_slot(); // 0x00755370 (activity.cpp:978)
    void release_all_vehicle_slots(); // 0x007553B0 (activity.cpp:1002)
    bool should_reserve_human_at_init() const; // 0x00755450 (activity.cpp:1056)
    bool should_reserve_human_at_start() const; // 0x00755470 (activity.cpp:1068)
    bool reserve_human_slot(); // 0x00755490 (activity.cpp:1081)
    void release_human_slots(); // 0x007554D0 (activity.cpp:1107)
    bool should_start_music_score() const; // 0x00755500 (activity.cpp:1117)
    void stop_activity_music(); // 0x00755580 (activity.cpp:1160)

    // 0x00 vfptr
public:
    const activity_modes mode; // 0x04
    const base_activity_properties activity_props; // 0x08
    farray<unsigned int,256> layer_objects; // 0x0C
private:
    struct
    {
        unsigned int object_restored : 1; // 0x00:0
        unsigned int list_compiled : 1; // 0x00:1
        unsigned int victory_recorded : 1; // 0x00:2
        unsigned int voice_reserved : 1; // 0x00:3
        unsigned int primary_vehicle_reserved : 1; // 0x00:4
        unsigned int secondary_vehicle_reserved : 1; // 0x00:5
        unsigned int human_reserved : 1; // 0x00:6
        unsigned int cleaned_up : 1; // 0x00:7
        unsigned int music_started : 1; // 0x00:8
    } base_activity_flags; // 0x41C
    unsigned int activity_disable_flags; // 0x420
    unsigned int activity_suspend_flags; // 0x424
    base_activity_states state; // 0x428
    activity_interface_states interface_state; // 0x42C
    unsigned int spawn_object_handle; // 0x430
    timestamp time_in_interface_state; // 0x434
    timestamp state_duration; // 0x438
    unsigned int district_handle; // 0x43C
    base_activity_data* base_data; // 0x440
};

// size 0x448
class activity_base_free : public activity_base
{
public:
    activity_base_free(activity_modes new_mode); // 0x0077DC90 (.h:622)
    virtual void activity_reset(); // vtbl+0x04 0x00755E30 (activity.cpp:3074)
    virtual void system_init(); // vtbl+0x1C 0x00755E20 (activity.cpp:3068)
    virtual void level_init(); // vtbl+0x24 0x00755E90 (activity.cpp:3082)
    virtual bool activity_start(); // vtbl+0x38 0x007ACB30 (activity.cpp:3088)
    virtual void activity_end(); // vtbl+0x3C 0x00796140 (activity.cpp:3111)
    virtual void activity_kill(); // vtbl+0x40 0x00796160 (activity.cpp:3118)
    virtual bool is_active() const; // vtbl+0x7C 0x00755DE0 (activity.cpp:3040)
    virtual void record_victory(); // vtbl+0xB0 0x00755DF0 (activity.cpp:3054)
    virtual ~activity_base_free(); // vtbl+0x00 0x00768C70

private:
    bool active; // 0x444
};

// size 0x44C
class activity_base_modal : public activity_base
{
public:
    activity_base_modal(activity_modes new_mode, bool can_always_start_); // 0x0077DBF0 (.h:548)
    virtual void activity_reset(); // vtbl+0x04 0x00755D70 (activity.cpp:3023)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x00755D60 (activity.cpp:3017)
    virtual bool can_initialize() const; // vtbl+0x10 0x007A0530 (activity.cpp:2801)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x007960C0 (activity.cpp:2848)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x0077F4D0 (activity.cpp:2821)
    virtual bool was_initialized() const; // vtbl+0x18 0x00755C90 (activity.cpp:2869)
    virtual void system_init(); // vtbl+0x1C 0x00755C80 (activity.cpp:2794)
    virtual void level_init(); // vtbl+0x24 0x00755CB0 (activity.cpp:2880)
    virtual bool activity_start(); // vtbl+0x38 0x007ACAC0 (activity.cpp:2894)
    virtual void activity_end(); // vtbl+0x3C 0x00796120 (activity.cpp:2931)
    virtual void activity_kill(); // vtbl+0x40 0x00796130 (activity.cpp:2940)
    virtual void activity_clean_up_once(); // vtbl+0x6C 0x00755D00 (activity.cpp:2973)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x00755D30 (activity.cpp:3003)
    virtual void activity_complete(); // vtbl+0x4C 0x0077F530 (activity.cpp:2993)
    virtual bool is_active() const; // vtbl+0x7C 0x00755C70 (activity.cpp:2719)
    virtual void record_victory(); // vtbl+0xB0 0x0077F3D0 (activity.cpp:2733)
    void activity_shutdown_period_override(unsigned int override_ms); // 0x00755D20 (activity.cpp:2984)
private:
    void activity_end_common(); // 0x00769950 (activity.cpp:2949)
public:
    virtual ~activity_base_modal(); // vtbl+0x00 0x00768C60

    timestamp activity_cooldown_period; // 0x444
private:
    timestamp shutdown_period; // 0x448
};

// 0x00753B50 (.h:82)
inline base_activity_properties::base_activity_properties(bool modal_, bool can_always_start_)
{
    can_always_start = can_always_start_;
    modal = modal_;
}

// 0x00768B90 (.h:96)
inline activity_base::~activity_base()
{
}

// 0x0077DB90 (.h:110). Original: the state, flags and handles stay unset until level_init calls activity_reset.
inline activity_base::activity_base(activity_modes new_mode, bool modal_, bool can_always_start_)
    : mode(new_mode), activity_props(modal_, can_always_start_), activity_disable_flags(0)
{
}

// 0x00753B80 (.h:142)
inline bool activity_base::is_disabled() const
{
    return activity_disable_flags != 0 || activity_suspend_flags != 0;
}

// 0x00753BA0 (.h:157)
inline bool activity_base::was_initialized_or_active() const
{
    return was_initialized() || is_active();
}

// 0x00753BD0 (.h:219)
inline void activity_base::render_icons() const
{
}

// 0x00753BE0 (.h:222)
inline void activity_base::render_regions() const
{
}

// 0x00768BA0 (.h:228)
inline const char* activity_base::get_title() const
{
    return nullptr;
}

// 0x00768BB0 (.h:232)
inline const char* activity_base::get_subtitle() const
{
    return nullptr;
}

// 0x00768BC0 (.h:235)
inline const char* activity_base::get_icon_image_name() const
{
    return nullptr;
}

// 0x00753BF0 (.h:259)
inline bool activity_base::is_modal() const
{
    return activity_props.modal;
}

// 0x00753C00 (.h:262)
inline base_activity_states activity_base::get_activity_state() const
{
    return state;
}

// 0x00753C10 (.h:265)
inline activity_interface_states activity_base::get_interface_state() const
{
    return interface_state;
}

// 0x00753C20 (.h:268)
inline int activity_base::get_interface_state_duration() const
{
    return time_in_interface_state.time_since();
}

// 0x00753C30 (.h:285)
inline unsigned int activity_base::get_district_handle() const
{
    return district_handle;
}

// 0x00753C40 (.h:288)
inline bool activity_base::get_victor_record_flag() const
{
    return base_activity_flags.victory_recorded;
}

// 0x00768BD0 (.h:291)
inline void activity_base::register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints)
{
}

// 0x00768BE0 (.h:292)
inline void activity_base::register_death(const object* target_obj, const object* attacker_obj)
{
}

// 0x00768BF0 (.h:293)
inline void activity_base::register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent)
{
}

// 0x00768C00 (.h:294)
inline void activity_base::register_building_destroyed(const rfg_mover* objp)
{
}

// 0x00768C10 (.h:295)
inline void activity_base::register_explosion(const object* owner, const explosion_info* p_explosion_info)
{
}

// 0x00768C20 (.h:299)
inline bool activity_base::get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const
{
    return false;
}

// 0x00753C50 (.h:314)
inline int activity_base::get_state_duration() const
{
    return state_duration.time_since();
}

// 0x00753C60 (.h:317)
inline unsigned int activity_base::get_spawn_object_handle() const
{
    return spawn_object_handle;
}

// 0x00753C70 (.h:392)
inline void activity_base::reset_flags()
{
    *reinterpret_cast<unsigned int*>(&base_activity_flags) = 0;
    activity_disable_flags = 0;
    activity_suspend_flags = 0;
}

// 0x0077DBF0 (.h:548)
inline activity_base_modal::activity_base_modal(activity_modes new_mode, bool can_always_start_)
    : activity_base(new_mode, true, can_always_start_)
{
}

// 0x0077DC90 (.h:622). Original: a free activity counts as can_always_start.
inline activity_base_free::activity_base_free(activity_modes new_mode)
    : activity_base(new_mode, false, true), active(false)
{
}

inline activity_base_modal::~activity_base_modal()
{
}

inline activity_base_free::~activity_base_free()
{
}

#include "volition/rfg/code/level/world.h"

// The layer's objects of one class. <object_npc_spawn_node> 0x00793460, <object_roadblock_node> 0x00793220,
// <object_restricted_area> 0x007931B0, <object_turret_spawn_node> 0x00793540, <object_dummy> 0x007935B0,
// <object_vehicle_spawn_node> 0x007934D0, <object_raid_node> 0x0077C010, <object_spawn_region> 0x007933F0 (.h:508).
// base_array::add reports a full list.
template<typename T>
void activity_base::collect_layer_objects(base_array<T*>& obj_list)
{
    obj_list.clear();
    for (int i = 0; i < layer_objects.size(); i++)
    {
        T* objp = World.object_from_handle<T>(layer_objects[i]);
        if (objp != nullptr)
        {
            obj_list.add(objp);
        }
    }
}
