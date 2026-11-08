// Original: volition/rfg/code/level/world.h
// Implementation: volition/rfg/code/level/world.cpp
//
// The world: every object in the level, the zones of the loaded territory, streaming of objects with their zones, and
// saving/restoring world state (decompiled). The single instance is World. Methods are spread over world.cpp,
// world_stream.cpp (streaming), world_serialize.cpp (object creation and zone serialization) and world_restore.cpp
// (restoring destroyed objects); the typed templates are in world.inl.
// - Objects: all_objects holds every live object; type_objects[object_type] and subtype_objects[object_sub_type] hold
//   indices into it, so get_object<T>(i) is all_objects[type_objects[T type][i]] and get_num_objects<T> the list size.
//   object_handle_hash maps a handle to its object; object_from_handle skips stubs (obj_flags.stub). A handle is
//   allocated from the origin zone's handle_manager, so its upper bits are the zone's grid id.
// - Creation (create_object_internal): the position comes from the OBJ_PROP_JUST_POS or OBJ_PROP_COMBINED property; the
//   origin zone is the cell under it (16x16 cells of 511 units from -4088, global_zone_grid[z * 16 + x]). A stub is
//   allocated through obj_class_instantiator (alloc_object_stub: class pool, type, handle, bpo index) and turned into
//   the object by create_object_from_stub, which runs the class's serialize(pblock, load). create_object<T> uses
//   T::get_classname_hash(); mission_object_creation_mode marks the objects as created by a mission or activity.
// - Streaming (do_frame, called from the sim/render handshake): flagged objects are processed, then, unless a load is
//   pending or state is being saved, stream_pos follows the local player (Stream_follows_player) or the camera, zones
//   that left the streaming region are serialized (monitor_leaving_zones), the grid is updated (monitor_grid,
//   stream_grid::do_frame and terrain subzones), entering zones are deserialized from their .rfgzone_pc and saved state
//   (monitor_entering_zones), objects whose streamed data arrived are finished (process_streaming_objects), and
//   restorations run (restore_do_frame_pre/post). It returns stream2_q_busy(). Stream_pause stops all of it.
// - Zones leaving the region are serialized into their world_zone::stored_zone_state through a small state machine
//   (deserialize_state: DS_PENDING -> DS_COLLECT -> DS_COMPRESS -> DS_SAVE_FINALIZE, with compress_stream), on pages
//   from Common_page_pool.
// - Loading (set_pending_territory_load -> gameseq GS_LOAD -> load_territory): init_territory reads the territory's zone
//   list and creates the world_zones, the districts and time of day are loaded, a saved game is read from save_file
//   (district state, player start), then post_load_stream streams the start zone and its neighbours.
// - Saving: save_and_continue / save_and_reload / save_and_warp set sl_flags; the state goes to stored_world_state
//   [cur_world_state] (WSM_DEFAULT or WSM_CHECKPOINT) and from there to a save slot (platform_save_to_disk).
// - Destroyed level objects are remembered by handle (destroyed_object_table) so they are not recreated when their zone
//   streams in again.
#pragma once

// Test seam: see world::post_load_stream.
class vector;
extern bool g_openrfgStartPosSet;
extern vector g_openrfgStartPos;

#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/os/cs_wrapper.h"

struct cfile;
class object;
class obj_zone;
class world_zone;
struct zone_file;
struct zone_object;
class obj_prop_block;
struct game_save_info;
class virtual_mempool;
class zlib_stream;
class stream_grid;
struct havok_bpi;
struct c_chunk_base;
class rfg_base_destroyable_shape;

enum world_state_mode
{
    WSM_DEFAULT = 0,
    WSM_CHECKPOINT = 1,
    NUM_WORLD_STATE_MODES = 2,
};

enum world_deserialize_state
{
    DS_IDLE = 0,
    DS_PENDING = 1,
    DS_COLLECT = 2,
    DS_COMPRESS = 3,
    DS_SAVE_FINALIZE = 4,
};

