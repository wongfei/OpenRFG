// Original: volition/rfg/code/gameplay/object_district.h
// Implementation: volition/rfg/code/gameplay/object_district.cpp
//
// district, a district of the map (OT_DISTRICT): name, color, control and morale, liberation state and the
// voice lines played on liberation. The territory data (t_district) is in level/territory_shared.h.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/varray.h"

struct cfile;
class matrix;
class vector;

// size 0x164
class district : public object
{
public:
    district(); // 0x0078EF00 (:118)
    static const object_type get_type_static(); // 0x00743B90 (.h:38)
    static const object_sub_type get_subtype_static(); // 0x00743BA0 (.h:38)
    static const char* get_classname_static(); // 0x00754380 (.h:38)
    static int get_bpo_index_static(); // 0x00754390 (.h:38)
    virtual const char* get_classname() const; // vtbl+0x0C 0x0078EF40 (.h:38)
    static unsigned int get_classname_hash(); // 0x0075ECA0 (:117)
    static int pool_remaining(); // 0x00774D90 (:117)
    virtual void pool_free(); // vtbl+0x80 0x0079E440 (:117)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x0078EF70 (:130)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x0075ECB0 (:180)
    float get_control() const; // 0x0075ECC0 (:190)
    void set_control(float control_value); // 0x007ABE40 (:195)
    void change_control(float control_delta); // 0x007ABED0 (:215)
    float get_morale() const; // 0x00774DA0 (:220)
    void set_morale(float morale_value); // 0x0075ECD0 (:234)
    void change_morale(float morale_delta); // 0x0075ED50 (:247)
    float get_control_max() const; // 0x0075EDF0 (:252)
    void set_control_max(float control_max_value); // 0x007ABF70 (:257)
    void change_control_max(float control_max_delta); // 0x007AC000 (:265)
    float get_morale_max() const; // 0x0075EE00 (:270)
    void set_morale_max(float morale_max_value); // 0x0075EE10 (:276)
    void change_morale_max(float morale_max_delta); // 0x0075EEB0 (:284)
    int get_minimum_edf_progression() const; // 0x0075F090 (:439)
    void set_minimum_edf_progression(int level); // 0x0075F0A0 (:444)
    bool is_liberated() const; // 0x005DDB70 (.h:88)
    void load_liberated_value(bool load_value); // 0x007543A0 (.h:89)
    bool needs_to_play_line() const; // 0x007543B0 (.h:90)
    bool play_radio_line(); // 0x0075EF60 (:289)
    void load_liberated_play_line_value(bool load_value); // 0x007543C0 (.h:92)
    bool liberate_district(); // 0x007A5CE0 (:304)
    void set_liberated_tech_level(float level); // 0x0075EFB0 (:365)
    void set_liberated_marauder_level(int level); // 0x0075EFD0 (:370)
    void set_liberated_edf_level(int level); // 0x0075EFE0 (:375)
    void set_liberated_radio_line(voice_line_handle radio_line); // 0x0075EFF0 (:380)
    void set_capstone_radio_lines(); // 0x0075F000 (:385)
    void play_capstone_radio_lines(); // 0x0079E480 (:400)
    bool set_gid_list(const farray<grid_info,21>& list_to_set); // 0x0078F140 (:460)
    const farray<grid_info,21>& get_gid_list(); // 0x0075F0B0 (:490)
    bool encompasses_gid(unsigned char grid_x, unsigned char grid_y) const; // 0x00774DD0 (:502)
    void gid_to_grid_index(unsigned char& grid_x, unsigned char& grid_y, unsigned short gid); // 0x0075F0C0 (:517)
    void set_grid_color(const color& col_to_set); // 0x0075F0E0 (:529)
    void get_grid_color(unsigned char& r, unsigned char& g, unsigned char& b) const; // 0x0075F0F0 (:540)
    void get_grid_color_f(float& r, float& g, float& b) const; // 0x0075F120 (:553)
    const rl_color_float& get_vfx_tint() const; // 0x00743BB0 (.h:109)
    void set_vfx_tint(const rl_color_float& vfx_tint); // 0x007543D0 (.h:110)
    void set_localized_name_hash(unsigned int hash); // 0x007543F0 (.h:113)
    unsigned int get_localized_name_hash() const; // 0x00754400 (.h:114)
    void set_district_index(unsigned char index); // 0x00754410 (.h:117)
    unsigned char get_district_index() const; // 0x0091B560 (.h:118)
    bool get_allow_ambient_cough() const; // 0x0075F190 (:561)
    bool get_allow_amb_edf_civ_dump() const; // 0x0075F1A0 (:566)
    bool get_disable_morale_change() const; // 0x0075F1B0 (:571)
    bool get_disable_control_change() const; // 0x0075F1C0 (:576)
    void set_loading_screen_images(const char** image_names, int num_images); // 0x0075F1D0 (:582)
    int get_loading_screen_images(const char** image_names); // 0x0075F210 (:595)
    virtual ~district(); // vtbl+0x00 0x0078EF50

private:
    float control; // 0x9C
    float morale; // 0xA0
    float control_max; // 0xA4
    float morale_max; // 0xA8
    float liberated_tech_level; // 0xAC
    int liberated_marauder_level; // 0xB0
    int liberated_edf_level; // 0xB4
    voice_line_handle liberated_radio_line_handle; // 0xB8
    voice_line_handle capstone_line_1; // 0xBC
    voice_line_handle capstone_line_2; // 0xC0
    int min_edf_progression_level; // 0xC4
    bool liberated; // 0xC8
    bool needs_to_play_radio_line; // 0xC9
    district_flags flags; // 0xCA
    color col; // 0xCB
    unsigned int localized_name_hash; // 0xD0
    farray<grid_info,21> grid_ids; // 0xD4
    unsigned char district_index; // 0x138
    rl_color_float m_vfx_tint; // 0x13C
    const char* load_screen_images[5]; // 0x14C
    unsigned char num_load_screen_images; // 0x160
};

