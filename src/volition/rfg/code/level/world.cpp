// Original: volition/rfg/code/level/world.cpp
//
// Partial: the world system, object names and lists, territory loading for a new game or from a saved state and level
// shutdown. Not implemented: the debug console commands (unused_dcf_set_player_start).
//
// Loading a territory (load_territory, from the GS_LOAD game state or a headless driver):
//  1. init_territory reads the territory's zone list (rfg_zonex_get_territory_files, "<territory>_xx_yy" cells plus the
//     persistent zone), creates a world_zone per file in the 16x16 global_zone_grid (the persistent zone is cell
//     (15, 0) = index 15), adds the territory packfiles (stream_layout_level_init), builds the persistent zone file
//     and loads the stream layout (init_territory_streaming_resources).
//  2. The districts and the time of day are loaded (seams for now).
//  3. post_load_stream serializes the persistent zone, places the camera at the start zone's centre (or the saved
//     player start), streams until stream2 is idle (stream_hard_wait), moves the start to the first initial-spawn
//     player_start of the start zone, runs gamestate_gp_level_init_post and streams again.
// setup_territory then runs a few world frames until missions are ready.
//
// Object names live in Obj_name_buf (2048 entries of 48 characters) with a free and a used list (circular, by index);
// object::name_index points into it.
//
// Deviations:
// - system_init: a subtype without a registered class gets an empty list (the original would read class_index[-1];
//   in the game every subtype has a class).
//
// Notes (decompiled behaviour, kept as is):
// - level_shutdown resets the stream layout itself (the inlined stream_layout_level_shutdown) and so does
//   shutdown_territory_streaming_resources.

#include "volition/rfg/code/level/world_internal.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/objects/critical_area.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"
#include "volition/rfg/code/level/rfg_stream_layout_shared.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Object names: a fixed pool of 2048 strings with free and used lists (alloc_obj_name / dealloc_obj_name); an object
// stores the index of its name (set_object_name / get_object_name, "" when it has none).
// size 0x34
struct obj_name_string
{
    char str[48]; // 0x00
    unsigned short next; // 0x30
    unsigned short prev; // 0x32
};

// ---- globals
world World; // 0x02F98490
bool Stream_pause; // 0x02CA1670

// ---- file-static globals
static obj_name_string Obj_name_buf[2048]; // 0x02CA1678
static unsigned short Obj_name_used_list; // 0x02CCB678
static unsigned short Obj_name_free_list; // 0x02CCBA84
static bool Debug_player_start; // 0x02CEC1E3
static bool Stream_follows_player; // 0x02CEC1EC
static vector Debug_player_start_pos; // 0x02F9B048
static matrix Debug_player_start_orient; // 0x02FDE4F0

static const unsigned short NO_NAME = 0xFFFF;

// Unlinks entry from the circular list whose head is list (inlined in alloc_obj_name / dealloc_obj_name).
static void name_list_remove(unsigned short& list, unsigned short entry)
{
    if (list == entry && (list = Obj_name_buf[entry].next) == entry)
    {
        list = NO_NAME;
    }
    else
    {
        Obj_name_buf[Obj_name_buf[entry].prev].next = Obj_name_buf[entry].next;
        Obj_name_buf[Obj_name_buf[entry].next].prev = Obj_name_buf[entry].prev;
    }
    Obj_name_buf[entry].prev = NO_NAME;
    Obj_name_buf[entry].next = NO_NAME;
}

// Appends entry at the end of the circular list whose head is list.
static void name_list_append(unsigned short& list, unsigned short entry)
{
    if (list == NO_NAME)
    {
        Obj_name_buf[entry].next = entry;
        Obj_name_buf[entry].prev = entry;
        list = entry;
        return;
    }
    Obj_name_buf[entry].next = list;
    Obj_name_buf[entry].prev = Obj_name_buf[list].prev;
    Obj_name_buf[Obj_name_buf[list].prev].next = entry;
    Obj_name_buf[list].prev = entry;
}

// 0x0091C0A0 (:124)
void world_set_debug_player_start(bool enabled, const vector& pos, const matrix& orient)
{
    Debug_player_start = enabled;
    Debug_player_start_pos = pos;
    Debug_player_start_orient = orient;
}

// 0x0091C260 (:187)
static char* alloc_obj_name(unsigned short& name_index)
{
    if (Obj_name_free_list == NO_NAME)
    {
        return nullptr;
    }
    const unsigned short entry = Obj_name_free_list;
    name_list_remove(Obj_name_free_list, entry);
    name_list_append(Obj_name_used_list, entry);
    name_index = entry;
    return Obj_name_buf[entry].str;
}

// 0x0091C350 (:202)
// LTCG: name_index in EDI.
static void dealloc_obj_name(unsigned short& name_index)
{
    const unsigned short entry = name_index;
    if (entry == NO_NAME || entry >= 2048)
    {
        return;
    }
    name_list_remove(Obj_name_used_list, entry);
    name_list_append(Obj_name_free_list, entry);
    name_index = NO_NAME;
}

// ---- world_zone

// 0x0092CD30 (:219)
world_zone::world_zone(const char* filename)
{
    obj_handle_mgr.clear();
    srid = 0;
    stored_zone_state.buf = nullptr;
    stored_zone_state.cur_size = 0;
    stored_zone_state.max_size = 0;
    state = WZS_UNLOADED;
    deserialize_header = nullptr;
    deserialize_header_f = nullptr;
    deserialize_f = nullptr;
    zone_objp = nullptr;
    if (filename == nullptr)
    {
        // No file: the whole 16x16 grid.
        name[0] = '\0';
        rfg_zonex_get_maximal_bbox(bmin, bmax);
        return;
    }
    cf_drop_ext(name, sizeof(name), filename);
    rfg_zonex_get_zone_bbox(name, bmin, bmax);
}

// 0x0091C450 (world.cpp:250)
unsigned short world_zone::get_gid() const
{
    return gid;
}

// 0x009235D0 (world.cpp:255)
void world_zone::set_gid(unsigned short _gid)
{
    obj_handle_mgr.set_upper_bits(_gid);
    gid = _gid;
}

// 0x009235F0 (world.cpp:261)
unsigned int world_zone::alloc_obj_handle()
{
    return obj_handle_mgr.allocate_handle();
}

// 0x00923600 (world.cpp:266)
void world_zone::reserve_obj_handle(unsigned int handle)
{
    obj_handle_mgr.reserve_handle(handle);
}

// 0x00923620 (world.cpp:271)
void world_zone::release_obj_handle(unsigned int handle)
{
    obj_handle_mgr.release_handle(handle);
}

// 0x00923650 (world.cpp:276)
bool world_zone::is_obj_handle_allocated(unsigned int handle)
{
    return obj_handle_mgr.is_handle_allocated(handle);
}

// 0x0091C460 (world.cpp:281)
void world_zone::get_grid_coords(int& grid_x, int& grid_y)
{
    grid_x = gid >> 12;
    grid_y = (gid >> 8) & 0xF;
}

