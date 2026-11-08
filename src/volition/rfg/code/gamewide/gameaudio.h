// Original: volition/rfg/code/gamewide/gameaudio.h
// Implementation: volition/rfg/code/gamewide/gameaudio.cpp
//
// The game's audio driver: audio_lib init in two stages (gameaudio_get_system_init_params, the streamed file callbacks),
// the stream2 bank callbacks, per-frame update (listener, weather, the player health low pass and volume filter from
// low_health), save/load, multiplayer preload, game state and level start/end, and the cue ids the game plays by name
// (Ga_cue_ids). Placement of the types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"

struct audiolib_system_init_info;
struct cfile;
struct cpu_job;
class matrix;
class mempool;
class vector;

namespace keen
{
    class MemoryAllocator;
}

namespace keen
{
    namespace SoundSystem
    {
        struct System;
    }
}

// size 0x28
struct gameaudio_low_health_info
{
    float Max_low_pass_due_to_health; // 0x00
    float Min_volume_due_to_health; // 0x04
    float Affect_sound_below_health; // 0x08
    float Affect_sound_above_health; // 0x0C
    float Fade_rate; // 0x10
    short cat_ids[10]; // 0x14
};

// size 0x20C
struct global_sound_cue_id_struct
{
    int ui_sound_move_down; // 0x00
    int ui_sound_move_up; // 0x04
    int ui_sound_slider_add; // 0x08
    int ui_sound_slider_subtract; // 0x0C
    int ui_sound_press_select; // 0x10
    int ui_sound_press_abort; // 0x14
    int ui_sound_press_misc; // 0x18
    int ui_sound_scroll_left; // 0x1C
    int ui_sound_scroll_right; // 0x20
    int ui_sound_bumper_left; // 0x24
    int ui_sound_bumper_right; // 0x28
    int ui_sound_invalid; // 0x2C
    int ui_ambience_slider_cue_id; // 0x30
    int ui_music_slider_cue_id; // 0x34
    int ui_sfx_slider_cue_id; // 0x38
    int ui_voice_slider_cue_id; // 0x3C
    int ui_voicechat_slider_cue_id; // 0x40
    int ui_overall_slider_cue_id; // 0x44
    int jetpack_boost_fail_pc; // 0x48
    int jetpack_start_pc; // 0x4C
    int jetpack_loop_pc; // 0x50
    int jetpack_release_pc; // 0x54
    int jetpack_boost_fail_npc; // 0x58
    int jetpack_start_npc; // 0x5C
    int jetpack_loop_npc; // 0x60
    int jetpack_release_npc; // 0x64
    int jetpack_boost_fail_walker; // 0x68
    int jetpack_start_walker; // 0x6C
    int jetpack_loop_walker; // 0x70
    int jetpack_release_walker; // 0x74
    int player_radio_start; // 0x78
    int radio_message_start; // 0x7C
    int radio_message_end; // 0x80
    int edf_projected_start; // 0x84
    int heartbeat; // 0x88
    int radiation_start; // 0x8C
    int radiation_loop; // 0x90
    int radiation_kill; // 0x94
    int bftp_pickup_tag; // 0x98
    int bftp_arm_bomb; // 0x9C
    int weapon_cabinet_scroll_up; // 0xA0
    int weapon_cabinet_scroll_down; // 0xA4
    int weapon_cabinet_open; // 0xA8
    int weapon_cabinet_close; // 0xAC
    int weapon_upgrade_menu_open; // 0xB0
    int weapon_upgrade_menu_close; // 0xB4
    int weapon_upgrade_purchase; // 0xB8
    int weapon_upgrade_no_purchase; // 0xBC
    int timer_clock_appears; // 0xC0
    int timer_one_minute_remains; // 0xC4
    int timer_ten_seconds_remains; // 0xC8
    int timer_final_seconds_remains; // 0xCC
    int wrecking_crew_weapon_fired; // 0xD0
    int wrecking_crew_red_barrel_destroyed; // 0xD4
    int wrecking_crew_blue_barrel_destroyed; // 0xD8
    int wrecking_crew_countdown_number; // 0xDC
    int wrecking_crew_countdown_go; // 0xE0
    int wrecking_crew_match_won; // 0xE4
    int wrecking_crew_round_won_stamp; // 0xE8
    int wrecking_crew_round_over; // 0xEC
    int general_alert_changed; // 0xF0
    int general_alert_changed_to_red; // 0xF4
    int general_player_died; // 0xF8
    int general_handbook_pop_up; // 0xFC
    int general_new_mission_pop_up; // 0x100
    int general_other_pop_up; // 0x104
    int general_tooltip_pop_up; // 0x108
    int general_pause_menu_open; // 0x10C
    int map_scroll; // 0x110
    int map_select_point_of_interest; // 0x114
    int map_gps_point_placed; // 0x118
    int map_gps_point_removed; // 0x11C
    int map_zoom_in_attack; // 0x120
    int map_zoom_out_start; // 0x124
    int map_open; // 0x128
    int map_close; // 0x12C
    int map_activity_lock; // 0x130
    int minimap_zoom_in_attack; // 0x134
    int minimap_zoom_out_start; // 0x138
    int mission_briefing_intro; // 0x13C
    int mission_briefing_outro; // 0x140
    int mission_heading; // 0x144
    int mission_text; // 0x148
    int mission_complete; // 0x14C
    int mission_fail; // 0x150
    int mission_point_tally_alr; // 0x154
    int mission_complete_text_hit; // 0x158
    int avmg_target_enemy; // 0x15C
    int avmg_zoom_to_convoy; // 0x160
    int avmg_stop_zoom_to_convoy; // 0x164
    int activity_complete_sound; // 0x168
    int activity_fail_sound; // 0x16C
    int district_liberated; // 0x170
    int district_liberation_countdown_loop; // 0x174
    int district_liberation_countdown_disappear; // 0x178
    int unlock_popup; // 0x17C
    int unlock_slam; // 0x180
    int target_destroyed; // 0x184
    int rocket_lockon; // 0x188
    int popup_killing_spree; // 0x18C
    int popup_propeganda; // 0x190
    int popup_mayhem_started; // 0x194
    int morale_popup; // 0x198
    int morale_lower; // 0x19C
    int morale_raise; // 0x1A0
    int control_change; // 0x1A4
    int control_lower; // 0x1A8
    int control_mission_unlock; // 0x1AC
    int control_mission_complete; // 0x1B0
    int multiplayer_select_start; // 0x1B4
    int multiplayer_camera_switch; // 0x1B8
    int multiplayer_player_death; // 0x1BC
    int multiplayer_player_hits_player; // 0x1C0
    int multiplayer_player_kills_player; // 0x1C4
    int multiplayer_player_respawn; // 0x1C8
    int multiplayer_target_repair; // 0x1CC
    int multiplayer_target_destroyed; // 0x1D0
    int multiplayer_target_unlocked; // 0x1D4
    int multiplayer_engineer_killed; // 0x1D8
    int multiplayer_become_engineer; // 0x1DC
    int multiplayer_engineer_passed; // 0x1E0
    int multiplayer_destruction_points; // 0x1E4
    int multiplayer_flag_dropped; // 0x1E8
    int multiplayer_flag_taken; // 0x1EC
    int multiplayer_flag_captured; // 0x1F0
    int multiplayer_flag_returned; // 0x1F4
    int multiplayer_bag_picked_up; // 0x1F8
    int multiplayer_bag_dropped; // 0x1FC
    int multiplayer_bag_team_picked_up; // 0x200
    int multiplayer_bag_team_dropped; // 0x204
    int multiplayer_xp_gain; // 0x208
};

