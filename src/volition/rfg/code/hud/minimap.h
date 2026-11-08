// Original: volition/rfg/code/hud/minimap.h
// Implementation: volition/rfg/code/hud/minimap.cpp
//
// The minimap and map renderer (cartographer, the global Cartographer): map modes, the building and terrain layers,
// icons (snap to edge, groups, player markers), highlights, decals, district boundaries and liberation icons, building
// health, the cached bitmaps and their UVs, zoom and the render flags; render_mini draws into the HUD's vint map
// element. Placement of the data types inferred (all held by cartographer). The icon constructors' default arguments and
// the header-inline constructors come from the farray constructors that build the cartographer's arrays.
#pragma once

#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/varray.h"

struct bbox;
struct bm_image;
struct cfile;
class checksum_stri;
class district;
class gr_state;
class human;
class obj_zone;
class object;
class object_bftp_node;
class object_mover;
class object_restricted_area;
class player;
class projectile;
class rl_color;
class rl_material;
struct rl_material_data;
class vehicle;

extern color Default_map_item_color; // 0x016621A0 (the icons' default color, white)
extern color Completed_icon_color; // 0x016621A4 (the icons of finished activities, grey)

enum minimap_elements
{
    MAP_ELEMENT_INVALID = -1,
    MAP_ELEMENT_STAR_RED_ICON = 0,
    MAP_ELEMENT_STAR_BLUE_ICON = 1,
    MAP_ELEMENT_STAR_GREEN_ICON = 2,
    MAP_ELEMENT_STAR_WHITE_ICON = 3,
    MAP_ELEMENT_CIRCLE_BLUE_ICON = 4,
    MAP_ELEMENT_CIRCLE_BLUE_1_ICON = 5,
    MAP_ELEMENT_CIRCLE_BLUE_2_ICON = 6,
    MAP_ELEMENT_CIRCLE_BLUE_3_ICON = 7,
    MAP_ELEMENT_CIRCLE_BLUE_4_ICON = 8,
    MAP_ELEMENT_CIRCLE_VIOLET_1_ICON = 9,
    MAP_ELEMENT_CIRCLE_VIOLET_2_ICON = 10,
    MAP_ELEMENT_CIRCLE_VIOLET_3_ICON = 11,
    MAP_ELEMENT_CIRCLE_VIOLET_4_ICON = 12,
    MAP_ELEMENT_AMMO_BOX = 13,
    MAP_ELEMENT_CONVOY_ICON = 14,
    MAP_ELEMENT_CONVOY_CAPTURE_ICON = 15,
    MAP_ELEMENT_RIDING_SHOTGUN = 16,
    MAP_ELEMENT_AREA_DEFENSE = 17,
    MAP_ELEMENT_COURIER_ICON = 18,
    MAP_ELEMENT_RAID_ICON = 19,
    MAP_ELEMENT_RAID_ICON_ALWAYS_SHOW = 20,
    MAP_ELEMENT_DEF_RAID_ICON = 21,
    MAP_ELEMENT_BASE_ICON = 22,
    MAP_ELEMENT_HOUSE_ICON = 23,
    MAP_ELEMENT_HOUSE_ICON_ALWAYS_SHOW = 24,
    MAP_ELEMENT_DEMOLITIONS_MASTER = 25,
    MAP_ELEMENT_GUERRILLA_EXPRESS = 26,
    MAP_ELEMENT_GENERIC_ACTIVITY = 27,
    MAP_ELEMENT_ARROW_ICON = 28,
    MAP_ELEMENT_CIRCLE_FILLED_OUTLINE = 29,
    MAP_ELEMENT_CONVOY_END_POINT = 30,
    MAP_ELEMENT_GUERILLA_EXP_END_POINT = 31,
    MAP_ELEMENT_MISSION_START = 32,
    MAP_ELEMENT_TARGET = 33,
    MAP_ELEMENT_TARGET_PRIORITY_1 = 34,
    MAP_ELEMENT_TARGET_PRIORITY_2 = 35,
    MAP_ELEMENT_TARGET_PRIORITY_3 = 36,
    MAP_ELEMENT_TARGET_PRIORITY_4 = 37,
    MAP_ELEMENT_SAFE_HOUSE = 38,
    MAP_ELEMENT_DIRECTIONAL_ARROW = 39,
    MAP_ELEMENT_MISSION_OBJECTIVE = 40,
    MAP_ELEMENT_MISSION_OBJECTIVE_UP = 41,
    MAP_ELEMENT_MISSION_OBJECTIVE_DOWN = 42,
    MAP_ELEMENT_TANK_ICON = 43,
    MAP_ELEMENT_FLAG_ICON = 44,
    MAP_ELEMENT_KOTH_GUERRILLA = 45,
    MAP_ELEMENT_KOTH_EDF = 46,
    MAP_ELEMENT_KOTH_NEUTRAL = 47,
    MAP_ELEMENT_CARBOMB = 48,
    MAP_ELEMENT_ICON_COUNT = 49,
    MAP_ELEMENT_BLIP = 49,
    MAP_ELEMENT_BLIP_HUMAN = 50,
    MAP_ELEMENT_PLAYER = 51,
    MAP_ELEMENT_STRUCTURE_EDF = 52,
    MAP_ELEMENT_VEHICLE = 53,
    MAP_ELEMENT_IMPORTANCE_MEDIUM = 54,
    MAP_ELEMENT_IMPORTANCE_HIGH = 55,
    MAP_ELEMENT_NORTH = 56,
    MAP_ELEMENT_RING_ICON = 57,
    MAP_ELEMENT_CAMERA_CONE = 58,
    NUM_MAP_ELEMENTS = 59,
};

