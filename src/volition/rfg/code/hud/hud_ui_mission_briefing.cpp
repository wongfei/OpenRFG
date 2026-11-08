// Original: volition/rfg/code/hud/hud_ui_mission_briefing.cpp
//
// The mission briefing screen: mission_briefing_text.xtbl's subtitles and voice lines, the Bink video they follow, and
// the accept and cancel buttons. A mission with no video activates straight away.

#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern bool Mission_parker_liberation_warp; // missions.cpp

// ---- globals
mission_briefing_info Mb_info; // 0x02BDB180
const char* cMission_briefing_table = "mission_briefing_text.xtbl"; // 0x01661070

// ---- file-static globals
static const char* cMission_briefing_table_root = "Table"; // 0x01661074
static const char* cMission_briefing_lines = "MissionBriefingLines"; // 0x01661078
static const char* cMission_briefing_name = "Name"; // 0x0166107C
static const char* cMission_description = "MissionDescription"; // 0x01661080
static const char* cMission_briefing_list = "MissionBriefingList"; // 0x01661084
static const char* cMission_briefing_line_info = "LineInfo"; // 0x01661088
static const char* cMission_briefing_music = "BackgroundMusic"; // 0x0166108C
static const char* cMission_briefing_volume = "BackgroundMusicVolume"; // 0x01661090
static const char* cMission_briefing_bink_volume = "BinkVideoVolume"; // 0x01661094
static const char* cMission_briefing_line_persona = "Persona"; // 0x01661098
static const char* cMission_briefing_line_frame = "LineFrame"; // 0x0166109C
static const char* cMission_briefing_line_name = "Line"; // 0x016610A0
static const char* cMission_briefing_line_color = "LineColor"; // 0x016610A4
static const char* cMission_briefing_pause_flag = "PauseUntilPrevAudioDone"; // 0x016610A8
static const char* cMission_briefing_loop_frames = "loop_frames"; // 0x016610AC
static const char* cBlankBriefing = " "; // 0x016610B0
static const char* cUI_mission_briefing_doc = "rfg_ui_mission_briefing"; // 0x016610B4
static const char* cUI_bink_video_briefing = "video"; // 0x016610B8
static const char* cUI_video_group = "video_group"; // 0x016610BC
static const char* cProp_screen_size = "screen_size"; // 0x016610C4
static const char* cLocation = "MENU_MISSION_BRIEFING_LOCATION_TEXT"; // 0x016610C8
static const char* cObjectives = "MENU_MISSION_BRIEFING"; // 0x016610CC
static const char* cDistrict_lib = "MENU_DISTRICT_LIBERATED"; // 0x016610D0
static const char* cAccept = "MENU_ACCEPT"; // 0x016610D4
static const char* cCancel = "MENU_CANCEL"; // 0x016610D8
static const char* cContinueBriefing = "MENU_CONTINUE"; // 0x016610DC
static const char* cAcceptSound = "HUD_ACCEPT"; // 0x016610E4
static const char* cCancelSoundBriefing = "HUD_BACK"; // 0x016610E8
static const char* kHintImg2 = "hint_b_button"; // 0x0166225C
static const char* kHintText2 = "hint_text_2"; // 0x01662260
static const char* kHintImg1 = "hint_a_button"; // 0x01662264
static const char* kHintText1 = "hint_text_1"; // 0x01662268
static timestamp_realtime Allow_input_timer; // 0x016617EC
static timestamp_realtime Stream_wait_timeout; // 0x016617F0
static int last_index = -1; // 0x016626EC
static float one_over_30 = 0.033333335f; // 0x016626F0
static bool hasAnswered; // 0x02B9DD04
static int Mission_line_infos_mark; // 0x02B9DD08

// 0x008497E0 (:211)
static int start_index_compare(const void* arg1, const void* arg2)
{
    const int a = ((const mission_briefing_line*)arg1)->start_frame;
    const int b = ((const mission_briefing_line*)arg2)->start_frame;
    if (a < b)
    {
        return -1;
    }
    return b < a ? 1 : 0;
}

// 0x00849800 (:225)
static void humb_init_mb_info()
{
    Mb_info.bink_position.x = 0;
    Mb_info.bink_position.y = 0;
    Mb_info.bink_alpha = 0.0f;
    const vector2i& screen = ui_get_screen_dimensions();
    Mb_info.bink_size.x = screen.x;
    Mb_info.bink_size.y = screen.y;
    Mb_info.bink_handle = 0;
    Mb_info.mempool_mark = 0;
    Mb_info.mempool_mark_gpu = 0;
    Mb_info.mb_state = MBS_TRIGGER_PLAY;
    Mb_info.mb_last_state = MBS_TRIGGER_PLAY;
    Mb_info.waiting_for_world = false;
    Mb_info.total_frames = 0;
    Mb_info.user_response = 0;
    Mb_info.control_id = 0;
    Mb_info.bink_filename[0] = '\0';
    Mb_info.mission_name[0] = '\0';
    Mb_info.mission_description[0] = '\0';
    Mb_info.data_handles[2] = 0;
    Mb_info.data_handles[1] = 0;
    Mb_info.data_handles[0] = 0;
    Mb_info.doc_handle = -1;
    Mb_info.first_play = true;
    Mb_info.exiting = false;
    Mb_info.loaded = false;
    Mb_info.unloaded_doc = false;
    Mb_info.which_mission = -1;
    Mb_info.which_mission_line_index = -1;
    Mb_info.bink_volume = 0.0f;
}