// ---- save_load_info and world_state_buf

// 0x00919F10 (.h:111)
void save_load_info::clear()
{
    pending_new_game = false;
    reset_destruction = false;
    pending_save_game = false;
    performing_save_game = false;
    pending_save_state = false;
    performing_save_state = false;
    pending_single_zone = false;
    save_game_warp = false;
    game_save_checkpoint = false;
    game_save_to_disk = false;
    pending_load_game_from_memory = false;
    auto_save_game = false;
    saving_state_data = false;
    player_start_at_safehouse = false;
    player_start_pos = ZERO_VECTOR;
    player_start_orient = IDENTITY_MATRIX;
}

// 0x0091C240
save_load_info::save_load_info()
{
}

// 0x0091C230
world_state_buf::world_state_buf()
{
}

// ---- world

// 0x0093BA50
world::world()
    : destroyed_object_table(false)
{
}

// 0x0093A790
world::~world()
{
}

// 0x0093BB00 (:294)
void world::system_init()
{
    flagged_objects = nullptr;
    current_flagged_object = nullptr;
    current_flagged_mode = 0;
    save_file = nullptr;
    object_handle_hash.clear();
    level_ambient.x = 0.1f;
    level_ambient.y = 0.1f;
    level_ambient.z = 0.3f;
    tech_level = 0.0f;
    territory_name[0] = '\0';
    last_loaded_territory[0] = '\0';
    tech_level_max = 5.0f;
    sl_flags.clear();
    sl_flags.pending_new_game = true;
    pending_filename[0] = '\0';
    load_aborted = false;
    grid = nullptr;
    deserialize_state = DS_IDLE;
    deserialize_pool = nullptr;
    deserialize_list.remove_all();
    cur_world_state = WSM_DEFAULT;

    // The two saved world states share World_state_mempool.
    const unsigned int state_size = World_state_mempool.space_free() >> 1;
    for (int i = 0; i < NUM_WORLD_STATE_MODES; i++)
    {
        stored_world_state[i].max_size = (int)state_size;
        stored_world_state[i].cur_size = 0;
        stored_world_state[i].buf = (unsigned char*)World_state_mempool.alloc(state_size, 0);
    }
    compress_stream = (zlib_stream*)Permanent_mempool.alloc(sizeof(zlib_stream), 16);
    if (compress_stream != nullptr)
    {
        new (compress_stream) zlib_stream("World compress stream");
    }
    decompress_stream = (zlib_stream*)Permanent_mempool.alloc(sizeof(zlib_stream), 16);
    if (decompress_stream != nullptr)
    {
        new (decompress_stream) zlib_stream("World decompress stream");
    }

    // One list per object type, sized by the pools of the classes of that type.
    max_world_objects = 0;
    for (int type = 0; type < NUM_OBJECT_TYPES; type++)
    {
        int count = 0;
        for (int sub_type = OT_SUB_UNDEFINED; sub_type < NUM_OBJECT_SUB_TYPES; sub_type++)
        {
            const int class_index = obj_class_instantiator::get_class_index_by_types((object_type)type, (object_sub_type)sub_type);
            if (class_index >= 0)
            {
                count += obj_class_instantiator::max_objects[obj_class_instantiator::class_index[class_index]];
            }
        }
        max_world_objects += count;
        unsigned short* indices = (unsigned short*)Permanent_mempool.alloc(count * 2, 2);
        type_objects[type].init(indices, count, 0);
    }
    if (max_world_objects > 0xFFFF)
    {
        keen::debug::nativePrintArgs("world::max_objects must fit within 16 bits - please check object pool sizes!");
    }
    object** objects = (object**)Permanent_mempool.alloc(max_world_objects * 4, 4);
    all_objects.init(objects, max_world_objects, 0);
    object_handle_hash.init(Permanent_mempool, max_world_objects);

    // One list per subtype, sized by the pool of the class with that subtype.
    for (int sub_type = 0; sub_type < NUM_OBJECT_SUB_TYPES; sub_type++)
    {
        const int class_index = obj_class_instantiator::get_class_index_by_subtype((object_sub_type)sub_type);
        // Deviation: no class for the subtype means an empty list (see the file comment).
        const int count = class_index >= 0 ? obj_class_instantiator::max_objects[obj_class_instantiator::class_index[class_index]] : 0;
        unsigned short* indices = (unsigned short*)Permanent_mempool.alloc(count * 2, 2);
        subtype_objects[sub_type].init(indices, count, 0);
    }

    // Broadphase object counts per bpo index.
    int max_bpo_objects[4] = { 0x1EC0, 2000, 0xCC8, 0x1400 };
    havok_bpo_system_init(4, max_bpo_objects);
    load_most_recent_level_filename();
    set_thread_ownership();
    dlc_bundle_id = -1;
}

// 0x0091C490 (:424)
void world::system_shutdown()
{
}

// 0x00940AB0 (:432)
bool world::do_frame(bool hard_load)
{
    process_flagged_objects(1);
    havok_remove_entity_flush();
    havok_remove_phantom_flush();
    if (Stream_pause)
    {
        return false;
    }
    if (deserialize_state == DS_IDLE && gameseq_get_pending_state() != GS_LOAD && !sl_flags.saving_state_data)
    {
        // Stream around the local player, or the camera when there is none.
        player* local_player = player_get_local_player();
        if (local_player == nullptr || !Stream_follows_player)
        {
            stream_pos = Rfg_camera.real_pos;
        }
        else
        {
            stream_pos = local_player->pos;
        }
        monitor_leaving_zones();
        if (deserialize_state == DS_IDLE)
        {
            monitor_grid();
            monitor_entering_zones();
            if (hard_load)
            {
                havok_bpo_do_frame();
            }
            process_streaming_objects(hard_load);
            restore_do_frame_pre();
            process_flagged_objects(1);
            havok_remove_entity_flush();
            havok_remove_phantom_flush();
            restore_do_frame_post();
            process_flagged_objects(1);
            havok_remove_entity_flush();
            havok_remove_phantom_flush();
        }
    }
    if (!process_deserialization())
    {
        game_boot_to_main_menu(BC_SAVE_ERROR);
    }
    process_flagged_objects(1);
    havok_remove_entity_flush();
    havok_remove_phantom_flush();
    return stream2_q_busy();
}