enum map_mode_type
{
    MAP_MODE_MINI = 0,
    MAP_MODE_FULL = 1,
    NUM_MAP_MODES = 2,
};

enum icon_item_type
{
    IIT_ICON = 0,
    IIT_OBJ_ICON = 1,
    IIT_TEMP_ICON = 2,
    IIT_STICKY = 3,
    IIT_BLDG = 4,
    IIT_PLAYER_MARKER = 5,
    IIT_PLAYER = 6,
    IIT_LAST = 7,
};

enum rot_uvs
{
    RUVS_0 = 0,
    RUVS_90 = 1,
    RUVS_180 = 2,
    RUVS_270 = 3,
    RUVS_FLIP_LR = 4,
    RUVS_FLIP_UD = 5,
};

// size 0x18
struct map_icon
{
    bool operator==(const map_icon& test_icon); // 0x0083FD50 (.h:219)
    map_icon(unsigned int in_obj = 0xFFFFFFFF, int in_bitmap = -1, vector2 pos = ZERO_VECTOR2); // 0x0084C700 (:1123)

    unsigned int handle; // 0x00
    int bitmap; // 0x04
    timestamp_realtime time_added; // 0x08
    bool only_show_closest; // 0x0C
    bool sticky; // 0x0D
    vector2 render_pos; // 0x10
};

// size 0x34
struct map_icon_temp
{
    bool operator==(const map_icon_temp& test_icon); // 0x0084CAB0 (:1471)
    map_icon_temp(const vector& in_pos, unsigned int in_activity_handle); // 0x0083FD60 (.h:262)
    map_icon_temp(const vector& in_pos = ZERO_VECTOR, const vector2& in_bmin = ZERO_VECTOR2, const vector2& in_bmax = ZERO_VECTOR2, int in_bitmap = -1, const color& in_color = Default_map_item_color, bool in_stick_to_edge = true, unsigned int in_handle = 0xFFFFFFFF, bool in_blink = false, unsigned int in_activity_handle = 0xFFFFFFFF) // 0x00850BB0 (.h:256)
    {
        pos.x = in_pos.x;
        pos.y = in_pos.z;
        bmin = in_bmin;
        bmax = in_bmax;
        bitmap_color = in_color;
        bitmap = in_bitmap;
        stick = in_stick_to_edge;
        blink = in_blink;
        handle = in_handle;
        activity_handle = in_activity_handle;
    }

    vector2 pos; // 0x00
    vector2 bmin; // 0x08
    vector2 bmax; // 0x10
    color bitmap_color; // 0x18
    int bitmap; // 0x1C
    bool stick; // 0x20
    bool blink; // 0x21
    unsigned int handle; // 0x24
    vector2 render_pos; // 0x28
    unsigned int activity_handle; // 0x30
};

// size 0x40
struct highlight_item
{
    void clear(); // 0x0084CC20 (:1534)
    void set(highlight_item& item); // 0x0084CC70 (:1548)

    bool m_valid; // 0x00
    unsigned int m_obj_handle; // 0x04
    unsigned int m_act_handle; // 0x08
    int m_icon_index; // 0x0C
    icon_item_type m_type; // 0x10
    vector m_obj_pos; // 0x14
    vector m_fake_pos; // 0x20
    vector2 m_render_pos; // 0x2C
    float m_dsqr; // 0x34
    float m_angle_rad; // 0x38
    color m_col; // 0x3C
};