// 0x008498C0 (:254)
static void humb_fadein_sound()
{
    if (Mb_info.play_sound_loop != -1)
    {
        return;
    }
    Mb_info.play_sound_loop = audiolib_cue_play(Mb_info.sound_mb_background, nullptr, nullptr);
    float volume;
    audiolib_play_get_volume(Mb_info.play_sound_loop, &volume);
    // Magic numbers: 0.01, the volume difference that counts as equal, and a 100 ms fade. The test changes nothing: both
    // branches set the same volume, as in rfg.exe.
    if (fabsf(volume - Mb_info.background_music_volume) <= 0.01f)
    {
        audiolib_play_set_volume(Mb_info.play_sound_loop, Mb_info.background_music_volume, 100);
    }
    audiolib_play_set_volume(Mb_info.play_sound_loop, Mb_info.background_music_volume, 100);
}

// 0x00849960 (:269)
static void humb_fadeout_sound()
{
    if (Mb_info.play_sound_loop != -1)
    {
        audiolib_play_stop(Mb_info.play_sound_loop);
        Mb_info.play_sound_loop = -1;
    }
}

// 0x00849980 (:278)
static void humb_stop_video_lines()
{
    if (!Mb_info.playing_audio_line)
    {
        return;
    }
    if (human_voice_persona_from_handle(Mb_info.curr_persona_handle) != nullptr)
    {
        dialogue_manager_cancel(Mb_info.curr_vl_handle, true);
    }
    Mb_info.playing_audio_line = false;
    Mb_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    Mb_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
}

// 0x008499D0 (:298)
static void humb_load_ui_information()
{
    if (Mb_info.doc_handle == -1)
    {
        return;
    }
    unsigned int handle = 0;
    vint_data_item* item = vint_data_item::create_new("MissionBriefingInfo");
    if (item != nullptr)
    {
        static const vint_variant_type types[13] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 13);
        handle = item->get_handle();
    }
    Mb_info.data_handles[0] = (int)handle;
    ui_add_data_item(Mb_info.doc_handle, (int)handle);
    item = vint_data_item::create_new("MissionBriefingLine");
    if (item != nullptr)
    {
        static const vint_variant_type types[6] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 6);
        handle = item->get_handle();
    }
    Mb_info.data_handles[1] = (int)handle;
    ui_add_data_item(Mb_info.doc_handle, (int)handle);
    item = vint_data_item::create_new("MissionBriefingEnd");
    if (item != nullptr)
    {
        static const vint_variant_type types[1] = {VINT_PROP_TYPE_BOOL};
        item->set_template(types, 1);
        handle = item->get_handle();
    }
    Mb_info.data_handles[2] = (int)handle;
    ui_add_data_item(Mb_info.doc_handle, (int)handle);
}

// 0x00849B20 (:383)
static void humb_unload_ui_information()
{
    if (Mb_info.doc_handle != -1)
    {
        ui_unload_document(Mb_info.doc_handle, DFOT_NONE, 0.5f);
        Mb_info.doc_handle = -1;
        hud_ui_reload_items();
    }
    if (Mb_info.control_id != 0)
    {
        ui_relinquish_control_id(Mb_info.control_id);
        Mb_info.control_id = 0;
    }
    Mb_info.unloaded_doc = true;
}

