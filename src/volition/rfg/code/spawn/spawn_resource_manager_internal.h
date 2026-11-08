// Original: volition/rfg/code/spawn/spawn_resource_manager_internal.h
// Implementation: volition/rfg/code/spawn/spawn_resource_manager.cpp
//
// The resource slots of the spawn resource manager: per slot type a limit with its current owner, and the slot
// records (the stream resource id, the reservations and the allocated objects) kept in a slot list.
#pragma once

#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

// size 0x14
class resource_slot_limit
{
public:
    resource_slot_limit(spawn_slot_types slot_type_, int slot_max_); // 0x00B48E40 (.h:35)
    void reset(); // 0x00B43EE0 (spawn_resource_manager.cpp:2124)
    int count_used_slots() const; // 0x00B51F50 (spawn_resource_manager.cpp:2131)
    int count_free_slots() const; // 0x00B51FE0 (spawn_resource_manager.cpp:2146)
    bool is_allowed_to_use(resource_slot_user_id new_owner_id) const; // 0x00B43F00 (spawn_resource_manager.cpp:2152)

    const spawn_slot_types slot_type; // 0x00
    const int slot_bit_pos; // 0x04
    const int slot_max; // 0x08
    resource_slot_user_id owner_id; // 0x0C
    timestamp slot_release_timestamp; // 0x10
};

// size 0x18
class resource_slot_record
{
public:
    resource_slot_record(unsigned int new_srid, object_type new_obj_type); // 0x00B43F60 (spawn_resource_manager.cpp:2177)
    resource_slot_record(); // 0x00B43F20 (spawn_resource_manager.cpp:2167)
    bool is_reserved() const; // 0x00B43FD0 (spawn_resource_manager.cpp:2209)
    bool is_allocated() const; // 0x00B43FE0 (spawn_resource_manager.cpp:2224)
    void set_allocated_flag(); // 0x00B43DD0 (.h:81)
    void clear_allocated_flag();
    bool high_priority() const; // 0x00B48E80 (.h:85)
    bool is_reserved_by(spawn_slot_types slot_type) const; // 0x00B43FF0 (spawn_resource_manager.cpp:2235)
    bool is_reserved_by_other(spawn_slot_types slot_type) const; // 0x00B44010 (spawn_resource_manager.cpp:2241)
    void set_reservation_flag(spawn_slot_types slot_type); // 0x00B44030 (spawn_resource_manager.cpp:2247)
    void clear_reservation_flag(spawn_slot_types slot_type); // 0x00B44070 (spawn_resource_manager.cpp:2257)
    bool low_priority() const; // 0x00B48E90 (.h:96)
    void reset(unsigned int new_srid, object_type new_obj_type); // 0x00B43FA0 (spawn_resource_manager.cpp:2188)
    void reset_internal(); // 0x00B49110 (spawn_resource_manager.cpp:2199)
    bool in_use() const; // 0x00B44090 (spawn_resource_manager.cpp:2267)
    bool can_be_used() const; // 0x00B440C0 (spawn_resource_manager.cpp:2278)
    unsigned int get_srid() const; // 0x00B43DE0 (.h:109)
    int get_object_count() const; // 0x00B52220 (spawn_resource_manager.cpp:2452)
    float get_object_removal_fitness() const; // 0x00B52280 (spawn_resource_manager.cpp:2467)
    int get_destroyed_count() const; // 0x00B52330 (spawn_resource_manager.cpp:2483)
    bool stream_request(); // 0x00B440F0 (spawn_resource_manager.cpp:2284)
    void stream_release(); // 0x00B44130 (spawn_resource_manager.cpp:2298)
    bool stream_requested() const;
    void clear_reservation_flags(); // 0x00B44150 (spawn_resource_manager.cpp:2310)
    bool check_destroyed_status() const; // 0x00B44160 (spawn_resource_manager.cpp:2351)
    object_type get_obj_type() const; // 0x00B43DF0 (.h:134)
    void set_player_flag(); // 0x00B43E00 (.h:137)
    bool is_player_slot() const; // 0x00B43E10 (.h:140)
    const char* get_record_name() const; // 0x00B49160 (spawn_resource_manager.cpp:2317)

private:
    timestamp reservation_time; // 0x00
    timestamp destroyed_time; // 0x04
    unsigned int srid; // 0x08
    int reservation_flags; // 0x0C
    object_type obj_type; // 0x10
    struct
    {
        unsigned int player_slot : 1; // 0x00:0
        unsigned int allocated_slot : 1; // 0x00:1
        unsigned int debug_allocation : 1; // 0x00:2
        unsigned int stream_requested : 1; // 0x00:3
    } slot_flags; // 0x14
};

