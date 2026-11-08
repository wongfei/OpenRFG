// Original: volition/rfg/code/multi/multi_game/multi_game_badge.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_badge.cpp
//
// Badges: the player's badge (icon, colors, fields that count stats), saved in the profile, sent to the other players and
// unlocked by experience. Placement of the badge types inferred.
#pragma once

#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/varray.h"

class rfg_split_mempool;

enum multi_badge_field_types
{
    MBFT_INVALID = 0,
    MBFT_ICON = 0,
    MBFT_COLOR_SCHEME = 1,
    NUM_MULTI_BADGE_FIELD_TYPES = 2,
};

enum multi_badge_unlock_type
{
    MBUT_ICON = 0,
    MBUT_EMBLEM = 1,
    MBUT_COLOR_SCHEME = 2,
    NUM_MULTI_BADGE_UNLOCK_TYPES = 3,
};

// size 0x24
struct multi_badge_field_entry
{
    int level; // 0x00
    char name[32]; // 0x04
};

// size 0x2C
struct multi_badge_icon_info : public multi_badge_field_entry
{
    int bitmap_id; // 0x24
    int name_number; // 0x28
};

// size 0x30
struct multi_badge_color_scheme : public multi_badge_field_entry
{
    multi_badge_color_scheme(); // 0x0096BC00

    color primary_color; // 0x24
    color secondary_color; // 0x28
    color tertiary_color; // 0x2C
};

// size 0x278
struct multi_badge_field
{
    multi_badge_field(); // 0x009A6FD0
    ~multi_badge_field(); // 0x0098C4A0

    checksum_stri name_crc; // 0x00
    int num_bits; // 0x04
    int num_previous_bits; // 0x08
    int num_values; // 0x0C
    int render_order; // 0x10
    bool automatic; // 0x14
    multi_badge_field_types field_type; // 0x18
    dynamic_object_pool<multi_badge_icon_info,1> icon_pool; // 0x20
    int icon_offset_x; // 0x60
    int icon_offset_y; // 0x64
    float icon_scale; // 0x68
    bool icon_defines_clipping_region; // 0x6C
    int icon_clipping_offset_x; // 0x70
    int icon_clipping_offset_y; // 0x74
    int icon_clipping_width; // 0x78
    int icon_clipping_height; // 0x7C
    int icon_previous_level; // 0x80
    int icon_count_at_this_level; // 0x84
    dynamic_object_pool<multi_badge_color_scheme,1> color_scheme_pool; // 0x88
    farray<checksum_stri,32> color_scheme_primary_field_names; // 0xC8
    farray<checksum_stri,32> color_scheme_secondary_field_names; // 0x158
    farray<checksum_stri,32> color_scheme_tertiary_field_names; // 0x1E8
};

// size 0x8
struct xp_badge_unlock_info
{
    unsigned int unlock_info_index; // 0x00
    int unlock_level; // 0x04
};

// The badge icons' bitmap sheet, streamed with the multiplayer interface.
extern unsigned int Multi_badges_bitmap_sheet_srid; // 0x02FEB65C

void unused_dcf_mp_badge_set(); // 0x009AC100 (:536)
void unused_dcf_mp_badge_reload_table(); // 0x009F0800 (:555)
void multi_game_badge_system_init(rfg_split_mempool& pool); // 0x009DCD50 (:569)
void multi_game_badge_system_shutdown(); // 0x009DCF90 (:662)
void multi_game_badge_system_render(); // 0x009AC1E0 (:669)
void multi_game_save_badge_to_profile(const unsigned int& badge); // 0x009AC220 (:676)
void multi_game_badge_increment_field(unsigned int& badge, const char* field_name); // 0x00996100 (:686)
void multi_game_badge_decrement_field(unsigned int& badge, const char* field_name); // 0x00996170 (:701)
void multi_game_badge_get_field_value_name(wchar_t* buffer, int buffer_size, const unsigned int& badge, const char* field_name); // 0x009961E0 (:718)
void multi_game_badge_get_my_badge(unsigned int& output); // 0x00996300 (:755)
bool multi_game_badge_is_valid(const unsigned int& badge, int player_level); // 0x00996330 (:766)
bool multi_game_badge_field_is_valid(const unsigned int& badge, const char* field_name, int player_level); // 0x009963A0 (:786)
void multi_game_badge_render(const unsigned int& badge, int x, int y, float alpha, float scale); // 0x009963F0 (:794)
void multi_game_badge_write_to_packet(const unsigned int& badge, unsigned char* packet_data, unsigned short& packet_size); // 0x0094F2D0 (:834)
void multi_game_badge_read_from_packet(unsigned int& badge, const unsigned char* packet_data, int& packet_offset); // 0x0094F340 (:842)
void multi_game_badge_get_xp_unlocks(int& num_xp_badge_unlocks, xp_badge_unlock_info* unlock_infos, int xp_unlock_type, int max_unlock_infos); // 0x00979A30 (:867)
const char* multi_game_badge_get_xp_unlock_group_name(unsigned int index); // 0x00979AC0 (:881)
