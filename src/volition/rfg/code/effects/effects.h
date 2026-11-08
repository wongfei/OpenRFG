// Original: volition/rfg/code/effects/effects.h
// Implementation: volition/rfg/code/effects/effects.cpp
//
// The effect system: the effect table (effects_shared.h) and the radial blur table (radial_blur.xtbl, up to 200 entries,
// looked up by name for visual_effect_info::radial_blur_info_p), template preloading through stream2 (DLC and
// multiplayer sets, backpack effects), and playback: at a position or on a host object's tag (the movers' script point
// and prefix effects), returning the effect object's handle or 0xFFFFFFFF. The effects pool lock functions serve the
// async stream2 allocator.
#pragma once

#include "volition/rfg/code/effects/object_effect.h"

struct effect_info;
class matrix;
class rfg_split_mempool;
class vector;
class vfx_effect_template;

// size 0x58
struct radial_blur_info
{
    char m_name[65]; // 0x00
    float m_strength; // 0x44
    float m_duration; // 0x48
    float m_radius; // 0x4C
    float m_distance_fade; // 0x50
    int m_priority; // 0x54
};

extern rfg_split_mempool DLC_effect_preload_mempool; // 0x02534A20, placement inferred (Effect_preload_mempool is in rfg.h)
extern unsigned int Dlc_effects_preload_srid; // 0x02182CC0
extern int Num_visual_effect_types_dlc_mark; // 0x02182DFC
extern unsigned int Num_effect_types_dlc_mark; // 0x02182DF4
extern bool Effects_lod_enabled; // 0x01650931
extern bool Effects_force_update_render; // 0x02182E0C
extern char effect_last_played_console[256]; // 0x02182CF0

void visual_effects_parse_table(int dlc_id); // 0x007452B0 (:169)
void get_effect_name_without_extension(char* effect_name_without_extension, unsigned int capacity, const char* src_effect_name); // 0x007424C0 (:393)
unsigned int get_backpack_array_index(const char* effect_name); // 0x00742500 (:406)
bool is_backpack_effect(const char* effect_name); // 0x007425A0 (:422)
bool is_backpack_effect(vfx_effect_template* template_p); // 0x007477F0 (:438)
bool effects_template_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data_p, int cpu_size, unsigned char* gpu_data_p, int gpu_size); // 0x0074B800 (:443)
void effects_template_unload_cb(const char* filename, unsigned int srid); // 0x00747810 (:494)
void mp_preload_visual_effects(); // 0x00745620 (:520)
void mp_unload_visual_effects(); // 0x00742640 (:593)
void effects_system_init(); // 0x00747860 (:676)
void effects_system_shutdown(); // 0x007427E0 (:696)
void effects_system_release_backpack_effects(); // 0x00742800 (:706)
void effects_level_init_pre(); // 0x00742820 (:717)
void effects_level_shutdown(); // 0x00742830 (:721)
radial_blur_info* effects_get_radial_blur_info(const char* name); // 0x00742840 (:725)
vfx_effect_template* effects_template_find(const char* name); // 0x007478B0 (:738)
unsigned int effects_get_handle_from_vfx_filename(const char* name); // 0x007457D0 (:751)
unsigned int effects_play(unsigned int effect_handle, const vector& pos, const matrix& orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float effect_volume, bool attach_to_camera, object_effect_category category); // 0x00747910 (:792)
unsigned int effects_play(const effect_info* info, const vector& pos, const matrix& orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float effect_volume, bool attach_to_camera, object_effect_category category); // 0x007458B0 (:817)
unsigned int effects_play(unsigned int effect_handle, unsigned int host_handle, int host_tag, bool looping, unsigned int source_handle, float effect_volume, bool attach_to_camera); // 0x00747970 (:920)
unsigned int effects_play(const effect_info* info, unsigned int host_handle, int host_tag, bool looping, unsigned int source_handle, float effect_volume, bool attach_to_camera); // 0x00745AF0 (:941)
unsigned int effects_play_at_script_point(unsigned int effect_handle, unsigned int script_point_ref_handle, int script_point_index, bool looping, const vector* use_pos, const matrix* use_orient); // 0x007479C0 (:959)
void effects_play_radial_blur(const radial_blur_info* blur_info, const vector& pos); // 0x007428B0 (:1059)
void effects_play_file_replace(const char* filename, bool looping); // 0x00748090 (:1088)
bool __effects_pool_lock_async(); // 0x00742900 (:1135)
rfg_split_mempool* __effects_pool_lock_async_poll(); // 0x00742940 (:1149)
void __effects_pool_unlock_async(); // 0x007429C0 (:1176)
bool __effects_pool_unlock_async_poll(); // 0x007429E0 (:1186)
void unused_dcf_kill_all_effects(); // 0x00748120 (:1214)
void unused_dcf_play_last_effect(); // 0x00748130 (:1293)
bool effects_get_enabled(); // 0x00742A20 (:1547)
