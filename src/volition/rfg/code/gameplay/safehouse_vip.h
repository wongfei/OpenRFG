// Original: volition/rfg/code/gameplay/safehouse_vip.h
// Implementation: volition/rfg/code/gameplay/safehouse_vip.cpp
//
// The safehouse VIP: the guerrilla leaders who ask the player for help from safehouses (status, request answers, the
// demolition VIP lines and npc name lookup), saved with the game. Placement of the enums and line tables inferred.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

struct cfile;
class human_info;
class object_squad_spawn_node;

enum vip_statuses
{
    VS_LOCATION_KNOWN = 0,
    VS_LOCATION_UNKNOWN = 1,
    VS_DEAD = 2,
    VS_COUNTDOWN = 3,
};

enum vip_request_statuses
{
    VRS_NO = 0,
    VRS_UNKNOWN = 1,
    VRS_YES = 2,
};

// size 0x104
struct vip_line_info
{
    vip_line_info(); // 0x0094D560

    wchar_t buffer[128]; // 0x00
    timestamp fade_timer; // 0x100
};

// size 0x8
struct vip_npc_name_lookup
{
    unsigned int npc_name_crc; // 0x00
    unsigned int npc_handle; // 0x04
};

void safehouse_vip_level_init_pre(); // 0x00775510 (:424)
void safehouse_vip_level_init_post(); // 0x00790730 (:433)
void safehouse_vip_do_frame(); // 0x007907B0 (:459)
void safehouse_vip_notify(object_squad_spawn_node* node, bool load); // 0x007909A0 (:513)
void safehouse_vip_mission_start(); // 0x00775530 (:532)
void safehouse_vip_disable(const human_info* hinfo, bool disable_vip); // 0x007909F0 (:544)
void safehouse_vip_save(cfile* save_file); // 0x00775540 (:560)
void safehouse_vip_load(cfile* load_file); // 0x00790A20 (:574)
