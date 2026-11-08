// Original: volition/rfg/code/level/world_internal.h
//
// Types and globals shared by the world_*.cpp files. Contents inferred (the PDB records no inline code for this
// header).
// - world_zone: one cell of the loaded territory (or the persistent zone): bounds, name, streaming state, its obj_zone
//   object and streamed container (srid), the handle allocator for the objects that originate in it (gid as upper
//   bits, so handles name their zone) and the saved state of its objects (stored_zone_state) while it is not streamed.
//   deserialize_header/_f read the zone's .rfgzone_pc while it is streamed in.
// - The functions of world_restore.cpp, world_serialize.cpp and world_stream.cpp that have external linkage.
// - The restore tables (world_restore.cpp): Pending_restorations (handles to restore when their zone streams in),
//   Restored_objects (restored this session), Auto_restorations, Restored_shapes and Zone_table (zones touched by a
//   restoration).
#pragma once

#include "volition/rfg/code/level/world.h"
#include "volition/vlib/code/util/handle_manager.h"

struct zone_header;
struct bbox;

enum world_zone_state
{
    WZS_UNLOADED = 0,
    WZS_STREAMING = 1,
    WZS_LOADED = 2,
};

// size 0x480
class world_zone
{
public:
    world_zone(const char* filename); // 0x0092CD30 (world.cpp:219)
    unsigned short get_gid() const; // 0x0091C450 (world.cpp:250)
    void set_gid(unsigned short _gid); // 0x009235D0 (world.cpp:255)
    unsigned int alloc_obj_handle(); // 0x009235F0 (world.cpp:261)
    void reserve_obj_handle(unsigned int handle); // 0x00923600 (world.cpp:266)
    void release_obj_handle(unsigned int handle); // 0x00923620 (world.cpp:271)
    bool is_obj_handle_allocated(unsigned int handle); // 0x00923650 (world.cpp:276)
    void get_grid_coords(int& grid_x, int& grid_y); // 0x0091C460 (world.cpp:281)

    vector bmin; // 0x00
    vector bmax; // 0x0C
    char name[64]; // 0x18
    world_zone_state state; // 0x58
    zone_header* deserialize_header; // 0x5C
    cfile* deserialize_header_f; // 0x60
    cfile* deserialize_f; // 0x64
    world_state_buf stored_zone_state; // 0x68
    obj_zone* zone_objp; // 0xD4
    unsigned int srid; // 0xD8
    bool is_border_zone; // 0xDC
private:
    // world sets up and reads the handle allocator directly (inlined in world.cpp and world_serialize.cpp; inferred).
    friend class world;
    handle_manager<7280> obj_handle_mgr; // 0xE0
    unsigned short gid; // 0x47C
};

extern hash_table<unsigned int,&uint_hash,1024,unsigned short> Auto_restorations; // 0x01668B60
extern hash_table<unsigned int,&uint_hash,3072,unsigned short> Pending_restorations; // 0x0166AB68
extern hash_table<unsigned int,&uint_hash,2048,unsigned short> Restored_objects; // 0x01670B70
extern hash_table<unsigned int,&hash_uint,2048,unsigned short> Restored_shapes; // 0x01674BB8
extern hash_table<unsigned int,&uint_hash,9,unsigned char> Zone_table; // 0x01674B78

bool verify_hierarchy(); // 0x00934740 (world_restore.cpp:79)
void remove_descendants_from_auto_restore(unsigned int handle); // 0x0092D250 (world_restore.cpp:206)
void restore_destroy_movers(hash_table<unsigned int,&hash_uint,2048,unsigned short>& shape_table); // 0x0092D2B0 (world_restore.cpp:287)
void restore_clean_up_object(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, object* objp); // 0x0091CFB0 (world_restore.cpp:302)
void restore_destroy_object(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, unsigned int handle); // 0x0093D9F0 (world_restore.cpp:327)
void restore_destroy_object_recursive(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, unsigned int handle); // 0x0093DA50 (world_restore.cpp:346)
bool restore_object_just_restored(unsigned int handle); // 0x0092D410 (world_restore.cpp:433)
int restore_pending_pop_helper(hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int*& restore_list, unsigned int handle, bool is_persistent_zone); // 0x00934D10 (world_restore.cpp:455)
void remove_descendants_from_restored_table(unsigned int handle); // 0x0092D470 (world_restore.cpp:507)
bool restore_clean_box_mover_cb(object* hit_objp, unsigned int, const void*); // 0x0092D4D0 (world_restore.cpp:784)
bool restore_clean_box_npc_cb(object* hit_objp, unsigned int, const void*); // 0x0092D510 (world_restore.cpp:808)
bool restore_clean_box_vehicle_cb(object* hit_objp, unsigned int, const void*); // 0x0092D540 (world_restore.cpp:829)
bool restore_clean_box_shape_cutter_cb(object* hit_objp, unsigned int, const void*); // 0x0092D580 (world_restore.cpp:860)
bool restore_clean_box_effect_cb(object* hit_objp, unsigned int, const void*); // 0x0092D5B0 (world_restore.cpp:875)
bool restore_clean_box_navpoint_cb(object* hit_objp, unsigned int, const void*); // 0x0092D5F0 (world_restore.cpp:892)
void restore_clean_box(bbox& clean_box); // 0x0092D630 (world_restore.cpp:912)
bool restore_post_zone_serialize_cleanup_helper(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, bbox& root_box, unsigned int handle, unsigned short zone_gid); // 0x0093C2F0 (world_restore.cpp:970)
void platform_save_to_disk(void* data, int len, game_save_info* save_slot, bool auto_save); // 0x0092D770 (world_serialize.cpp:80)
void unused_dcf_show_zone_navmesh(); // 0x0091D3C0 (world_stream.cpp:86)
void unused_dcf_stream_radius(); // 0x0091D3E0 (world_stream.cpp:98)
void unused_dcf_stream_radius_terrain(); // 0x0091D510 (world_stream.cpp:111)
void unused_dcf_stream_debug(); // 0x0091D670 (world_stream.cpp:137)
