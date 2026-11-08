// Original: volition/rfg/code/cutscene/cutscene_internal.h
//
// The cutscene data the load code fills and the playback reads: the cutscene info of the table, the current cutscene
// (shots, cameras, actors, slates, effects, soundtracks, time of day), the shot sequences (character animations), NPCs,
// vehicles and navpoints spawned for the cutscene, and the play parameters. Placement of the data types other than
// cs_base_sequence, cs_shot_char_animation and cutscene_shot (which have inline methods here) inferred.
#pragma once

#include "volition/rfg/code/camera/camera_cutscene.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/cutscene_load.h"
#include "volition/rfg/code/cutscene/cutscene_options.h"
#include "volition/rfg/code/cutscene/cutscene_timer.h"
#include "volition/rfg/code/cutscene/cutscene_tween.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

struct cpu_job;
struct light_set;
class rfg_mover;
struct xml_element;

// size 0x14
struct cs_base_sequence
{
    cs_base_sequence(); // 0x00705230 (.h:115)
    void parse_duration(xml_element* sequence_xml, unsigned int& total_shot_duration); // 0x00705FF0 (cutscene_load.cpp:293)
    void parse_duration_overlap(xml_element* sequence_xml, unsigned int& total_shot_duration, cs_base_sequence* prev_seq_element); // 0x00706030 (cutscene_load.cpp:309)
    void set_duration_overlap(const unsigned int duration, const unsigned int overlap, unsigned int& total_shot_duration, cs_base_sequence* prev_seq_element); // 0x007060A0 (cutscene_load.cpp:338)
    bool should_start(const unsigned int shot_frame); // 0x00705250 (.h:123)
    bool should_stop(const unsigned int shot_frame); // 0x00705270 (.h:135)
    bool is_playing() const;
    unsigned int get_shot_start_frame() const; // 0x00705290 (.h:149)
    unsigned int get_shot_end_frame() const;
    unsigned int get_ramp_in() const; // 0x007052A0 (.h:157)
    unsigned int get_ramp_out() const; // 0x007052B0 (.h:161)

private:
    unsigned int ramp_in; // 0x00
    unsigned int ramp_out; // 0x04
    unsigned int shot_start_frame; // 0x08
    unsigned int shot_end_frame; // 0x0C
    bool has_started; // 0x10
    bool has_stopped; // 0x11
};

// 0x00705230 (.h:115)
inline cs_base_sequence::cs_base_sequence()
    : ramp_in(0), ramp_out(0), shot_start_frame(0), shot_end_frame(0), has_started(false), has_stopped(false)
{
}

// 0x00705250 (.h:123)
inline bool cs_base_sequence::should_start(const unsigned int shot_frame)
{
    if (!has_started && shot_start_frame <= shot_frame)
    {
        has_started = true;
        return true;
    }
    return false;
}

// 0x00705270 (.h:135)
inline bool cs_base_sequence::should_stop(const unsigned int shot_frame)
{
    if (!has_stopped && shot_end_frame - ramp_out <= shot_frame)
    {
        has_stopped = true;
        return true;
    }
    return false;
}

inline bool cs_base_sequence::is_playing() const
{
    return has_started && !has_stopped;
}

// 0x00705290 (.h:149)
inline unsigned int cs_base_sequence::get_shot_start_frame() const
{
    return shot_start_frame;
}

inline unsigned int cs_base_sequence::get_shot_end_frame() const
{
    return shot_end_frame;
}

// 0x007052A0 (.h:157)
inline unsigned int cs_base_sequence::get_ramp_in() const
{
    return ramp_in;
}

// 0x007052B0 (.h:161)
inline unsigned int cs_base_sequence::get_ramp_out() const
{
    return ramp_out;
}

// size 0x30
struct cs_shot_char_animation : public cs_base_sequence
{
    cs_shot_char_animation(); // 0x007052C0 (.h:188)
    unsigned int get_play_time() const; // 0x00705300 (.h:191)

    const char* anim_name; // 0x14
    unsigned int start_frame; // 0x18
    unsigned int end_frame; // 0x1C
    float max_weight; // 0x20
    bool cs_loaded; // 0x24
    int anim_handle; // 0x28
    unsigned int anim_inst; // 0x2C
};