// size 0x24
class resource_slot_list
{
public:
    resource_slot_list(object_type obj_type_, int master_records_size_, resource_slot_record* master_record_array_, int streamed_records_size_, resource_slot_record** streamed_records_array_); // 0x00B48ED0 (.h:199)
    void destroy_slot(resource_slot_record* record); // 0x00B523E0 (spawn_resource_manager.cpp:2593)
    bool reserve_slot(resource_slot_record* record, spawn_slot_types slot_type); // 0x00B524C0 (spawn_resource_manager.cpp:2628)
    void reserve_slot_player(resource_slot_record* record); // 0x00B52570 (spawn_resource_manager.cpp:2670)
    void unreserve_slot(resource_slot_record* record, spawn_slot_types slot_type); // 0x00B57AC0 (spawn_resource_manager.cpp:2678)
    void unreserve_slot_except(resource_slot_record* record, spawn_slot_types exception_type); // 0x00B57B10 (spawn_resource_manager.cpp:2694)
    bool allocate_slot(resource_slot_record* record); // 0x00B525F0 (spawn_resource_manager.cpp:2719)
    void clear_all_reservations_except(spawn_slot_types exception_type); // 0x00B5D580 (spawn_resource_manager.cpp:2516)
    int free_low_priority_slots(int count); // 0x00B57740 (spawn_resource_manager.cpp:2531)
    void free_all_unused_slots(); // 0x00B57B90 (spawn_resource_manager.cpp:2758)
    void free_all_unused_slots_except(unsigned int exception_srid); // 0x00B57C60 (spawn_resource_manager.cpp:2786)
    // free_unused_slots<human_info> 0x00B58CA0, <vehicle_info> 0x00B58D90 (spawn_resource_manager.cpp:2845)
    void free_unused_human_slots(const base_array<human_info const*>& obj_info_list);
    void free_unused_vehicle_slots(const base_array<vehicle_info const*>& obj_info_list);
    resource_slot_record* get_record(int slot_id); // 0x00B491F0 (spawn_resource_manager.cpp:2880)
    void reset(); // 0x00B52680 (spawn_resource_manager.cpp:2888)
    void level_init(); // 0x00B52720 (spawn_resource_manager.cpp:2907)
    void system_init(); // 0x00B52730 (spawn_resource_manager.cpp:2913)
    int count_slots_used() const; // 0x00B49210 (spawn_resource_manager.cpp:2919)
    void reserve_mission_allocated_slots(); // 0x00B52740 (spawn_resource_manager.cpp:2965)
    void destroy_allocated_slots();
    int count_free_slots() const; // 0x00B49220 (spawn_resource_manager.cpp:2925)
    resource_slot_record* find_first_reserved_record(spawn_slot_types slot_type); // 0x00B49230 (spawn_resource_manager.cpp:2931)
    int count_slots_used_by_type(spawn_slot_types slot_type) const; // 0x00B49270 (spawn_resource_manager.cpp:2948)
    bool all_resources_streamed() const; // 0x00B492B0 (spawn_resource_manager.cpp:2980)
    bool resources_streamed(spawn_slot_types slot_type) const; // 0x00B49360 (spawn_resource_manager.cpp:3083)
    bool other_variant_loaded(const resource_slot_record* record) const; // 0x00B492F0 (spawn_resource_manager.cpp:3044)
    void release_unused_reserved_slots(spawn_slot_types slot_type); // 0x00B57D30 (spawn_resource_manager.cpp:3007)
    void release_reserved_slots(spawn_slot_types slot_type); // 0x00B57E20 (spawn_resource_manager.cpp:3026)
    int get_maximum_slot_count() const;
    void check_slot_status(); // 0x00B57E90 (spawn_resource_manager.cpp:3114)
    void check_destroyed_slot(resource_slot_record* record); // 0x00B52860 (spawn_resource_manager.cpp:3181)
    void check_reserved_slot(resource_slot_record* record); // 0x00B527E0 (spawn_resource_manager.cpp:3132)
    ~resource_slot_list(); // 0x00B48F90

    const object_type obj_type; // 0x00
    base_array<resource_slot_record> master_records_list; // 0x04
private:
    base_array<resource_slot_record*> streamed_records_list; // 0x14
};

extern resource_slot_list Human_records_list; // 0x016B8AAC
extern resource_slot_list Vehicle_records_list; // 0x016B8AD0