// 0x0091C4A0 (:505)
bool world::set_pending_territory_load(const char* filename, bool new_game, bool single_zone, bool player_start_at_safehouse, int dlc_id, bool reset_destruction)
{
    hud_ui_save_load_kill_msgbox();
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state || save_file != nullptr)
    {
        GAME_TRACE("world", "territory load of %s refused: a load or save is in progress", filename != nullptr ? filename : "(none)");
        return false;
    }
    if (filename == nullptr)
    {
        filename = "";
    }
    GAME_TRACE("world", "territory load pending: %s new_game=%d single_zone=%d safehouse=%d", filename, new_game ? 1 : 0, single_zone ? 1 : 0, player_start_at_safehouse ? 1 : 0);
    strcpy(pending_filename, filename);
    str_make_lower(pending_filename);
    sl_flags.pending_new_game = new_game;
    sl_flags.reset_destruction = reset_destruction;
    sl_flags.pending_single_zone = single_zone;
    sl_flags.player_start_at_safehouse = player_start_at_safehouse;
    dlc_bundle_id = dlc_id;
    gameseq_set_state(GS_LOAD, false);
    return true;
}

// 0x0091C560 (:538)
bool world::set_pending_reload()
{
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state)
    {
        return false;
    }
    if (last_loaded_territory[0] == '\0')
    {
        return false;
    }
    sl_flags.pending_new_game = true;
    return set_pending_territory_load(last_loaded_territory, true, sl_flags.pending_single_zone, false, World.dlc_bundle_id, false);
}

// 0x0091C5D0 (:560)
bool world::set_pending_load_from_memory()
{
    GAME_TRACE("world", "load from memory pending");
    sl_flags.pending_load_game_from_memory = true;
    return true;
}

// 0x0091C5E0 (:567)
world_state_buf* world::get_state_data()
{
    if (stored_world_state[cur_world_state].cur_size > 0)
    {
        return &stored_world_state[cur_world_state];
    }
    return nullptr;
}

// Closes World.save_file (inlined in the loaders).
static void close_save_file()
{
    if (World.save_file != nullptr)
    {
        cf_close(World.save_file);
        World.save_file = nullptr;
    }
}

// 0x00941430 (:574)
bool world::load_territory()
{
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state)
    {
        return false;
    }
    stream_pos = ZERO_VECTOR;
    gamestate_gp_level_init_pre();
    num_streaming_objects = 0;
    stub_serialization_in_progress = false;
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    GAME_TRACE("world", "territory load starts: %s", pending_filename);
    world_zone* start_zone = init_territory(pending_filename, sl_flags.pending_new_game, sl_flags.pending_single_zone);
    if (start_zone == nullptr)
    {
        console_printf("Could not find territory %s\n", pending_filename);
        return false;
    }
    Load_timer_indent -= 2;
    if (Game_boot_exit_flag)
    {
        return false;
    }
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    game_time_of_day_load_global(territory_name);
    Load_timer_indent -= 2;
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    object_district_preload_default_district();
    object_district_preload_districts(territory_name);
    if (!sl_flags.pending_new_game)
    {
        // Saved district state (each saved block is "u32 own offset, u32 offset of its end, data"). The offset is taken
        // before the read: the operands of != have no evaluation order.
        const unsigned int pos = cf_tell(save_file);
        if (pos != cf_read_uint32(save_file))
        {
            return false;
        }
        const unsigned int end = cf_read_uint32(save_file);
        object_district_load_districts(save_file);
        if (cf_tell(World.save_file) != end)
        {
            close_save_file();
            return false;
        }
    }
    Load_timer_indent -= 2;
    if (!sl_flags.pending_new_game)
    {
        // The saved player starts.
        const unsigned int pos = cf_tell(World.save_file);
        if (pos != cf_read_uint32(World.save_file))
        {
            return false;
        }
        const unsigned int end = cf_read_uint32(World.save_file);
        Player_died = cf_read_bool(World.save_file);
        world_state_buf& state = World.stored_world_state[cur_world_state];
        cf_read_vector(state.player_start_pos, World.save_file);
        cf_read_matrix(state.player_start_orient, World.save_file);
        cf_read_vector(state.player_start_pos_safehouse, World.save_file);
        cf_read_matrix(state.player_start_orient_safehouse, World.save_file);
        if (cf_tell(World.save_file) != end)
        {
            close_save_file();
            return false;
        }
    }
    char territory[64];
    cf_drop_ext(territory, sizeof(territory), last_loaded_territory);
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    post_load_stream(start_zone);
    if (Game_boot_exit_flag)
    {
        return false;
    }
    Load_timer_indent -= 2;
    if ((!Multiplayer && !wrecking_crew_is_active()) || !load_aborted)
    {
        sl_flags.pending_new_game = false;
        return true;
    }
    return false;
}

// 0x00940BE0 (:681)
bool world::setup_territory()
{
    if (Multiplayer)
    {
        return true;
    }
    stream2_do_frame(true);
    player_process_object_interaction(player_get_local_player());
    if (mission_get_checkpoint_handle() != INVALID_MISSION_HANDLE)
    {
        mission_activate(mission_get_checkpoint_handle(), false);
    }
    mission_setup_do_frame();
    process_save_load();
    World.do_frame(true);
    if (mission_is_active() && !mission_is_ready())
    {
        return false;
    }
    World.do_frame(true);
    object_spawn_do_spawn();
    return true;
}

// 0x0093FE40 (:723)
world_zone* world::init_territory(const char* filename, bool new_game, bool single_zone)
{
    destroyed_object_table.clear();
    char zone_filename[64];
    if (new_game)
    {
        cf_add_ext(zone_filename, sizeof(zone_filename), filename, ".rfgzone_pc", false);
        cur_world_state = WSM_DEFAULT;
        gfm_reset_tech_level();
    }
    else
    {
        // A saved game: a .rfgs_pc file, or (any other name, usually "") the state saved in memory.
        if (str_stri(cf_get_ext(filename), "rfgs_pc") != nullptr)
        {
            save_file = cf_open(filename, "rb", PLATFORM_PC, false);
        }
        else
        {
            world_state_buf& state = stored_world_state[cur_world_state];
            if (state.cur_size < 1 || state.buf == nullptr)
            {
                GAME_TRACE("world", "load from memory refused: no saved state");
                return nullptr;
            }
            save_file = cf_open(state.buf, state.cur_size, "rb", PLATFORM_PC);
        }
        if (save_file == nullptr)
        {
            return nullptr;
        }
        // "RFGS", version 0x34: see world_serialize.cpp.
        if (cf_read_uint32(save_file) != 0x53474652 || cf_read_uint32(save_file) != 0x34)
        {
            close_save_file();
            ui_add_msgbox(MBT_OK, "MENU_LOAD_FAILED_TITLE", "PLT_LOAD_CORRUPT", nullptr, false, true, nullptr, nullptr, false);
            return nullptr;
        }
        char start_zone_name[64];
        cf_read_string(start_zone_name, sizeof(start_zone_name), save_file);
        cf_add_ext(zone_filename, sizeof(zone_filename), start_zone_name, ".rfgzone_pc", false);
        cf_pad_align_read(4, save_file);
    }

    int grid_x;
    int grid_y;
    int grid_z;
    is_territory = rfg_zonex_get_territory_info(zone_filename, territory_name, grid_x, grid_y, grid_z);
    if (!Multiplayer)
    {
        if (wrecking_crew_is_active())
        {
            World.dlc_bundle_id = wrecking_crew_dlc_bundle_id();
        }
    }
    else
    {
        // Multiplayer levels come from Mp_server_options (not implemented yet): no DLC bundle.
        World.dlc_bundle_id = -1;
    }
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    const bool campaign = !Multiplayer && !wrecking_crew_is_active();
    if (!stream_layout_level_init(zone_filename, campaign, World.dlc_bundle_id))
    {
        return nullptr;
    }
    Load_timer_indent -= 2;
    mempool_mark_scope common_scope(&Common_mempool);
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    // A new campaign game preloads the territory's scripts.
    if (sl_flags.pending_new_game && !Multiplayer && !wrecking_crew_is_active() && !sl_flags.pending_single_zone && !preload_script_files(territory_name))
    {
        return nullptr;
    }
    Load_timer_indent -= 2;
    mempool_mark_scope files_scope(&Common_mempool);
    const char** files = (const char**)Common_mempool.alloc(0x404, 0);
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    const int num_files = rfg_zonex_get_territory_files(is_territory, files, 0x101, zone_filename, ".rfgzone_pc");
    Load_timer_indent -= 2;
    strcpy(last_loaded_territory, zone_filename);
    char persistent_filename[64];
    rfg_zonex_get_persistent_zone_filename(persistent_filename, sizeof(persistent_filename), zone_filename);
    files[num_files] = persistent_filename;
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    if (!init_world_zones(num_files + 1, files))
    {
        return nullptr;
    }
    Load_timer_indent -= 2;
    zone_file* persistent_zf = zone_create_persistent_zone_file(zone_filename);
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    if (!init_territory_streaming_resources(persistent_zf))
    {
        return nullptr;
    }
    Load_timer_indent -= 2;
    char start_zone_name[64];
    cf_drop_ext(start_zone_name, sizeof(start_zone_name), zone_filename);
    return get_world_zone_by_name(start_zone_name);
}

