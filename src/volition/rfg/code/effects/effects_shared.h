// Original: volition/rfg/code/effects/effects_shared.h
// Implementation: volition/rfg/code/effects/effects_shared.cpp
//
// Effect definitions from effects.xtbl, shared with the tools. An effect_info combines a visual effect
// (visual_effect_info: .cefct_pc template, streamed through GSP_VFX, with LOD settings) and a sound; effects are
// addressed by handle (index into the table) or name (effects_get_handle, effects_get_effect_info).
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
struct xml_element;
class vfx_effect_template;
struct radial_blur_info;
struct pasm_data;

// size 0xBC
struct visual_effect_info
{
    // size 0x14
    struct lod
    {
        bool enabled; // 0x00
        bool spawn_view_dependent; // 0x01
        bool restore_faded_object; // 0x02
        bool update_view_dependent; // 0x03
        float view_dependent_minimum_time; // 0x04
        float spawn_distance; // 0x08
        float update_render_fade_start; // 0x0C
        float update_render_fade_end; // 0x10
    };

    visual_effect_info(); // 0x007420C0 (.h:108)

    char name[65]; // 0x00
    char vfx_filename[65]; // 0x41
    float radius; // 0x84
    float los_blocker_opacity; // 0x88
    int los_blocker_life_span; // 0x8C
    int flags; // 0x90
    vfx_effect_template* template_p; // 0x94
    unsigned int srid; // 0x98
    int stream_category; // 0x9C
    radial_blur_info* radial_blur_info_p; // 0xA0
    lod m_lod; // 0xA4
    float m_start_time; // 0xB8
};

// size 0x58
struct effect_info
{
    effect_info(); // 0x007420E0 (.h:134)

    char name[65]; // 0x00
    unsigned int visual_handle; // 0x44
    int sound_handle; // 0x48
    int sound_alr_handle; // 0x4C
    float vfx_kill_particles_fade_time_s; // 0x50
    struct
    {
        bool sound_kill_oneshot_when_done : 1; // 0x00:0
        bool sound_follows_effect_pos : 1; // 0x00:1
        bool vfx_kill_particles_on_stop : 1; // 0x00:2
        bool stop_when_host_destroyed : 1; // 0x00:3
    } flags; // 0x54
};

extern effect_info Effect_info[1500]; // 0x02182E60
extern unsigned int Num_effect_types; // 0x02182DF0
extern visual_effect_info Visual_effect_info[650]; // 0x021BEFA0
extern int Num_visual_effect_types; // 0x02182DF8

bool effects_is_valid(effect_info* info); // 0x00742F10 (:291)
void visual_effects_parse_table_shared(pasm_data* peg_data, int& cpu_mark, int& gpu_mark, mempool_base* tmp_pool, mempool_base* pool, bool multiplayer_pass, vlib_platform plat, bool verify_only, int dlc_id); // 0x00742F30 (:317)
void effects_parse_table_shared(int dlc_id); // 0x00745BB0 (:527)
effect_info* effects_get_effect_info(unsigned int handle); // 0x007431D0 (:633)
effect_info* effects_get_effect_info(const char* name); // 0x00745E00 (:642)
unsigned int effects_get_handle(const char* name); // 0x007431F0 (:647)
unsigned int effects_get_handle(const effect_info* info); // 0x00743240 (:662)
visual_effect_info* effects_get_visual_effect_info(unsigned int handle); // 0x00743260 (:667)
visual_effect_info* effects_get_visual_effect_info(const char* name); // 0x00745E30 (:676)
unsigned int effects_get_visual_effect_info_handle(const visual_effect_info* info); // 0x00743280 (:685)
unsigned int effects_get_visual_effect_info_handle(const char* name); // 0x007432A0 (:690)

int get_visual_effect_streaming_category(const char* stream_cat_text); // 0x00742110 (.h:174)
