// Original: volition/rfg/code/objects/ladder.h
// Implementation: volition/rfg/code/objects/ladder.cpp
//
// Ladders (OT_LADDER): a placed object with a number of rungs that humans mount, climb, slide down, jump off and
// dismount (ladder_process_human); disabled when the building it is attached to is damaged.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"

struct event_data;
class human;
struct notify_info;
class vector;

enum ladder_state_enum
{
    MP_LADDER_STATE_NONE = -1,
    MP_LADDER_STATE_CLIMB = 0,
    MP_LADDER_STATE_IDLE = 1,
    MP_LADDER_STATE_SLIDE_DOWN = 2,
    MP_LADDER_STATE_SLIDE_STOP = 3,
};

// size 0xA8
class ladder : public object
{
public:
    ladder(); // 0x00A127B0 (:121)
    static const object_type get_type_static(); // 0x006C70A0 (.h:26)
    static const object_sub_type get_subtype_static(); // 0x006C70B0 (.h:26)
    static const char* get_classname_static(); // 0x00A06980 (.h:26)
    static int get_bpo_index_static(); // 0x00A06990 (.h:26)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A127D0 (.h:26)
    static unsigned int get_classname_hash(); // 0x00A127A0 (:120)
    static int pool_remaining(); // 0x00A36AC0 (:120)
    virtual void pool_free(); // vtbl+0x80 0x00A8A6A0 (:120)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AB34F0 (:133)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A127F0 (:161)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A36AF0 (:171)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A12800 (:186)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A12810 (:191)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A12820 (:196)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A12870 (:208)
    virtual void set_enabled(bool enable); // vtbl+0x84 0x00A12900 (:827)
    bool is_enabled(); // 0x00A069A0 (.h:43)
    void set_in_use(bool _in_use); // 0x00A069B0 (.h:45)
    bool is_in_use(); // 0x00A069D0 (.h:46)
    int get_use_count(); // 0x00A069E0 (.h:47)
    bool is_enabled() const { return ladder_enabled; } // human_move_pathfind.cpp reads the field directly
    virtual ~ladder(); // vtbl+0x00 0x00A127E0

    int rungs; // 0x9C
private:
    // ladder.cpp's mount and dismount count the users (the original reads the private fields there directly).
    friend void human_mount_ladder(human* hp, ladder* lp);
    friend void human_dismount_ladder(human* hp);
    bool ladder_enabled; // 0xA0
    bool in_use; // 0xA1
    int use_count; // 0xA4
};

const char* classname_ladder(); // 0x00A12790 (:120)
object* alloc_ladder(); // 0x00A5E4E0 (:120)
int bpo_index_ladder(); // 0x00A12780 (:120)
void types_ladder(signed char& type, signed char& subtype); // 0x00A12770 (:120)
void human_dismount_ladder(human* hp); // 0x00AC4300 (:215)
bool human_ladder_jump(human* hp); // 0x00AC4470 (:258)
void human_mount_ladder(human* hp, ladder* lp); // 0x00AB8AE0 (:275)
void ladder_process_human(human* hp, float vertical_stick_val); // 0x00AC4520 (:505)
event_result ladder_building_damaged_callback(const event_data& in_event, notify_info* info); // 0x00A36B10 (:884)
int ladder_compute_current_rung(const human* hp, unsigned int ladder_handle); // 0x00A36C10 (:962)
void ladder_system_init(); // 0x00A12A10 (:974)
void ladder_level_init(); // 0x00A36C80 (:983)

// 0x00A069A0 (.h:43)
inline bool ladder::is_enabled()
{
    return ladder_enabled;
}

// 0x00A069B0 (.h:45)
inline void ladder::set_in_use(bool _in_use)
{
    in_use = _in_use;
}

// 0x00A069D0 (.h:46)
inline bool ladder::is_in_use()
{
    return in_use;
}

// 0x00A069E0 (.h:47)
inline int ladder::get_use_count()
{
    return use_count;
}
