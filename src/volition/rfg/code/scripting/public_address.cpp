// Original: volition/rfg/code/scripting/public_address.cpp
//
// Public address speakers and propaganda kiosks. Speakers belong to a group (pa_groups.xtbl) and the player's zone picks
// the range of speakers that can sound: ambient lines, dusk and dawn lines, the alert siren, the all clear. A kiosk near
// the player takes over the persona and plays its unlocked lines (kiosk_lines.xtbl) with music and screen effects.

#include "volition/rfg/code/scripting/public_address.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

// size 0x18
struct kiosk_screen_effect
{
    char name[16]; // 0x00
    unsigned int large_effect; // 0x10
    unsigned int small_effect; // 0x14
};

// Magic numbers: the sizes of the tables.
static const int MAX_PA_GROUPS = 32;
static const int MAX_KIOSK_SCREEN_EFFECTS = 16;
static const int MAX_KIOSK_LINES = 256;
static const int MAX_SIREN_PLAYS = 8;

// ---- file statics
static unsigned int Public_address_zone_handle = 0xFFFFFFFF; // 0x016B0CFC
static int Public_address_zone_upper_bound = -1; // 0x016B0D00
static int Public_address_zone_lower_bound = -1; // 0x016B0D04
static public_address_group_handle Active_group = INVALID_PA_HANDLE; // 0x016B0D08
static human_voice_persona_handle Public_address_voice_persona; // 0x016B0D0C
static voice_line_handle Public_address_edf_general_amb_line; // 0x016B0D10
static voice_line_handle Public_address_edf_alert_all_clear_line; // 0x016B0D14
static voice_line_handle Public_address_edf_alert_reinforcements_line; // 0x016B0D18
static voice_line_handle Public_address_civ_alert_all_clear_line; // 0x016B0D1C
static voice_line_handle Public_address_civ_alert_reinforcements_line; // 0x016B0D20
static int Public_address_siren_loop_foley; // 0x016B0D24
static int Public_address_siren_death_foley; // 0x016B0D28
static int Public_address_siren_loop_count = 1; // 0x016B0D2C
static int Public_address_siren_delay_ms; // 0x016B0D30
static int Public_address_siren_overlap_ms; // 0x016B0D34
static int Public_address_prefix_foley; // 0x016B0D3C
static unsigned int Public_address_recent_liberated_district = 0xFFFFFFFF; // 0x016B0D40
static float Public_address_min_liberated_dist_sq = 2500.0f; // 0x016B0D44
static int Kiosk_min_available_lines = 5; // 0x016B0D48
static float Kiosk_max_dist_sq = 2500.0f; // 0x016B0D4C
static int Kiosk_inactive_delay_ms; // 0x016B0D50
static int Kiosk_inactive_delay_long_ms; // 0x016B0D54
static int Kiosk_next_line_delay_ms; // 0x016B0D58
static int Kiosk_min_session_line_play_count; // 0x016B0D5C
static int Kiosk_max_session_line_play_count; // 0x016B0D60
static int Kiosk_play_id; // 0x016B0D64
static int Kiosk_music_stinger_cue_id; // 0x016B0D68
static int Kiosk_music_loop_cue_id; // 0x016B0D6C
static int Kiosk_music_out_cue_id; // 0x016B0D70
static int Kiosk_music_stinger_play_id; // 0x016B0D74
static int Kiosk_music_loop_play_id; // 0x016B0D78
static int Kiosk_music_out_play_id; // 0x016B0D7C
static voice_line_handle Kiosk_station_id_line; // 0x016B0D80
static farray<public_address_speaker,256> Public_address_speakers; // 0x016B2D28
static timestamp Public_address_siren_timer; // 0x016B3538
static farray<unsigned int,64> Kiosk_objects; // 0x016B3540
static farray<kiosk_line*,256> Kiosk_available_lines; // 0x016B3650
static timestamp Kiosk_inactive_timer; // 0x016B3A60
static timestamp Kiosk_next_line_timer; // 0x016B3A64
static timestamp Kiosk_screen_effect_timer; // 0x016B3A68
static int Kiosk_num_screen_effects; // 0x03611230
static kiosk_screen_effect* Kiosk_alert_screen_effect; // 0x03611234
static kiosk_screen_effect* Kiosk_next_active_screen_effect; // 0x03611238
static kiosk_screen_effect* Kiosk_rf_idle_screen_effect; // 0x0361123C
static kiosk_screen_effect* Kiosk_active_screen_effect; // 0x03611240
static kiosk_screen_effect* Kiosk_idle_screen_effect; // 0x0361124C
static int Public_address_num_groups; // 0x03611250
static int Kiosk_session_line_play_count_cutoff; // 0x03611254
static int Public_address_siren_play_id[MAX_SIREN_PLAYS]; // 0x03611258
static game_clock_time_of_day Public_address_current_tod; // 0x03611278
static int Kiosk_num_lines; // 0x03611280
static human_voice_persona_handle Kiosk_voice_persona; // 0x03611284
static int Kiosk_current_active_kiosk_index; // 0x0361128C
static kiosk_screen_effect Kiosk_screen_effects[MAX_KIOSK_SCREEN_EFFECTS]; // 0x03611290
static kiosk_screen_effect* Kiosk_current_active_screen_effect; // 0x03611418
static bool Public_address_speaker_list_is_sorted; // 0x0361141F
static bool Public_address_waiting_for_persona_load; // 0x03611428
static bool Public_address_ignore_next_all_clear; // 0x03611429
static bool Kiosk_is_active; // 0x0361142A
static bool Kiosk_should_deactivate; // 0x0361142B
static int Kiosk_session_line_play_count; // 0x0361142C
static int Kiosk_music_loop_delay_ms; // 0x03611430
static public_address_group Public_address_groups[MAX_PA_GROUPS]; // 0x03694768
static kiosk_line Kiosk_lines[MAX_KIOSK_LINES]; // 0x036F1340

static void kiosk_set_active(bool active);

// ---- public_address_speaker
// 0x00B239F0 (:154)
public_address_speaker::public_address_speaker(public_address_group_handle _group, unsigned int _speaker)
    : speaker(_speaker), group(_group)
{
}

// ---- public_address_group
// 0x00B340F0 (:200)
public_address_group::public_address_group()
{
    clear();
}

// 0x00B34160
public_address_group::~public_address_group()
{
}

// 0x00B2E3B0 (:203)
void public_address_group::clear()
{
    last_played_id = -1;
    prefix_play_id = -1;
    last_root_voice_line_handle = INVALID_VOICE_LINE_HANDLE;
    ambient_timer.invalidate();
    play_ambient_lines = true;
    override_ambient_line = INVALID_VOICE_LINE_HANDLE;
    queued_lines.clear();
}

// ---- kiosk_line
// 0x00B2E3E0 (:237)
kiosk_line::kiosk_line()
{
    clear();
}

// 0x00B23A10 (:240)
void kiosk_line::clear()
{
    line = INVALID_VOICE_LINE_HANDLE;
    min_play_count = 0;
    starts_unlocked = false;
    unlocked_by_mission = INVALID_MISSION_HANDLE;
    unlocked_by_activity = nullptr;
    screen_effect = nullptr;
    screen_effect_delay_ms = 0;
    play_count = 0;
    played_this_session = false;
}

// ---- public address, internal

// 0x00B2E410 (:403)
// Not a dead speaker, and not a general mover in the destroyed state. Magic number: 0x10, that mover flag.
static bool public_address_speaker_should_play_sounds(object* op)
{
    if (op == nullptr)
    {
        return false;
    }
    if (op->get_obj_type() == OT_MOVER && op->get_sub_type() == OT_SUB_MOVER_GENERAL && (((object_mover*)op)->mover_flags & 0x10) != 0)
    {
        return false;
    }
    return !op->is_killed();
}

// 0x00B23A40 (:419)
static bool public_address_valid_group_handle(public_address_group_handle group)
{
    return (int)group > -1 && (int)group < Public_address_num_groups;
}

// 0x00B23A60 (:424)
static bool public_address_siren_is_active()
{
    return Public_address_siren_timer.valid();
}

// 0x00B23A70 (:433)
static bool public_address_speaker_less_than_or_equal(const public_address_speaker& a, const public_address_speaker& b)
{
    return a.speaker <= b.speaker;
}

// 0x00B34170 (:438)
static void public_address_sort_speakers()
{
    Public_address_speakers.sort(public_address_speaker_less_than_or_equal);
    Public_address_speaker_list_is_sorted = true;
}

