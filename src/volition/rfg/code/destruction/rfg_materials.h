// Original: volition/rfg/code/destruction/rfg_materials.h
// Implementation: volition/rfg/code/destruction/rfg_materials.cpp
//
// Destruction materials, read from the material tables by rfg_materials_system_init. A physical material (density,
// brittleness, restitution, friction, link strength) points to an effect material, which holds per material event
// type (impact, scrape, break, bullet, melee, ...) an event candidate: the effect handles by shape type and event
// size (and by the other material, melee group or bullet group). Bullet groups also carry decal infos.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/memory/vmemory.h"

class object;
class vector;

enum material_shape_type
{
    INVALID_MATERIAL_EVENT_SHAPE_TYPE = -1,
    MATERIAL_EVENT_SHAPE_SOLID = 0,
    MATERIAL_EVENT_SHAPE_SHEET = 1,
    MATERIAL_EVENT_SHAPE_POLE = 2,
    MATERIAL_EVENT_SHAPE_FENCE = 3,
    MATERIAL_EVENT_SHAPE_CATWALK = 4,
    NUM_MATERIAL_EVENT_SHAPE_TYPES = 5,
};

enum material_event_type
{
    INVALID_MATERIAL_EVENT = -1,
    MATERIAL_EVENT_EXPLODE = 0,
    MATERIAL_EVENT_LOAD_BALANCE = 1,
    MATERIAL_EVENT_STRESS = 2,
    MATERIAL_EVENT_DETACH = 3,
    MATERIAL_EVENT_GROAN = 4,
    MATERIAL_EVENT_SHATTER = 5,
    MATERIAL_EVENT_COLLAPSE = 6,
    MATERIAL_EVENT_MELEE = 7,
    MATERIAL_EVENT_BULLET = 8,
    NUM_MATERIAL_SINGLE_EVENT_TYPES = 9,
    MATERIAL_EVENT_IMPACT = 9,
    MATERIAL_EVENT_SHIFT = 10,
    MATERIAL_EVENT_ROLL = 11,
    MATERIAL_EVENT_SLIDE = 12,
    NUM_MATERIAL_EVENT_TYPES = 13,
    NUM_MATERIAL_PAIR_EVENT_TYPES = 4,
};

enum material_event_size
{
    INVALID_MATERIAL_EVENT_SIZE = -1,
    MATERIAL_EVENT_SIZE_SMALL = 0,
    MATERIAL_EVENT_SIZE_MEDIUM = 1,
    MATERIAL_EVENT_SIZE_LARGE = 2,
    NUM_MATERIAL_EVENT_SIZES = 3,
};

enum material_event_category
{
    INVALID_MATERIAL_EVENT_CATEGORY = -1,
    MATERIAL_EVENT_CATEGORY_DEFAULT = 0,
    MATERIAL_EVENT_CATEGORY_COLLISION = 1,
    MATERIAL_EVENT_CATEGORY_STRESS = 2,
    MATERIAL_EVENT_CATEGORY_LOAD_BALANCE = 3,
    NUM_MATERIAL_EVENT_CATEGORIES = 4,
};

// size 0x10
struct base_event_candidate
{
    base_event_candidate(); // 0x00716250 (:1812)
    virtual ~base_event_candidate(); // vtbl+0x00 0x007104D0 (.h:230)
    material_shape_type get_default_shape_type(material_shape_type material_default, unsigned char other_mat); // 0x00716280 (:1825)
    virtual unsigned int get_event_handle(material_shape_type, material_event_size, unsigned char, unsigned char, unsigned char) = 0; // vtbl+0x04
    virtual void set_event_handle(unsigned int, material_shape_type, material_event_size, unsigned char, unsigned char, unsigned char) = 0; // vtbl+0x08

    // 0x00 vfptr
    float event_size[3]; // 0x04
};

// size 0x4C
struct single_event_candidate : public base_event_candidate
{
    single_event_candidate(); // 0x0071D190 (:249)
    virtual unsigned int get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x04 0x0071D1F0 (:256)
    virtual void set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x08 0x0071D220 (:267)
    virtual ~single_event_candidate(); // vtbl+0x00 0x0071D250

protected:
    unsigned int event_handle[5][3]; // 0x10
};