// 0x0093A830 (:888)
bool world::init_world_zones(int num_files, const char** files)
{
    // Multiplayer and Wrecking Crew maps have at most 10 zones.
    if ((Multiplayer || wrecking_crew_is_active()) && num_files > 10)
    {
        return false;
    }
    num_territory_zones = 0;
    all_zones = (world_zone**)Level_mempool.alloc(num_files * 4, 0);
    if (all_zones == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
    }
    for (int i = 0; i < num_files; i++)
    {
        all_zones[i] = nullptr;
    }
    for (int i = 0; i < 257; i++)
    {
        global_zone_grid[i] = nullptr;
    }
    for (int i = 0; i < num_files; i++)
    {
        char name[128];
        int grid_x;
        int grid_y;
        int grid_z;
        rfg_zonex_get_territory_info(files[i], name, grid_x, grid_y, grid_z);
        // Multiplayer and Wrecking Crew maps only use cells (7..9, 7..9) and the persistent zone.
        if ((Multiplayer || wrecking_crew_is_active()) && ((unsigned int)(grid_x - 7) > 2 || grid_y < 7 || grid_y > 9))
        {
            if (grid_x != 15 || grid_y != 0)
            {
                return false;
            }
        }
        world_zone* wz = (world_zone*)Level_mempool.alloc(sizeof(world_zone), 0);
        if (wz == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            new (wz) world_zone(files[i]);
        }
        // The grid id: x and y in the high byte, 0xFF in the low one; it is also the upper half of every handle
        // allocated in the zone.
        const unsigned int gid = ((grid_y & 0xF) | (grid_x & 0xF) << 4) << 8 | 0xFF;
        wz->gid = (unsigned short)gid;
        wz->obj_handle_mgr.upper_bits = gid << 16;
        global_zone_grid[grid_y * 16 + grid_x] = wz;
        all_zones[num_territory_zones] = wz;
        wz->stored_zone_state.max_size = 0;
        wz->stored_zone_state.cur_size = 0;
        wz->stored_zone_state.buf = nullptr;
        num_territory_zones++;
    }
    if (save_file == nullptr)
    {
        return true;
    }
    return load_saved_systems();
}

// Reads a section's header (inlined at every section of the loaders). Returns false when the image does not line up;
// original: the callers then leave the save file open.
static bool load_section_begin(unsigned int& end)
{
    const unsigned int pos = cf_tell(World.save_file);
    if (pos != cf_read_uint32(World.save_file))
    {
        return false;
    }
    end = cf_read_uint32(World.save_file);
    return true;
}