// 0x00B341A0 (:444)
static int public_address_find_speaker_index(unsigned int handle)
{
    if (!Public_address_speaker_list_is_sorted)
    {
        public_address_sort_speakers();
    }
    int low = 0;
    int high = Public_address_speakers.size() - 1;
    while (low <= high)
    {
        const int middle = (high + low) / 2;
        const unsigned int speaker = Public_address_speakers[middle].speaker;
        if (handle < speaker)
        {
            high = middle - 1;
        }
        else if (handle > speaker)
        {
            low = middle + 1;
        }
        else
        {
            return middle;
        }
    }
    return -1;
}

// 0x00B34210 (:467)
// The speakers of a zone sit together, since the top byte of a handle is its zone. Finds one by that byte, and walks out to
// both ends of the run. Original: the walk down steps before it looks, so when the search lands on the first speaker of
// the run the last speaker of the zone before it is taken in too.
static bool public_address_find_speaker_index_range(unsigned int zone_handle)
{
    if (!Public_address_speaker_list_is_sorted)
    {
        public_address_sort_speakers();
    }
    Public_address_zone_upper_bound = -1;
    Public_address_zone_lower_bound = -1;
    if (zone_handle == 0xFFFFFFFF)
    {
        zone_handle = Public_address_zone_handle;
        if (zone_handle == 0xFFFFFFFF)
        {
            return false;
        }
    }
    const unsigned char zone = (unsigned char)(zone_handle >> 24);
    int low = 0;
    const int last = Public_address_speakers.size() - 1;
    int high = last;
    while (low <= high)
    {
        const int middle = (high + low) / 2;
        const unsigned char speaker_zone = (unsigned char)(Public_address_speakers[middle].speaker >> 24);
        if (zone < speaker_zone)
        {
            high = middle - 1;
        }
        else if (zone > speaker_zone)
        {
            low = middle + 1;
        }
        else
        {
            int lower = middle;
            while (lower > 0)
            {
                lower--;
                if (lower < 1 || (unsigned char)(Public_address_speakers[lower - 1].speaker >> 24) != zone)
                {
                    break;
                }
            }
            Public_address_zone_lower_bound = lower;
            int upper = middle;
            while (upper < last && (unsigned char)(Public_address_speakers[upper + 1].speaker >> 24) == zone)
            {
                upper++;
            }
            Public_address_zone_upper_bound = upper;
            return true;
        }
    }
    return false;
}

// 0x00B3A940 (:519)
// LTCG: the list in EDI.
static void public_address_get_current_group_list(farray<public_address_group_handle,16>& list)
{
    list.clear();
    if (!Public_address_speaker_list_is_sorted)
    {
        public_address_find_speaker_index_range(0xFFFFFFFF);
    }
    if (Public_address_zone_lower_bound != -1)
    {
        for (int i = Public_address_zone_lower_bound; i <= Public_address_zone_upper_bound; i++)
        {
            list.add_unique(Public_address_speakers[i].group);
        }
    }
}

// 0x00B23A80 (:538)
// LTCG: the group in EAX and the extra time in ESI.
static void public_address_set_ambient_timer(public_address_group_handle group, int extra_time)
{
    public_address_group& pa = Public_address_groups[group];
    if (pa.override_ambient_line != INVALID_VOICE_LINE_HANDLE)
    {
        pa.ambient_timer.set_rand(pa.override_min_delay_ms + extra_time, pa.override_max_delay_ms + extra_time);
    }
    else
    {
        pa.ambient_timer.set_rand(pa.ambient_min_delay_ms + extra_time, pa.ambient_max_delay_ms + extra_time);
    }
}

// 0x00B23AD0 (:551)
static bool public_address_current_district_liberated()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pp->pos);
    return dp != nullptr && dp->is_liberated();
}

// 0x00B3EBB0 (:566)
// LTCG: the group in EDI.
static void public_address_ambient_play(public_address_group_handle group)
{
    if (!public_address_valid_group_handle(group))
    {
        return;
    }
    public_address_group& pa = Public_address_groups[group];
    // Magic number: 20 s until the group tries again.
    pa.ambient_timer.set(20000);
    human_voice_persona* persona = human_voice_persona_from_handle(Public_address_voice_persona);
    if (persona == nullptr)
    {
        return;
    }
    if (pa.override_ambient_line != INVALID_VOICE_LINE_HANDLE)
    {
        persona = human_voice_persona_from_handle(pa.override_persona);
        if (persona != nullptr)
        {
            public_address_start_dialogue(pa.override_ambient_line, persona, group);
        }
        return;
    }
    voice_line_handle line = INVALID_VOICE_LINE_HANDLE;
    if (public_address_current_district_liberated())
    {
        pa.liberated_ambient_lines.get_random_element(line);
    }
    else if (!pa.play_edf_ambient_lines)
    {
        pa.ambient_lines.get_random_element(line);
    }
    else
    {
        // EDF groups mostly play the general line, and their own lines by percentage.
        if (int_rand_chance(pa.ambient_line_percent))
        {
            pa.ambient_lines.get_random_element(line);
        }
        if (line == INVALID_VOICE_LINE_HANDLE)
        {
            line = Public_address_edf_general_amb_line;
        }
    }
    if (line != INVALID_VOICE_LINE_HANDLE)
    {
        public_address_start_dialogue(line, persona, group);
    }
}

// Every speaker of the zone that can sound, up to the 16 emitters of one play.
static void public_address_gather_siren_positions(audiolib_params& params)
{
    for (int i = Public_address_zone_lower_bound; i <= Public_address_zone_upper_bound; i++)
    {
        object* op = World.object_from_handle(Public_address_speakers[i].speaker);
        if (public_address_speaker_should_play_sounds(op))
        {
            // Magic number: audiolib_params::position_list has 16 entries.
            if (params.num_positions > 15)
            {
                break;
            }
            params.position_list[params.num_positions] = op->pos;
            params.num_positions++;
        }
    }
}

// 0x00B34320 (:618)
static void public_address_play_siren(int& play_id, int delay_ms)
{
    if (!Public_address_speaker_list_is_sorted)
    {
        public_address_find_speaker_index_range(0xFFFFFFFF);
    }
    if (Public_address_zone_lower_bound == -1)
    {
        return;
    }
    audiolib_params params;
    public_address_gather_siren_positions(params);
    if (params.num_positions > 0)
    {
        // Magic number: 0x81, the 3D and multiple-emitter flags of a play.
        params.flags = 0x81;
        params.delay_ms = delay_ms;
        play_id = audiolib_cue_play(Public_address_siren_loop_foley, &params, nullptr);
    }
}

// 0x00B34440 (:657)
// The loops are queued one after the other, overlapping a little, and the timer runs to the end of the last one plus the
// delay.
static void public_address_start_siren()
{
    if (audiolib_is_playing(Public_address_siren_play_id[0], true))
    {
        return;
    }
    public_address_play_siren(Public_address_siren_play_id[0], 0);
    int length = 0;
    audiolib_play_get_play_time(Public_address_siren_play_id[0], &length, nullptr);
    if (length > 0)
    {
        for (int i = 1; i < Public_address_siren_loop_count; i++)
        {
            public_address_play_siren(Public_address_siren_play_id[i], (length - Public_address_siren_overlap_ms) * i);
        }
    }
    Public_address_siren_timer.set((length - Public_address_siren_overlap_ms) * Public_address_siren_loop_count + Public_address_siren_delay_ms);
}

// 0x00B23B00 (:682)
static void public_address_stop_siren()
{
    if (Public_address_siren_timer.valid())
    {
        Public_address_siren_timer.invalidate();
    }
}

// 0x00B23B20 (:700)
// LTCG: the speaker in EAX and the group in ECX.
static void public_address_stop_speaker(public_address_group_handle group, unsigned int speaker)
{
    const object* op = World.object_from_handle(speaker);
    if (op != nullptr)
    {
        audiolib_play_remove_emitter(Public_address_groups[group].last_played_id, const_cast<vector*>(&op->pos));
        audiolib_play_remove_emitter(Public_address_groups[group].prefix_play_id, const_cast<vector*>(&op->pos));
    }
}

// 0x00B23B60 (:711)
static void public_address_kill_siren_at_speaker(unsigned int speaker)
{
    const object* op = World.object_from_handle(speaker);
    if (op == nullptr)
    {
        return;
    }
    bool removed = false;
    for (int i = 0; i < Public_address_siren_loop_count; i++)
    {
        if (audiolib_play_remove_emitter(Public_address_siren_play_id[i], const_cast<vector*>(&op->pos)) == AUDIOLIB_ERR_NONE)
        {
            removed = true;
        }
    }
    if (removed)
    {
        audiolib_cue_play_3d(Public_address_siren_death_foley, &op->pos, nullptr, nullptr, nullptr);
    }
}