// size 0x34
struct save_load_info
{
    void clear(); // 0x00919F10 (.h:111)
    save_load_info(); // 0x0091C240

    bool pending_new_game : 1; // 0x00:0
    bool reset_destruction : 1; // 0x00:1
    bool pending_save_game : 1; // 0x00:2
    bool performing_save_game : 1; // 0x00:3
    bool pending_save_state : 1; // 0x00:4
    bool performing_save_state : 1; // 0x00:5
    bool pending_single_zone : 1; // 0x00:6
    bool save_game_warp : 1; // 0x00:7
    bool game_save_checkpoint : 1; // 0x01:0
    bool game_save_to_disk : 1; // 0x01:1
    bool pending_load_game_from_memory : 1; // 0x01:2
    bool auto_save_game : 1; // 0x01:3
    bool saving_state_data : 1; // 0x01:4
    bool player_start_at_safehouse : 1; // 0x01:5
    vector player_start_pos; // 0x04
    matrix player_start_orient; // 0x10
};

// size 0x6C
struct world_state_buf
{
    world_state_buf(); // 0x0091C230

    vector player_start_pos; // 0x00
    matrix player_start_orient; // 0x0C
    vector player_start_pos_safehouse; // 0x30
    matrix player_start_orient_safehouse; // 0x3C
    unsigned char* buf; // 0x60
    int cur_size; // 0x64
    int max_size; // 0x68
};