// size 0x40
struct map_decal
{
    map_decal(const vector& in_pos = ZERO_VECTOR, const matrix& in_orient = IDENTITY_MATRIX, int in_bitmap = -1, float scale_x = 1.0f, float scale_y = 1.0f, const color& in_color = Default_map_item_color, bool in_apply_orient_to_icon = false, bool in_inverted = false) // 0x00850B40 (.h:207)
        : orient(in_orient)
    {
        pos.x = in_pos.x;
        pos.y = in_pos.z;
        bitmap = in_bitmap;
        bitmap_color = in_color;
        scale.x = scale_x;
        scale.y = scale_y;
        uses_orient = in_apply_orient_to_icon;
        inverted = in_inverted;
    }

    vector2 pos; // 0x00
    matrix orient; // 0x08
    int bitmap; // 0x2C
    color bitmap_color; // 0x30
    vector2 scale; // 0x34
    bool uses_orient; // 0x3C
    bool inverted; // 0x3D
};

// size 0x1C
struct map_bldg_icon
{
    map_bldg_icon(const vector& in_pos = ZERO_VECTOR, int in_bitmap = -1, float in_health = 0.0f, unsigned int in_handle = 0xFFFFFFFF) // 0x0083FD80 (.h:279)
    {
        pos.x = in_pos.x;
        pos.y = in_pos.z;
        bitmap = in_bitmap;
        health = in_health;
        handle = in_handle;
    }

    vector2 pos; // 0x00
    int bitmap; // 0x08
    float health; // 0x0C
    unsigned int handle; // 0x10
    vector2 render_pos; // 0x14
};

// size 0x10
struct bldg_health_info
{
    bldg_health_info(unsigned int handle, float health_pct); // 0x0083FDE0 (.h:339)
    bldg_health_info() // 0x0083FDB0 (.h:337)
    {
        m_valid = false;
        m_mover_handle = 0;
        m_last_health_pct = 1.0f;
        m_time_since_damaged = -1.0f;
    }

    bool m_valid; // 0x00
    unsigned int m_mover_handle; // 0x04
    float m_last_health_pct; // 0x08
    float m_time_since_damaged; // 0x0C
};

// size 0x20
struct bmp_uv_information
{

    vector2 uvs[4]; // 0x00
};

// size 0x8
struct map_image_info
{
    const bm_image* image_data; // 0x00
    int bitmap_handle; // 0x04
};

// size 0x28
struct cached_bmp_info
{

    map_image_info mii; // 0x00
    bmp_uv_information bui; // 0x08
};

// size 0x24
struct cached_struct_info
{

    int obj_handle; // 0x00
    vector2 window_pos[4]; // 0x04
};

// size 0x8
struct convert_guerrilla_icon
{

    unsigned int m_human_handle; // 0x00
    timestamp m_timer; // 0x04
};

// size 0x140
struct district_boundries
{
    unsigned char num_tris; // 0x00
    unsigned char num_indices; // 0x01
    unsigned char num_verts; // 0x02
    unsigned char x[48]; // 0x03
    unsigned char z[48]; // 0x33
    unsigned char zone_xz[48]; // 0x63
    unsigned char indices[141]; // 0x93
    char district_name[32]; // 0x120
};

// size 0x3
struct district_liberated_icon
{
    unsigned char x; // 0x00
    unsigned char z; // 0x01
    unsigned char zone_xz; // 0x02
};

// size 0x34
struct map_size_info
{

    vector2 map_posn; // 0x00
    vector2 map_size; // 0x08
    vector2 map_end_posn; // 0x10
    vector center_pos_world; // 0x18
    float zoom_radius; // 0x24
    float zoom_radius_squared; // 0x28
    vector2 map_scale; // 0x2C
};

// size 0x2C
struct object_info_terrain_bbox
{

    unsigned int handle; // 0x00
    vector2 corners[4]; // 0x04
    unsigned int mat_handle; // 0x24
    int bm_handle; // 0x28
};

// size 0xA08
struct structure_buff
{
    unsigned int movers[512]; // 0x00
    unsigned char flags[512]; // 0x800
    int num_movers; // 0xA00
    bool edf_buildings_used; // 0xA04
    bool non_edf_buildings_used; // 0xA05
};

// size 0x10
struct window_coord_info
{
    window_coord_info() {} // 0x00840D30

    vector2 actual_posn; // 0x00
    float x_dot_product; // 0x08
    float y_dot_product; // 0x0C
};