// 0x00B34600 (:734)
static event_result public_address_object_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DESTROYED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int handle = ((const building_destroyed_event&)in_event).building;
    const int index = public_address_find_speaker_index(handle);
    if (index == -1)
    {
        return EVENT_IGNORED;
    }
    public_address_stop_speaker(Public_address_speakers[index].group, handle);
    public_address_kill_siren_at_speaker(handle);
    return EVENT_OK;
}

// 0x00B3ECC0 (:752)
static void public_address_play_dusk_dawn_lines(bool dusk)
{
    farray<public_address_group_handle,16> groups;
    public_address_get_current_group_list(groups);
    for (int i = 0; i < groups.size(); i++)
    {
        const public_address_group_handle group = groups[i];
        const voice_line_handle line = dusk ? Public_address_groups[group].dusk_line : Public_address_groups[group].dawn_line;
        if (line != INVALID_VOICE_LINE_HANDLE)
        {
            public_address_start_dialogue(line, nullptr, group);
        }
    }
}

// 0x00B3ED50 (:766)
// rfg.exe inlines public_address_start_dialogue. Original: starting a line while the kiosk holds the persona queues it
// on this same list and the end of this function then empties the list.
// Deviation from rfg.exe: a separate function, which it inlines in the group update.
static void public_address_group_play_queued_lines(public_address_group_handle group)
{
    farray<voice_line_handle,5>& queued = Public_address_groups[group].queued_lines;
    for (int i = 0; i < queued.size(); i++)
    {
        public_address_start_dialogue(queued[i], nullptr, group);
    }
    queued.clear();
}

// 0x00B3EEB0 (:775)
static void public_address_do_frame()
{
    const player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const obj_zone* zone = World.get_zone_by_pos(pp->pos);
        if (zone == nullptr)
        {
            return;
        }
        if (zone->get_handle() != Public_address_zone_handle || !Public_address_speaker_list_is_sorted)
        {
            public_address_find_speaker_index_range(zone->get_handle());
            Public_address_zone_handle = zone->get_handle();
        }
    }
    if (Public_address_zone_lower_bound == -1)
    {
        return;
    }
    // A speaker knocked loose stops sounding.
    for (int i = Public_address_zone_lower_bound; i <= Public_address_zone_upper_bound; i++)
    {
        const object* op = World.object_from_handle(Public_address_speakers[i].speaker);
        if (op != nullptr && op->havok_handle != 0xFFFFFFFF)
        {
            vector velocity;
            havok_body_get_linear_velocity(op->havok_handle, velocity);
            if (!velocity.is_null())
            {
                public_address_stop_speaker(Public_address_speakers[i].group, Public_address_speakers[i].speaker);
                public_address_kill_siren_at_speaker(Public_address_speakers[i].speaker);
            }
        }
    }
    if (Public_address_siren_timer.elapsed())
    {
        public_address_start_siren();
    }
    if (Public_address_waiting_for_persona_load)
    {
        if (!human_voice_persona_from_handle(Public_address_voice_persona)->is_loaded())
        {
            return;
        }
        Public_address_waiting_for_persona_load = false;
        for (int group = 0; group < Public_address_num_groups; group++)
        {
            public_address_group_play_queued_lines((public_address_group_handle)group);
        }
    }
    const game_clock_time_of_day tod = game_clock_get_time_of_day();
    if (!Kiosk_is_active && tod != Public_address_current_tod)
    {
        // Original: the current time of day only moves on when the kiosk is quiet, and a change while the alert is up is
        // lost, not delayed.
        if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && !public_address_current_district_liberated())
        {
            public_address_play_dusk_dawn_lines(tod == GC_NIGHT);
        }
        Public_address_current_tod = tod;
    }
    // One random speaker a frame decides for its group.
    const int index = int_rand_range(Public_address_zone_lower_bound, Public_address_zone_upper_bound);
    object* op = World.object_from_handle(Public_address_speakers[index].speaker);
    if (!public_address_speaker_should_play_sounds(op))
    {
        return;
    }
    const public_address_group_handle group = Public_address_speakers[index].group;
    public_address_group& pa = Public_address_groups[group];
    const district* dp = district_get_from_pos(op->pos);
    if (dp != nullptr && dp->get_handle() == Public_address_recent_liberated_district && pa.liberated_line_once != INVALID_VOICE_LINE_HANDLE && pp != nullptr)
    {
        if (!Kiosk_is_active && gsm_get_alert_level() == ALERT_LEVEL_GREEN && vec_dist_squared(op->pos, pp->pos) <= Public_address_min_liberated_dist_sq)
        {
            public_address_start_dialogue(pa.liberated_line_once, nullptr, group);
            Public_address_recent_liberated_district = 0xFFFFFFFF;
        }
        return;
    }
    // Held back: the timer is pushed out each frame.
    if (Kiosk_is_active || ((int)gsm_get_alert_level() > 0 && pa.override_ambient_line == INVALID_VOICE_LINE_HANDLE) || !pa.play_ambient_lines)
    {
        public_address_set_ambient_timer(group, 0);
    }
    if (pa.ambient_timer.elapsed())
    {
        public_address_ambient_play(group);
    }
}

// 0x00B34680 (:920)
static void public_address_groups_table_load()
{
    xml_element* root = xml_table_parse("pa_groups.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Public_address_num_groups = 0;
    for (xml_element* elem = root->elements; elem != nullptr; elem = elem->next)
    {
        if (v_stricmp(elem->name, "PA_Group") != 0)
        {
            continue;
        }
        if (Public_address_num_groups >= MAX_PA_GROUPS)
        {
            break;
        }
        public_address_group& pa = Public_address_groups[Public_address_num_groups];
        pa.name_crc = crc_stri(xml_table_get_req_string_ref(elem, "Name"), 0);
        xml_table_get_req_bool(pa.is_edf, elem, "Is_EDF");
        xml_table_get_req_bool(pa.play_edf_ambient_lines, elem, "Play_EDF_Ambient_Lines");
        pa.ambient_lines.clear();
        xml_element* lines = xml_table_find_opt(elem, "Ambient_Lines");
        if (lines != nullptr)
        {
            for (xml_element* line = xml_find(lines, "Ambient_Line"); line != nullptr; line = xml_find_next(lines, line, "Ambient_Line"))
            {
                const voice_line_handle handle = voice_line_handle_from_name(line->text);
                if (handle != INVALID_VOICE_LINE_HANDLE)
                {
                    pa.ambient_lines.add(handle);
                }
            }
        }
        const char* once = xml_table_get_opt_string_ref(elem, "Liberated_Line_Once");
        pa.liberated_line_once = once != nullptr ? voice_line_handle_from_name(once) : INVALID_VOICE_LINE_HANDLE;
        pa.liberated_ambient_lines.clear();
        lines = xml_table_find_opt(elem, "Liberated_Ambient_Lines");
        if (lines != nullptr)
        {
            for (xml_element* line = xml_find(lines, "Ambient_Line"); line != nullptr; line = xml_find_next(lines, line, "Ambient_Line"))
            {
                const voice_line_handle handle = voice_line_handle_from_name(line->text);
                if (handle != INVALID_VOICE_LINE_HANDLE)
                {
                    pa.liberated_ambient_lines.add(handle);
                }
            }
        }
        pa.ambient_line_percent = 0;
        xml_table_get_opt_int(pa.ambient_line_percent, elem, "Ambient_line_percent");
        float min_delay = 0.0f;
        float max_delay = 0.0f;
        xml_table_get_req_float(min_delay, elem, "Min_Ambient_Delay");
        xml_table_get_req_float(max_delay, elem, "Max_Ambient_Delay");
        if (max_delay < min_delay)
        {
            min_delay = max_delay;
        }
        // Magic number: the table is in seconds.
        pa.ambient_min_delay_ms = (int)(min_delay * 1000.0f);
        pa.ambient_max_delay_ms = (int)(max_delay * 1000.0f);
        const char* dusk = xml_table_get_opt_string_ref(elem, "Dusk_Line");
        pa.dusk_line = dusk != nullptr ? voice_line_handle_from_name(dusk) : INVALID_VOICE_LINE_HANDLE;
        const char* dawn = xml_table_get_opt_string_ref(elem, "Dawn_Line");
        pa.dawn_line = dawn != nullptr ? voice_line_handle_from_name(dawn) : INVALID_VOICE_LINE_HANDLE;
        Public_address_num_groups++;
    }
    xml_table_close(root);
}

// ---- kiosks, internal

// 0x00B2E440 (:1033)
// The first live kiosk within reach of the player, level distance.
static int kiosk_find_potential_kiosk_index()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < Kiosk_objects.size(); i++)
    {
        const object* op = World.object_from_handle(Kiosk_objects[i]);
        if (op != nullptr && !op->is_killed())
        {
            const float dx = pp->pos.x - op->pos.x;
            const float dz = pp->pos.z - op->pos.z;
            if (dz * dz + dx * dx < Kiosk_max_dist_sq)
            {
                return i;
            }
        }
    }
    return -1;
}