// size 0x50
struct pair_event_candidate : public base_event_candidate
{
    pair_event_candidate(); // 0x0071D280 (:285)
    virtual unsigned int get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x04 0x0071D310 (:305)
    virtual void set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x08 0x0071D360 (:323)
    virtual ~pair_event_candidate(); // vtbl+0x00 0x0071D3F0

protected:
    unsigned int default_handles[5][3]; // 0x10
    unsigned int (** custom_handles)[5][3]; // 0x4C
};

// size 0x14
struct melee_event_candidate : public base_event_candidate
{
    melee_event_candidate(); // 0x0071D420 (:354)
    virtual unsigned int get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x04 0x0071D4A0 (:365)
    virtual void set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x08 0x0071D4D0 (:376)
    virtual ~melee_event_candidate(); // vtbl+0x00 0x0071D500

protected:
    unsigned int (*event_handle)[5]; // 0x10
};

// size 0x18
struct bullet_event_candidate : public base_event_candidate
{
    bullet_event_candidate(); // 0x0071D530 (:400)
    virtual unsigned int get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x04 0x0071D5C0 (:417)
    virtual void set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index); // vtbl+0x08 0x0071D5F0 (:428)
    int get_bullet_decal_info(unsigned char bullet_group_index); // 0x00715790 (:437)
    void set_bullet_decal_info(int di, unsigned char bullet_group_index); // 0x007157C0 (:442)
    virtual ~bullet_event_candidate(); // vtbl+0x00 0x0071D620

protected:
    unsigned int (*event_handle)[5]; // 0x10
    int* decal_info; // 0x14
};

// size 0x10
struct gust_effect_info
{
    unsigned int effect; // 0x00
    float fade_time; // 0x04
    float percent_chance; // 0x08
    bool near_far; // 0x0C
};

// size 0x128
struct rfg_effect_material
{
    unsigned int get_bullet_impact_effect(material_shape_type shape_type, unsigned char bullet_group_index); // 0x007161F0 (:1797)
    int get_bullet_impact_decal_info(unsigned char bullet_group_index); // 0x00716220 (:1803)

    char name[32]; // 0x00
    material_shape_type default_shape_type; // 0x20
    rl_color_float effect_tint; // 0x24
    bool effect_tint_enabled; // 0x34
    base_event_candidate* effect_candidate[13]; // 0x38
    int blast_decal; // 0x6C
    unsigned int trail_effects[2]; // 0x70
    int num_gust_effects; // 0x78
    gust_effect_info gust_effects[10]; // 0x7C
    bool driving_do_tracks; // 0x11C
    unsigned int driving_front_effect; // 0x120
    unsigned int driving_rear_effect; // 0x124
};

// size 0x4C
struct rfg_physical_material
{
    char name[32]; // 0x00
    unsigned int name_checksum; // 0x20
    char suffix[3]; // 0x24
    unsigned int suffix_checksum; // 0x28
    char* base_props_name; // 0x2C
    float density; // 0x30
    float brittleness; // 0x34
    float restitution; // 0x38
    float friction; // 0x3C
    unsigned int link_strength; // 0x40
    rfg_effect_material* effect_material; // 0x44
    unsigned int flags; // 0x48
};

extern rfg_effect_material* Rfg_effect_material_default; // 0x01EF82D0
extern unsigned char Rfg_effect_material_default_index; // 0x01650359
extern rfg_physical_material* Rfg_physical_material_default; // 0x01EF82CC
extern unsigned char Rfg_physical_material_default_index; // 0x01650358
extern unsigned char Rfg_melee_group_default_index; // 0x0165035A
extern unsigned char Rfg_bullet_group_default_index; // 0x0165035B
extern mempool Effect_material_pool; // 0x01F13D40, "Effect candidate event pool": materials, candidates, group names
extern const char* Material_event_type_name[13]; // 0x0165035C
extern const char* Material_event_shape_type_name[5]; // 0x01650390
extern const char* Material_flag_names[12]; // 0x016503B4