// size 0xC: the property block a district is created from (world::create_object_global).
class object_district_props : public obj_prop_block
{
public:
    object_district_props(float control, float control_max, float morale, float morale_max, bool liberated, bool needs_to_play_radio_line, district_flags flags, const vector& _position, const matrix& _orient); // 0x00768F80 (.h:142)
    ~object_district_props(); // 0x0075F230
};

int bpo_index_district(); // 0x0075EC80 (:117)
object* alloc_district(); // 0x0078EEF0 (:117)
const char* classname_district(); // 0x0075EC90 (:117)
void types_district(signed char& type, signed char& subtype); // 0x0075EC70 (:117)
unsigned int get_district_by_hash(unsigned int district_hash); // 0x0078F200 (:604)
unsigned int get_district_by_name(const char* district_name); // 0x0078F2C0 (:618)
district* district_local_player(); // 0x0079E510 (:637)
unsigned int district_handle_local_player(); // 0x0079E540 (:646)
unsigned int district_handle_from_grid_location(int x, int y); // 0x00774E30 (:658)
unsigned int district_handle_from_pos(const vector& pos); // 0x00774E60 (:674)
unsigned int district_get_default(); // 0x0075F220 (:684)
district* district_get_from_pos(const vector& pos); // 0x0078F3C0 (:692)
bool district_play_howl_at_pos(const vector& pos); // 0x00774EB0 (:699)
bool district_marauder_zone_at_pos(const vector& pos); // 0x00774F70 (:740)
bool object_district_preload_default_district(); // 0x00775030 (:780)
bool object_district_release_default_district(); // 0x0075F240 (:805)
bool object_district_preload_districts(const char* territory_name); // 0x0078F660 (:861)
bool object_district_save_districts(cfile* save_file); // 0x0078F680 (:873)
bool object_district_load_districts(cfile* save_file); // 0x007AC0B0 (:898)
bool object_district_play_liberated_line(); // 0x0078F780 (:929)
bool object_district_radio_line_ready(); // 0x0078F810 (:939)
unsigned int object_district_num_liberated(); // 0x0078F860 (:946)

// 0x00754400 (.h:114)
inline unsigned int district::get_localized_name_hash() const
{
    return localized_name_hash;
}

// 0x0075F190 (:561)
inline bool district::get_allow_ambient_cough() const
{
    return flags.allow_cough;
}

// 0x0075F1A0 (:566)
inline bool district::get_allow_amb_edf_civ_dump() const
{
    return flags.allow_amb_edf_civilian_dump;
}