// 0x00B2E500 (:1059)
// Magic number: 3, the highest alert level, so the effect sees the level as a percentage.
static void kiosk_set_effect_variable(unsigned int effect_handle)
{
    if (effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* ep = World.object_from_handle<object_effect>(effect_handle);
    if (ep != nullptr)
    {
        ep->set_expression_variable(EFFECTS_VAR_PERCENT, (float)(int)gsm_get_alert_level() / 3.0f);
    }
}

// 0x00B2E550 (:1069)
// LTCG: the kiosk in EAX and the index in ECX. Restarts the screen's effect for the kiosk's state. The state zero of a
// script point is its live state.
static bool kiosk_start_effect_on_screen(object_mover* kiosk_op, int sp_idx)
{
    if (kiosk_op == nullptr)
    {
        return false;
    }
    const level_object_inst_properties* props = kiosk_op->m_level_obj_properties;
    script_point_effect_inst* inst = props->m_script_point_effect_insts[sp_idx];
    if (inst == nullptr || inst->state != 0)
    {
        return false;
    }
    const script_point_effect_info* point_info = props->m_base_properties->m_script_point_effect_infos[sp_idx];
    World.destroy_object(inst->current_effect, false);
    kiosk_screen_effect* screen = Kiosk_idle_screen_effect;
    if (Kiosk_is_active)
    {
        screen = Kiosk_current_active_screen_effect;
    }
    else if ((int)gsm_get_alert_level() >= 1)
    {
        screen = Kiosk_alert_screen_effect;
    }
    else if (mission_final_mission_complete())
    {
        // Once the game is won the station id goes too.
        screen = Kiosk_rf_idle_screen_effect;
        Kiosk_station_id_line = INVALID_VOICE_LINE_HANDLE;
    }
    if (screen == nullptr)
    {
        return false;
    }
    unsigned int effect;
    // Magic numbers: spe_flags 0x40 is a small screen and 0x20 a large one.
    if ((point_info->spe_flags & 0x40) != 0)
    {
        effect = screen->small_effect;
    }
    else if ((point_info->spe_flags & 0x20) != 0)
    {
        effect = screen->large_effect;
    }
    else
    {
        return false;
    }
    if (effect == 0xFFFFFFFF)
    {
        return false;
    }
    vector tag_pos;
    matrix tag_orient;
    kiosk_op->get_tag_world_coords(inst->script_point_idx, tag_pos, &tag_orient, nullptr, nullptr);
    inst->current_effect = effects_play(effect, tag_pos, tag_orient, true, kiosk_op->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    kiosk_set_effect_variable(inst->current_effect);
    return inst->current_effect != 0xFFFFFFFF;
}

// 0x00B3A990 (:1123)
// LTCG: active in AL. Original: in multiplayer it only clears the flag.
static void kiosk_set_active(bool active)
{
    if (Multiplayer)
    {
        Kiosk_is_active = false;
        return;
    }
    Kiosk_is_active = active;
    human_voice_persona* persona = human_voice_persona_from_handle(active ? Kiosk_voice_persona : Public_address_voice_persona);
    if (persona == nullptr)
    {
        return;
    }
    persona->background_load_request();
    if (!Kiosk_is_active)
    {
        Kiosk_inactive_timer.set(Kiosk_inactive_delay_ms);
        Kiosk_next_line_timer.invalidate();
        Kiosk_screen_effect_timer.invalidate();
        Kiosk_current_active_kiosk_index = -1;
        audiolib_play_stop(Kiosk_play_id);
        audiolib_play_stop(Kiosk_music_stinger_play_id);
        audiolib_play_stop(Kiosk_music_loop_play_id);
        audiolib_play_stop(Kiosk_music_out_play_id);
        if (!persona->is_loaded())
        {
            Public_address_waiting_for_persona_load = true;
        }
        kiosk_update_all_effects_and_action_nodes();
        return;
    }
    Kiosk_inactive_timer.invalidate();
    Kiosk_next_line_timer.set(0);
    Kiosk_screen_effect_timer.invalidate();
    Kiosk_session_line_play_count = 0;
    Kiosk_should_deactivate = false;
    for (int i = 0; i < Kiosk_available_lines.size(); i++)
    {
        Kiosk_available_lines[i]->played_this_session = false;
    }
    Kiosk_current_active_screen_effect = Kiosk_active_screen_effect;
    Kiosk_session_line_play_count_cutoff = int_rand_range(Kiosk_min_session_line_play_count, Kiosk_max_session_line_play_count);
    kiosk_update_all_effects_and_action_nodes();
}

// 0x00B3AAD0 (:1183)
static event_result kiosk_building_destroyed_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() == EVENT_BUILDING_DESTROYED && Kiosk_current_active_kiosk_index != -1 && ((const building_destroyed_event&)in_event).building == Kiosk_objects[Kiosk_current_active_kiosk_index])
    {
        kiosk_set_active(false);
        return EVENT_OK;
    }
    return EVENT_IGNORED;
}

// 0x00B2E670 (:1202)
// With too many lines waiting, the most played one past its minimum makes room.
static void kiosk_maybe_remove_available_line()
{
    if (Kiosk_available_lines.size() <= Kiosk_min_available_lines)
    {
        return;
    }
    int most_over = -1;
    int index = -1;
    for (int i = 0; i < Kiosk_available_lines.size(); i++)
    {
        const int over = Kiosk_available_lines[i]->play_count - Kiosk_available_lines[i]->min_play_count;
        if (most_over < over)
        {
            most_over = over;
            index = i;
        }
    }
    if (index != -1)
    {
        Kiosk_available_lines.remove(index);
    }
}