bool rfg_material_event_type_is_valid(material_event_type m); // 0x007104C0 (.h:211)
bool rfg_material_event_size_is_valid(material_event_size s); // 0x0071D1D0 (.h:213)
bool rfg_material_shape_type_is_valid(material_shape_type t); // 0x0071D1E0 (.h:214)
rfg_effect_material* rfg_get_effect_material(const char* name); // 0x0071D740 (:505)
unsigned char rfg_get_effect_material_index(const char* name); // 0x00715800 (:512)
unsigned char rfg_get_effect_material_index(rfg_effect_material* material); // 0x00715870 (:526)
unsigned char rfg_get_effect_material_index(unsigned char mat_idx); // 0x0071D770 (:532)
unsigned char rfg_get_melee_group_index(const char* name); // 0x007158A0 (:539)
unsigned char rfg_get_bullet_group_index(const char* name); // 0x00715900 (:553)
void rfg_materials_system_init(bool ignore_max_only_materials, const char* pDataSourcePath); // 0x007252F0 (:1236)
void rfg_materials_system_shutdown(); // 0x00715CD0 (:1439)
int rfg_get_physical_material_count(); // 0x00715D00 (:1455)
int rfg_get_effect_material_count(); // 0x00715D10 (:1462)
int rfg_get_melee_group_count(); // 0x00715D20 (:1468)
int rfg_get_bullet_group_count(); // 0x00715D30 (:1474)
bool rfg_physical_material_index_is_valid(unsigned char i); // 0x00715D40 (:1480)
bool rfg_is_same_material(unsigned char idx, const char* name); // 0x00715D60 (:1486)
bool rfg_effect_material_index_is_valid(unsigned char i); // 0x00715DA0 (:1492)
bool rfg_is_same_effect_material(unsigned char idx, const char* name); // 0x00715DC0 (:1498)
rfg_physical_material* rfg_get_physical_material_by_index(unsigned char index); // 0x00715E10 (:1514)
rfg_effect_material* rfg_get_effect_material_by_index(unsigned char index); // 0x00715E40 (:1525)
rfg_physical_material* rfg_get_physical_material(const char* name); // 0x007257B0 (:1540)
unsigned char rfg_get_physical_material_index(const char* name); // 0x0071E530 (:1554)
unsigned char rfg_get_physical_material_index(rfg_physical_material* material); // 0x00715E70 (:1560)
rfg_physical_material* rfg_get_physical_material_by_checksum(unsigned int name_checksum); // 0x0071E570 (:1572)
unsigned char rfg_get_physical_material_index_by_checksum(unsigned int name_checksum); // 0x00715EA0 (:1585)
rfg_physical_material* rfg_get_physical_material_by_suffix(const char* suffix); // 0x0071E5C0 (:1600)
unsigned char rfg_get_physical_material_index_by_suffix(const char* suffix); // 0x00715EE0 (:1607)
const char* rfg_physical_material_get_suffix(unsigned char mat_index); // 0x00715F20 (:1621)
material_event_category rfg_material_category_from_type(material_event_type event_type); // 0x00715F50 (:1633)
material_shape_type rfg_material_get_default_shape_type(unsigned char mat_index); // 0x00715FA0 (:1662)
material_shape_type rfg_material_get_shape_type(const vector& bmin, const vector& bmax); // 0x00715FD0 (:1677)
material_shape_type rfg_material_get_shape_type(const object* objp, unsigned int shape_key); // 0x00725810 (:1710)
material_shape_type rfg_material_get_shape_type(const char* name); // 0x00716110 (:1750)
const char* rfg_material_get_shape_type_name(material_shape_type type); // 0x00716170 (:1765)
int rfg_effect_material_get_event_handle(material_event_type type, material_event_size size, material_shape_type shape_type, unsigned char mat_index, unsigned char other_mat_index, unsigned char melee_group_index); // 0x00716190 (:1776)
