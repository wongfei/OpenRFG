// Original: volition/rfg/code/ai/search_system_internal.h
// Implementation: volition/rfg/code/ai/search_system_internal.cpp
//
// The search objects: targets, locations and searchers allocated by the search object manager.
#pragma once

#include "volition/rfg/code/ai/search_system.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class ai_search_object_manager;
class cs_spinlock;

enum ai_search_types
{
    AI_ST_BUILDING_INTERIOR = 0,
    AI_ST_EXTERNAL_AREA = 1,
    NUM_AI_SEARCH_TYPES = 2,
};

enum ai_search_states
{
    AI_SS_NOT_SEARCHED = 0,
    AI_SS_IN_PROGRESS = 1,
    AI_SS_FAILED = 2,
    AI_SS_SEARCHED = 3,
    NUM_AI_SEARCH_STATES = 4,
};

// size 0x14
struct ai_search_location_info
{
    ai_search_location_info(); // 0x005F1770

    ai_search_types search_type; // 0x00
    unsigned int search_obj; // 0x04
    vector pos; // 0x08
};

// size 0x1
struct ai_search_target_info
{
};

// size 0xA0
struct ai_search_searcher_info
{
    ai_search_searcher_info(); // 0x0066B460
    ~ai_search_searcher_info(); // 0x0066B480

    unsigned int searcher_handle; // 0x00
    unsigned int search_handle; // 0x04
    ai_search_location_history location_history; // 0x08
};

// size 0x24
struct ai_search_location
{
    ai_search_states get_state() const; // 0x005F1730 (.h:40)
    unsigned int get_handle() const; // 0x005F1740 (.h:41)
    int get_num_searchers() const; // 0x005F1750 (.h:42)
    void set_state(ai_search_states new_state); // 0x005F1960 (:60)
    ai_search_location(); // 0x00641810 (.h:45)
private:
    friend class ai_search_object_manager;

    void init(); // 0x00641820 (.h:57)
    void release(); // 0x005F1760 (.h:58)

public:
    ai_search_location_info info; // 0x00
private:
    ai_search_states state; // 0x14
    timestamp retry_timer; // 0x18
    unsigned int handle; // 0x1C
    unsigned char searcher_ref_count; // 0x20
    unsigned char search_ref_count; // 0x21
};

// size 0x5C
class ai_search_target
{
public:
    unsigned int get_handle() const;
    bool get_locations(farray<ai_search_location*,10>& list_out) const; // 0x0066B920 (:97)
    bool has_location(const ai_search_location* location) const; // 0x00641960 (:118)
    ai_search_target(); // 0x0066B430 (.h:80)
private:
    friend class ai_search_object_manager;

    void init(); // 0x00641840 (.h:92)
    void release(); // 0x005F1780 (.h:93)

public:
    unsigned int target_handle; // 0x00
    vector best_guess_position; // 0x04
    timestamp position_update_timer; // 0x10
    ai_search_target_info info; // 0x14
private:
    farray<unsigned int,10> locations; // 0x18
    timestamp expire_timer; // 0x50
    unsigned int handle; // 0x54
    unsigned char searcher_ref_count; // 0x58
};

// size 0xB0
class ai_search_searcher
{
public:
    void reset_keepalive(); // 0x005F1990 (:134)
    ai_search_location* get_location() const; // 0x0066B9B0 (:139)
    ai_search_target* get_target() const; // 0x0066B9F0 (:144)
    ai_search_searcher(); // 0x00684A40 (.h:106)
private:
    friend class ai_search_object_manager;

    void init(); // 0x005F1790 (.h:116)
    void release(); // 0x005F17B0 (.h:117)

public:
    ai_search_searcher_info info; // 0x00
private:
    timestamp keepalive; // 0xA0
    unsigned int current_target; // 0xA4
    unsigned int current_location; // 0xA8
};

// size 0x2720
class ai_search_object_manager
{
public:
    ai_search_object_manager(); // 0x006BF2F0 (:162)
    void level_init(); // 0x006A9910 (:168)
    void do_frame(); // 0x006A99F0 (:185)
    ai_search_target* get_new_search(); // 0x0066BA20 (:280)
    ai_search_location* get_new_location(); // 0x0066BA80 (:295)
    ai_search_searcher* get_new_searcher(); // 0x0066BAE0 (:310)
    bool add_searcher_to_search(ai_search_searcher* searcher, ai_search_target* target); // 0x00685400 (:327)
    bool add_location_to_search(ai_search_location* location, ai_search_target* target); // 0x0066BB40 (:364)
    bool assign_searcher_location(ai_search_searcher* searcher, ai_search_location* location); // 0x0066BBA0 (:392)
    void unassign_searcher_location(ai_search_searcher* searcher, ai_search_location* location); // 0x005F19B0 (:471)
    bool remove_location_from_search(ai_search_location* location, ai_search_target* target); // 0x00641990 (:417)
    bool unassign_searcher(ai_search_searcher* searcher); // 0x006854A0 (:439)
    ai_search_searcher* find_searcher(unsigned int searcher_handle); // 0x0066BC80 (:491)
    ai_search_target* find_target(unsigned int target_handle); // 0x0066BCF0 (:504)
    ai_search_target* search_from_handle(unsigned int handle); // 0x006419F0 (:517)
    ai_search_location* location_from_handle(unsigned int handle); // 0x00641A20 (:542)
    ai_search_searcher* searcher_from_handle(unsigned int handle); // 0x00641A50 (:567)
    cs_spinlock& get_searcher_cs_wrapper(); // 0x005F17C0 (.h:153)
    ai_search_searcher* get_first_searcher(); // 0x00641880 (.h:154)
    ai_search_searcher* get_next_searcher(const ai_search_searcher* prev_searcher); // 0x0066B490 (.h:155)
    cs_spinlock& get_locations_cs_wrapper(); // 0x005F17D0 (.h:156)
    ai_search_location* get_first_location(); // 0x006418B0 (.h:157)
    ai_search_location* get_next_location(const ai_search_location* prev_location); // 0x0066B4A0 (.h:158)
private:
    bool remove_searcher_from_search(ai_search_searcher* searcher, ai_search_target* target, bool clear_location); // 0x0066BC10 (:452)
    void free_location_internal(ai_search_location* location); // 0x00685530 (:595)
    void free_target_internal(ai_search_target* target); // 0x00685550 (:604)
    void free_searcher_internal(ai_search_searcher* searcher); // 0x006855B0 (:624)
    unsigned int get_new_search_handle(int index); // 0x005F19F0 (:650)
public:
    ~ai_search_object_manager(); // 0x006853F0

private:
    unsigned int search_handle_uid_counter; // 0x00
    object_pool<ai_search_location,100,1> locations; // 0x10
    object_pool<ai_search_target,12,1> targets; // 0x1060
    object_pool<ai_search_searcher,25,1> searchers; // 0x1530
};

const ai_search_searcher* searcher_info_to_data(const ai_search_searcher_info* info); // 0x005F17E0 (.h:208)
ai_search_searcher_info* searcher_data_to_info(ai_search_searcher* node); // 0x005F17F0 (.h:209)

extern ai_search_object_manager Ai_search_object_manager; // 0x01D72710