// The save part of init_world_zones (0x0093A830, inlined there): the saved systems and the zone states, in the order
// world::save_state_start and save_state_finish wrote them.
bool world::load_saved_systems()
{
    const int num_zones = cf_read_int32(save_file);
    if (num_zones != num_territory_zones)
    {
        close_save_file();
        return false;
    }
    unsigned int end;
    if (!load_section_begin(end))
    {
        return false;
    }
    destroyed_object_table_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    mission_system_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }

    // The scripts: u32 compressed size, data. Original: always read from the memory image, not through the save file.
    if (!load_section_begin(end))
    {
        return false;
    }
    const unsigned int compressed = cf_read_uint32(save_file);
    {
        const unsigned int common_mark = Common_mempool.mark();
        // Magic number: 1 MB of script state after decompression.
        unsigned char* script_buf = (unsigned char*)Common_mempool.alloc(0x100000, 0);
        cfile* script_file = script_buf != nullptr ? cf_open(script_buf, 0x100000, "rb", PLATFORM_PC) : nullptr;
        if (script_file == nullptr)
        {
            return false;
        }
        unsigned int written = 0;
        unsigned int consumed = 0;
        const unsigned int image_pos = cf_tell(save_file);
        decompress_stream->decompress_init();
        const zlib_result result = decompress_stream->decompress(script_buf, 0x100000, &written, stored_world_state[cur_world_state].buf + image_pos, compressed, &consumed);
        decompress_stream->decompress_finish();
        if (result != ZR_OK)
        {
            VLIB_CRITICAL_FAILURE();
            cf_close(script_file);
            Common_mempool.restore_to_mark(common_mark);
            return false;
        }
        script_system_load(script_file);
        cf_close(script_file);
        Common_mempool.restore_to_mark(common_mark);
        if (!cf_seek(save_file, consumed, CF_SEEK_CUR))
        {
            return false;
        }
    }
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }

    typedef void (*section_loader)(cfile*);
    static const section_loader first_loaders[] = {progression_level_system_load, activity_system_load, restore_list_system_load, activity_raid_system_load, safehouse_vip_load, object_spawn_node_system_load};
    for (section_loader loader : first_loaders)
    {
        if (!load_section_begin(end))
        {
            return false;
        }
        loader(save_file);
        if (cf_tell(save_file) != end)
        {
            close_save_file();
            return false;
        }
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    tech_level = cf_read_float(save_file);
    tech_level_max = cf_read_float(save_file);
    Allow_walker_spawn = cf_read_bool(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    gameaudio_system_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    ui_system_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    restore_system_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    critical_area_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    public_address_kiosk_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    human_teams_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    stats_serialize_in_save_game_stats(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }
    if (!load_section_begin(end))
    {
        return false;
    }
    game_clock_load(save_file);
    if (cf_tell(save_file) != end)
    {
        close_save_file();
        return false;
    }

    // The zone states.
    for (int i = 0; i < num_zones; i++)
    {
        char name[64];
        cf_read_string(name, sizeof(name), save_file);
        cf_pad_align_read(4, save_file);
        world_zone* wz = get_world_zone_by_name(name);
        if (wz == nullptr)
        {
            close_save_file();
            return false;
        }
        wz->stored_zone_state.cur_size = cf_read_int32(save_file);
        wz->stored_zone_state.max_size = cf_read_int32(save_file);
        wz->stored_zone_state.buf = (unsigned char*)Level_mempool.alloc(wz->stored_zone_state.max_size, 0);
        if (wz->stored_zone_state.buf == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
        if (wz->stored_zone_state.cur_size > 0)
        {
            cf_read(wz->stored_zone_state.buf, wz->stored_zone_state.cur_size, save_file, false);
            cf_pad_align_read(4, save_file);
        }
    }
    GAME_TRACE("world", "saved systems and %d zone states read", num_zones);
    return true;
}

// 0x0093D710 (:1194)
void world::level_shutdown()
{
    process_deserialization_level_shutdown();

    // Instantiated terrain subzones.
    {
        csspinlock_scope lock(Terrain_subzones.thread_access);
        for (rfg_terrain_subzone* subzone = Terrain_subzones.get_first(0); subzone != nullptr;)
        {
            rfg_terrain_subzone* next = Terrain_subzones.get_next(subzone);
            if (subzone->instanciated)
            {
                terrain_data* data = nullptr;
                world_zone* wz = get_world_zone_by_pos(subzone->midpoint);
                if (wz != nullptr && wz->state != WZS_UNLOADED && wz->zone_objp != nullptr)
                {
                    data = wz->zone_objp->get_terrain_data();
                }
                terrain_deinstantiate_subzone(subzone, data);
            }
            subzone = next;
        }
    }

    // Streamed zones: their zone objects and containers (the persistent zone has neither).
    for (int i = 0; i < num_territory_zones; i++)
    {
        world_zone* wz = all_zones[i];
        if (wz->state == WZS_STREAMING || wz->state == WZS_LOADED)
        {
            if (wz == global_zone_grid[15])
            {
                wz->zone_objp = nullptr;
            }
            else
            {
                if (wz->zone_objp != nullptr)
                {
                    wz->zone_objp->deinstantiate_resources();
                    destroy_object(wz->zone_objp, false);
                }
                wz->zone_objp = nullptr;
                stream2_release(wz->srid);
            }
            wz->state = WZS_UNLOADED;
        }
    }
    for (int i = 0; i < all_objects.size(); i++)
    {
        destroy_object(all_objects[i], false);
    }
    object_district_release_default_district();
    process_flagged_objects(1);
    havok_remove_entity_flush();
    havok_remove_phantom_flush();
    stream_layout_level_shutdown();
    grid = nullptr;
    zone_free_persistent_zone_file(territory_name);
}

// 0x0091C600 (:1265)
void world::clear_load_aborted()
{
    load_aborted = false;
}

// 0x0091C610 (:1270)
bool world::check_load_aborted()
{
    return Game_boot_exit_flag;
}

// 0x0091C620 (:1290)
void world::abort_load()
{
    load_aborted = true;
}

// 0x0091C630 (:1296)
void world::stream_pause(bool pause)
{
    Stream_pause = pause;
}

// 0x0091C640 (:1302)
bool world::streaming_is_idle()
{
    return !stream2_q_busy();
}

// 0x00923680 (:1378)
int world::get_num_objects() const
{
    return all_objects.size();
}

// 0x00923690 (:1383)
object* world::get_object(int i) const
{
    return all_objects[i];
}

// 0x0092CE70 (:1389)
obj_zone* world::get_zone_by_pos(const vector& pos) const
{
    world_zone* wz = get_world_zone_by_pos(pos);
    if (wz != nullptr && wz->state != WZS_UNLOADED)
    {
        return wz->zone_objp;
    }
    return nullptr;
}

// 0x0092CEE0 (:1395)
bool world::get_zone_bbox_from_pos(const vector& pos, vector& bmin, vector& bmax)
{
    world_zone* wz = get_world_zone_by_pos(pos);
    if (wz == nullptr)
    {
        return false;
    }
    bmin = wz->bmin;
    bmax = wz->bmax;
    return true;
}

// 0x0091C650 (:1414)
void world::put_on_flagged_list(object* objp, unsigned char flagged_state)
{
    // flagged_objects is one circular list; flagged_list_state says why an object is on it (1 free, 2 stream parent
    // notify, 4 broadphase refresh).
    const bool linked = objp->flagged_prev != nullptr || objp->flagged_next != nullptr;
    if ((objp->obj_flags.flagged_list_state & flagged_state & 7) != 0)
    {
        return;
    }
    objp->obj_flags.flagged_list_state |= flagged_state & 7;
    if (linked)
    {
        // Already on the list: move it to the end, unless it is the one being processed.
        if (current_flagged_object == objp)
        {
            return;
        }
        if (flagged_objects == objp && objp->flagged_next == objp)
        {
            objp->flagged_next = nullptr;
            objp->flagged_prev = nullptr;
            flagged_objects = nullptr;
        }
        else
        {
            if (flagged_objects == objp)
            {
                flagged_objects = objp->flagged_next;
            }
            objp->flagged_prev->flagged_next = objp->flagged_next;
            objp->flagged_next->flagged_prev = objp->flagged_prev;
            objp->flagged_next = nullptr;
            objp->flagged_prev = nullptr;
        }
    }
    if (flagged_objects != nullptr)
    {
        objp->flagged_prev = flagged_objects->flagged_prev;
        objp->flagged_next = flagged_objects;
        flagged_objects->flagged_prev->flagged_next = objp;
        flagged_objects->flagged_prev = objp;
        return;
    }
    objp->flagged_next = objp;
    objp->flagged_prev = objp;
    flagged_objects = objp;
}

// 0x0091C720 (:1462)
void world::remove_from_flagged_list(object* objp, unsigned char flagged_state, bool force)
{
    if (!force && (objp->obj_flags.flagged_list_state & flagged_state & 7) == 0)
    {
        return;
    }
    objp->obj_flags.flagged_list_state &= ~flagged_state;
    if (force)
    {
        objp->obj_flags.flagged_list_state = 0;
    }
    else if (objp->obj_flags.flagged_list_state != 0)
    {
        return;
    }
    if (flagged_objects == objp)
    {
        if (objp->flagged_next == objp)
        {
            objp->flagged_next = nullptr;
            objp->flagged_prev = nullptr;
            flagged_objects = nullptr;
            return;
        }
        flagged_objects = objp->flagged_next;
    }
    objp->flagged_prev->flagged_next = objp->flagged_next;
    objp->flagged_next->flagged_prev = objp->flagged_prev;
    objp->flagged_prev = nullptr;
    objp->flagged_next = nullptr;
}

// 0x0093BF70 (:1491)
void world::process_flagged_objects(unsigned char flagged_state)
{
    current_flagged_mode = flagged_state;
    current_flagged_object = flagged_objects;
    while (current_flagged_object != nullptr)
    {
        object* objp = current_flagged_object;
        object* next;
        if ((objp->obj_flags.flagged_list_state & flagged_state & 7) == 0)
        {
            next = objp->flagged_next != flagged_objects ? objp->flagged_next : nullptr;
        }
        else
        {
            // A freed object is removed from the list whatever else it was flagged for.
            bool force = false;
            if (flagged_state == 1)
            {
                free_object(objp);
                force = true;
            }
            else if (flagged_state == 2)
            {
                apply_stream_parent_notify(objp);
            }
            else if (flagged_state == 4)
            {
                if (objp->bp_handle != nullptr)
                {
                    havok_bpo_refresh_status(objp->bp_handle);
                }
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
            }
            next = current_flagged_object->flagged_next;
            next = next != flagged_objects ? next : nullptr;
            remove_from_flagged_list(current_flagged_object, flagged_state, force);
        }
        current_flagged_object = next;
    }
}

// 0x0093C050 (:1549)
object* world::object_from_handle(unsigned int handle)
{
    // Stubs (objects allocated but not created yet) are not returned.
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object** objp = object_handle_hash.get_ptr(handle);
    if (objp == nullptr || *objp == nullptr || (*objp)->obj_flags.stub)
    {
        return nullptr;
    }
    return *objp;
}

// 0x0091C7B0 (:1661)
const char* world::get_territory_name()
{
    return territory_name;
}

// 0x0091C7C0 (:1670)
void world::inherit_children_and_dependants(object* dst, object* src)
{
    // Moves every child of src to the end of dst's child list.
    for (object* child = src->child_ptr; child != nullptr;)
    {
        object* next = child->child_next != src->child_ptr ? child->child_next : nullptr;
        if (src->child_ptr == child && child->child_next == child)
        {
            child->child_next = nullptr;
            child->child_prev = nullptr;
            src->child_ptr = nullptr;
        }
        else
        {
            if (src->child_ptr == child)
            {
                src->child_ptr = child->child_next;
            }
            child->child_prev->child_next = child->child_next;
            child->child_next->child_prev = child->child_prev;
            child->child_next = nullptr;
            child->child_prev = nullptr;
        }
        if (dst->child_ptr == nullptr)
        {
            child->child_next = child;
            child->child_prev = child;
            dst->child_ptr = child;
        }
        else
        {
            child->child_prev = dst->child_ptr->child_prev;
            child->child_next = dst->child_ptr;
            dst->child_ptr->child_prev->child_next = child;
            dst->child_ptr->child_prev = child;
        }
        child = next;
    }
}

// 0x0093D980 (:1681)
void world::set_child(object* child, object* parent)
{
    if (child == parent)
    {
        return;
    }
    if (parent != nullptr)
    {
        if (!child->obj_flags.streamed)
        {
            // The root of the hierarchy refreshes its broadphase status.
            while (parent->parent != 0xFFFFFFFF)
            {
                parent = object_from_handle(parent->parent);
            }
            if ((parent->obj_flags.flagged_list_state & 4) == 0)
            {
                put_on_flagged_list(parent, 4);
            }
        }
        else
        {
            stream_parent_notify(child);
        }
    }
    if (!child->obj_flags.destroyed)
    {
        child->set_streaming_dependent(parent == nullptr);
    }
}

// 0x0091C850 (:1717)
void world::set_object_name(object* objp, const char* new_name)
{
    if (objp->name_index != NO_NAME)
    {
        dealloc_obj_name(objp->name_index);
    }
    if (new_name != nullptr && *new_name != '\0')
    {
        char* dest = alloc_obj_name(objp->name_index);
        if (dest != nullptr)
        {
            strncpy_safe(dest, new_name, 0x2F);
        }
    }
}

// 0x0091C8A0 (:1736)
const char* world::get_object_name(const object* objp) const
{
    const unsigned short index = objp->name_index;
    if (index != NO_NAME && index < 2048)
    {
        return Obj_name_buf[index].str;
    }
    return "";
}

// game_trace.h (not in rfg.exe).
const char* game_trace_object_name(unsigned int handle)
{
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return "none";
    }
    const char* name = World.get_object_name(objp);
    if (name[0] != '\0')
    {
        return name;
    }
    // Magic number: a line can show a few objects, so a few buffers in turn.
    static char buffers[4][16];
    static int next_buffer;
    char* text = buffers[next_buffer++ & 3];
    snprintf(text, sizeof(buffers[0]), "type %d", (int)objp->get_obj_type());
    return text;
}

// 0x0091C8D0 (:1746)
bool world::preload_script_files(const char* filename)
{
    // Every "<territory>*" script.
    const char* scripts[192];
    char pattern[256];
    memset(pattern, 0, sizeof(pattern));
    cf_drop_ext(pattern, sizeof(pattern), filename);
    sprintf(pattern + strlen(pattern), "*%s", ".scriptx");
    const unsigned int num_scripts = cf_find_pattern(scripts, 192, pattern);
    for (int i = 0; i < (int)num_scripts; i++)
    {
        if (script_load(scripts[i]) < 0)
        {
            return false;
        }
    }
    return true;
}

// 0x00940C60 (:1843)
void world::stream_hard_wait()
{
    // World frames until stream2 is idle (the body is do_frame's with hard loading), then one more of each.
    do
    {
        process_flagged_objects(1);
        havok_remove_entity_flush();
        havok_remove_phantom_flush();
        if (Stream_pause)
        {
            break;
        }
        if (deserialize_state == DS_IDLE && gameseq_get_pending_state() != GS_LOAD && !sl_flags.saving_state_data)
        {
            player* local_player = player_get_local_player();
            if (local_player == nullptr || !Stream_follows_player)
            {
                stream_pos = Rfg_camera.real_pos;
            }
            else
            {
                stream_pos = local_player->pos;
            }
            monitor_leaving_zones();
            if (deserialize_state == DS_IDLE)
            {
                monitor_grid();
                monitor_entering_zones();
                havok_bpo_do_frame();
                process_streaming_objects(true);
                restore_do_frame_pre();
                process_flagged_objects(1);
                havok_remove_entity_flush();
                havok_remove_phantom_flush();
                restore_do_frame_post();
                process_flagged_objects(1);
                havok_remove_entity_flush();
                havok_remove_phantom_flush();
            }
        }
        if (!process_deserialization())
        {
            game_boot_to_main_menu(BC_SAVE_ERROR);
        }
        process_flagged_objects(1);
        havok_remove_entity_flush();
        havok_remove_phantom_flush();
        if (!stream2_q_busy())
        {
            break;
        }
        stream2_do_frame(true);
    } while (!Game_boot_exit_flag);
    stream2_do_frame(true);
    do_frame(true);
}

// Test seam (off by default, OpenRFG --sp-pos, stub_impl.md S9): a new game's player start. OpenRFG has no other way to
// reach a place that spawns something while missions and the map are not ported.
bool g_openrfgStartPosSet = false;
vector g_openrfgStartPos;

// 0x00940DB0 (:1867)
bool world::post_load_stream(world_zone* start_zone)
{
    GAME_TRACE("world", "territory stream done: start zone %s", start_zone->name);
    // The persistent zone is always loaded and has no zone object.
    world_zone* persistent = global_zone_grid[15];
    persistent->zone_objp = nullptr;
    persistent->state = WZS_LOADED;
    char persistent_filename[64];
    rfg_zonex_get_persistent_zone_filename(persistent_filename, sizeof(persistent_filename), start_zone->name);
    serialize_zone(persistent, zone_find(persistent_filename), false);

    // The player start: the start zone's centre for a new game (refined below), else the saved one.
    bool new_game = false;
    Debug_player_start = false;
    if (sl_flags.pending_new_game)
    {
        new_game = true;
        sl_flags.player_start_pos.x = start_zone->bmin.x + (start_zone->bmax.x - start_zone->bmin.x) * 0.5f;
        sl_flags.player_start_pos.y = start_zone->bmin.y + (start_zone->bmax.y - start_zone->bmin.y) * 0.5f;
        sl_flags.player_start_pos.z = start_zone->bmin.z + (start_zone->bmax.z - start_zone->bmin.z) * 0.5f;
        if (g_openrfgStartPosSet)
        {
            sl_flags.player_start_pos = g_openrfgStartPos;
        }
        if (!Multiplayer && !wrecking_crew_is_active())
        {
            sl_flags.player_start_orient.make_identity();
        }
        else
        {
            sl_flags.player_start_orient = IDENTITY_MATRIX;
        }
    }
    else
    {
        world_state_buf& state = stored_world_state[cur_world_state];
        if (sl_flags.player_start_at_safehouse)
        {
            sl_flags.player_start_pos = state.player_start_pos_safehouse;
            sl_flags.player_start_orient = state.player_start_orient_safehouse;
        }
        else
        {
            sl_flags.player_start_pos = state.player_start_pos;
            sl_flags.player_start_orient = state.player_start_orient;
        }
    }
    camera_set_pos(sl_flags.player_start_pos);
    world_zone* stream_zone = get_world_zone_by_pos(sl_flags.player_start_pos);
    game_track_mem_usage_start("pre stream level");
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    stream_hard_wait();
    if (Game_boot_exit_flag)
    {
        return false;
    }
    Load_timer_indent -= 2;
    game_track_mem_usage_end(false);
    if (new_game && !g_openrfgStartPosSet)
    {
        // The first player start of the zone that is available for the initial spawn wins; otherwise the last one.
        unsigned int handles[12];
        const int num = collect_objects<player_start>(stream_zone->bmin, stream_zone->bmax, handles, 12, nullptr, nullptr);
        for (int i = 0; i < num; i++)
        {
            if (handles[i] == 0xFFFFFFFF)
            {
                continue;
            }
            object* objp = object_from_handle(handles[i]);
            if (objp == nullptr || objp->get_obj_type() != OT_PLAYER_START)
            {
                continue;
            }
            sl_flags.player_start_pos = objp->pos;
            sl_flags.player_start_orient = objp->orient;
            if (((player_start*)objp)->available_for_initial_spawn)
            {
                break;
            }
        }
    }
    if (stream_zone->state == WZS_LOADED)
    {
        timer_get(TIMER_MILLISECONDS);
        Load_timer_indent += 2;
        game_track_mem_usage_start("gamestate_gp_level_init_post level");
        gamestate_gp_level_init_post();
        game_track_mem_usage_end(false);
        Load_timer_indent -= 2;
    }
    if (World.save_file != nullptr)
    {
        // The saved roads, the last section of the image.
        const unsigned int pos = cf_tell(World.save_file);
        if (pos != cf_read_uint32(World.save_file))
        {
            return false;
        }
        const unsigned int end = cf_read_uint32(World.save_file);
        road_data_load(save_file);
        const bool at_end = cf_tell(World.save_file) == end;
        close_save_file();
        if (!at_end)
        {
            return false;
        }
    }
    game_track_mem_usage_start("post_stream_level");
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    stream_hard_wait();
    if (Game_boot_exit_flag)
    {
        return false;
    }
    Load_timer_indent -= 2;
    game_track_mem_usage_end(false);
    gameplay_level_init_spawn();
    return true;
}

// 0x0092CF70 (:2009)
bool world::object_physically_in_original_zone(object* objp)
{
    if (objp->obj_flags.persistent)
    {
        return true;
    }
    world_zone* wz = get_world_zone_by_pos(objp->pos);
    if (wz == nullptr)
    {
        return false;
    }
    // The zone's grid id against the one in the handle's upper half.
    return ((wz->gid ^ (unsigned short)(objp->handle >> 16)) & 0xFF00) == 0;
}

// 0x0091C9B0 (:2026)
bool world::object_add_to_destroyed_table(unsigned int handle)
{
    return destroyed_object_table.insert(handle);
}

// 0x0092D000 (:2037)
bool world::object_was_destroyed(unsigned int handle)
{
    return destroyed_object_table.exists(handle);
}

// 0x0092D010 (:2042)
bool world::destroyed_object_table_save(cfile* save_filep)
{
    cf_write_uint32(destroyed_object_table.get_count(), save_filep);
    unsigned int handle;
    if (destroyed_object_table.get_first_key(handle))
    {
        do
        {
            cf_write_uint32(handle, save_filep);
        } while (destroyed_object_table.get_next_key(handle, handle));
    }
    return true;
}

// 0x0091C9D0 (:2055)
bool world::destroyed_object_table_load(cfile* save_filep)
{
    for (unsigned int count = cf_read_uint32(save_filep); count != 0; count--)
    {
        destroyed_object_table.insert(cf_read_uint32(save_filep));
    }
    return true;
}

// 0x0091CA10 (:2075)
void world::save_most_recent_level_filename()
{
    cfile* f = cf_open("rfg_last_level_pc.txt", "wt", PLATFORM_PC, false);
    if (f != nullptr)
    {
        cf_write_string(last_loaded_territory, f);
        if (sl_flags.pending_single_zone)
        {
            cf_write_string("single", f);
        }
        cf_close(f);
    }
}

// 0x0091CA60 (:2101)
void world::load_most_recent_level_filename()
{
    cfile* f = cf_open("rfg_last_level_pc.txt", "rt", PLATFORM_PC, false);
    if (f != nullptr)
    {
        cf_read_string(last_loaded_territory, sizeof(last_loaded_territory), f);
        char mode[64];
        if (cf_read_string(mode, sizeof(mode), f) && v_stricmp(mode, "single") == 0)
        {
            sl_flags.pending_single_zone = true;
        }
        cf_close(f);
    }
}

// 0x0091CB30 (:2139)
// Prepares loading a save: the save's unlocks and bonus campaign. A null save is the autosave of the current bonus
// campaign; without one the game goes to the main menu.
void world::platform_load(game_save_info* save_game, bool set_default_mode)
{
    if (set_default_mode || sl_flags.pending_new_game)
    {
        cur_world_state = WSM_DEFAULT;
    }
    mempool_mark_scope mark(&Common_mempool);
    if (save_game == nullptr)
    {
        // Magic number: the 12 save slots.
        const int size = game_save_get_list_buf_size(12);
        game_save_info* list = (game_save_info*)Common_mempool.alloc(size, 0);
        memset(list, 0, size);
        save_game = game_save_get_autosave(list, size, 12, 0, false, World.dlc_bundle_id);
        if (save_game == nullptr)
        {
            gameseq_set_state(GS_MAINMENU, true);
            return;
        }
    }
    dlc_bundle_id = save_game->dlc_id;
    stats_turn_off_all_cheats();
    gamelog_campaign_start_logged(false);
    loading_screen_set_district(save_game->territory_index, save_game->district_index);
    // The old byte of hammers joins the low half of the large field (saves from before the field existed).
    uint64_t hammers;
    memcpy(&hammers, save_game->new_data.hammers_unlocked_large.raw_data, sizeof(hammers));
    meta_data_set_hammers_unlocked(hammers | save_game->hammers_unlocked);
    Wants_to_start_game_with_golden_hammer = save_game->golden_hammer_desired;
    unsigned short backpacks;
    memcpy(&backpacks, save_game->new_data.backpacks_unlocked.raw_data, sizeof(backpacks));
    meta_data_set_backpacks_unlocked(backpacks);
    meta_data_set_jetpack_upgrade_level(save_game->new_data.jetpack_unlock_level);
}

// 0x0091CAF0 (:2129)
void* world::get_load_buf()
{
    return stored_world_state[cur_world_state].buf;
}

// 0x0091CB10 (:2134)
void world::set_cur_size(int size)
{
    stored_world_state[cur_world_state].cur_size = size;
}

// 0x00A054F0 (.h:196)
cfile* world::get_save_file()
{
    return save_file;
}

// 0x00883810 (.h:230)
const char* world::get_pending_filename()
{
    return pending_filename;
}

// 0x00753B30 (.h:436)
world_state_mode world::get_world_state()
{
    return cur_world_state;
}

// 0x00753B40 (.h:437)
void world::set_world_state(world_state_mode new_mode)
{
    // Deviation from rfg.exe: the trace line is the port's (--game-trace).
    GAME_TRACE("world", "state %d", (int)new_mode);
    cur_world_state = new_mode;
}

// 0x0093C0B0 (:2194)
void world_system_init()
{
    // Every name entry starts on the free list.
    Obj_name_used_list = NO_NAME;
    Obj_name_free_list = NO_NAME;
    for (int i = 0; i < 2048; i++)
    {
        name_list_append(Obj_name_free_list, (unsigned short)i);
    }
    World.system_init();
}

// 0x0091CC40 (:2209)
bool world_level_exists(const char* filename, bool multiplayer_level)
{
    if (gameseq_get_state() == GS_MULTI_INIT)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    game_enable_standard_access();
    char path[256];
    if (Use_packfiles)
    {
        if (multiplayer_level)
        {
            sprintf(path, "%s\\%s.%s", "/gamebuild", filename, "vpp_pc");
        }
        else
        {
            char territory[64];
            int grid_x;
            int grid_y;
            int grid_z;
            rfg_zonex_get_territory_info(filename, territory, grid_x, grid_y, grid_z);
            sprintf(path, "%s\\%s_l0.%s", "/gamebuild", territory, "vpp_pc");
        }
    }
    else
    {
        cf_add_ext(path, sizeof(path), filename, "vpp_pc", false);
    }
    const bool exists = cf_exist(path);
    game_disable_standard_access();
    return exists;
}

// 0x0092D070 (:2252)
bool world_collect_descendants(const object* obj, base_array<object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data)
{
    // Depth first; returns true once the list is full.
    if (obj_list.full())
    {
        return true;
    }
    for (object* child = obj->child_ptr; child != nullptr;)
    {
        if (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data))
        {
            obj_list.add(child);
            if (obj_list.full())
            {
                return true;
            }
        }
        if (world_collect_descendants(child, obj_list, cb_func, cb_data))
        {
            return true;
        }
        child = child->child_next;
        if (child == obj->child_ptr)
        {
            break;
        }
    }
    return false;
}