// 0x00B23BC0 (:1230)
// LTCG: the handle in ESI.
static int kiosk_line_index_from_voice_line_handle(voice_line_handle line_handle)
{
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        if (Kiosk_lines[i].line == line_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x00B349C0 (:1253)
// LTCG: the index in ESI. Lines given by a mission are unlocked even after the game is won.
static void kiosk_unlock_from_index(int idx, bool from_mission)
{
    if (idx < 0 || idx >= Kiosk_num_lines)
    {
        return;
    }
    if ((!mission_final_mission_complete() || from_mission) && Kiosk_lines[idx].play_count < Kiosk_lines[idx].min_play_count)
    {
        Kiosk_available_lines.add_unique(&Kiosk_lines[idx]);
        kiosk_maybe_remove_available_line();
    }
}

// 0x00B34A20 (:1273)
static void kiosk_init_available_lines()
{
    Kiosk_available_lines.clear();
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        if (Kiosk_lines[i].starts_unlocked)
        {
            Kiosk_available_lines.add(&Kiosk_lines[i]);
        }
    }
}

// 0x00B3AB10 (:1285)
// Outside missions, at no alert, once the quiet time is over, when a kiosk is in reach.
static bool kiosk_maybe_activate(int& best_kiosk_idx)
{
    if (mission_is_active())
    {
        return false;
    }
    if ((int)gsm_get_alert_level() >= 1)
    {
        Kiosk_inactive_timer.set(Kiosk_inactive_delay_ms);
        return false;
    }
    if (!Kiosk_inactive_timer.elapsed())
    {
        return false;
    }
    best_kiosk_idx = kiosk_find_potential_kiosk_index();
    if (best_kiosk_idx == -1)
    {
        Kiosk_inactive_timer.set(Kiosk_inactive_delay_ms);
        return false;
    }
    kiosk_set_active(true);
    return true;
}

// 0x00B2E6E0 (:1314)
// The station id and the outro music, then kiosk_do_frame ends the session when the timer runs out.
static void kiosk_start_deactivate()
{
    Kiosk_should_deactivate = true;
    audiolib_play_stop(Kiosk_music_loop_play_id);
    const object* op = World.object_from_handle(Kiosk_objects[Kiosk_current_active_kiosk_index]);
    if (op == nullptr)
    {
        return;
    }
    human_voice_persona* persona = human_voice_persona_from_handle(Kiosk_is_active ? Kiosk_voice_persona : Public_address_voice_persona);
    if (persona == nullptr || !persona->is_loaded())
    {
        return;
    }
    unsigned int delay = 0;
    unsigned short lipsync;
    if (persona->persona_play_voice_line(Kiosk_station_id_line, op->pos, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, Kiosk_play_id, lipsync, nullptr, false, false, true, true, nullptr))
    {
        audiolib_play_get_play_time(Kiosk_play_id, (int*)&delay, nullptr);
    }
    audiolib_params params;
    params.flags = 1;
    params.pos = op->pos;
    params.delay_ms = delay;
    Kiosk_music_out_play_id = audiolib_cue_play(Kiosk_music_out_cue_id, &params, nullptr);
    int music_length = 0;
    audiolib_play_get_play_time(Kiosk_music_out_play_id, &music_length, nullptr);
    Kiosk_next_line_timer.set(delay + music_length);
}

// 0x00B3AB70 (:1356)
static void kiosk_update_current_sound_pos()
{
    if (Kiosk_current_active_kiosk_index == -1)
    {
        return;
    }
    const object* op = World.object_from_handle(Kiosk_objects[Kiosk_current_active_kiosk_index]);
    if (op == nullptr)
    {
        kiosk_set_active(false);
        return;
    }
    audiolib_play_set_3d_position(Kiosk_play_id, &op->pos, nullptr, nullptr);
    audiolib_play_set_3d_position(Kiosk_music_stinger_play_id, &op->pos, nullptr, nullptr);
    audiolib_play_set_3d_position(Kiosk_music_loop_play_id, &op->pos, nullptr, nullptr);
    audiolib_play_set_3d_position(Kiosk_music_out_play_id, &op->pos, nullptr, nullptr);
}

// 0x00B23BF0 (:1375)
static kiosk_screen_effect* kiosk_get_screen_effect_from_name(const char* const name)
{
    for (int i = 0; i < Kiosk_num_screen_effects; i++)
    {
        // Magic number: 0x10, the size of the name.
        if (v_strnicmp(Kiosk_screen_effects[i].name, name, 0x10) == 0)
        {
            return &Kiosk_screen_effects[i];
        }
    }
    return nullptr;
}

// 0x00B34A90 (:1387)
// A random one of the least played lines not played this session.
static kiosk_line* kiosk_find_available_line()
{
    farray<kiosk_line*,256> choices;
    human_voice_persona* persona = human_voice_persona_from_handle(Kiosk_voice_persona);
    for (int i = 0; i < Kiosk_available_lines.size(); i++)
    {
        kiosk_line* line = Kiosk_available_lines[i];
        if (line->played_this_session || !persona->can_play_line(line->line, 0, nullptr))
        {
            continue;
        }
        if (choices.size() > 0 && line->play_count < choices[0]->play_count)
        {
            choices.clear();
        }
        else if (choices.size() != 0 && line->play_count != choices[0]->play_count)
        {
            continue;
        }
        choices.add(line);
    }
    if (choices.size() <= 0)
    {
        return nullptr;
    }
    if (choices.size() == 1)
    {
        return choices[0];
    }
    return choices[int_rand_range(0, choices.size() - 1)];
}

// 0x00B3ABF0 (:1424)
static void kiosk_do_frame()
{
    if (!Kiosk_is_active)
    {
        if (!Kiosk_inactive_timer.valid())
        {
            Kiosk_inactive_timer.set_rand(Kiosk_inactive_delay_ms, Kiosk_inactive_delay_ms * 2);
        }
        int kiosk_index = -1;
        if (kiosk_maybe_activate(kiosk_index))
        {
            const object* op = World.object_from_handle(Kiosk_objects[kiosk_index]);
            if (op != nullptr)
            {
                audiolib_params params;
                params.pos = op->pos;
                params.flags = 1;
                Kiosk_music_stinger_play_id = audiolib_cue_play(Kiosk_music_stinger_cue_id, &params, nullptr);
                params.delay_ms = Kiosk_music_loop_delay_ms;
                Kiosk_music_loop_play_id = audiolib_cue_play(Kiosk_music_loop_cue_id, &params, nullptr);
                Kiosk_next_line_timer.set(Kiosk_next_line_delay_ms + Kiosk_music_loop_delay_ms);
                Kiosk_current_active_kiosk_index = kiosk_index;
            }
        }
        return;
    }
    human_voice_persona* persona = human_voice_persona_from_handle(Kiosk_voice_persona);
    if (persona == nullptr || !persona->is_loaded())
    {
        return;
    }
    if (!Kiosk_next_line_timer.elapsed())
    {
        if (Kiosk_next_line_timer.valid())
        {
            if (Kiosk_screen_effect_timer.elapsed())
            {
                Kiosk_current_active_screen_effect = Kiosk_next_active_screen_effect;
                Kiosk_next_active_screen_effect = nullptr;
                kiosk_update_all_effects_and_action_nodes();
                Kiosk_screen_effect_timer.invalidate();
            }
            kiosk_update_current_sound_pos();
        }
        return;
    }
    if (Kiosk_should_deactivate)
    {
        kiosk_set_active(false);
        Kiosk_inactive_timer.set(Kiosk_inactive_delay_long_ms);
        return;
    }
    const int kiosk_index = kiosk_find_potential_kiosk_index();
    kiosk_line* line = nullptr;
    if (Kiosk_session_line_play_count < Kiosk_session_line_play_count_cutoff && kiosk_index != -1)
    {
        line = kiosk_find_available_line();
    }
    if (line == nullptr)
    {
        kiosk_start_deactivate();
        return;
    }
    const object* op = World.object_from_handle(Kiosk_objects[kiosk_index]);
    if (op == nullptr)
    {
        return;
    }
    unsigned short lipsync;
    persona->persona_play_voice_line(line->line, op->pos, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, Kiosk_play_id, lipsync, nullptr, false, false, true, true, nullptr);
    if (Kiosk_play_id == -1)
    {
        kiosk_set_active(false);
        return;
    }
    // Magic number: 10 s when the line's time is not known.
    int length = 10000;
    audiolib_play_get_play_time(Kiosk_play_id, &length, nullptr);
    Kiosk_next_line_timer.set(length + Kiosk_next_line_delay_ms);
    line->play_count++;
    Kiosk_session_line_play_count++;
    line->played_this_session = true;
    if (line->screen_effect != nullptr)
    {
        Kiosk_screen_effect_timer.set(line->screen_effect_delay_ms);
        Kiosk_next_active_screen_effect = line->screen_effect;
    }
    if (!Kiosk_screen_effect_timer.valid() || !Kiosk_screen_effect_timer.elapsed())
    {
        Kiosk_current_active_screen_effect = Kiosk_active_screen_effect;
        kiosk_update_all_effects_and_action_nodes();
    }
    if (line->min_play_count <= line->play_count && Kiosk_min_available_lines < Kiosk_available_lines.size())
    {
        Kiosk_available_lines.remove_matching(line);
    }
}

// 0x00B23C40 (:1554)
static void kiosk_screen_table_load()
{
    xml_element* root = xml_table_parse("kiosk_screens.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Kiosk_num_screen_effects = 0;
    for (xml_element* elem = xml_find(root, "Kiosk_Screen"); elem != nullptr && Kiosk_num_screen_effects < MAX_KIOSK_SCREEN_EFFECTS; elem = xml_find_next(root, elem, "Kiosk_Screen"))
    {
        kiosk_screen_effect& screen = Kiosk_screen_effects[Kiosk_num_screen_effects];
        xml_table_get_req_string(screen.name, 0x10, elem, "Name");
        screen.large_effect = effects_get_handle(xml_table_get_req_string_ref(elem, "Large_Effect"));
        screen.small_effect = effects_get_handle(xml_table_get_req_string_ref(elem, "Small_Effect"));
        Kiosk_num_screen_effects++;
    }
    xml_table_close(root);
}

// 0x00B23D00 (:1584)
// A line whose voice line is unknown is skipped, and its slot used for the next.
static void kiosk_line_table_load()
{
    xml_element* root = xml_table_parse("kiosk_lines.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Kiosk_num_lines = 0;
    for (xml_element* elem = xml_find(root, "Kiosk_Line"); elem != nullptr && Kiosk_num_lines < MAX_KIOSK_LINES; elem = xml_find_next(root, elem, "Kiosk_Line"))
    {
        kiosk_line& line = Kiosk_lines[Kiosk_num_lines];
        line.clear();
        line.line = voice_line_handle_from_name(xml_table_get_req_string_ref(elem, "Name"));
        if (line.line == INVALID_VOICE_LINE_HANDLE)
        {
            continue;
        }
        xml_table_get_req_int(line.min_play_count, elem, "Min_replay_count");
        if (line.min_play_count < 1)
        {
            line.min_play_count = 1;
        }
        xml_table_get_req_bool(line.starts_unlocked, elem, "Starts_unlocked");
        const char* text = xml_table_get_opt_string_ref(elem, "Unlocked_by_mission");
        if (text != nullptr)
        {
            line.unlocked_by_mission = mission_handle_find(text);
        }
        text = xml_table_get_opt_string_ref(elem, "Unlocked_by_activity");
        if (text != nullptr)
        {
            line.unlocked_by_activity = activity_data_find_handle(text);
        }
        text = xml_table_get_opt_string_ref(elem, "Screen_Effect");
        if (text != nullptr)
        {
            line.screen_effect = kiosk_get_screen_effect_from_name(text);
        }
        xml_table_get_opt_int(line.screen_effect_delay_ms, elem, "Screen_Effect_Delay");
        Kiosk_num_lines++;
    }
    xml_table_close(root);
}

// 0x00B3F200 (:1646)
// Green: the all clear, unless a script asked to skip it. Red: the siren. Leaving red: the siren stops. Any alert: a kiosk
// session ends and the screens follow the level. Into orange or red from below: the reinforcements line.
static event_result public_address_alert_level_callback(const event_data& in_event, notify_info* info)
{
    const alert_level_change_event& event = (const alert_level_change_event&)in_event;
    const int new_level = (int)event.alert_level;
    const int old_level = (int)event.old_level;
    farray<public_address_group_handle,16> groups;
    if (new_level == ALERT_LEVEL_GREEN)
    {
        if (Public_address_ignore_next_all_clear)
        {
            Public_address_ignore_next_all_clear = false;
        }
        else
        {
            public_address_get_current_group_list(groups);
            for (int i = 0; i < groups.size(); i++)
            {
                public_address_start_dialogue(Public_address_groups[groups[i]].is_edf ? Public_address_edf_alert_all_clear_line : Public_address_civ_alert_all_clear_line, nullptr, groups[i]);
            }
        }
    }
    if (new_level == ALERT_LEVEL_RED)
    {
        public_address_get_current_group_list(groups);
        for (int i = 0; i < groups.size(); i++)
        {
            dialogue_manager_cancel(Public_address_groups[groups[i]].last_root_voice_line_handle, true);
            audiolib_play_stop(Public_address_groups[groups[i]].prefix_play_id);
        }
        public_address_start_siren();
    }
    if (old_level == ALERT_LEVEL_RED && public_address_siren_is_active())
    {
        public_address_stop_siren();
    }
    if (new_level > 0)
    {
        if (old_level == ALERT_LEVEL_GREEN)
        {
            kiosk_set_active(false);
        }
        for (int i = 0; i < Kiosk_objects.size(); i++)
        {
            if (Kiosk_objects[i] == 0xFFFFFFFF)
            {
                continue;
            }
            object_mover* mover = World.object_from_handle<object_mover>(Kiosk_objects[i]);
            if (mover == nullptr || !mover->obj_flags.streamed)
            {
                continue;
            }
            const level_object_inst_properties* props = mover->m_level_obj_properties;
            // Magic number: 4 script point effects.
            for (int j = 0; j < 4; j++)
            {
                if (props->m_script_point_effect_insts[j] != nullptr)
                {
                    kiosk_set_effect_variable(props->m_script_point_effect_insts[j]->current_effect);
                }
            }
        }
    }
    if (new_level > 1 && old_level < 2)
    {
        public_address_get_current_group_list(groups);
        for (int i = 0; i < groups.size(); i++)
        {
            dialogue_manager_cancel(Public_address_groups[groups[i]].last_root_voice_line_handle, true);
            public_address_start_dialogue(Public_address_groups[groups[i]].is_edf ? Public_address_edf_alert_reinforcements_line : Public_address_civ_alert_reinforcements_line, nullptr, groups[i]);
        }
    }
    if (new_level < 1 && old_level > 0)
    {
        kiosk_update_all_effects_and_action_nodes();
    }
    return EVENT_OK;
}

// 0x00B23E50 (:1744)
static void public_address_defaults_table_load()
{
    xml_element* root = xml_table_parse("pa_defaults.xtbl");
    if (root == nullptr)
    {
        return;
    }
    xml_element* defaults = xml_table_find_req(root, "PA_Defaults");
    const char* text = xml_table_get_req_string_ref(defaults, "Name");
    Public_address_voice_persona = text != nullptr ? human_voice_persona_handle_from_name(text) : INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    text = xml_table_get_req_string_ref(defaults, "Kiosk_persona");
    Kiosk_voice_persona = text != nullptr ? human_voice_persona_handle_from_name(text) : INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    xml_element* lines = xml_table_find_req(defaults, "Voice_Lines");
    Public_address_edf_general_amb_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "EDF_General_Ambient"));
    Public_address_civ_alert_all_clear_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "Civilian_Alert_All_Clear"));
    Public_address_edf_alert_all_clear_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "EDF_Alert_All_Clear"));
    Public_address_civ_alert_reinforcements_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "Civilian_Alert_Reinforcements"));
    Public_address_edf_alert_reinforcements_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "EDF_Alert_Reinforcements"));
    Kiosk_station_id_line = voice_line_handle_from_name(xml_table_get_req_string_ref(lines, "Kiosk_Station_ID"));
    xml_element* foleys = xml_table_find_req(defaults, "Foleys");
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Siren_Loop"), &Public_address_siren_loop_foley);
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Siren_Death"), &Public_address_siren_death_foley);
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Prefix"), &Public_address_prefix_foley);
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Kiosk_Music_Stinger"), &Kiosk_music_stinger_cue_id);
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Kiosk_Music_Loop"), &Kiosk_music_loop_cue_id);
    audiolib_cue_get_id(xml_table_get_req_string_ref(foleys, "Kiosk_Music_Out"), &Kiosk_music_out_cue_id);
    xml_element* screens = xml_table_find_req(defaults, "Kiosk_Screens");
    Kiosk_idle_screen_effect = kiosk_get_screen_effect_from_name(xml_table_get_req_string_ref(screens, "Idle_Screen"));
    Kiosk_active_screen_effect = kiosk_get_screen_effect_from_name(xml_table_get_req_string_ref(screens, "Active_Screen"));
    Kiosk_alert_screen_effect = kiosk_get_screen_effect_from_name(xml_table_get_req_string_ref(screens, "Alert_Screen"));
    Kiosk_rf_idle_screen_effect = kiosk_get_screen_effect_from_name(xml_table_get_req_string_ref(screens, "RF_Idle_Screen"));
    xml_element* other = xml_table_find_req(defaults, "Other_Settings");
    xml_table_get_req_int(Public_address_siren_loop_count, other, "Siren_Loop_Count");
    // Magic numbers: 1 to 8 plays, the size of Public_address_siren_play_id.
    if (Public_address_siren_loop_count > MAX_SIREN_PLAYS)
    {
        Public_address_siren_loop_count = MAX_SIREN_PLAYS;
    }
    else if (Public_address_siren_loop_count < 1)
    {
        Public_address_siren_loop_count = 1;
    }
    xml_table_get_req_int(Public_address_siren_delay_ms, other, "Siren_Delay");
    xml_table_get_req_int(Public_address_siren_overlap_ms, other, "Siren_Overlap");
    xml_table_get_req_int(Kiosk_next_line_delay_ms, other, "Kiosk_line_delay");
    xml_table_get_req_int(Kiosk_inactive_delay_ms, other, "Kiosk_inactive_delay");
    xml_table_get_req_int(Kiosk_inactive_delay_long_ms, other, "Kiosk_inactive_delay_long");
    xml_table_get_req_int(Kiosk_min_session_line_play_count, other, "Kiosk_session_min_lines");
    xml_table_get_req_int(Kiosk_max_session_line_play_count, other, "Kiosk_session_max_lines");
    xml_table_get_req_int(Kiosk_music_loop_delay_ms, other, "Kiosk_Music_Loop_Delay");
    xml_table_close(root);
}