extern gameaudio_low_health_info low_health; // 0x0252D6C4
extern global_sound_cue_id_struct Ga_cue_ids; // 0x02532CC0
// SFX.md SX6: the human voice's clear or radio lines switch to radio beyond it (m).
extern float gameaudio_clr_radio_crossover_distance; // 0x0165CB0C

void unused_dcf_gameaudio_restart(); // 0x007BB310 (:122)
void unused_dcf_sound_reset_stats(); // 0x007BB370 (:145)
void unused_dcf_sound(); // 0x007BB380 (:151)
void unused_dcf_sound_play(); // 0x007BB490 (:182)
void unused_dcf_sound_play_3d(); // 0x007BB680 (:257)
void gameaudio_update_weather(); // 0x007CB740 (:368)
extern bool Audio_stage_1_init; // 0x02533848
extern bool Audio_stage_2_init; // 0x02533849
bool gameaudio_process(void* user_data, int curr_time, int time_step); // 0x007BB930 (:671)
event_result ga_global_event_callback(const event_data& event, notify_info* data); // 0x007BB940 (:692)
void ga_unregister_for_global_events(); // 0x007BB960 (:723)
bool gameaudio_soundbank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x007BB980 (:740)
void gameaudio_soundbank_unloaded_callback(const char* name, unsigned int srid); // 0x007BB9B0 (:755)
bool gameaudio_foley_bank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x007BB9C0 (:763)
void gameaudio_foley_bank_unloaded_callback(const char* name, unsigned int srid); // 0x007BB9E0 (:779)
bool gameaudio_file_vehicle_wavebank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x007BB9F0 (:786)
void gameaudio_file_vehicle_wavebank_unloaded_callback(const char* name, unsigned int srid); // 0x007BBAE0 (:853)
void* gameaudio_streaming_file_load(char* filepath, mempool* pool, void (*loaded_callback)(const char*, void*, unsigned char*, int), void (*unloaded_callback)(const char*, void*)); // 0x007BBB70 (:887)
audiolib_result gameaudio_streaming_file_status(void* id); // 0x007BBC60 (:917)
bool gameaudio_streaming_file_initialize(mempool* pool); // 0x007BBCC0 (:946)
bool gameaudio_parse_settings(const char* filename); // 0x007BBCE0 (:953)
int gameaudio_get_2d_crossover(int threed); // 0x007BDB30 (:1527)
int gameaudio_get_3d_crossover(int twod); // 0x007BDB70 (:1546)
bool gameaudio_system_init_stage_1(); // 0x007BDBA0 (:1570)
bool gameaudio_system_init_stage_2_complete(); // 0x007BDBC0 (:1581)
void gameaudio_get_system_init_params(audiolib_system_init_info& audiolib_init_info); // 0x007BDBD0 (:1588)
bool gameaudio_system_init_stage_2(keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator, keen::SoundSystem::System* pSoundSystem); // 0x007E8E60 (:1608)
void gameaudio_system_shutdown(); // 0x007CB980 (:1654)
void gameaudio_adjust_low_pass_and_volume_due_to_player_health(); // 0x007BDC80 (:1679)
bool gameaudio_update(const vector& listener_pos, const matrix& listener_orient, const vector& listener_vel); // 0x007ED7B0 (:1752)
void gameaudio_update_job(cpu_job* lj, unsigned int arg1, unsigned int arg2); // 0x007ED860 (:1783)
float gameaudio_get_listener_los(); // 0x007BDEC0 (:1852)
void gameaudio_system_save(cfile* file); // 0x007CB9D0 (:1861)
void gameaudio_system_load(cfile* file); // 0x007D8020 (:1867)
void gameaudio_mp_preload(); // 0x007BDED0 (:1877)
void gameaudio_mp_unload(); // 0x007BDEE0 (:1946)
int gameaudio_play_cross_linked_sound(int cue_id, unsigned int source_handle, const vector* orient, const vector* velocity); // 0x007BDEF0 (:2009)
void gameaudio_set_game_state(game_state curr_state); // 0x007BDFA0 (:2049)
void gameaudio_level_start(); // 0x007BE030 (:2075)
void gameaudio_level_end(); // 0x007E8F50 (:2088)