// 0x0091CD60 (:2383)
float world::get_tech_level()
{
    return tech_level;
}

// Clamps a tech level to [0, max] (inlined in the setters).
static float clamp_tech_level(float level, float max)
{
    if (level - 0.0f < 0.0f)
    {
        level = 0.0f;
    }
    if (0.0f <= level - max)
    {
        return max;
    }
    return level;
}

// 0x0091CD70 (:2388)
void world::set_tech_level(float tech_level_value)
{
    tech_level = clamp_tech_level(tech_level_value, tech_level_max);
}

// 0x0091CDD0 (:2393)
void world::change_tech_level(float tech_level_delta)
{
    tech_level = clamp_tech_level(tech_level_delta + tech_level, tech_level_max);
}

// 0x0091CE40 (:2398)
float world::get_tech_level_max()
{
    return tech_level_max;
}

// 0x0091CE50 (:2403)
void world::set_tech_level_max(float tech_level_max_value)
{
    tech_level_max = tech_level_max_value;
    tech_level = clamp_tech_level(tech_level, tech_level_max_value);
}

// 0x0091CEB0 (:2411)
void world::change_tech_level_max(float tech_level_max_delta)
{
    tech_level_max = tech_level_max_delta + tech_level_max;
    tech_level = clamp_tech_level(tech_level, tech_level_max);
}

// 0x0091CF30 (:2416)
void world::set_thread_ownership()
{
    if (deserialize_pool != nullptr)
    {
        deserialize_pool->set_thread_ownership();
    }
    thread_id = get_current_thread_id();
}

// 0x0091CF50 (:2425)
void world::check_thread_ownership()
{
}

// 0x0091CF60 (:2442)
void world::sanity_check_object_lists()
{
}

// 0x0091CF70 (:2580)
void unused_dcf_havok_bpo_counts()
{
}