// ---- public address, the interface

// 0x00B2E840 (:1952)
public_address_group_handle public_address_find_group(const char* const group_name)
{
    return public_address_find_group(crc_stri(group_name, 0));
}

// 0x00B24130 (:1958)
public_address_group_handle public_address_find_group(unsigned int name_crc)
{
    for (int i = 0; i < Public_address_num_groups; i++)
    {
        if (name_crc == Public_address_groups[i].name_crc)
        {
            return (public_address_group_handle)i;
        }
    }
    return INVALID_PA_HANDLE;
}

// 0x00B34BD0 (:1970)
void public_address_add_speaker(public_address_group_handle group, unsigned int speaker)
{
    if (public_address_valid_group_handle(group) && public_address_find_speaker_index(speaker) == -1)
    {
        Public_address_speakers.add(public_address_speaker(group, speaker));
        Public_address_speaker_list_is_sorted = false;
    }
}

// 0x00B34C20 (:1988)
void public_address_add_speaker(const char* const group_name, unsigned int speaker)
{
    public_address_add_speaker(public_address_find_group(group_name), speaker);
}

// 0x00B34CA0 (:1993)
void public_address_remove_speaker(unsigned int speaker)
{
    const int index = public_address_find_speaker_index(speaker);
    if (index == -1)
    {
        return;
    }
    public_address_stop_speaker(Public_address_speakers[index].group, speaker);
    public_address_kill_siren_at_speaker(speaker);
    Public_address_speakers.remove(index);
    if (index < Public_address_zone_lower_bound)
    {
        Public_address_zone_lower_bound--;
    }
    if (index < Public_address_zone_upper_bound)
    {
        Public_address_zone_upper_bound--;
    }
}