// size 0x2BB8
class world
{
public:
    void system_init(); // 0x0093BB00 (:294)
    void system_shutdown(); // 0x0091C490 (:424)
    bool do_frame(bool hard_load); // 0x00940AB0 (:432)
    void level_shutdown(); // 0x0093D710 (:1194)
    void clear_load_aborted(); // 0x0091C600 (:1265)
    bool check_load_aborted(); // 0x0091C610 (:1270)
    void abort_load(); // 0x0091C620 (:1290)
    void stream_pause(bool pause); // 0x0091C630 (:1296)
    bool streaming_is_idle(); // 0x0091C640 (:1302)
    void stream_hard_wait(); // 0x00940C60 (:1843)
    bool set_pending_territory_load(const char* filename, bool new_game, bool single_zone, bool player_start_at_safehouse, int dlc_id, bool reset_destruction); // 0x0091C4A0 (:505)
    bool set_pending_load_from_memory(); // 0x0091C5D0 (:560)
    bool set_pending_reload(); // 0x0091C560 (:538)
    world_state_buf* get_state_data(); // 0x0091C5E0 (:567)
    bool save_state_data(); // 0x009413B0 (world_serialize.cpp:2601)
    void save_and_continue(game_save_info* save_slot, bool checkpoint, bool auto_save); // 0x0091D1C0 (world_serialize.cpp:2641)
    void save_and_reload(); // 0x0091D260 (world_serialize.cpp:2744)
    void save_and_warp(const vector& warp_pos, const matrix& warp_orient); // 0x0091D2A0 (world_serialize.cpp:2850)
    void save_and_continue_warp(const vector& warp_pos, const matrix& warp_orient); // 0x0091D330 (world_serialize.cpp:2870)
    cfile* get_save_file(); // 0x00A054F0 (.h:196)
    void platform_load(game_save_info* save_game, bool set_default_mode); // 0x0091CB30 (:2139)
    void set_cur_size(int size); // 0x0091CB10 (:2134)
    void* get_load_buf(); // 0x0091CAF0 (:2129)
    bool load_territory(); // 0x00941430 (:574)
    bool setup_territory(); // 0x00940BE0 (:681)
    bool is_inside_streaming_region(vector& bmin, vector& bmax); // 0x009239E0 (world_stream.cpp:309)
    bool is_zone_streamed(unsigned short id) const; // 0x00923B60 (world_stream.cpp:353)
    bool is_zone_streamed(unsigned int handle) const; // 0x0092E1F0 (world_stream.cpp:348)
    bool is_zone_streamed(const vector& pos) const; // 0x0092E180 (world_stream.cpp:337)
    bool is_zone_border_zone(const vector& pos) const; // 0x0092E110 (world_stream.cpp:324)
    bool get_zone_bbox_from_pos(const vector& pos, vector& bmin, vector& bmax); // 0x0092CEE0 (:1395)
    int get_max_world_objects() const;
    const char* get_pending_filename(); // 0x00883810 (.h:230)
    int get_num_objects() const; // 0x00923680 (:1378)
    object* get_object(int i) const; // 0x00923690 (:1383)
    object* object_from_handle(unsigned int handle); // 0x0093C050 (:1549)
    // Typed access (world.inl, one instantiation per object class; T provides get_type_static/get_subtype_static,
    // get_classname_hash and its pool).
    template<typename T>
    int get_num_objects() const; // <automobile> 0x00649800 (world.inl:16)
    template<typename T>
    int get_num_free_objects() const; // <weapon> 0x00A25570 (world.inl:39)
    template<typename T>
    T* get_object(int i) const; // <player_start> 0x0077CD70 (world.inl:44)
    template<typename T>
    T* object_from_handle(unsigned int handle); // <object_convoy_end_point> 0x00766840 (world.inl:54)
    template<typename T>
    int collect_objects(const vector& bmin, const vector& bmax, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const; // <object_upgrade_node> 0x00A4D760 (world.inl:72)
    template<typename T>
    int collect_objects(const vector& pos, float radius, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const; // <object_mover> 0x00722EC0
    template<typename T>
    int collect_objects(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const; // <human> 0x00A48FF0
    template<typename T>
    T* object_cast(object* objp) const; // <item> 0x00813460 (world.inl:176)
    template<typename T>
    bool is_exact_object_type(object* objp) const; // <weapon> 0x00A256F0 (world.inl:198)
    template<typename T>
    T* create_object(obj_prop_block& pblock, unsigned int parent); // <object_squad> 0x00767130 (world.inl:236)
    template<typename T>
    T* create_object_global(obj_prop_block& pblock, unsigned int parent); // <district> 0x00767090 (world.inl:243)
    void begin_mission_object_creation(); // 0x0091D010 (world_serialize.cpp:282)
    void end_mission_object_creation(); // 0x0091D020 (world_serialize.cpp:289)
    bool destroy_object(unsigned int handle, bool preserve_handle); // 0x0093C740 (world_serialize.cpp:853)
    bool destroy_object(object* objp, bool preserve_handle); // 0x0091D070 (world_serialize.cpp:754)
    obj_zone* get_object_zone_by_index(int index) const; // 0x0091D9F0 (world_stream.cpp:1291)
    obj_zone* get_object_zone_by_grid_id(unsigned short id) const; // 0x0091DA20 (world_stream.cpp:1298)
    obj_zone* get_zone_by_pos(const vector& pos) const; // 0x0092CE70 (:1389)
    const char* get_territory_name(); // 0x0091C7B0 (:1661)
    float get_tech_level(); // 0x0091CD60 (:2383)
    void set_tech_level(float tech_level_value); // 0x0091CD70 (:2388)
    void change_tech_level(float tech_level_delta); // 0x0091CDD0 (:2393)
    float get_tech_level_max(); // 0x0091CE40 (:2398)
    void set_tech_level_max(float tech_level_max_value); // 0x0091CE50 (:2403)
    void change_tech_level_max(float tech_level_max_delta); // 0x0091CEB0 (:2411)
    bool serialize_layer(const char* layer_name, unsigned int* object_list, int max_object_list_size, int& returned_objects); // 0x0093EC70 (world_serialize.cpp:1042)
    void set_thread_ownership(); // 0x0091CF30 (:2416)
private:
    void check_thread_ownership(); // 0x0091CF50 (:2425)
public:
    void sanity_check_object_lists(); // 0x0091CF60 (:2442)
private:
    void inherit_children_and_dependants(object* dst, object* src); // 0x0091C7C0 (:1670)
    void set_child(object* child, object* parent); // 0x0093D980 (:1681)
    void set_object_name(object* objp, const char* new_name); // 0x0091C850 (:1717)
    const char* get_object_name(const object* objp) const; // 0x0091C8A0 (:1736)
    unsigned char calc_bpo_flags(const object* objp); // 0x0091D040 (world_serialize.cpp:596)
    bool preload_script_files(const char* filename); // 0x0091C8D0 (:1746)
    bool check_for_load_abort();
    world_zone* init_territory(const char* filename, bool new_game, bool single_zone); // 0x0093FE40 (:723)
    bool init_world_zones(int num_files, const char** files); // 0x0093A830 (:888)
    bool load_saved_systems(); // the save part of init_world_zones, inlined there in rfg.exe
    bool post_load_stream(world_zone* start_zone); // 0x00940DB0 (:1867)
    void save_most_recent_level_filename(); // 0x0091CA10 (:2075)
    void load_most_recent_level_filename(); // 0x0091CA60 (:2101)
    void put_on_flagged_list(object* objp, unsigned char flagged_state); // 0x0091C650 (:1414)
    void remove_from_flagged_list(object* objp, unsigned char flagged_state, bool force); // 0x0091C720 (:1462)
    void process_flagged_objects(unsigned char flagged_state); // 0x0093BF70 (:1491)
public:
    world_state_mode get_world_state(); // 0x00753B30 (.h:436)
    void set_world_state(world_state_mode new_mode); // 0x00753B40 (.h:437)
private:
    object* stub_from_handle(unsigned int handle); // 0x0093B2F0 (world_serialize.cpp:206)
    bool alloc_obj_handle(object* stub, world_zone* wz); // 0x0093B340 (world_serialize.cpp:216)
    bool alloc_obj_handle_preexisting(object* stub, unsigned int handle); // 0x0093B380 (world_serialize.cpp:232)
    void free_obj_handle(object* stub); // 0x0093B3E0 (world_serialize.cpp:260)
    object* alloc_object_stub(unsigned int classname_hash, world_zone* origin_zone, unsigned int parent); // 0x0093B450 (world_serialize.cpp:328)
    object* alloc_object_stub_preexisting(unsigned int classname_hash, unsigned int handle, unsigned int parent); // 0x0093B530 (world_serialize.cpp:372)
    bool create_object_from_stub(object* stub, obj_prop_block& pblock); // 0x0093E410 (world_serialize.cpp:421)
    bool create_object_from_stub(object* stub, zone_object* zobjp); // 0x0093E610 (world_serialize.cpp:513)
    object* create_object_internal(unsigned int classname_hash, obj_prop_block& pblock, world_zone* origin_zone, unsigned int parent); // 0x0093E6B0 (world_serialize.cpp:539)
    void free_object_stub(object* stub); // 0x0093B600 (world_serialize.cpp:617)
    void free_object(object* objp); // 0x0093B630 (world_serialize.cpp:635)
    void free_all_dead_objects(); // 0x0093C720 (world_serialize.cpp:740)
    object* visit_stub(zone_object*& zobjp); // 0x0093B860 (world_serialize.cpp:872)
    void serialize_stub_list(int num_stubs, object** stub_list, zone_object** zobj_list); // 0x0093E8D0 (world_serialize.cpp:904)
    bool serialize_zone(world_zone* wz, zone_file* orig_zone, bool preload); // 0x0093EE40 (world_serialize.cpp:1205)
    bool should_restore_object_if_not_in_safe_file(zone_object* zobjp); // 0x0092DA10 (world_serialize.cpp:1157)
    bool should_restore_object_if_resetting_destruction(zone_object* zobjp); // 0x0092DAE0 (world_serialize.cpp:1180)
    // Reimplementation helper: the body both should_restore_object_* functions inline.
    static bool ask_class(zone_object* zobjp, bool (object::*question)(obj_prop_block&));
    void deserialize_object(world_zone* wz, object* objp, bool force_delete, bool do_persistent); // 0x0092DBB0 (world_serialize.cpp:1580)
    bool deserialize_zone(world_zone* wz); // 0x0092DE70 (world_serialize.cpp:1766)
    bool process_deserialization(); // 0x0093F7F0 (world_serialize.cpp:1784)
    bool process_deserialize_collect(world_zone* wz); // 0x0092DEF0 (world_serialize.cpp:1906)
    bool process_deserialize_compress(world_zone* wz); // 0x00923720 (world_serialize.cpp:2005)
    bool process_deserialize_close_zone(world_zone* wz); // 0x0091D140 (world_serialize.cpp:2097)
    bool process_deserialize_close(); // 0x0091D190 (world_serialize.cpp:2118)
    bool process_deserialization_level_shutdown(); // 0x00923920 (world_serialize.cpp:2131)
    bool save_state_start(); // 0x0093C770 (world_serialize.cpp:2171)
    bool save_state_finish(); // 0x0093DB80 (world_serialize.cpp:2442)
    static void zone_visit_cb(const char* filename, zone_file* zf); // 0x0093F9C0 (world_stream.cpp:213)
    bool init_territory_streaming_resources(zone_file* persistent_zf); // 0x0093F9F0 (world_stream.cpp:236)
    void shutdown_territory_streaming_resources(); // 0x009239A0 (world_stream.cpp:301)
    void visit_object(base_array<object*>& list, base_array<object*>& constraints, object* objp); // 0x0092E230 (world_stream.cpp:365)
    void collect_sorted_list(base_array<object*>& list, havok_bpi* island); // 0x0093D4D0 (world_stream.cpp:390)
    bool process_streaming_objects(bool hard_load); // 0x0093FB40 (world_stream.cpp:654)
    bool process_streaming_object_immediate(object* objp); // 0x0093D610 (world_stream.cpp:784)
    bool process_streaming_object(object* objp); // 0x0092E430 (world_stream.cpp:797)
    void stream_object(object* objp); // 0x0093E3D0 (world_stream.cpp:898)
    void destream_object(object* objp); // 0x0091D700 (world_stream.cpp:931)
    bool monitor_grid(); // 0x00935060 (world_stream.cpp:435)
    void monitor_leaving_zones(); // 0x0092E2E0 (world_stream.cpp:518)
    void monitor_entering_zones(); // 0x00940690 (world_stream.cpp:563)
    bool process_streaming_zone(world_zone* wz); // 0x00940550 (world_stream.cpp:957)
    bool stream_zone(world_zone* wz); // 0x0091D750 (world_stream.cpp:1027)
    void destream_zone(world_zone* wz); // 0x0091D790 (world_stream.cpp:1061)
    void stream_request(object* objp, unsigned int srid, stream2_priority priority); // 0x0091D7E0 (world_stream.cpp:1083)
    void get_hierarchy_bbox(vector& bmin, vector& bmax, object* objp); // 0x0091D810 (world_stream.cpp:1098)
    void stream_parent_notify(object* child); // 0x0093D650 (world_stream.cpp:1113)
    bool apply_stream_parent_notify(object* parent); // 0x0091D880 (world_stream.cpp:1125)
public:
    bool object_physically_in_original_zone(object* objp); // 0x0092CF70 (:2009)
    bool objects_original_zone_is_serialized(unsigned int handle); // 0x0092E650 (world_stream.cpp:1156)
    bool object_add_to_destroyed_table(unsigned int handle); // 0x0091C9B0 (:2026)
    bool object_was_destroyed(unsigned int handle); // 0x0092D000 (:2037)
    bool destroyed_object_table_save(cfile* save_filep); // 0x0092D010 (:2042)
    bool destroyed_object_table_load(cfile* save_filep); // 0x0091C9D0 (:2055)
    world_zone* get_world_zone_by_pos(const vector& pos) const; // 0x00923BA0 (world_stream.cpp:1174)
    bool world_zone_is_border_zone(const world_zone* wz) const; // 0x0091D940 (world_stream.cpp:1185)
    int get_obj_zones_by_bbox(const vector& bmin, const vector& bmax, obj_zone** zones, int max_zones); // 0x00923FF0 (world_stream.cpp:1276)
private:
    world_zone* get_world_zone_by_object_handle(unsigned int handle) const; // 0x00923C00 (world_stream.cpp:1191)
    world_zone* get_world_zone_by_grid_id(unsigned short id) const; // 0x0091D970 (world_stream.cpp:1215)
    world_zone* get_world_zone_by_index(int index) const; // 0x0091D950 (world_stream.cpp:1204)
    world_zone* get_world_zone_by_name(const char* name) const; // 0x0091D9A0 (world_stream.cpp:1222)
    int get_zones_by_bbox(const vector& _bmin, const vector& _bmax, world_zone** zones, int max_zones); // 0x00923C40 (world_stream.cpp:1235)
public:
    bool restore_object(unsigned int handle); // 0x00934CE0 (world_restore.cpp:371)
    bool restore_object_is_pending(unsigned int handle) const; // 0x0092D370 (world_restore.cpp:389)
    bool restore_object_is_immediate(unsigned int handle) const; // 0x0092D3B0 (world_restore.cpp:407)
    bool restore_is_pending() const; // 0x00923700 (world_restore.cpp:396)
    void restore_do_frame_pre(); // 0x0093DAB0 (world_restore.cpp:699)
    void restore_do_frame_post(); // 0x00940430 (world_restore.cpp:733)
    void restore_level_init(); // 0x009349F0 (world_restore.cpp:116)
    void restore_system_init(); // 0x009236A0 (world_restore.cpp:130)
    void restore_system_save(cfile* file); // 0x0092D110 (world_restore.cpp:139)
    void restore_system_load(cfile* file); // 0x00934AC0 (world_restore.cpp:181)
    bool restore_cache_in_table(data_hash_table<unsigned int,unsigned int,&uint_hash,7280,unsigned short>* table, zone_object* zobj, unsigned char* objects); // 0x00934C20 (world_restore.cpp:234)
    int restore_pending_pop_handle(unsigned int handle, hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int*& restore_list, bool is_persistent_zone); // 0x00934DE0 (world_restore.cpp:531)
    int restore_pending_pop(hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int* restore_list, unsigned short zone_gid, bool include_auto_restore); // 0x00934E30 (world_restore.cpp:555)
    bool restore_serialize_zone(world_zone* wz, zone_file* orig_zone); // 0x00940230 (world_restore.cpp:606)
    void restore_post_zone_serialize_cleanup(unsigned char* objects, unsigned short zone_gid); // 0x0093C4C0 (world_restore.cpp:1039)
    void process_pending_restorations();
    world(); // 0x0093BA50
    ~world(); // 0x0093A790

    // object and obj_zone call the private name, child and streaming helpers; the chunk object lookups read the zone
    // grid for their error message (inferred).
    friend class object;
    friend class obj_zone;
    friend class navpoint; // serialize clears the name, stream creates a node on the mesh (inferred)
    friend class cover_node; // serialize clears the name (inferred)
    friend const char* game_trace_object_name(unsigned int handle); // not in rfg.exe: the game trace (game_trace.h)
    friend class general_mover; // serialize and apply_damage use calc_bpo_flags and get_object_name (inferred)
    friend class player_meta_data; // set_current_district hashes the district name (get_object_name; inferred)
    friend class object_convoy_end_point; // serialize saves the home district's name (get_object_name; inferred)
    friend int chunk_get_general_object_index(const c_chunk_base* chunk, unsigned int uid, object* objp);
    friend rfg_base_destroyable_shape* chunk_get_destroyable_object(const c_chunk_base* chunk, unsigned int uid, object* objp);

    bool mission_object_creation_mode; // 0x00
    vector level_ambient; // 0x04
    vector level_back_ambient; // 0x10
    char last_loaded_territory[64]; // 0x1C
private:
    int max_world_objects; // 0x5C
    base_array<object*> all_objects; // 0x60
    base_array<unsigned short> type_objects[53]; // 0x70
    base_array<unsigned short> subtype_objects[11]; // 0x3C0
    float tech_level; // 0x470
    float tech_level_max; // 0x474
    volatile unsigned int thread_id; // 0x478
public:
    dynamic_uint_hash_table<object*> object_handle_hash; // 0x480
    hash_table<unsigned int,&hash_uint,1024,unsigned short> destroyed_object_table; // 0x4C8
    cfile* save_file; // 0x24D0
private:
    object* flagged_objects; // 0x24D4
    object* current_flagged_object; // 0x24D8
    unsigned char current_flagged_mode; // 0x24DC
    world_deserialize_state deserialize_state; // 0x24E0
    virtual_mempool* deserialize_pool; // 0x24E4
public:
    farray<world_zone*,32> deserialize_list; // 0x24E8
private:
    world_state_mode cur_world_state; // 0x2578
    world_state_buf stored_world_state[2]; // 0x257C
    zlib_stream* compress_stream; // 0x2654
    zlib_stream* decompress_stream; // 0x2658
public:
    save_load_info sl_flags; // 0x265C
    game_save_info* pending_game_save_slot; // 0x2690
    int dlc_bundle_id; // 0x2694
private:
    char pending_filename[64]; // 0x2698
    vector pending_game_load_warp_to_pos; // 0x26D8
    matrix pending_game_load_warp_to_orient; // 0x26E4
    volatile bool load_aborted; // 0x2708
public:
    stream_grid* grid; // 0x270C
    vector stream_pos; // 0x2710
private:
    int num_territory_zones; // 0x271C
    world_zone** all_zones; // 0x2720
    world_zone* global_zone_grid[257]; // 0x2724
    bool is_territory; // 0x2B28
    char territory_name[128]; // 0x2B29
    int num_streaming_objects; // 0x2BAC
    bool stub_serialization_in_progress; // 0x2BB0
};

extern world World; // 0x02F98490
extern bool Stream_pause; // 0x02CA1670
extern cs_wrapper Temp_destroy_cs; // 0x02FDE514

void world_set_debug_player_start(bool enabled, const vector& pos, const matrix& orient); // 0x0091C0A0 (:124)
void unused_dcf_set_player_start(); // 0x0091C110 (:130)
void world_system_init(); // 0x0093C0B0 (:2194)
bool world_level_exists(const char* filename, bool multiplayer_level); // 0x0091CC40 (:2209)
bool world_collect_descendants(const object* obj, base_array<object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data); // 0x0092D070 (:2252)
void unused_dcf_havok_bpo_counts(); // 0x0091CF70 (:2580)

// Collects the descendants of top_objp that are of type T (depth first through child_ptr/child_next), optionally
// filtered by cb_func, until obj_list is full; returns true when it is full.
template<typename T>
bool world_collect_descendants(const object* top_objp, base_array<const T*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data); // <navpoint> 0x006726F0 (.h:644)
bool world_collect_descendants(const object* top_objp, base_array<const object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data); // 0x00A05500 (.h:648)
template<typename T>
bool world_collect_descendants(const object* top_objp, base_array<T*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data); // <object_dummy> 0x00A4D680 (world.inl:298)

#include "volition/rfg/code/level/world.inl"
