// Original: volition/rfg/code/scripting/public_address.h
// Implementation: volition/rfg/code/scripting/public_address.cpp
//
// Public address speakers and kiosks: speaker groups play ambient and scripted dialogue (district liberation, all clear),
// and the propaganda kiosks are unlocked by voice lines, missions and activities, with screen effects and action nodes.
// Placement of public_address_group_handle inferred.
#pragma once

#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class base_activity_data;
struct cfile;
class human_voice_persona;
struct kiosk_screen_effect;
class object;
class object_mover;

enum public_address_group_handle
{
    INVALID_PA_HANDLE = -1,
    PA_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

// size 0x8
struct public_address_speaker
{
    public_address_speaker() {} // inlined: the sort's temporaries
    public_address_speaker(public_address_group_handle _group, unsigned int _speaker); // 0x00B239F0 (:154)

    unsigned int speaker; // 0x00
    public_address_group_handle group; // 0x04
};

// size 0xA0
struct public_address_group
{
    public_address_group(); // 0x00B340F0 (:200)
    void clear(); // 0x00B2E3B0 (:203)
    ~public_address_group(); // 0x00B34160

    unsigned int name_crc; // 0x00
    bool is_edf; // 0x04
    bool play_edf_ambient_lines; // 0x05
    farray<enum voice_line_handle,3> ambient_lines; // 0x08
    int ambient_line_percent; // 0x24
    int ambient_min_delay_ms; // 0x28
    int ambient_max_delay_ms; // 0x2C
    voice_line_handle liberated_line_once; // 0x30
    farray<enum voice_line_handle,3> liberated_ambient_lines; // 0x34
    voice_line_handle dusk_line; // 0x50
    voice_line_handle dawn_line; // 0x54
    bool play_ambient_lines; // 0x58
    voice_line_handle override_ambient_line; // 0x5C
    int override_min_delay_ms; // 0x60
    int override_max_delay_ms; // 0x64
    human_voice_persona_handle override_persona; // 0x68
    timestamp ambient_timer; // 0x6C
    int last_played_id; // 0x70
    int prefix_play_id; // 0x74
    voice_line_handle last_root_voice_line_handle; // 0x78
    farray<enum voice_line_handle,5> queued_lines; // 0x7C
};

// size 0x24
struct kiosk_line
{
    kiosk_line(); // 0x00B2E3E0 (:237)
    void clear(); // 0x00B23A10 (:240)

    voice_line_handle line; // 0x00
    int min_play_count; // 0x04
    bool starts_unlocked; // 0x08
    mission_handle unlocked_by_mission; // 0x0C
    const base_activity_data* unlocked_by_activity; // 0x10
    kiosk_screen_effect* screen_effect; // 0x14
    int screen_effect_delay_ms; // 0x18
    int play_count; // 0x1C
    bool played_this_session; // 0x20
};

public_address_group_handle public_address_find_group(const char* const group_name); // 0x00B2E840 (:1952)
public_address_group_handle public_address_find_group(unsigned int name_crc); // 0x00B24130 (:1958)
void public_address_add_speaker(public_address_group_handle group, unsigned int speaker); // 0x00B34BD0 (:1970)
void public_address_add_speaker(const char* const group_name, unsigned int speaker); // 0x00B34C20 (:1988)
void public_address_remove_speaker(unsigned int speaker); // 0x00B34CA0 (:1993)
void public_address_set_active_group(public_address_group_handle group); // 0x00B24160 (:2019)
void public_address_unset_active_group(); // 0x00B24180 (:2026)
void public_address_override_ambient(public_address_group_handle group, voice_line_handle ambient_line, human_voice_persona_handle ambient_persona, int min_delay_ms, int max_delay_ms); // 0x00B3F550 (:2031)
void public_address_stop_ambient(public_address_group_handle group); // 0x00B24190 (:2043)
void public_address_restore_ambient(public_address_group_handle group); // 0x00B241B0 (:2050)
void public_address_start_dialogue(voice_line_handle line_handle, human_voice_persona* hvpp, public_address_group_handle group); // 0x00B3AF30 (:2062)
int public_address_play(int cue, public_address_group_handle group); // 0x00B34D60 (:2117)
void public_address_register_district_liberated(unsigned int handle); // 0x00B241E0 (:2195)
void public_address_ignore_next_all_clear_message(); // 0x00B241F0 (:2200)
object* kiosk_get_active_kiosk(); // 0x00B2E880 (:2212)
void kiosk_add_kiosk_object(unsigned int handle); // 0x00B34F30 (:2221)
void kiosk_unlock_from_voice_line_handle(voice_line_handle handle); // 0x00B34F40 (:2226)
void kiosk_unlock_from_mission(mission_handle handle); // 0x00B34FD0 (:2232)
void kiosk_unlock_from_activity(base_activity_data* base_data); // 0x00B35030 (:2242)
void kiosk_update_all_effects_and_action_nodes(); // 0x00B350A0 (:2252)
void kiosk_update_effects_and_action_nodes(object_mover* main_op); // 0x00B2E8B0 (:2265)
bool kiosk_mover_is_kiosk(const object_mover* omp); // 0x00B24210 (:2331)
void kiosk_handle_game_complete(); // 0x00B350F0 (:2336)
void public_address_kiosk_do_frame(); // 0x00B3F5A0 (:2393)
void public_address_kiosk_level_init_pre(); // 0x00B3F5B0 (:2404)
void public_address_kiosk_level_init_post(); // 0x00B35100 (:2475)
void public_address_kiosk_level_shutdown(); // 0x00B24240 (:2483)
void public_address_kiosk_system_init(); // 0x00B35140 (:2493)
void public_address_kiosk_save(cfile* save_file); // 0x00B2EA20 (:2506)
void public_address_kiosk_load(cfile* save_file); // 0x00B35160 (:2543)
human_voice_persona_handle public_address_kiosk_get_active_persona(); // 0x00B24260 (:2593)
void public_address_kiosk_restore(); // 0x00B2EB50 (:2603)