// 0x00B24160 (:2019)
void public_address_set_active_group(public_address_group_handle group)
{
    if (public_address_valid_group_handle(group))
    {
        Active_group = group;
    }
}

// 0x00B24180 (:2026)
void public_address_unset_active_group()
{
    Active_group = INVALID_PA_HANDLE;
}

// 0x00B3F550 (:2031)
void public_address_override_ambient(public_address_group_handle group, voice_line_handle ambient_line, human_voice_persona_handle ambient_persona, int min_delay_ms, int max_delay_ms)
{
    if (public_address_valid_group_handle(group))
    {
        public_address_group& pa = Public_address_groups[group];
        pa.override_ambient_line = ambient_line;
        pa.override_min_delay_ms = min_delay_ms;
        pa.override_max_delay_ms = max_delay_ms;
        pa.override_persona = ambient_persona;
        public_address_ambient_play(group);
    }
}

// 0x00B24190 (:2043)
void public_address_stop_ambient(public_address_group_handle group)
{
    if (public_address_valid_group_handle(group))
    {
        Public_address_groups[group].play_ambient_lines = false;
    }
}

// 0x00B241B0 (:2050)
void public_address_restore_ambient(public_address_group_handle group)
{
    if (public_address_valid_group_handle(group))
    {
        Public_address_groups[group].play_ambient_lines = true;
        Public_address_groups[group].override_ambient_line = INVALID_VOICE_LINE_HANDLE;
    }
}

// 0x00B3AF30 (:2062)
// While the kiosk holds the PA persona the line waits on the queue and the kiosk session ends.
void public_address_start_dialogue(voice_line_handle line_handle, human_voice_persona* hvpp, public_address_group_handle group)
{
    if (line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const public_address_group_handle pa_group = group == INVALID_PA_HANDLE ? Active_group : group;
    if (!public_address_valid_group_handle(pa_group))
    {
        return;
    }
    human_voice_persona* persona = hvpp != nullptr ? hvpp : human_voice_persona_from_handle(Public_address_voice_persona);
    if (persona == nullptr)
    {
        return;
    }
    public_address_group& pa = Public_address_groups[pa_group];
    if ((Kiosk_is_active || Public_address_waiting_for_persona_load) && human_voice_persona_from_handle(Public_address_voice_persona) == persona)
    {
        kiosk_set_active(false);
        pa.queued_lines.add(line_handle);
        return;
    }
    if (dialogue_manager_is_dialogue_playing(pa.last_root_voice_line_handle))
    {
        // Magic number: 500 ms after the line that is playing.
        dialogue_manager_add_line(line_handle, persona, pa.last_root_voice_line_handle, 500, pa_group, nullptr);
        return;
    }
    dialogue_manager_start_dialogue(line_handle, persona, DIALOGUE_PRIORITY_AMBIENT, pa_group, nullptr);
    pa.last_root_voice_line_handle = line_handle;
}

// 0x00B34D60 (:2117)
// The prefix foley, then the cue after it, from every speaker of the group in this zone. The group's ambient timer waits
// for both to finish.
int public_address_play(int cue, public_address_group_handle group)
{
    if (group == INVALID_PA_HANDLE)
    {
        group = Active_group;
    }
    if (!public_address_valid_group_handle(group))
    {
        return -1;
    }
    if (!Public_address_speaker_list_is_sorted)
    {
        public_address_find_speaker_index_range(0xFFFFFFFF);
    }
    if (Public_address_zone_lower_bound == -1)
    {
        public_address_set_ambient_timer(group, 0);
        return -1;
    }
    audiolib_params params;
    for (int i = Public_address_zone_lower_bound; i <= Public_address_zone_upper_bound; i++)
    {
        if (Public_address_speakers[i].group != group)
        {
            continue;
        }
        object* op = World.object_from_handle(Public_address_speakers[i].speaker);
        if (public_address_speaker_should_play_sounds(op))
        {
            if (params.num_positions > 15)
            {
                break;
            }
            params.position_list[params.num_positions] = op->pos;
            params.num_positions++;
        }
    }
    public_address_group& pa = Public_address_groups[group];
    int extra_time = 0;
    if (params.num_positions == 0)
    {
        pa.last_played_id = -1;
    }
    else
    {
        params.flags = 0x81;
        pa.prefix_play_id = audiolib_cue_play(Public_address_prefix_foley, &params, nullptr);
        int prefix_time = 0;
        audiolib_play_get_play_time(pa.prefix_play_id, &prefix_time, nullptr);
        params.delay_ms = prefix_time;
        pa.last_played_id = audiolib_cue_play(cue, &params, nullptr);
        audiolib_play_get_play_time(pa.last_played_id, &extra_time, nullptr);
        extra_time += prefix_time;
    }
    public_address_set_ambient_timer(group, extra_time);
    return pa.last_played_id;
}

// 0x00B241E0 (:2195)
void public_address_register_district_liberated(unsigned int handle)
{
    Public_address_recent_liberated_district = handle;
}

// 0x00B241F0 (:2200)
void public_address_ignore_next_all_clear_message()
{
    if (gsm_get_alert_level() != ALERT_LEVEL_GREEN)
    {
        Public_address_ignore_next_all_clear = true;
    }
}

// ---- kiosks, the interface

// 0x00B2E880 (:2212)
object* kiosk_get_active_kiosk()
{
    if (Kiosk_current_active_kiosk_index == -1)
    {
        return nullptr;
    }
    return World.object_from_handle(Kiosk_objects[Kiosk_current_active_kiosk_index]);
}

// 0x00B34F30 (:2221)
void kiosk_add_kiosk_object(unsigned int handle)
{
    Kiosk_objects.add_unique(handle);
}

// 0x00B34F40 (:2226)
void kiosk_unlock_from_voice_line_handle(voice_line_handle handle)
{
    kiosk_unlock_from_index(kiosk_line_index_from_voice_line_handle(handle), false);
}

// 0x00B34FD0 (:2232)
void kiosk_unlock_from_mission(mission_handle handle)
{
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        if (Kiosk_lines[i].unlocked_by_mission == handle)
        {
            kiosk_unlock_from_index(i, true);
        }
    }
}

// 0x00B35030 (:2242)
void kiosk_unlock_from_activity(base_activity_data* base_data)
{
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        if (Kiosk_lines[i].unlocked_by_activity == base_data)
        {
            kiosk_unlock_from_index(i, false);
        }
    }
}

// 0x00B350A0 (:2252)
void kiosk_update_all_effects_and_action_nodes()
{
    for (int i = 0; i < Kiosk_objects.size(); i++)
    {
        if (Kiosk_objects[i] == 0xFFFFFFFF)
        {
            continue;
        }
        object_mover* mover = World.object_from_handle<object_mover>(Kiosk_objects[i]);
        if (mover != nullptr)
        {
            kiosk_update_effects_and_action_nodes(mover);
        }
    }
}