// 0x007052C0 (.h:188)
inline cs_shot_char_animation::cs_shot_char_animation()
    : anim_name(nullptr), start_frame(0), end_frame(0), max_weight(0.0f), cs_loaded(false), anim_handle(-1),
      anim_inst(0xFFFFFFFF)
{
}

// 0x00705300 (.h:191)
inline unsigned int cs_shot_char_animation::get_play_time() const
{
    return end_frame - start_frame;
}

// size 0x10
struct cs_shot_actor_options
{
    const char* weapon_name; // 0x00
    const char* item_name; // 0x04
    bool fat_bones; // 0x08
    bool hide_char; // 0x09
    bool draw_weapon; // 0x0A
    bool holster_weapon; // 0x0B
    bool dont_reposition; // 0x0C
};

// size 0x10
struct cs_shot_char_tod
{
    cs_shot_char_tod(); // 0x00709980
    ~cs_shot_char_tod(); // 0x00709990
    cs_shot_char_tod& operator=(const cs_shot_char_tod& __that); // 0x0070B000

    base_array<bool> lights_enabled; // 0x00
};

// size 0x5C
struct cs_shot_actor
{
    cs_shot_actor(); // 0x00709940
    ~cs_shot_actor(); // 0x00709960
    cs_shot_actor& operator=(const cs_shot_actor& __that); // 0x0070B020

    const char* actor_name; // 0x00
    unsigned int handle; // 0x04
    vector position; // 0x08
    float orient_y; // 0x14
    cs_shot_actor_options options; // 0x18
    int cur_playing_anim; // 0x28
    base_array<cs_shot_char_tod> time_of_days; // 0x2C
    base_array<cs_shot_char_animation> m_animations; // 0x3C
    base_array<cs_shot_char_animation> m_animation_states; // 0x4C
};

// size 0x80
struct cs_shot_camera
{
    cs_shot_camera(); // 0x00709A10

    tweener<vector> position; // 0x00
    tweener<vector> orient; // 0x28
    tweener<float> field_of_view; // 0x50
    tweener<float> dolly_in_pct; // 0x68
};

// size 0x8
struct cs_tod_character
{
    const char* lightset_filename; // 0x00
    light_set* lightset; // 0x04
};

// size 0x10
struct cs_shot_tod
{
    cs_shot_tod(); // 0x00709900
    ~cs_shot_tod(); // 0x00709910
    cs_shot_tod& operator=(const cs_shot_tod& __that); // 0x0070B0A0

    base_array<bool> lights_enabled; // 0x00
};

// size 0x28
struct cs_time_of_day
{
    cs_time_of_day(); // 0x00709810
    ~cs_time_of_day(); // 0x00709830
    cs_time_of_day& operator=(const cs_time_of_day& __that); // 0x0070B0E0

    unsigned int start_time; // 0x00
    unsigned int end_time; // 0x04
    base_array<cs_tod_character> characters; // 0x08
    base_array<char const*> light_names; // 0x18
};

// size 0x10
struct cs_camera_index_info
{
    const char* name; // 0x00
    vector position; // 0x04
};

// size 0xC
struct cutscene_effect
{
    unsigned int handle; // 0x00
    unsigned int srid; // 0x04
    bool preloaded; // 0x08
};

// size 0x138
struct cutscene_shot
{
    float shot_duration_sec() const; // 0x00705310 (.h:302)
    cutscene_shot(); // 0x0070D640
    ~cutscene_shot(); // 0x007099E0
    cutscene_shot& operator=(const cutscene_shot& __that); // 0x0070B140

    const char* slate_name; // 0x00
    vector slate_fiddle_pos; // 0x04
    float slate_fiddle_orient; // 0x10
    cs_shot_camera camera; // 0x14
    bool depth_of_field_enabled; // 0x94
    tweener<cs_depth_of_field> depth_of_field; // 0x98
    base_array<cs_shot_actor> m_characters; // 0xD8
    base_array<cs_shot_actor> m_vehicles; // 0xE8
    base_array<cs_shot_option> m_options; // 0xF8
    base_array<cutscene_effect> m_effect_infos; // 0x108
    base_array<cs_shot_tod> TODs; // 0x118
    float start_time; // 0x128
    unsigned int duration_fm; // 0x12C
    float fade_out_duration_sec; // 0x130
    bool fade_out_letterbox; // 0x134
};

// size 0x44
struct cutscene_slate
{
    cutscene_slate(); // 0x00705C20

