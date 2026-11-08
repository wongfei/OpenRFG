// Original: volition/rfg/code/objects/object_safehouse.h
// Implementation: volition/rfg/code/objects/object_safehouse.cpp
//
// Safehouses: the guerrilla safehouses with their vehicle spawns, buildings and trigger regions; the player respawns
// at them, and enemies ignore a player inside.
#pragma once

#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/varray.h"

class matrix;
class vector;

enum safehouse_disable_reason
{
    SDR_NORMAL = 0,
    SDR_ACTIVITY = 1,
    SDR_TEMP_DISABLE_UNTIL_STREAM = 2,
    NUM_SAFEHOUSE_DISABLE_REASONS = 3,
};

// size 0x134
class object_safehouse : public object
{
public:
    object_safehouse(); // 0x00A6B490 (:397)
    static const object_type get_type_static(); // 0x005DFB00 (.h:41)
    static const object_sub_type get_subtype_static(); // 0x005DFB10 (.h:41)
    static const char* get_classname_static(); // 0x00A06A40 (.h:41)
    static int get_bpo_index_static(); // 0x00A06A50 (.h:41)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A6B520 (.h:41)
    static unsigned int get_classname_hash(); // 0x00A1C1B0 (:396)
    static int pool_remaining(); // 0x00A3E280 (:396)
    virtual void pool_free(); // vtbl+0x80 0x00A93900 (:396)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAB900 (:409)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AB4570 (:484)
    virtual void free_this(); // vtbl+0x28 0x00A93950 (:554)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1C1C0 (:472)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A3E290 (:561)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A3E3C0 (:583)
    bool player_is_in_safehouse() const; // 0x00A6B550 (:607)
    bool in_safehouse_area(const vector& posp) const; // 0x00A3E490 (:627)
    bool is_visible() const; // 0x00A1C1E0 (:651)
    void set_visible(bool set_val); // 0x00A1C1D0 (:646)
    bool is_enabled() const { return disabled_flags == 0; } // 0x005DFB20 (.h:65)
    bool is_enabled_for_player_spawn() const; // 0x00A1C200 (:660)
    void set_disabled(safehouse_disable_reason reason); // 0x00A93970 (:673)
    void clear_disabled(safehouse_disable_reason reason); // 0x00AABAD0 (:691)
    void despawn_squad(const char* name); // 0x00A939E0 (:708)
    void respawn_vehicles(); // 0x00A93AF0 (:735)
    void update_safehouse_name_display(bool player_in_safehouse); // 0x00A1C230 (:748)
    virtual ~object_safehouse(); // vtbl+0x00 0x00A6B530

private:
    bool visible; // 0x9C
    unsigned int disabled_flags; // 0xA0
    bool run_enabled_func_on_stream; // 0xA4
    farray<unsigned int,4> vehicle_spawns; // 0xA8
    farray<unsigned int,8> buildings; // 0xC8
    farray<unsigned int,8> trigger_regions; // 0xF8
    unsigned int display_name_hash; // 0x128
    bool player_was_in_safehouse_last_update; // 0x12C
    hud_message_handle in_safehouse_message_handle; // 0x130
};

// External here because the spawn node and squad classes name them as friends; file-local in rfg.exe.
void object_safehouse_handle_enable(object_safehouse* osp); // 0x00AAB580 (:123)
void object_safehouse_handle_disable(object_safehouse* osp); // 0x00A933C0 (:221)
void unused_dcf_safehouse_enable(); // 0x00AB44D0 (:330)
void unused_dcf_safehouse_set_visible(); // 0x00A93860 (:364)
object* alloc_object_safehouse(); // 0x00A6B480 (:396)
int bpo_index_object_safehouse(); // 0x00A1C190 (:396)
void types_object_safehouse(signed char& type, signed char& subtype); // 0x00A1C180 (:396)
const char* classname_object_safehouse(); // 0x00A1C1A0 (:396)
object_safehouse* safehouse_find_nearest_safehouse(const vector& pos, bool include_invisible, bool include_disabled); // 0x00A6B570 (:833)
object_safehouse* safehouse_find_spawn_safehouse(const vector& pos); // 0x00A6B660 (:862)
object_safehouse* safehouse_find_safehouse_by_grid_id(const unsigned short gid); // 0x00A6B7C0 (:909)
bool safehouse_get_start_pos_orient(vector& pos, matrix& orient, const object_safehouse* osp); // 0x00A6B860 (:928)
bool safehouse_contains_player(); // 0x00A1C2E0 (:963)
object_safehouse* safehouse_get_safehouse_containing_player(); // 0x00A3E560 (:969)
bool safehouse_should_ignore_player(); // 0x00A1C2F0 (:979)
void safehouse_do_frame_postsim(); // 0x00A93B40 (:985)
bool safehouse_level_init_pre(); // 0x00A1C300 (:1148)