// 0x00B2E8B0 (:2265)
// Each of the 4 screens restarts its effect or, on a dead kiosk, stops it. The kiosk's action nodes on a side with a
// working screen are open while a session runs; the others stay closed.
void kiosk_update_effects_and_action_nodes(object_mover* main_op)
{
    if (!main_op->obj_flags.streamed || !kiosk_mover_is_kiosk(main_op))
    {
        return;
    }
    bool front_screen = false;
    bool back_screen = false;
    const level_object_inst_properties* props = main_op->m_level_obj_properties;
    for (int i = 0; i < 4; i++)
    {
        script_point_effect_inst* inst = props->m_script_point_effect_insts[i];
        if (inst == nullptr)
        {
            continue;
        }
        if (main_op->is_killed())
        {
            if (inst->state == 0)
            {
                object_effect* ep = World.object_from_handle<object_effect>(inst->current_effect);
                if (ep != nullptr)
                {
                    ep->stop(object_effect::SS_STOP_EMITTING);
                }
                inst->current_effect = 0xFFFFFFFF;
            }
        }
        else if (kiosk_start_effect_on_screen(main_op, i))
        {
            vector tag_pos;
            matrix tag_orient;
            main_op->get_tag_world_coords(inst->script_point_idx, tag_pos, &tag_orient, nullptr, nullptr);
            if (main_op->orient.fvec * tag_orient.fvec <= 0.0f)
            {
                back_screen = true;
            }
            else
            {
                front_screen = true;
            }
        }
    }
    object* child = main_op->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            object_action_node* node = (object_action_node*)child;
            const float facing = main_op->orient.fvec * child->orient.fvec;
            if (Kiosk_is_active && ((front_screen && facing < 0.0f) || (back_screen && facing >= 0.0f)))
            {
                node->clear_disabled_by(ANDR_KIOSK);
            }
            else
            {
                node->set_disabled_by(ANDR_KIOSK);
            }
        }
        child = child->child_next;
        if (child == main_op->child_ptr)
        {
            break;
        }
    }
}

// 0x00B24210 (:2331)
// Magic numbers: the chunk flag 0x4000 marks a kiosk, and mover flag 4 a dislodged piece.
bool kiosk_mover_is_kiosk(const object_mover* omp)
{
    return (omp->gameplay_props->chunk_flags & 0x4000) != 0 && (omp->mover_flags & 4) == 0;
}

// 0x00B350F0 (:2336)
void kiosk_handle_game_complete()
{
    Kiosk_available_lines.clear();
    kiosk_update_all_effects_and_action_nodes();
}

// ---- both

// 0x00B3F5A0 (:2393)
void public_address_kiosk_do_frame()
{
    public_address_do_frame();
    kiosk_do_frame();
}

// 0x00B3F5B0 (:2404)
void public_address_kiosk_level_init_pre()
{
    for (int i = 0; i < Public_address_num_groups; i++)
    {
        Public_address_groups[i].clear();
        public_address_set_ambient_timer((public_address_group_handle)i, 0);
    }
    notify_info info;
    info.type = EVENT_BUILDING_DESTROYED;
    info.callback = public_address_object_destroyed_callback;
    info.data = nullptr;
    gem_register_for_event(info);
    info.type = EVENT_ALERT_LEVEL_CHANGED;
    info.callback = public_address_alert_level_callback;
    gem_register_for_event(info);
    Public_address_speakers.clear();
    Public_address_zone_handle = 0xFFFFFFFF;
    Public_address_zone_upper_bound = -1;
    Public_address_zone_lower_bound = -1;
    for (int i = 0; i < MAX_SIREN_PLAYS; i++)
    {
        Public_address_siren_play_id[i] = -1;
    }
    Public_address_siren_timer.invalidate();
    Public_address_recent_liberated_district = 0xFFFFFFFF;
    Public_address_ignore_next_all_clear = false;
    kiosk_init_available_lines();
    kiosk_set_active(false);
    Kiosk_inactive_timer.invalidate();
    Kiosk_objects.clear();
    Kiosk_play_id = -1;
    Kiosk_music_stinger_play_id = -1;
    Kiosk_music_loop_play_id = -1;
    Kiosk_music_out_play_id = -1;
    info.type = EVENT_BUILDING_DESTROYED;
    info.callback = kiosk_building_destroyed_callback;
    gem_register_for_event(info);
    // A cue that loops by itself must not also be queued.
    if (audiolib_cue_is_looping(Public_address_siren_loop_foley))
    {
        Public_address_siren_loop_count = 1;
    }
    Kiosk_current_active_screen_effect = Kiosk_active_screen_effect;
}

// 0x00B35100 (:2475)
void public_address_kiosk_level_init_post()
{
    Public_address_current_tod = game_clock_get_time_of_day();
    public_address_kiosk_restore();
}

// 0x00B24240 (:2483)
void public_address_kiosk_level_shutdown()
{
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        Kiosk_lines[i].play_count = 0;
    }
}

// 0x00B35140 (:2493)
void public_address_kiosk_system_init()
{
    public_address_groups_table_load();
    kiosk_screen_table_load();
    kiosk_line_table_load();
    public_address_defaults_table_load();
}

// 0x00B2EA20 (:2506)
// The save has the group's name, not its index, so that a table change does not misplace a speaker.
void public_address_kiosk_save(cfile* save_file)
{
    cf_write_int32(Public_address_speakers.size(), save_file);
    cf_write_bool(Public_address_speaker_list_is_sorted, save_file);
    for (int i = 0; i < Public_address_speakers.size(); i++)
    {
        cf_write_uint32(Public_address_speakers[i].speaker, save_file);
        cf_write_uint32(Public_address_groups[Public_address_speakers[i].group].name_crc, save_file);
    }
    cf_write_int32(Kiosk_objects.size(), save_file);
    for (int i = 0; i < Kiosk_objects.size(); i++)
    {
        cf_write_uint32(Kiosk_objects[i], save_file);
    }
    cf_write_int32(Kiosk_num_lines, save_file);
    for (int i = 0; i < Kiosk_num_lines; i++)
    {
        cf_write_uint32((unsigned int)Kiosk_lines[i].line, save_file);
        cf_write_int32(Kiosk_lines[i].play_count, save_file);
    }
    cf_write_int32(Kiosk_available_lines.size(), save_file);
    for (int i = 0; i < Kiosk_available_lines.size(); i++)
    {
        cf_write_uint32((unsigned int)Kiosk_available_lines[i]->line, save_file);
    }
}

// 0x00B35160 (:2543)
// A group or line this build does not know is skipped. Original: only the available lines are cleared first, so the
// speakers and kiosks of a load add to what the level already registered.
void public_address_kiosk_load(cfile* save_file)
{
    int count = cf_read_int32(save_file);
    Public_address_speaker_list_is_sorted = cf_read_bool(save_file);
    for (; count > 0; count--)
    {
        const unsigned int speaker = cf_read_uint32(save_file);
        const public_address_group_handle group = public_address_find_group(cf_read_uint32(save_file));
        if (group != INVALID_PA_HANDLE)
        {
            Public_address_speakers.add(public_address_speaker(group, speaker));
        }
    }
    for (count = cf_read_int32(save_file); count > 0; count--)
    {
        Kiosk_objects.add_unique(cf_read_uint32(save_file));
    }
    for (count = cf_read_int32(save_file); count > 0; count--)
    {
        const int index = kiosk_line_index_from_voice_line_handle((voice_line_handle)cf_read_uint32(save_file));
        const int play_count = cf_read_int32(save_file);
        if (index != -1)
        {
            Kiosk_lines[index].play_count = play_count;
        }
    }
    Kiosk_available_lines.clear();
    for (count = cf_read_int32(save_file); count > 0; count--)
    {
        const int index = kiosk_line_index_from_voice_line_handle((voice_line_handle)cf_read_uint32(save_file));
        if (index != -1)
        {
            Kiosk_available_lines.add_unique(&Kiosk_lines[index]);
        }
    }
}

// 0x00B24260 (:2593)
human_voice_persona_handle public_address_kiosk_get_active_persona()
{
    return Kiosk_is_active ? Kiosk_voice_persona : Public_address_voice_persona;
}

// 0x00B2EB50 (:2603)
void public_address_kiosk_restore()
{
    const int count = Kiosk_objects.size();
    for (int i = 0; i < count; i++)
    {
        World.restore_object(Kiosk_objects[i]);
    }
}