// size 0x16EE4
class cartographer
{
public:
    cartographer(); // 0x00877470 (:1573)
    void shutdown(); // 0x0085BCF0 (:1580)
    void level_init_pre(); // 0x0087FF50 (:1587)
    void render(); // 0x00882D00 (:1697)
    void render_pre_fog(); // 0x0086CAD0 (:1762)
    void handshake(); // 0x00877710 (:1793)
    void process(); // 0x0087CE30 (:1817)
    int get_bitmap_id(minimap_elements);
    vector get_player_position(); // 0x0086CBA0 (:2001)
    vector get_center_pos_world(); // 0x0084CDF0 (:2025)
    matrix get_camera_orientation(bool is_full); // 0x0084CE20 (:2031)
    matrix get_player_orientation(bool is_full); // 0x0086CC20 (:2067)
    void set_target(unsigned int in_target_handle); // 0x0084CE40 (:2101)
    player* get_target_player() const; // 0x0085BD50 (:2107)
    map_mode_type get_map_mode() const { return m_map_mode; } // 0x0083FE10 (.h:388)
    void set_map_mode(map_mode_type type); // 0x0085BDD0 (:2128)
    void set_full_map_size_info(const map_size_info& info); // 0x0084CEE0 (:2161)
    bool does_icon_exist(unsigned int obj) const; // 0x00862DE0 (:8105)
    bool does_sticky_icon_exist(unsigned int obj) const; // 0x00862E50 (:8111)
    void remove_icon(unsigned int obj); // 0x00874380 (:8098)
    void add_icon(unsigned int h_obj, minimap_elements icon_bitmap, bool snap_to_edge, bool show_closest_group, bool player_marker); // 0x00879AD0 (:8053)
    void add_temporary_icon(const vector& pos, minimap_elements icon_bitmap, bool stick_to_edge, unsigned int in_handle, const color& in_color, bool blink, unsigned int in_activity_handle); // 0x00873FD0 (:7970)
    void add_temporary_icon(const vector& pos, minimap_elements icon_bitmap, bool stick_to_edge, const vector2& in_bmin, const vector2& in_bmax, unsigned int in_handle, const color& in_color, bool blink); // 0x008741B0 (:8040)
    void add_decal(const vector& pos, const matrix& orient, minimap_elements decal_bitmap, float scale_x, float scale_y, const color& in_color, bool inverted); // 0x00873E70 (:7945)
    void add_decal(const vector& pos, minimap_elements decal_bitmap, float radius, const color& in_color, bool inverted); // 0x00873F80 (:7960)
    void add_district_liberated_icon(unsigned char x, unsigned char y, unsigned char zone_xz); // 0x00873E00 (:7929)
    void remove_district_liberated_icons(); // 0x00862DD0 (:7939)
    void add_converted_guerrilla(unsigned int obj_h); // 0x00874320 (:8087)
    void add_patrol_path(unsigned int obj, bool edf_patrol); // 0x00881350 (:5158)
    void remove_patrol_path(unsigned int obj); // 0x00881680 (:5217)
    void remove_all_paths(); // 0x008802D0 (:5238)
    float get_zoom_radius() const; // 0x0083FE40 (.h:762)
    void render_bitmap_alt(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state); // 0x0085ED60 (:3217)
    void render_bitmap_alt_3d(int bitmap_id, const vector& pt, const matrix& orient, float radius, const gr_state& state); // 0x0085EF30 (:3235)
    const highlight_item* get_highlight_item(); // 0x0083FE60 (.h:773)
    void render_single_icon(const vector& pos, float icon_size, int icon_index, rl_primitive_state& rl_state); // 0x00861D70 (:6390)
    void render_single_icon(const vector& pos, const vector2& in_bmin, const vector2& in_bmax, int icon_index, rl_primitive_state& rl_state); // 0x00861C80 (:6376)
    void get_mission_handles(farray<int,64>& handles); // 0x00871A60 (:6434)
    bool has_player_marker(); // 0x0084D000 (:6457)
    bool has_mission_marker(); // 0x0084D100 (:6471)
    bool replace_marker_on_add(); // 0x0083FE20 (.h:426)
    void add_player_marker(const vector& pos, bool mission, unsigned int act_handle, bool player_placed, bool replace_on_add); // 0x0087DFB0 (:6477)
    void remove_player_marker(bool mission); // 0x0087E410 (:6531)
    bool player_marker_is_highlight_item(); // 0x0084D080 (:6466)
    void object_set_visible_over_fow(unsigned int handle, bool visible); // 0x008743F0 (:8181)
    void minimap_save(cfile* file); // 0x00874430 (:8197)
    void minimap_load(cfile* file); // 0x0084DD70 (:8212)
    int get_structure_importance_icon(const object_mover* mp); // 0x00862D30 (:7159)
    void get_window_coordinate_info(window_coord_info& wci, const vector& world_posn); // 0x0084D180 (:6759)
    void get_world_posn(const vector2& window_posn, vector& world_posn); // 0x0084D290 (:6833)
    const district* get_district_from_point(const vector& pnt); // 0x00879BD0 (:8541)
    const district* get_district_from_point_2d(const vector2& pnt); // 0x008745D0 (:8551)
    void load_district_boundries(cfile* cf); // 0x00874850 (:8587)
    district_boundries* get_district_boundry(int index); // 0x00863900 (:8600)
    int get_num_district_boundries(); // 0x00863930 (:8609)
    void get_district_boundry_point(int district, int index, vector& pnt); // 0x00863940 (:8618)
    void enable_mini_static(unsigned int obj, float radius); // 0x0084E3E0 (:8744)
    void collect_structures(); // 0x00872E80 (:7363)
    bool point_on_map(const vector& point, float radius_scalar); // 0x0084CD30 (:1994)
    void set_first_gps_point(const vector& pos); // 0x0084E460 (:8908)
    void set_gps_to_spawned_activity(); // 0x0087E9F0 (:9168)
    unsigned int get_gps_target_activity_handle(); // 0x0084E510 (:9185)
    bool can_warp_to_safehouse(); // 0x0084E590 (:9190)
    int get_num_safehouse(); // 0x0083FE30 (.h:461)
    unsigned int get_nearest_safehouse(); // 0x00755050 (.h:462)
    bool get_activity_pos(vector2& pos); // 0x0084E5D0 (:9221)
    bool get_activity_rewards(float& control, float& morale, int& salvage); // 0x00864B00 (:9232)
    void show_important_bldgs(bool show); // 0x0084E600 (:9389)
    bool in_fullscreen_map(); // 0x0084E610 (:9394)
    void reset_structure_collect_timer(); // 0x0084D810 (:7408)
    const map_size_info& get_current_size_info() const { return m_current_size_info; } // inlined in rfg.exe
    void collect_terrain(); // 0x00872D20 (:7324), fullscreen_map_handshake calls it
private:
    void get_image_info(map_image_info& mii, bmp_uv_information& bui, int bmp_lookup); // 0x0084CF80 (:2441)
    void set_current_map_size_info(const map_size_info& info); // 0x0084CE50 (:2147)
    void determine_onscreen_position(); // 0x0084D3C0 (:7194)
    void determine_map_information_mini(); // 0x00872980 (:7247)
    void determine_map_information_full(); // 0x0084D580 (:7295)
    void update_gps(); // 0x008739B0 (:7597)
    vector2 gps_look_ahead(); // 0x0084C420 (:471)
    void draw_gps_prototypes(); // 0x0086A400 (:592)
    void draw_compass(vector2& target); // 0x00869DF0 (:519)
    void collect_humans(); // 0x0086D140 (:3576)
    void collect_bftp_nodes(); // 0x0086D450 (:3677)
    void collect_vehicles(); // 0x0086D590 (:3709)
    void collect_restricted_areas(); // 0x00872FC0 (:7415)
    void collect_convoys(); // 0x00873230 (:7452)
    void collect_projectiles(); // 0x0086D000 (:3557)
    void process_full();
    void render_full(); // 0x00881A10 (:2189)
    void render_mini(); // 0x00882130 (:2284)
    void render_alpha_mask(); // 0x00862170 (:6848)
    void render_overlay_map(); // 0x00862280 (:6884)
    void render_terrain(); // 0x0085F7A0 (:3476)
    void render_humans(float zoom_scale); // 0x0086D710 (:3809)
    void render_projectiles(float zoom_scale); // 0x0085FCC0 (:3743)
    void render_blast_from_the_past_tags(float zoom_scale); // 0x00860310 (:3970)
    void render_vehicles(float zoom_scale); // 0x0086DED0 (:4021)
    void render_restricted_areas(); // 0x00860560 (:4103)
    void sort_structures(structure_buff* structures); // 0x00860B10 (:4181)
    void render_structures(float zoom_scale); // 0x0087D2E0 (:4254)
    void render_structures_full(float zoom_scale); // 0x0087D960 (:4440)
    void render_bldg_icons(float zoom_scale, bool is_full); // 0x0086F4D0 (:4772)
    void render_masked_structures(object_mover** edf_array, int num_edf, object_mover** non_edf_array, int num_non_edf); // 0x0086E2A0 (:4483)
    void render_structures_post(); // 0x0086FF50 (:4863)
    void render_path_filler_pts(const vector2& curr_pt, const vector2& prev_pt, const vector& bmin, const vector& bmax, bmp_uv_information* bui, map_image_info* mii, bool is_full); // 0x00861100 (:5277)
    void render_path_route(const farray<vector,384>& path_pt_list, bool is_full, bool dots, float zoom_scale, int last_line_index, bool edf_route, bool convoy_route); // 0x00861410 (:5329)
    void render_convoys(bool is_full); // 0x0084CFD0 (:5096)
    void render_convoy_routes(float zoom_scale, bool is_full); // 0x00870940 (:5479)
    void render_patrol_routes(float zoom_scale, bool is_full); // 0x00881700 (:5503)
    void render_decals(bool is_full); // 0x00870A20 (:5533)
    void render_temp_icons(float zoom_scale, bool is_full); // 0x00870F80 (:5662)
    void render_icons(float zoom_scale, bool is_full); // 0x00877C20 (:5827)
    void render_sticky_icons(float zoom_scale, bool is_full); // 0x00878180 (:5953)
    void render_player(bool is_full); // 0x008723A0 (:6669)
    void render_player_marker(float zoom_scale, bool is_full); // 0x00871B30 (:6591)
    void render_scan_line(); // 0x00863A60 (:8636)
    void render_static_zones(); // 0x00860D10 (:4992)
    void render_district_liberated_icons(float zoom_scale); // 0x0087DB60 (:4952)
    void render_single_icon_direct(const vector2& pos, float icon_size, int icon_index, int alpha, rl_primitive_state& rl_state); // 0x00861F40 (:6419)
    void render_single_icon_direct(const vector2& pos, const vector2& in_bmin, const vector2& in_bmax, int icon_index, int alpha, rl_primitive_state& rl_state); // 0x00861E50 (:6405)
    void render_north(); // 0x00862020 (:6560)
    void get_path_screen_pts(farray<vector,384>& csp, const path_info& path_data, bool is_full, bool convoy); // 0x008705C0 (:5102)
    void render_bitmap_direct(bmp_uv_information* bui, const vector2& screen_pos, const vector& bmin, const vector& bmax, const map_image_info* mii, unsigned char alpha); // 0x0085BE20 (:2454)
    void render_bitmap_direct_oriented(bmp_uv_information* bui, const vector2& screen_pos, float rotation_rad, const vector& bmin, const vector& bmax, const map_image_info* mii, unsigned char alpha); // 0x0085C0C0 (:2527)
    void render_bitmap(bmp_uv_information* bui, const vector& world_pos, const vector& bmin, const vector& bmax, const map_image_info* mii); // 0x0085C400 (:2606)
    void render_bitmap_oriented(bmp_uv_information* bui, const vector& world_pos, const matrix& mat, const vector& bmin, const vector& bmax, const map_image_info* mii, rot_uvs uv_flag, bool ignore_scale); // 0x0085CEA0 (:2818)
    void render_bitmap_oriented(bmp_uv_information* bui, const vector& bmin, const vector& bmax, const map_image_info* mii, rot_uvs uv_flag, bool ignore_scale); // 0x0085C7B0 (:2680)
    void render_bitmap_via_box(bmp_uv_information* bui, const vector& world_pos, const vector2& top_left, const vector2& top_right, const vector2& bottom_left, const vector2& bottom_right, const map_image_info* mii); // 0x0085E700 (:3156)
    void render_tiled_bitmap_oriented(const vector& world_pos, const matrix& orient, const vector& bmin, const vector& bmax, unsigned int src_w, unsigned int src_h); // 0x0085DE90 (:3014)
    void render_tiled_bitmap_oriented(const vector& bmin, const vector& bmax, unsigned int src_w, unsigned int src_h); // 0x0085D7E0 (:2959)
    void render_guerrilla_converts(float zoom_scale); // 0x00860020 (:3918)
    color get_human_color(const human* hp, human_team_dispositions disposition); // 0x00862400 (:6927)
    color get_vehicle_color(const vehicle* vp); // 0x00862680 (:7005)
    rl_color get_structure_color(const object_mover* mp); // 0x00862850 (:7048)
    void set_up_important_bldgs(const farray<unsigned int,512>& med, const farray<unsigned int,512>& high, bool is_full, float zoom_scale); // 0x00877750 (:4665)
    bool pos_is_in_range_radius_squared(const vector pos, float* out_dist_sq); // 0x0084D820 (:7706)
    bool object_is_in_range_radius_squared(const object* obj); // 0x0084D8A0 (:7726)
    int find_terrain(unsigned int handle); // 0x0084D920 (:7740)
    bool add_terrain(const obj_zone* zone); // 0x0084D950 (:7755)
    bool remove_terrain(int which); // 0x0084DA80 (:7817)
    int find_restricted(unsigned int handle); // 0x0084DB00 (:7848)
    bool add_restricted(const object_restricted_area* area); // 0x0084DB30 (:7863)
    bool remove_restricted(int which); // 0x0084DBA0 (:7890)
    void clear_invalid_icons(); // 0x00879A20 (:7916)
    void render_control_morale(); // 0x0084CFC0 (:3389)
    float get_avg_vehicle_speed(); // 0x0086CCD0 (:3394)
    void determine_nearest_safehouse(); // 0x0086CE50 (:3439)
    bool is_old_highlight_item(const vector& pos, unsigned int handle, int icon_index, icon_item_type type); // 0x0084DBF0 (:8117)
    bool check_highlight_for_item(const vector& pos, const vector2& render_pos, unsigned int handle, int icon_index, icon_item_type type, float angle_rad, color col, unsigned int act_handle); // 0x00862EC0 (:8132)
    bool icon_is_visible_over_fow(int icon_index, bool is_full, bool is_sticky, const vector& pos); // 0x00863130 (:8171)
    bool get_best_placement_info_for_string(bbox& box, const vector& world_pos, float str_width, float str_height, float icon_radius); // 0x00863240 (:8271)
    void clear_important_bldg_health_flags(); // 0x00863710 (:8351)
    void add_important_bldg(unsigned int handle, float health_pct); // 0x008744C0 (:8382)
    float get_important_bldg_alpha(int index); // 0x00863880 (:8448)
    float get_important_bldg_alpha(unsigned int handle); // 0x008637E0 (:8414)
    void update_bldg_health_flash_alpha(); // 0x0084DE40 (:8479)
    vector2 get_point_along_overlay(float heading_deg); // 0x0084DEE0 (:8496)
    void change_minimap_zoom(); // 0x0084E210 (:8707)
    bool create_materials(rl_material** edf_mat, rl_material** non_edf_mat); // 0x0087E5A0 (:8750)
    void release_materials(); // 0x0084E400 (:8892)
    int get_structure_color_index(const object_mover* mp, float& flash_percent); // 0x00862B00 (:7104)
    void set_structure_mask_constants(rl_material_data* edf_mat_data, rl_material_data* non_edf_mat_data); // 0x00874910 (:8873)
    bool is_player_marker_item(const unsigned int handle, const vector& pos); // 0x0084E480 (:9163)
    void repulse_icons(const vector2& in_pos, const vector2& in_bmin, const vector2& in_bmax, float zoom_scale); // 0x00863C10 (:9018)
    void determine_render_positions(float zoom_scale, bool is_full); // 0x00874A30 (:8913)
    void process_patrol_routes(); // 0x00870880 (:5228)
    void handle_zoom_in_sounds(bool zooming_in); // 0x0084E150 (:8669)
    void handle_zoom_out_sounds(bool zooming_out); // 0x0084E1B0 (:8688)
    // Blocks rfg.exe's compiler repeats at each use, kept as helpers.
    vector2 world_to_window(float x, float z) const;
    void render_gps_route(float icon_scale, bool is_full);
    void render_masked_mover(object_mover* mp, int tex_w, int tex_h, bool use_own_orient);
    void clear_player_marker();
    bool sticky_icon_pulses(const map_icon& icon, unsigned int icon_bitmap, const object* objp) const;
    void fill_mini_quad(struct rl_primitive_2d_vert* verts, unsigned char value, const vector2* uvs);
public:

    // Public: fullscreen_map.cpp, one unity with minimap.cpp in rfg.exe, reads and writes these directly.
    hash_table<unsigned int,&hash_uint,255,unsigned char> visible_over_fow_list; // 0x00
    bool m_determine_onscreen_posn; // 0x600
    bool m_has_player_marker; // 0x601
    bool m_valid_gps_path; // 0x602
    bool m_set_gps_marker_to_activity; // 0x603
    map_mode_type m_map_mode; // 0x604
    rl_primitive_state m_rl_render_state; // 0x608
    map_size_info m_mini_size_info; // 0x60C
    map_size_info m_full_size_info; // 0x640
    map_size_info m_current_size_info; // 0x674
    vector m_map_corner_posns[4]; // 0x6A8
    vector m_ne_to_nw_vector; // 0x6D8
    vector m_sw_to_nw_vector; // 0x6E4
    unsigned int m_target_handle; // 0x6F0
    unsigned int m_nearest_safehouse; // 0x6F4
    int m_num_safehouse; // 0x6F8
    unsigned int m_static_target; // 0x6FC
    float m_static_radius; // 0x700
    structure_buff m_collected_structures[1]; // 0x704
    int m_collected_structure_buf_index; // 0x110C
    timestamp m_collected_structures_timer[1]; // 0x1110
    path_info m_gps_path; // 0x1114
    bool m_replace_gps_on_add; // 0x3B38
    path_info m_convoy_route; // 0x3B3C
    bool m_mission_gps; // 0x6560
    vector m_mission_gps_pos; // 0x6564
    int m_counts[3]; // 0x6570
    object_info_terrain_bbox m_terrain[4]; // 0x657C
    unsigned int m_restricted_areas[32]; // 0x662C
    farray<human const*,32> m_human_list; // 0x66AC
    farray<object_bftp_node const*,4> m_bftp_nodes; // 0x673C
    farray<vehicle const*,32> m_vehicle_list; // 0x675C
    farray<path_info,4> m_patrol_routes; // 0x67EC
    farray<projectile const*,32> m_projectile_list; // 0x1108C
    farray<convert_guerrilla_icon,5> m_guerrilla_converts; // 0x1111C
    timestamp m_guerrilla_convert_delay; // 0x11154
    float m_convoy_index; // 0x11158
    float m_gps_timer; // 0x1115C
    float m_gps_index; // 0x11160
    farray<map_decal,32> m_decals; // 0x11164
    farray<map_icon_temp,128> m_temp_icons; // 0x11974
    farray<map_icon,48> m_sticky_icons; // 0x13384
    farray<map_icon,32> m_icons; // 0x13814
    farray<map_bldg_icon,112> m_bldg_icons; // 0x13B24
    farray<bldg_health_info,112> m_important_bldgs; // 0x14774
    float m_bldg_health_flash_alpha; // 0x14E84
    highlight_item m_highlight; // 0x14E88
    highlight_item m_last_highlight; // 0x14EC8
    highlight_item m_player_marker_item; // 0x14F08
    farray<float,64> m_vehicle_speed; // 0x14F48
    float m_vehicle_sums; // 0x15058
    int m_vehicle_posn; // 0x1505C
    farray<cached_bmp_info,59> m_bmp_info; // 0x15060
    float m_objective_selected_time; // 0x159A8
    float m_scan_line_pos; // 0x159AC
    cached_struct_info m_cached_struct_info[4]; // 0x159B0
    int m_num_cached; // 0x15A40
    vector m_player_marker_pos; // 0x15A44
    vector m_alternate_marker_pos; // 0x15A50
    vector m_first_gps_point; // 0x15A5C
    farray<district_boundries,16> fsm_district_boundries; // 0x15A68
    farray<district_liberated_icon,16> fsm_district_liberated_icons; // 0x16E78
    int m_bftp_timer; // 0x16EB8
    float m_current_min_zoom; // 0x16EBC
    bool m_mats_created; // 0x16EC0
    unsigned int m_edf_bldg_mat_handle; // 0x16EC4
    rl_material* m_edf_bldg_mat; // 0x16EC8
    rl_material_data* m_edf_mat_data; // 0x16ECC
    unsigned int m_non_edf_bldg_mat_handle; // 0x16ED0
    rl_material* m_non_edf_bldg_mat; // 0x16ED4
    rl_material_data* m_non_edf_mat_data; // 0x16ED8
    int zoom_in_alr_handle; // 0x16EDC
    int zoom_out_alr_handle; // 0x16EE0
};