// 0x00849B80 (:402)
// Where the video draws: the "video" element's anchor, moved by its offset times its size according to its auto offset.
static void humb_obtain_bink_screen_info_handbook()
{
    vint_document* doc = ui_get_vint_document(Mb_info.doc_handle);
    if (doc == nullptr)
    {
        return;
    }
    vint_element_base* element = (vint_element_base*)vint_object_base::find_object(cUI_bink_video_briefing, doc);
    if (element != nullptr)
    {
        vector2 anchor;
        element->get_global_anchor(anchor);
        vint_variant size_value;
        element->get_property_value(cProp_screen_size, size_value, 0);
        vector2 size(0.0f, 0.0f);
        if (size_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
        {
            size = (vector2)size_value;
        }
        vint_variant offset_value;
        element->get_offset(offset_value, 0);
        vector2 element_offset(0.0f, 0.0f);
        if (offset_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
        {
            element_offset = (vector2)offset_value;
        }
        vint_variant auto_offset;
        element->get_auto_offset(auto_offset, 0);
        const double dx = (double)element_offset.x * (double)size.x;
        const double dy = (double)element_offset.y * (double)size.y;
        // Magic numbers: the auto offset's nine values, by the signs the asm applies.
        switch ((int)auto_offset)
        {
        case 1:
            anchor.x = (float)(dx + (double)anchor.x);
            anchor.y = (float)(dy + (double)anchor.y);
            break;
        case 2:
            anchor.x = (float)(dx + (double)anchor.x);
            break;
        case 3:
            anchor.y = (float)(dy + (double)anchor.y);
            break;
        case 4:
            anchor.x = (float)((double)anchor.x - dx);
            break;
        case 5:
            anchor.y = (float)((double)anchor.y - dy);
            break;
        case 7:
            anchor.x = (float)((double)anchor.x - dx);
            anchor.y = (float)(dy + (double)anchor.y);
            break;
        case 8:
            anchor.x = (float)((double)anchor.x - dx);
            anchor.y = (float)((double)anchor.y - dy);
            break;
        case 9:
            anchor.x = (float)(dx + (double)anchor.x);
            anchor.y = (float)((double)anchor.y - dy);
            break;
        default:
            break;
        }
        const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
        const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
        Mb_info.bink_position.y = fl_round((float)((double)scale.y * (double)anchor.y + (double)offset.y));
        Mb_info.bink_position.x = fl_round((float)((double)scale.x * (double)anchor.x + (double)offset.x));
        Mb_info.bink_size.y = fl_round((float)((double)size.y * (double)scale.y));
        Mb_info.bink_size.x = fl_round((float)((double)size.x * (double)scale.x));
    }
    float alpha = 1.0f;
    vint_object_base* group = vint_object_base::find_object(cUI_video_group, doc);
    if (group != nullptr)
    {
        vint_variant alpha_value;
        group->get_property_value("alpha", alpha_value, 0);
        alpha = (float)alpha_value;
    }
    Mb_info.bink_alpha = alpha;
}

// 0x0084A000 (:482)
static void process_mission_information()
{
    vint_data_item* item = vint_data_item::find(Mb_info.data_handles[0]);
    if (item == nullptr)
    {
        return;
    }
    char description[128];
    char objective[128];
    char button[128];
    char image[128];
    strncpy_safe(description, Mb_info.mission_description, 0x80);
    const char* current_objective = hud_ui_missions_get_objective();
    strncpy_safe(objective, current_objective != nullptr ? current_objective : cBlankBriefing, 0x80);
    const district* dp = district_local_player();
    str_make_upper(description);
    str_make_upper(objective);
    // Deviation: rfg.exe also uppercases a third buffer here that nothing fills or reads, an uninitialized stack read.
    item->set_element(0, vint_variant(Mb_info.bink_handle));
    item->set_element(1, vint_variant(Mb_info.first_play));
    item->set_element(2, vint_variant(description));
    item->set_element(3, vint_variant(cLocation));
    if (dp != nullptr)
    {
        item->set_element(4, vint_variant(dp->get_localized_name_hash()));
    }
    else
    {
        item->set_element(4, vint_variant(0));
    }
    item->set_element(5, vint_variant(objective));
    item->set_element(6, vint_variant(Mb_info.district_lib ? cDistrict_lib : cObjectives));
    item->set_element(7, vint_variant(Mb_info.district_lib ? cContinueBriefing : cAccept));
    item->set_element(8, vint_variant(cCancel));
    item->set_element(9, vint_variant(Mb_info.district_lib));
    item->set_element(10, vint_variant(Mb_info.allowing_user_input));
    controls_get_button_image_name(button, 0x80, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(11, vint_variant(image));
    controls_get_button_image_name(button, 0x80, CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(12, vint_variant(image));
}

// 0x00857BA0 (:546)
static void process_mission_line()
{
    vint_data_item* item = vint_data_item::find(Mb_info.data_handles[1]);
    if (item == nullptr)
    {
        return;
    }
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    unsigned int line_crc = 0;
    float line_duration = 4.0f; // Magic number: seconds, when there is no next line to measure against.
    const int mission = Mb_info.which_mission;
    const int index = Mb_info.which_mission_line_index;
    if (mission != -1 && mission < Mb_info.mission_line_infos.size() && index != -1 && index < Mb_info.mission_line_infos[mission].lines.size())
    {
        farray<mission_briefing_line,40>& lines = Mb_info.mission_line_infos[mission].lines;
        const human_voice_persona_handle persona_handle = lines[index].persona_handle;
        const voice_line_handle line_handle = lines[index].vl_handle;
        if (persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && line_handle != INVALID_VOICE_LINE_HANDLE)
        {
            human_voice_persona* persona = human_voice_persona_from_handle(persona_handle);
            if (persona != nullptr)
            {
                persona->get_line_crc(line_handle, line_crc);
            }
        }
        red = (float)lines[index].line_color.red / 255.0f;
        green = (float)lines[index].line_color.green / 255.0f;
        blue = (float)lines[index].line_color.blue / 255.0f;
        if (index + 1 < lines.size())
        {
            line_duration = (float)(lines[index + 1].start_frame - lines[index].start_frame) * one_over_30;
            last_index = index;
        }
        else if (last_index > -1 && last_index < lines.size())
        {
            line_duration = (float)(Mb_info.total_frames - lines[last_index].start_frame) * one_over_30;
        }
    }
    item->set_element(0, vint_variant(index));
    item->set_element(1, vint_variant(line_crc));
    item->set_element(2, vint_variant(red));
    item->set_element(3, vint_variant(green));
    item->set_element(4, vint_variant(blue));
    item->set_element(5, vint_variant(line_duration));
}

// 0x0084A390 (:618)
static void process_mission_done()
{
    vint_data_item* item = vint_data_item::find(Mb_info.data_handles[2]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Mb_info.user_response != 0));
    }
}

// 0x0084A3E0 (:632)
static void humb_stop_video()
{
    if (Mb_info.bink_handle != 0)
    {
        video_player_stop(Mb_info.bink_handle);
    }
    humb_stop_video_lines();
}

// 0x0084A440 (:642)
static void humb_request_exit()
{
    Mb_info.exiting = true;
    Mb_info.allowing_user_input = false;
}

// 0x00857E70 (:650)
static void humb_process_data_items()
{
    if (Mb_info.loaded)
    {
        process_mission_information();
        process_mission_line();
        process_mission_done();
    }
}

// 0x0084A450 (:661)
static void humb_stop_callback(int handle)
{
    (void)handle;
    Mb_info.bink_handle = 0;
    Interface_mempool.restore_to_mark_cpu(Mb_info.mempool_mark);
}

// 0x0084A470 (:680)
static bool humb_is_audio_line_still_playing()
{
    if (Mb_info.playing_audio_line && Mb_info.curr_vl_handle != INVALID_VOICE_LINE_HANDLE && Mb_info.curr_persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        Mb_info.playing_audio_line = dialogue_manager_is_dialogue_playing(Mb_info.curr_vl_handle);
        if (!Mb_info.playing_audio_line)
        {
            dialogue_manager_cancel(Mb_info.curr_vl_handle, true);
        }
    }
    return Mb_info.playing_audio_line;
}

// 0x00857ED0 (:702)
static void humb_trigger_audio_line(int which_line)
{
    const int mission = Mb_info.which_mission;
    if (mission == -1 || mission >= Mb_info.mission_line_infos.size())
    {
        return;
    }
    farray<mission_briefing_line,40>& lines = Mb_info.mission_line_infos[mission].lines;
    if (Mb_info.which_mission_line_index < 0 || Mb_info.which_mission_line_index >= lines.size())
    {
        return;
    }
    Mb_info.curr_vl_handle = lines[which_line].vl_handle;
    Mb_info.curr_persona_handle = lines[which_line].persona_handle;
    const human_voice_persona* persona = human_voice_persona_from_handle(Mb_info.curr_persona_handle);
    if (persona != nullptr)
    {
        Mb_info.playing_audio_line = dialogue_manager_start_dialogue(Mb_info.curr_vl_handle, persona, DIALOGUE_PRIORITY_BRIEFING, INVALID_PA_HANDLE, nullptr);
        if (!Mb_info.playing_audio_line)
        {
            Mb_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
            Mb_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
        }
    }
}

// 0x00857F70 (:735)
static void humb_determine_audio_and_video_status(int curr_video_frame)
{
    const int mission = Mb_info.which_mission;
    if (mission == -1 || mission >= Mb_info.mission_line_infos.size())
    {
        return;
    }
    farray<mission_briefing_line,40>& lines = Mb_info.mission_line_infos[mission].lines;
    const int count = lines.size();
    if (count <= 0)
    {
        return;
    }
    const bool line_playing = humb_is_audio_line_still_playing();
    int which_line = -1;
    bool must_pause = false;
    for (int i = 0; i < count; i++)
    {
        if (!lines[i].has_triggered && lines[i].start_frame <= curr_video_frame)
        {
            if (which_line == -1)
            {
                which_line = i;
            }
            if (lines[i].pause_if_prev_not_done && line_playing)
            {
                must_pause = true;
            }
        }
    }
    if (!line_playing && which_line != -1)
    {
        if (!must_pause)
        {
            if (Mb_info.bink_handle != 0)
            {
                video_player_unpause(Mb_info.bink_handle);
                Mb_info.mb_state = MBS_PLAYING;
            }
            else if (!hasAnswered)
            {
                Mb_info.mb_state = MBS_TRIGGER_PLAY;
            }
        }
        Mb_info.which_mission_line_index = which_line;
        lines[which_line].has_triggered = true;
        humb_trigger_audio_line(which_line);
    }
    if (must_pause)
    {
        if (Mb_info.bink_handle != 0)
        {
            video_player_pause(Mb_info.bink_handle);
        }
        Mb_info.mb_state = MBS_PAUSED;
    }
}

// 0x0084A4C0 (:805)
static void humb_determine_video_screen_info()
{
    const vector2i old_position = Mb_info.bink_position;
    const vector2i old_size = Mb_info.bink_size;
    humb_obtain_bink_screen_info_handbook();
    if (old_position.x != Mb_info.bink_position.x || old_position.y != Mb_info.bink_position.y || old_size.x != Mb_info.bink_size.x || old_size.y != Mb_info.bink_size.y)
    {
        video_player_resize(Mb_info.bink_handle, Mb_info.bink_position.x, Mb_info.bink_position.y, Mb_info.bink_size.x, Mb_info.bink_size.y);
        video_player_set_alpha(Mb_info.bink_handle, Mb_info.bink_alpha);
    }
}

// 0x00858090 (:818)
void humb_clear_all_play_trigger_flags()
{
    for (int i = 0; i < Mb_info.mission_line_infos.size(); i++)
    {
        farray<mission_briefing_line,40>& lines = Mb_info.mission_line_infos[i].lines;
        for (int j = 0; j < lines.size(); j++)
        {
            lines[j].has_triggered = false;
        }
    }
}

// 0x008580F0 (:835)
static void humb_process_bink_video()
{
    switch (Mb_info.mb_state)
    {
    case MBS_TRIGGER_PLAY:
    {
        if (hasAnswered)
        {
            Mb_info.mb_state = MBS_LOOPING_END;
            break;
        }
        if (Mb_info.bink_handle != 0)
        {
            video_player_stop(Mb_info.bink_handle);
        }
        Mb_info.mempool_mark = (int)Interface_mempool.mark();
        float scalar = 1.0f;
        audiolib_system_get_volume_scalar(4, &scalar); // Magic number: the volume category the asm passes.
        // Original bug, kept: every new play scales the stored volume again.
        Mb_info.bink_volume = Mb_info.bink_volume * scalar;
        Mb_info.bink_handle = video_player_play(Mb_info.bink_filename, VPM_ONCE, Mb_info.bink_position.x, Mb_info.bink_position.y, Mb_info.bink_size.x, Mb_info.bink_size.y, Mb_info.bink_volume, true, humb_stop_callback, &Interface_mempool, nullptr, nullptr, 0, false, false);
        Mb_info.total_frames = video_player_get_total_frames(Mb_info.bink_handle);
        Mb_info.mb_state = MBS_PLAYING;
        if (Mb_info.bink_handle == 0)
        {
            hud_ui_mission_briefing_done();
            if (!Mb_info.district_lib)
            {
                mission_activate(Mb_info.mission_name, false);
            }
        }
        break;
    }
    case MBS_PAUSED:
        if (Mb_info.bink_handle != 0 && Mb_info.user_response == 0)
        {
            humb_determine_audio_and_video_status(video_player_get_current_frame(Mb_info.bink_handle));
        }
        break;
    case MBS_PLAYING:
        if (Mb_info.bink_handle != 0)
        {
            const int frame = video_player_get_current_frame(Mb_info.bink_handle);
            // Magic number: 3 frames from the end, the video jumps back to loop its last frames silently.
            if (Mb_info.total_frames - frame < 3)
            {
                video_player_set_volume(Mb_info.bink_handle, 0.0f);
                video_player_goto_frame(Mb_info.bink_handle, Mb_info.total_frames - Mb_info.loop_frames);
                Mb_info.mb_state = MBS_LOOPING_END;
                Mb_info.allowing_user_input = true;
            }
            else if (Mb_info.user_response == 0)
            {
                humb_determine_audio_and_video_status(frame);
            }
        }
        break;
    case MBS_LOOPING_END:
        if (Mb_info.bink_handle != 0 && Mb_info.total_frames - video_player_get_current_frame(Mb_info.bink_handle) < 3)
        {
            video_player_goto_frame(Mb_info.bink_handle, Mb_info.total_frames - Mb_info.loop_frames);
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
    Mb_info.mb_last_state = Mb_info.mb_state;
}

// 0x00880C00 (:973)
static void hud_ui_mission_briefing_add_mission_line_info(const char* mb_name, const char* mb_background_music, const char* mb_desc, float mb_back_music_volume, float mb_bink_video_volume, const farray<mission_briefing_line,40>& list, int loop_frames)
{
    if (mb_name == nullptr || mb_desc == nullptr || mb_background_music == nullptr)
    {
        return;
    }
    const int index = Mb_info.mission_line_infos.size();
    farray<human_voice_persona*,150> personas;
    Mb_info.mission_line_infos.add(mission_briefing_line_info());
    // Original bug, kept: with the 40 slots full, add reports an error and these write past the array.
    mission_briefing_line_info& info = Mb_info.mission_line_infos[index];
    strncpy_safe(info.mission_name, mb_name, 0x40);
    strncpy_safe(info.mission_description, mb_desc, 0x40);
    strncpy_safe(info.background_music, mb_background_music, 0x20);
    info.back_music_volume = mb_back_music_volume;
    info.bink_video_volume = mb_bink_video_volume;
    const int count = list.size();
    for (int i = 0; i < count; i++)
    {
        mission_briefing_line line = list[i];
        line.has_triggered = false;
        info.lines.add(line);
    }
    if (count > 0)
    {
        qsort(info.lines.elements(), (size_t)count, sizeof(mission_briefing_line), start_index_compare);
    }
    for (int i = 0; i < count; i++)
    {
        human_voice_persona* persona = human_voice_persona_from_handle(list[i].persona_handle);
        if (persona != nullptr)
        {
            personas.add_unique(persona);
        }
    }
    info.required_personas = personas.size() < 1 ? INVALID_VOICE_SET_HANDLE : human_voice_load_set(personas);
    info.loop_frames = loop_frames;
}

// 0x00880E40 (:1023)
bool hud_ui_mission_briefing_init_lines(const char* tbl_file)
{
    const unsigned int mark = Common_mempool.mark();
    xml_element* root = xml_parse(tbl_file, &Common_mempool);
    if (root == nullptr)
    {
        Common_mempool.restore_to_mark(mark);
        return false;
    }
    xml_element* table = xml_find(root, cMission_briefing_table_root);
    if (table == nullptr)
    {
        Common_mempool.restore_to_mark(mark);
        return false;
    }
    xml_element* entry = xml_find(table, cMission_briefing_lines);
    if (entry == nullptr)
    {
        Common_mempool.restore_to_mark(mark);
        return false;
    }
    for (; entry != nullptr; entry = xml_find_next(table, entry, cMission_briefing_lines))
    {
        xml_element* name = xml_find(entry, cMission_briefing_name);
        xml_element* music = xml_find(entry, cMission_briefing_music);
        xml_element* description = xml_find(entry, cMission_description);
        if (name == nullptr || music == nullptr || description == nullptr)
        {
            continue;
        }
        xml_element* line_list = xml_find(entry, cMission_briefing_list);
        if (line_list == nullptr)
        {
            continue;
        }
        float music_volume = 0.8f; // Magic number: the default.
        xml_element* value = xml_find(entry, cMission_briefing_volume);
        if (value != nullptr)
        {
            music_volume = (float)atof(value->text);
        }
        float bink_volume = 0.6f; // Magic number: the default.
        value = xml_find(entry, cMission_briefing_bink_volume);
        if (value != nullptr)
        {
            bink_volume = (float)atof(value->text);
        }
        farray<mission_briefing_line,40> lines;
        // Magic number: 40, the array's size.
        for (xml_element* line = xml_find(line_list, cMission_briefing_line_info); line != nullptr && lines.size() < 40; line = xml_find_next(line_list, line, cMission_briefing_line_info))
        {
            xml_element* frame = xml_find(line, cMission_briefing_line_frame);
            xml_element* line_name = xml_find(line, cMission_briefing_line_name);
            xml_element* line_color = xml_find(line, cMission_briefing_line_color);
            xml_element* persona = xml_find(line, cMission_briefing_line_persona);
            xml_element* pause_flag = xml_find(line, cMission_briefing_pause_flag);
            const int start_frame = frame != nullptr ? atoi(frame->text) : -1;
            bool pause = true;
            if (pause_flag != nullptr && v_stricmp(pause_flag->text, "false") == 0)
            {
                pause = false;
            }
            color col(255, 255, 255, 255);
            if (line_color != nullptr)
            {
                xml_element* r = xml_find(line_color, "R");
                xml_element* g = xml_find(line_color, "G");
                xml_element* b = xml_find(line_color, "B");
                // Magic number: 'w', 119, the default of a color's missing channel.
                unsigned char red = 'w';
                unsigned char green = 'w';
                unsigned char blue = 'w';
                if (r != nullptr)
                {
                    red = (unsigned char)atoi(r->text);
                }
                if (g != nullptr)
                {
                    green = (unsigned char)atoi(g->text);
                }
                if (b != nullptr)
                {
                    blue = (unsigned char)atoi(b->text);
                }
                col.red = red;
                col.green = green;
                col.blue = blue;
            }
            // Original bug, kept: a line with no Line or Persona element reads through null.
            lines.add(mission_briefing_line(start_frame, line_name->text, persona->text, pause, col));
        }
        int loop_frames = 0;
        value = xml_find(entry, cMission_briefing_loop_frames);
        if (value != nullptr)
        {
            loop_frames = atoi(value->text);
        }
        hud_ui_mission_briefing_add_mission_line_info(name->text, music->text, description->text, music_volume, bink_volume, lines, loop_frames);
    }
    Common_mempool.restore_to_mark(mark);
    return true;
}

// 0x00881260 (:1162)
void hud_ui_mission_briefing_init()
{
    state_info info;
    info.enter = hud_ui_mission_briefing_enter;
    info.exit = hud_ui_mission_briefing_exit;
    info.process = hud_ui_mission_briefing_process;
    info.handshake = hud_ui_mission_briefing_handshake;
    info.render = hud_ui_mission_briefing_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_MISSION_BRIEFING, info);
    humb_init_mb_info();
    hud_ui_mission_briefing_init_lines(cMission_briefing_table);
}

// 0x00858370 (:1183)
void hud_ui_mission_briefing_enter(game_state prev_state)
{
    (void)prev_state;
    Mb_info.doc_handle = ui_load_document(cUI_mission_briefing_doc, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Mb_info.doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    humb_clear_all_play_trigger_flags();
    hud_ui_unload_items();
    ui_set_screen_blur(true, true);
    Mb_info.control_id = ui_request_control_id();
    audiolib_cue_get_id(Mb_info.background_music, &Mb_info.sound_mb_background);
    Mb_info.play_sound_loop = -1;
    Mb_info.playing_audio_line = false;
    Mb_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    Mb_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    Mb_info.loaded = false;
    Mb_info.unloaded_doc = false;
    Mb_info.input_id = -1;
    Mb_info.allowing_user_input = false;
    Mb_info.user_response = 0;
    Allow_input_timer.set(2000); // Magic number: 2 seconds before the buttons answer.
    Stream_wait_timeout.set(35000); // Magic number: 35 seconds for the document and voices to load.
    Show_minimap = false;
    ui_wait_for_button_ups();
    if (!hasAnswered)
    {
        Mb_info.mb_state = MBS_TRIGGER_PLAY;
    }
    Mb_info.first_play = true;
    Mb_info.exiting = false;
    Mb_info.waiting_for_world = false;
    audiolib_cue_play(Ga_cue_ids.mission_briefing_intro, nullptr, nullptr);
    ui_set_max_frametime(0.033333335f); // Magic number: 1/30 second.
}

// 0x0084A570 (:1261)
void hud_ui_mission_briefing_exit(game_state new_state)
{
    (void)new_state;
    humb_stop_video();
    Show_minimap = true;
    humb_fadeout_sound();
    ui_relinquish_input_id(Mb_info.input_id);
    humb_unload_ui_information();
    if (!Mb_info.district_lib)
    {
        ui_fade_to_black_fade_in(false);
    }
    ui_set_screen_blur(false, false);
    ui_set_max_frametime(0.5f); // Magic number: the default.
    if (INVALID_VOICE_SET_HANDLE != Mb_info.loaded_persona_set)
    {
        human_voice_unload_set(Mb_info.loaded_persona_set);
        Mb_info.loaded_persona_set = INVALID_VOICE_SET_HANDLE;
    }
}

// 0x0084A660 (:1305)
static int hud_ui_mission_briefing_process_mouse()
{
    if (Mb_info.input_id != ui_get_current_input_id())
    {
        return 0;
    }
    int response = 0;
    if (ui_mouse_over_hint_combo(kHintImg1, kHintText1, Mb_info.doc_handle, true))
    {
        response = 2;
    }
    if (ui_mouse_over_hint_combo(kHintImg2, kHintText2, Mb_info.doc_handle, true))
    {
        response = 1;
    }
    return response;
}

// The exit half of hud_ui_mission_briefing_process, where every path ends in the same ui_process call.
static void hud_ui_mission_briefing_process_exiting()
{
    const bool state_is_top = gameseq_get_state() == GS_INGAME_MISSION_BRIEFING && gameseq_get_pending_state() == GS_NONE;
    if (Mb_info.waiting_for_world)
    {
        if ((ui_fade_to_black_is_faded() || ui_fade_to_black_is_fade_in_complete()) && !mission_is_active() && !Mb_info.district_lib)
        {
            mission_activate(Mb_info.mission_name, false);
        }
        if (mission_is_active() && mission_is_ready() && gameseq_get_state() == GS_INGAME_MISSION_BRIEFING && gameseq_get_pending_state() == GS_NONE)
        {
            gameseq_pop_state();
        }
        return;
    }
    if (!Mb_info.unloaded_doc)
    {
        if (Mb_info.bink_handle == 0)
        {
            humb_unload_ui_information();
            hasAnswered = false;
        }
        return;
    }
    if (!hud_ui_is_loaded() || Mb_info.bink_handle != 0)
    {
        return;
    }
    if (Mb_info.user_response == 2 && !Mb_info.district_lib)
    {
        Mb_info.waiting_for_world = true;
        ui_fade_to_black_fade_out(false);
        return;
    }
    if (Mission_parker_liberation_warp && Mb_info.district_lib && v_stricmp("Parker Liberation", Mb_info.mission_name) == 0)
    {
        if (!World.sl_flags.pending_save_game && !World.sl_flags.performing_save_state && state_is_top)
        {
            World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false);
        }
        return;
    }
    if (state_is_top)
    {
        gameseq_pop_state();
    }
}

// 0x00858490 (:1331)
void hud_ui_mission_briefing_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    bool processed = false;
    if (!msgbox_active)
    {
        if (Mb_info.exiting)
        {
            hud_ui_mission_briefing_process_exiting();
            ui_process(Mb_info.control_id, false);
            return;
        }
        if (!Mb_info.loaded)
        {
            Mb_info.loaded = human_voice_set_loaded(Mb_info.loaded_persona_set);
            if (!Mb_info.loaded)
            {
                voice_slotting_do_frame();
            }
            switch (ui_get_doc_asset_state(Mb_info.doc_handle))
            {
            case ST_UNLOADED:
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                hud_ui_mission_briefing_done();
                Mb_info.loaded = false;
                break;
            case ST_ERROR:
                hud_ui_mission_briefing_done();
                Mb_info.loaded = false;
                break;
            case ST_LOADING:
                Mb_info.loaded = false;
                break;
            case ST_LOADED:
                break;
            default:
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
            if (!Mb_info.loaded)
            {
                if (Stream_wait_timeout.elapsed())
                {
                    // Gave up on loading: this acts as an accept.
                    Mb_info.user_response = 2;
                    hud_ui_mission_briefing_done();
                }
                ui_process(Mb_info.control_id, false);
                return;
            }
            humb_load_ui_information();
            humb_obtain_bink_screen_info_handbook();
            Mb_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
        }
        if (!Mb_info.allowing_user_input && Allow_input_timer.elapsed())
        {
            Mb_info.allowing_user_input = true;
        }
        const int mouse_response = hud_ui_mission_briefing_process_mouse();
        bool answered = false;
        if (Mb_info.allowing_user_input)
        {
            // Magic numbers: 10 and 11, the document's accept and cancel buttons; 2 and 1, the answers.
            if (Mb_info.user_response == 0 && (ui_get_button_info(10, Mb_info.input_id) > 0.0f || mouse_response == 2))
            {
                hasAnswered = true;
                if (Mb_info.bink_handle != 0)
                {
                    video_player_pause(Mb_info.bink_handle);
                }
                Mb_info.mb_state = MBS_PAUSED;
                humb_stop_video();
                humb_fadeout_sound();
                Mb_info.user_response = 2;
                hud_ui_clear_current_radio_message(0);
                int cue;
                if (audiolib_cue_get_id(cAcceptSound, &cue) == AUDIOLIB_ERR_NONE)
                {
                    audiolib_cue_play(cue, nullptr, nullptr);
                }
                audiolib_cue_play(Ga_cue_ids.mission_briefing_outro, nullptr, nullptr);
                dialogue_manager_cancel_all();
                answered = true;
            }
            else if (Mb_info.user_response == 0 && !Mb_info.district_lib && (ui_get_button_info(11, Mb_info.input_id) > 0.0f || mouse_response == 1))
            {
                hasAnswered = true;
                if (Mb_info.bink_handle != 0)
                {
                    video_player_pause(Mb_info.bink_handle);
                }
                Mb_info.mb_state = MBS_PAUSED;
                humb_stop_video();
                humb_fadeout_sound();
                Mb_info.user_response = 1;
                int cue;
                if (audiolib_cue_get_id(cCancelSoundBriefing, &cue) == AUDIOLIB_ERR_NONE)
                {
                    audiolib_cue_play(cue, nullptr, nullptr);
                }
                audiolib_cue_play(Ga_cue_ids.mission_briefing_outro, nullptr, nullptr);
                dialogue_manager_cancel_briefing();
                answered = true;
            }
        }
        if (!answered)
        {
            processed = true;
            humb_process_bink_video();
            humb_process_data_items();
            ui_add_for_processing(Mb_info.doc_handle);
            if (Mb_info.user_response != 0)
            {
                humb_stop_video_lines();
            }
            Mb_info.first_play = false;
        }
    }
    ui_process(Mb_info.control_id, msgbox_active);
    if (processed)
    {
        humb_determine_video_screen_info();
    }
}

// 0x0084A6D0 (:1542)
void hud_ui_mission_briefing_handshake(rl_renderer*)
{
}

// 0x0084A6E0 (:1554)
void hud_ui_mission_briefing_render(rl_renderer*)
{
    if (!Mb_info.exiting && !Mb_info.first_play)
    {
        ui_add_for_rendering(Mb_info.doc_handle);
    }
    ui_render(Mb_info.control_id, false);
}

// 0x0084A720 (:1568)
void hud_ui_mission_briefing_pause(bool pause, bool from_script)
{
    if (Mb_info.bink_handle == 0)
    {
        if (pause)
        {
            Mb_info.mb_state = MBS_PAUSED;
        }
        else if (!hasAnswered)
        {
            Mb_info.mb_state = MBS_TRIGGER_PLAY;
        }
        return;
    }
    if (pause)
    {
        video_player_pause(Mb_info.bink_handle);
        Mb_info.mb_state = MBS_PAUSED;
        return;
    }
    if (from_script)
    {
        humb_fadein_sound();
        if (!Mb_info.district_lib)
        {
            Mb_info.allowing_user_input = true;
        }
    }
    video_player_unpause(Mb_info.bink_handle);
    Mb_info.mb_state = MBS_PLAYING;
}

// 0x008589B0 (:1605)
bool hud_ui_mission_briefing_start(const char* mission_name, bool district_lib)
{
    if ((!district_lib && (game_is_paused() || gameseq_get_pending_state() != GS_NONE)) || mission_name == nullptr)
    {
        return false;
    }
    // The video's name: the mission's, with underscores for spaces and no commas.
    char video_name[128];
    int length = 0;
    const int name_length = (int)strlen(mission_name);
    for (int i = 0; i < name_length; i++)
    {
        const char c = mission_name[i];
        if (c == ' ')
        {
            video_name[length++] = '_';
        }
        else if (c != ',')
        {
            video_name[length++] = c;
        }
    }
    video_name[length] = '\0';
    Mb_info.background_music[0] = '\0';
    float bink_volume = 0.6f; // Magic number: the defaults, as in init_lines.
    float music_volume = 0.8f;
    Mb_info.which_mission = -1;
    Mb_info.which_mission_line_index = -1;
    for (int i = 0; i < Mb_info.mission_line_infos.size(); i++)
    {
        const mission_briefing_line_info& info = Mb_info.mission_line_infos[i];
        if (v_stricmp(info.mission_name, mission_name) == 0)
        {
            Mb_info.which_mission = i;
            // The table's strings are the format strings here, as in rfg.exe.
            _snprintf_safe(Mb_info.background_music, 0x20, info.background_music);
            _snprintf_safe(Mb_info.mission_description, 0x40, info.mission_description);
            bink_volume = info.bink_video_volume;
            music_volume = info.back_music_volume;
        }
    }
    Mb_info.bink_volume = bink_volume;
    Mb_info.background_music_volume = music_volume;
    Mb_info.loop_frames = 15; // Magic number: the last 15 frames loop. The table's loop_frames is not read here.
    _snprintf_safe(Mb_info.mission_name, 0x40, "%s", mission_name);
    _snprintf_safe(Mb_info.bink_filename, 0x100, "%s.bik", video_name);
    str_make_lower(Mb_info.bink_filename);
    Mb_info.district_lib = district_lib;
    const bool has_video = video_player_video_exists(Mb_info.bink_filename, nullptr);
    if (has_video && Mb_info.which_mission != -1)
    {
        if (!human_voice_load_set(Mb_info.mission_line_infos[Mb_info.which_mission].required_personas))
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            game_disable_standard_access();
            return false;
        }
        Mb_info.loaded_persona_set = Mb_info.mission_line_infos[Mb_info.which_mission].required_personas;
    }
    else
    {
        Mb_info.loaded_persona_set = INVALID_VOICE_SET_HANDLE;
    }
    if (has_video)
    {
        gameseq_push_state(GS_INGAME_MISSION_BRIEFING, true, false);
    }
    else
    {
        mission_activate(Mb_info.mission_name, false);
    }
    return has_video;
}

// 0x0084A7A0 (:1700)
void hud_ui_mission_briefing_done()
{
    humb_request_exit();
    if (INVALID_VOICE_SET_HANDLE != Mb_info.loaded_persona_set)
    {
        human_voice_unload_set(Mb_info.loaded_persona_set);
        Mb_info.loaded_persona_set = INVALID_VOICE_SET_HANDLE;
    }
}

// 0x00858C60 (:1718)
mission_briefing_line_info* hud_ui_mission_briefing_get_line_info(const char* name)
{
    for (int i = 0; i < Mb_info.mission_line_infos.size(); i++)
    {
        if (v_stricmp(Mb_info.mission_line_infos[i].mission_name, name) == 0)
        {
            return &Mb_info.mission_line_infos[i];
        }
    }
    return nullptr;
}

// 0x00858CC0 (:1734)
void hud_ui_mission_briefing_mark()
{
    Mission_line_infos_mark = Mb_info.mission_line_infos.size();
}

// 0x008812C0 (:1739)
void hud_ui_mission_briefing_restore_to_mark()
{
    for (int i = Mb_info.mission_line_infos.size() - 1; i >= Mission_line_infos_mark; i--)
    {
        Mb_info.mission_line_infos.remove(i);
    }
}