    unsigned int m_object_handle; // 0x00
    bool m_loaded; // 0x04
    vector m_cutscene_pos; // 0x08
    vector m_world_pos; // 0x14
    matrix m_rotation; // 0x20
};

// size 0x8
struct cutscene_soundtrack
{
    int cue_id; // 0x00
    int play_id; // 0x04
};

// size 0x4
struct cutscene_info
{
    const char* name; // 0x00
};

// size 0x34
struct cutscene_navpoint
{
    cutscene_navpoint(); // 0x00705440

    char* name; // 0x00
    vector pos; // 0x04
    matrix orient; // 0x10
};

// size 0x38
struct cutscene_npc
{
    cutscene_npc(); // 0x00705450

    char* name; // 0x00
    vector pos; // 0x04
    matrix orient; // 0x10
    unsigned int handle; // 0x34
};

// size 0x38
struct cutscene_vehicle
{
    cutscene_vehicle(); // 0x00705460

    char* name; // 0x00
    vector pos; // 0x04
    matrix orient; // 0x10
    unsigned int handle; // 0x34
};

// size 0x1DC
struct cutscene_current_info
{
    cutscene_current_info(cutscene_info* const info); // 0x0070BEC0 (cutscene.cpp:244)

    cutscene_info* const cs_info; // 0x00
    const char* camera_script_filename; // 0x04
    cutscene_soundtrack soundtrack; // 0x08
    vector bbox_min; // 0x10
    vector bbox_max; // 0x1C
    farray<cutscene_slate,2> slates; // 0x28
    int current_slate; // 0xC0
    base_array<cs_time_of_day> time_of_days; // 0xC4
    const cs_time_of_day* current_tod; // 0xD4
    int current_tod_i; // 0xD8
    base_array<cutscene_npc*> cutscene_npcs; // 0xDC
    base_array<cutscene_vehicle*> cutscene_vehicles; // 0xEC
    const char* m_light_set_name; // 0xFC
    light_set* m_light_set; // 0x100
    bool first_shot_processed; // 0x104
    bool first_frame_fading; // 0x105
    base_array<int> m_effect_object_handles; // 0x108
    farray<unsigned int,1> hidden_object_handles; // 0x118
    base_array<cutscene_shot> m_shots; // 0x12C
    cutscene_shot* cur_shot; // 0x13C
    tweener<matrix> camera_orientation; // 0x140
    float total_cutscene_duration; // 0x198
    int bink_movie_handle; // 0x19C
    char* bink_material_name; // 0x1A0
    unsigned int m_slate_handle; // 0x1A4
    vector post_cutscene_restore_pos; // 0x1A8
    matrix post_cutscene_restore_orient; // 0x1B4
    int player_vehicle_handle; // 0x1D8
};

// size 0x54
struct cutscene_play_params
{
    cutscene_play_params(); // 0x00705480

    char name[32]; // 0x00
    unsigned int slate_handle; // 0x20
    vector restore_pos; // 0x24
    matrix restore_orient; // 0x30
};

// A file static in rfg.exe, where cutscene.cpp, cutscene_audio.cpp, cutscene_load.cpp, cutscene_options.cpp and
// cutscene_timer.cpp are one translation unit (their functions and statics interleave). External here, one file each.
extern cutscene_current_info* Current_cutscene; // 0x01E2B444
extern cutscene_play_params Cutscene_play_parameters; // 0x01E2B9B0
extern cutscene_timer Cutscene_timer; // 0x01E2B4AC
extern cutscene_npc Cutscene_npcs[12]; // 0x01E2B700
extern int Num_cutscene_npcs; // 0x01E2B3F8
extern cutscene_vehicle Cutscene_vehicles[6]; // 0x01E2B4C8
extern int Num_cutscene_vehicles; // 0x01E2B440
extern cutscene_navpoint Cutscene_navpoints[2]; // 0x01E2B618
extern int Num_cutscene_navpoints; // 0x01E2B3F4
extern bool Cutscene_skip_all; // 0x01E2B44E
extern char Bink_filename[33]; // 0x01E2B3D0
extern rfg_mover* Rfg_mover_playing_bink; // 0x01E2B46C
extern cpu_job* Cs_real_master_job; // 0x03758274
extern cpu_job* Cs_real_slave_job; // 0x03758270