extern cartographer Cartographer; // 0x02BB71F8
extern bool Minimap_render_humans; // 0x02B9DD4B
extern bool Minimap_render_restricted; // 0x02B9DD54
extern bool Minimap_render_old_mode; // 0x02B9DD55
extern bool Minimap_show_gps_lookahead; // 0x02B9DD56
extern bool Minimap_show_compass; // 0x02B9DD57
extern bool Minimap_rot0; // 0x02B9DD5A
extern bool Minimap_scissor_enabled; // 0x02B9DD5B
extern bool Rl_clamp_minimap; // 0x01661260
extern float Player_scale; // 0x01661268

void minimap_gr_convert_to_rl_state(rl_primitive_state& rls, const gr_state& grs); // 0x0084C310 (:372)
void get_uv_information_bmpsheet(const bm_image* image_data, const int bmp_handle, vector2& tl, vector2& tr, vector2& bl, vector2& br); // 0x0084C860 (:1242)
bool set_icon_zoomed_alpha(minimap_elements icon_bitmap, bool is_full, int& new_alpha, int base_alpha); // 0x0085BC00 (:1454)
int minimap_get_element_bitmap_id(minimap_elements element); // 0x00863180 (:8237)
void minimap_render_bitmap(int bitmap_id, float x, float y, float angle, float scalew, float scaleh, const gr_state& state); // 0x008631B0 (:8244)
void minimap_render_bitmap_3d(int bitmap_id, const vector& pnt, const matrix& orient, float radius, const gr_state& state); // 0x00863210 (:8250)
void minimap_start_fullscreen_map(const checksum_stri& tool_tip_name_crc, bool zoom_out_on_start, bool from_script); // 0x0084DDC0 (:8256)
