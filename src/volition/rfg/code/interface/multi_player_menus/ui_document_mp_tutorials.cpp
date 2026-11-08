// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp
//
// The training videos (GS_MULTIPLAYER_TUTORIALS, stub_impl.md S7) on rfg_ui_mp_shell: a list from mp_tutorials_layout.xtbl,
// the selected Bink video plays with the mission briefing table's voice lines; its last frames loop while paused.
//
// Original bugs kept (commented): the video's volume shrinks with each play; the document stays loaded on exit; an empty
// list divides by zero.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.h"

#include <cstdlib>
#include <cstring>

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_tutorials
{
    MMEA_T_POP = 0,
    MMEA_T_PUSH = 1,
};

enum mp_menu_states_tutorials
{
    MMS_T_LOADING = 0,
    MMS_T_LOADED = 1,
    MMS_T_STOPPING_VIDEO = 2,
    MMS_T_UNLOADING = 3,
    MMS_T_UNLOADED = 4,
    NUM_MP_T_MENU_STATES = 5,
};

enum tutorials_hint_choices
{
    MPT_HINT_INVALID = -1,
    MPT_HINT_ABORT = 0,
    NUM_MPT_HINTS = 1,
};

// size 0xC
struct tutorials_menu_option_info
{
    char* m_option; // 0x00
    char* m_video; // 0x04
    unsigned int m_description_crc; // 0x08
};
static_assert(sizeof(tutorials_menu_option_info) == 0xC, "tutorials_menu_option_info");

// size 0xC4
struct mp_tutorials_shell_info
{
    mp_menu_states_tutorials state; // 0x00
    mp_menu_states_tutorials pending_state; // 0x04
    mp_menu_states_tutorials prev_state; // 0x08
    mp_menu_exit_actions_tutorials exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
    tutorial_video_info bink_info; // 0x74
    rfg_split_mempool* _bink_mempool; // 0xC0
};
static_assert(sizeof(mp_tutorials_shell_info) == 0xC4, "mp_tutorials_shell_info");

// ---- file-static globals
static void tutorials_process_data_item_hint_text(vint_data_item* vdi);
static void tutorials_process_data_item_titles(vint_data_item* vdi);
static void tutorials_process_data_item_selection_info(vint_data_item* vdi);
static void tutorials_process_data_item_menu_type(vint_data_item* vdi);
static void tutorials_process_data_item_video(vint_data_item* vdi);
static tutorials_hint_choices Current_mouse_hint_choice_tutorials = MPT_HINT_INVALID; // 0x01663CE4
static mp_shell_data_item_info Tutorials_data_items[5] = { // 0x01663CE8
    {MPSDIT_HINT_INFO, tutorials_process_data_item_hint_text},
    {MPSDIT_TITLES, tutorials_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, tutorials_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, tutorials_process_data_item_menu_type},
    {MPSDIT_VIDEO, tutorials_process_data_item_video},
};
static const char* Back_string_tutorials = {"MENU_BACK"}; // 0x01663D10
static mp_tutorials_shell_info Mp_tutorials_info; // 0x01666BA0
static int tutorials_hint_config[1]; // 0x02BE9E20
static int tutorials_num_hints; // 0x02C0369C
static tutorials_menu_option_info* Tutorials_menu_options; // 0x02C036A0
static int Num_tutorials_menu_options; // 0x02C036A4
static int Tutorials_menu_selection; // 0x02C036A8

// ---- internal functions
static void tutorials_load_ui_information();
static void tutorials_unload_ui_information();

// 0x0088ECE0 (:130)
void tutorial_video_info::reset()
{
    filename = nullptr;
    handle = 0;
    line_info = nullptr;
    state = BINK_STATE_TUTORIALS_STOPPED;
    num_frames = 0;
    position.x = 0;
    position.y = 0;
    size.x = 0;
    size.y = 0;
    alpha = 1.0f;
    curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    cur_vl_id = -1;
    playing_audio_line = false;
    which_line_index = 0;
    mempool_mark = 0;
    mempool_mark_gpu = 0;
}

// 0x0088ED30
tutorial_video_info::tutorial_video_info()
{
    reset();
}

// Port helper: a string copied into the pool, inlined in rfg.exe.
static char* tutorials_pool_copy_string(mempool_base& pool, const char* text)
{
    const size_t size = strlen(text) + 1;
    char* copy = (char*)pool.alloc((unsigned int)size, 0);
    memcpy(copy, text, size);
    return copy;
}

// 0x008D4DF0 (:260)
static bool parse_video(mempool_base& pool, xml_element* root, tutorials_menu_option_info* video)
{
    const char* name = xml_table_get_req_string_ref(root, "Display_Name");
    if (name == nullptr)
    {
        return false;
    }
    video->m_option = tutorials_pool_copy_string(pool, name);
    const char* description = xml_table_get_req_string_ref(root, "Display_Description");
    if (description == nullptr)
    {
        return false;
    }
    video->m_description_crc = localization_get_id_hash(description);
    xml_element* video_element = xml_table_find_req(root, "Video");
    if (video_element == nullptr)
    {
        return false;
    }
    const char* filename = xml_table_get_req_string_ref(video_element, "Filename");
    if (filename == nullptr)
    {
        return false;
    }
    video->m_video = tutorials_pool_copy_string(pool, filename);
    return true;
}

// 0x008D4EB0 (:283)
static void parse_layout(mempool_base& pool)
{
    xml_element* root = xml_table_parse("mp_tutorials_layout.xtbl");
    if (root != nullptr)
    {
        Tutorials_menu_options = (tutorials_menu_option_info*)pool.alloc(xml_table_count(root, "videos") * sizeof(tutorials_menu_option_info), 0);
        Num_tutorials_menu_options = 0;
        for (xml_element* element = xml_find(root, "videos"); element != nullptr; element = xml_find_next(root, element, "videos"))
        {
            if (parse_video(pool, element, &Tutorials_menu_options[Num_tutorials_menu_options]))
            {
                Num_tutorials_menu_options++;
            }
        }
    }
    xml_table_close(root);
}

// 0x0088ED40 (:303)
static void tutorials_alloc_video_memory()
{
    if (Mp_tutorials_info._bink_mempool == nullptr)
    {
        // Magic number: the 8.5 MB the training videos need.
        Mp_tutorials_info._bink_mempool = salloc_pregame_split_lock(0x810000, 0);
    }
}

// 0x0088ED60 (:315)
static void tutorials_free_video_memory()
{
    if (Mp_tutorials_info._bink_mempool != nullptr)
    {
        salloc_pregame_split_unlock(Mp_tutorials_info._bink_mempool);
        Mp_tutorials_info._bink_mempool = nullptr;
    }
}

// 0x008E5D70 (:329)
static void tutorials_do_state_change()
{
    do
    {
        const mp_menu_states_tutorials previous = Mp_tutorials_info.state;
        Mp_tutorials_info.state = Mp_tutorials_info.pending_state;
        if (Mp_tutorials_info.pending_state == MMS_T_LOADED)
        {
            if (previous == MMS_T_LOADING)
            {
                tutorials_load_ui_information();
                tutorials_alloc_video_memory();
            }
        }
        else if (Mp_tutorials_info.pending_state == MMS_T_UNLOADING)
        {
            tutorials_unload_ui_information();
            tutorials_free_video_memory();
            if (Mp_tutorials_info.exit_action == MMEA_T_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_tutorials_info.exit_action == MMEA_T_PUSH)
            {
                gameseq_push_state(Mp_tutorials_info.exit_push_target, true, true);
            }
        }
        else
        {
            return;
        }
    }
    while (Mp_tutorials_info.state != Mp_tutorials_info.pending_state);
}

// 0x0088ED90 (:386)
static void tutorials_set_state(mp_menu_states_tutorials new_state)
{
    Mp_tutorials_info.pending_state = new_state;
}

// 0x0088EDA0 (:395) inlined
static void tutorials_init_state()
{
    Mp_tutorials_info.state = MMS_T_UNLOADED;
    Mp_tutorials_info.pending_state = MMS_T_UNLOADED;
    Mp_tutorials_info.prev_state = MMS_T_UNLOADED;
    Mp_tutorials_info.exit_action = MMEA_T_POP;
    Mp_tutorials_info.exit_push_target = GS_MULTIPLAYER_TUTORIALS;
}

// 0x008D5020 (:407)
// The video's handle goes to the document's video_holder, whose rectangle and alpha come back.
static void tutorials_obtain_bink_screen_info(vint_data_item* vdi)
{
    vint_doc_info* info = ui_get_doc_info(Mp_tutorials_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_element_base* element = (vint_element_base*)vint_object_base::find_object("video_holder", info->m_doc);
    if (element == nullptr)
    {
        return;
    }
    vint_variant handle;
    handle = Mp_tutorials_info.bink_info.handle;
    element->set_property_value("vid_id_handle", handle, 0);
    vector2 anchor;
    element->get_global_anchor(anchor);
    vint_variant size_value;
    element->get_property_value("screen_size", size_value, 0);
    vector2 size(0.0f, 0.0f);
    if (size_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
    {
        size = (vector2)size_value;
    }
    vint_variant alpha;
    element->get_property_value("alpha", alpha, 0);
    vint_res_offset offset_x;
    vint_res_offset offset_y;
    vint_res_scale scale_x;
    vint_res_scale scale_y;
    element->get_res_offsets_and_scales_inherited(offset_x, offset_y, scale_x, scale_y);
    const vector2 scale = vint_project_get_scale(scale_x, scale_y);
    const vector2 offset = vint_project_get_offset(offset_x, offset_y);
    Mp_tutorials_info.bink_info.position.x = fl_round(scale.x * anchor.x + offset.x);
    Mp_tutorials_info.bink_info.position.y = fl_round(scale.y * anchor.y + offset.y);
    Mp_tutorials_info.bink_info.size.x = fl_round(size.x * scale.x);
    Mp_tutorials_info.bink_info.size.y = fl_round(size.y * scale.y);
    Mp_tutorials_info.bink_info.alpha = (float)alpha;
}

// 0x0088EDD0 (:487)
static void tutorials_video_stop_callback(int handle)
{
    rfg_split_mempool* pool = Mp_tutorials_info._bink_mempool;
    if (pool == nullptr)
    {
        return;
    }
    Mp_tutorials_info.bink_info.handle = 0;
    Mp_tutorials_info.bink_info.state = BINK_STATE_TUTORIALS_STOPPED;
    // Magic number: 0x40 is the pool's GPU half flag.
    if ((pool->get_flags() & 0x40) != 0)
    {
        pool->restore_to_mark_gpu(Mp_tutorials_info.bink_info.mempool_mark_gpu);
    }
    pool->restore_to_mark_cpu(Mp_tutorials_info.bink_info.mempool_mark);
    if (gameseq_get_state() != GS_MULTIPLAYER_TUTORIALS)
    {
        tutorials_free_video_memory();
    }
}

// 0x0088EE40 (:509)
static void tutorials_pause_video(bool pause)
{
    if (pause)
    {
        video_player_pause(Mp_tutorials_info.bink_info.handle);
        Mp_tutorials_info.bink_info.state = BINK_STATE_TUTORIALS_PAUSED;
    }
    else
    {
        video_player_unpause(Mp_tutorials_info.bink_info.handle);
        Mp_tutorials_info.bink_info.state = BINK_STATE_TUTORIALS_PLAYING;
    }
}

// 0x0088EE80 (:520)
static void tutorials_stop_video_lines()
{
    if (Mp_tutorials_info.bink_info.playing_audio_line)
    {
        if (human_voice_persona_from_handle(Mp_tutorials_info.bink_info.curr_persona_handle) != nullptr)
        {
            audiolib_play_stop(Mp_tutorials_info.bink_info.cur_vl_id);
        }
        Mp_tutorials_info.bink_info.playing_audio_line = false;
        Mp_tutorials_info.bink_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
        Mp_tutorials_info.bink_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    }
}

// 0x008AEA90 (:538)
static void tutorials_clear_all_play_trigger_flags()
{
    if (Mp_tutorials_info.bink_info.line_info != nullptr)
    {
        farray<mission_briefing_line,40>& lines = Mp_tutorials_info.bink_info.line_info->lines;
        for (int i = 0; i < lines.size(); i++)
        {
            lines[i].has_triggered = false;
        }
    }
}

// 0x008AEAD0 (:550)
static void tutorials_stop_video()
{
    if (Mp_tutorials_info.bink_info.handle != 0)
    {
        video_player_stop(Mp_tutorials_info.bink_info.handle);
    }
    tutorials_stop_video_lines();
    tutorials_clear_all_play_trigger_flags();
}

// 0x0088EEC0 (:560)
static bool tutorials_is_video_line_still_playing()
{
    tutorial_video_info& bink = Mp_tutorials_info.bink_info;
    if (bink.playing_audio_line && bink.curr_vl_handle != INVALID_VOICE_LINE_HANDLE && bink.curr_persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        bink.playing_audio_line = audiolib_play_get_state(bink.cur_vl_id, nullptr) == AUDIOLIB_ERR_NONE;
        if (!bink.playing_audio_line)
        {
            audiolib_play_stop(bink.cur_vl_id);
        }
    }
    return bink.playing_audio_line;
}

// 0x008AEB60 (:581)
void tutorials_trigger_video_line(int which_line)
{
    tutorial_video_info& bink = Mp_tutorials_info.bink_info;
    if (bink.line_info == nullptr || bink.which_line_index < 0 || bink.which_line_index >= bink.line_info->lines.size())
    {
        return;
    }
    const mission_briefing_line& line = bink.line_info->lines[which_line];
    bink.curr_vl_handle = line.vl_handle;
    bink.curr_persona_handle = line.persona_handle;
    human_voice_persona* persona = human_voice_persona_from_handle(bink.curr_persona_handle);
    if (persona != nullptr)
    {
        const vector pos(0.0f, 0.0f, 0.0f);
        unsigned short radio_id;
        bink.playing_audio_line = persona->persona_play_voice_line(bink.curr_vl_handle, pos, AUDIOLIB_CUE_PRIORITY_MEDIUM, bink.cur_vl_id, radio_id, nullptr, false, false, true, true, nullptr);
        if (!bink.playing_audio_line)
        {
            bink.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
            bink.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
        }
    }
}

// 0x008AEC20 (:611)
// The next due line starts when the last one is through; a line that insists pauses the video until then.
static void tutorials_determine_audio_and_video_status(int curr_video_frame)
{
    tutorial_video_info& bink = Mp_tutorials_info.bink_info;
    if (bink.line_info == nullptr || bink.line_info->lines.size() < 1)
    {
        return;
    }
    farray<mission_briefing_line,40>& lines = bink.line_info->lines;
    const bool line_playing = tutorials_is_video_line_still_playing();
    int which_line = -1;
    bool pause = false;
    for (int i = 0; i < lines.size(); i++)
    {
        if (!lines[i].has_triggered && lines[i].start_frame <= curr_video_frame)
        {
            if (which_line == -1)
            {
                which_line = i;
            }
            if (lines[i].pause_if_prev_not_done && line_playing)
            {
                pause = true;
            }
        }
    }
    if (!line_playing && which_line != -1)
    {
        if (!pause)
        {
            tutorials_pause_video(false);
        }
        bink.which_line_index = which_line;
        lines[which_line].has_triggered = true;
        tutorials_trigger_video_line(which_line);
    }
    if (pause)
    {
        tutorials_pause_video(true);
    }
}

// 0x008AED00 (:679)
static void tutorials_process_video()
{
    tutorial_video_info& bink = Mp_tutorials_info.bink_info;
    switch (bink.state)
    {
    case BINK_STATE_TUTORIALS_PLAY:
        if (bink.handle != 0 || bink.line_info == nullptr)
        {
            tutorials_stop_video();
        }
        if (Mp_tutorials_info._bink_mempool != nullptr)
        {
            rfg_split_mempool* pool = Mp_tutorials_info._bink_mempool;
            bink.mempool_mark = pool->mark();
            // Magic number: 0x40 is the pool's GPU half flag.
            bink.mempool_mark_gpu = (pool->get_flags() & 0x40) != 0 ? pool->mark_gpu() : 0;
            float scalar = 1.0f;
            // Magic number: the volume scalar of the video category.
            audiolib_system_get_volume_scalar(4, &scalar);
            // Original bug: scaled in the shared table, so the video gets quieter with each play.
            bink.line_info->bink_video_volume = bink.line_info->bink_video_volume * scalar;
            bink.handle = video_player_play(bink.filename, VPM_ONCE, bink.position.x, bink.position.y, bink.size.x, bink.size.y, bink.line_info->bink_video_volume, true, tutorials_video_stop_callback, pool, nullptr, nullptr, 0, false, false);
            bink.num_frames = video_player_get_total_frames(bink.handle);
            bink.state = BINK_STATE_TUTORIALS_PLAYING;
        }
        break;
    case BINK_STATE_TUTORIALS_STOPPED:
        break;
    case BINK_STATE_TUTORIALS_PLAYING:
    {
        const int frame = video_player_get_current_frame(bink.handle);
        if (bink.handle != 0)
        {
            tutorials_determine_audio_and_video_status(frame);
        }
        break;
    }
    case BINK_STATE_TUTORIALS_PAUSED:
        if (bink.handle != 0)
        {
            const int frame = video_player_get_current_frame(bink.handle);
            // Magic number: the last frames; after them the video loops silently.
            if (bink.num_frames - frame < 3)
            {
                video_player_set_volume(bink.handle, 0.0f);
                video_player_goto_frame(bink.handle, 0);
                bink.state = BINK_STATE_TUTORIALS_LOOPING_END;
            }
            else
            {
                tutorials_determine_audio_and_video_status(frame);
            }
        }
        break;
    case BINK_STATE_TUTORIALS_LOOPING_END:
        os_sleep(1);
        if (bink.handle != 0 && bink.num_frames - video_player_get_current_frame(bink.handle) < 3)
        {
            video_player_goto_frame(bink.handle, bink.num_frames - bink.line_info->loop_frames);
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_tutorials.cpp", 679);
        break;
    }
}

// 0x008AEF20 (:781) inlined
static void tutorials_init_info()
{
    Mp_tutorials_info.group_item_handles.clear();
    Mp_tutorials_info.data_group_handle = -1;
}

// 0x008C29B0 (:789)
static void tutorials_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find(Mp_tutorials_info.data_group_handle);
    for (int i = 0; i < Num_tutorials_menu_options; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            vint_variant_type types[7] = {
                VINT_PROP_TYPE_INT, // index
                VINT_PROP_TYPE_INT,
                VINT_PROP_TYPE_STRING, // name
                VINT_PROP_TYPE_INT,
                VINT_PROP_TYPE_STRING,
                VINT_PROP_TYPE_INT,
                VINT_PROP_TYPE_BOOL,
            };
            item->set_template(types, 7);
            Mp_tutorials_info.group_item_handles.add((int)item->get_handle());
        }
    }
}

// 0x008D54E0 (:821) inlined
static void tutorials_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_tutorials_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_tutorials_info.doc_handle, Mp_tutorials_info.data_group_handle);
        tutorials_load_menu_option_data_items();
    }
}

// 0x008D5510 (:857)
static void tutorials_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Tutorials_data_items, 5);
    tutorials_load_data_groups();
}

// 0x008D5550 (:867)
static void tutorials_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    tutorials_init_info();
}

// 0x008E5E10 (:875)
static void tutorials_process_data_item_hint_text(vint_data_item* vdi)
{
    tutorials_hint_config[0] = MPT_HINT_ABORT;
    tutorials_num_hints = 1;
    multi_player_menu_system_begin_hints(vdi);
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Back_string_tutorials), CBA_MENU_ABORT);
    multi_player_menu_system_end_hints();
}

// 0x0088EF20 (:889)
static void tutorials_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("MENU_TRAINING_VIDEOS"));
    vdi->set_element(1, vint_variant(Tutorials_menu_options[Tutorials_menu_selection].m_option));
}

// 0x0088EFA0 (:895)
static void tutorials_process_data_item_selection_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Tutorials_menu_selection));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant(false));
}

// 0x0088F030 (:902)
static void tutorials_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant((int)0));
}

// 0x008D55C0 (:907)
static void tutorials_process_data_item_video(vint_data_item* vdi)
{
    tutorial_video_info& bink = Mp_tutorials_info.bink_info;
    if (bink.handle == 0)
    {
        return;
    }
    const vector2i old_position = bink.position;
    const vector2i old_size = bink.size;
    tutorials_obtain_bink_screen_info(vdi);
    if (old_position.x != bink.position.x || old_position.y != bink.position.y || old_size.x != bink.size.x || old_size.y != bink.size.y)
    {
        video_player_resize(bink.handle, bink.position.x, bink.position.y, bink.size.x, bink.size.y);
    }
    video_player_set_alpha(bink.handle, bink.alpha);
    if (bink.state != BINK_STATE_TUTORIALS_PLAYING)
    {
        return;
    }
    // The subtitle: the spoken line's, else the video's description.
    unsigned int text_crc = Tutorials_menu_options[Tutorials_menu_selection].m_description_crc;
    if (bink.which_line_index < bink.line_info->lines.size() && bink.curr_persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && bink.curr_vl_handle != INVALID_VOICE_LINE_HANDLE)
    {
        human_voice_persona* persona = human_voice_persona_from_handle(bink.curr_persona_handle);
        if (persona != nullptr)
        {
            persona->get_line_crc(bink.curr_vl_handle, text_crc);
        }
    }
    if (bink.num_frames - video_player_get_current_frame(bink.handle) < 3)
    {
        tutorials_pause_video(true);
        bink.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    }
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(Tutorials_menu_options[Tutorials_menu_selection].m_video));
    vdi->set_element(2, vint_variant(text_crc));
}

// 0x008AEF40 (:950)
static void tutorials_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find(Mp_tutorials_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int num_items = group->get_num_data_items();
    for (int i = 0; i < num_items; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_tutorials_info.group_item_handles[i]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(i + 1));
            item->set_element(1, vint_variant(-1));
            item->set_element(2, vint_variant(Tutorials_menu_options[i].m_option));
            item->set_element(3, vint_variant(-1));
            item->set_element(4, vint_variant(" "));
            item->set_element(5, vint_variant(-1));
            item->set_element(6, vint_variant(false));
        }
    }
}

// 0x008AF0E0 (:979) inlined
static void tutorials_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    tutorials_process_data_group_menu_options();
}

// 0x008F38D0 (:988)
static void tutorials_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_tutorials_info.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_tutorials.cpp", 988);
        ui_unload_document(Mp_tutorials_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_tutorials_info.doc_handle = -1;
        tutorials_set_state(MMS_T_UNLOADING);
        break;
    case ST_ERROR:
        ui_unload_document(Mp_tutorials_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_tutorials_info.doc_handle = -1;
        tutorials_set_state(MMS_T_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        tutorials_set_state(MMS_T_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_tutorials.cpp", 988);
        break;
    }
}

// 0x008AF0F0 (:1016)
static void tutorials_input_set_selection(int new_selection, bool& events_consumed)
{
    const int previous = Tutorials_menu_selection;
    const int num_rows = Mp_tutorials_info.group_item_handles.size();
    Tutorials_menu_selection = new_selection;
    if (new_selection < 0)
    {
        Tutorials_menu_selection = num_rows - 1;
    }
    if (new_selection > num_rows - 1)
    {
        Tutorials_menu_selection = 0;
    }
    const int cue = new_selection > previous ? Audio_info[1] : (new_selection < previous ? Audio_info[0] : Audio_info[11]);
    audiolib_cue_play(cue, nullptr, nullptr);
    if (Tutorials_menu_selection != previous && Mp_tutorials_info.bink_info.state != BINK_STATE_TUTORIALS_STOPPED)
    {
        tutorials_stop_video();
    }
    events_consumed = true;
}

// 0x008D57C0 (:1049)
static void tutorials_input_process_mouse(bool& events_consumed)
{
    Current_mouse_hint_choice_tutorials = MPT_HINT_INVALID;
    if ((Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)) && ui_mouse_over_hint(kHintText1, Mp_tutorials_info.doc_handle, true))
    {
        Current_mouse_hint_choice_tutorials = (tutorials_hint_choices)tutorials_hint_config[0];
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Tutorials_menu_selection, Mp_tutorials_info.group_item_handles.size(), Mp_tutorials_info.input_id, Mp_tutorials_info.doc_handle, events_consumed);
    if (selection >= 0 && mouse_just_released(0) && selection != Tutorials_menu_selection)
    {
        tutorials_input_set_selection(selection, events_consumed);
    }
}

// 0x008D5860 (:1077)
static void tutorials_process_loaded()
{
    tutorials_process_video();
    tutorials_process_data_items();
    const int input_id = Mp_tutorials_info.input_id;
    // Magic numbers: the button indices of ui_get_button_info.
    const bool select = ui_get_button_info(10, input_id) > 0.0f;
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f;
    bool back = ui_get_button_info(11, input_id) > 0.0f;
    bool events_consumed = false;
    tutorials_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    back = back || Current_mouse_hint_choice_tutorials == MPT_HINT_ABORT;
    const int previous = Tutorials_menu_selection;
    if (up)
    {
        Tutorials_menu_selection = previous - 1 < 0 ? Mp_tutorials_info.group_item_handles.size() - 1 : previous - 1;
        audiolib_cue_play(Tutorials_menu_selection != previous ? Audio_info[1] : Audio_info[11], nullptr, nullptr);
    }
    else if (down)
    {
        Tutorials_menu_selection = previous + 1;
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
    }
    // Original bug: with an empty list (no table entries) this divides by zero.
    Tutorials_menu_selection = Tutorials_menu_selection % Mp_tutorials_info.group_item_handles.size();
    if (Mp_tutorials_info.bink_info.state != BINK_STATE_TUTORIALS_STOPPED)
    {
        if (Tutorials_menu_selection != previous)
        {
            tutorials_stop_video();
        }
    }
    else
    {
        // Stopped: the selected video plays next, again and again.
        Mp_tutorials_info.bink_info.filename = Tutorials_menu_options[Tutorials_menu_selection].m_video;
        char name[512];
        cf_drop_ext(name, 512, Mp_tutorials_info.bink_info.filename);
        Mp_tutorials_info.bink_info.line_info = hud_ui_mission_briefing_get_line_info(name);
        tutorials_clear_all_play_trigger_flags();
        Mp_tutorials_info.bink_info.state = BINK_STATE_TUTORIALS_PLAY;
    }
    if (!select && back)
    {
        Mp_tutorials_info.exit_action = MMEA_T_POP;
        tutorials_set_state(MMS_T_STOPPING_VIDEO);
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
}

// 0x0088F060 (:1178)
static void tutorials_ui_pop()
{
    Mp_tutorials_info.exit_action = MMEA_T_POP;
    tutorials_set_state(MMS_T_STOPPING_VIDEO);
}

// 0x00917490 (:1190)
void ui_document_mp_tutorials_init()
{
    state_info info;
    info.enter = ui_document_mp_tutorials_enter;
    info.exit = ui_document_mp_tutorials_exit;
    info.process = ui_document_mp_tutorials_process;
    info.handshake = ui_document_mp_tutorials_handshake;
    info.render = ui_document_mp_tutorials_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_TUTORIALS, info);
}

// 0x008D5BD0 (:1207)
void ui_document_mp_tutorials_multi_init(mempool_base& pool)
{
    Tutorials_menu_options = nullptr;
    Num_tutorials_menu_options = 0;
    parse_layout(pool);
}

// 0x0088F080 (:1215)
void ui_document_mp_tutorials_multi_shutdown()
{
    Tutorials_menu_options = nullptr;
    Num_tutorials_menu_options = 0;
}

// 0x0090E890 (:1222)
void ui_document_mp_tutorials_enter(game_state prev_state)
{
    Mp_tutorials_info.bink_info.reset();
    Mp_tutorials_info._bink_mempool = nullptr;
    // Magic string: the training videos' narrator persona.
    human_voice_persona* persona = human_voice_persona_from_handle(human_voice_persona_handle_from_name("DAN"));
    if (persona != nullptr)
    {
        persona->request_stream(true);
    }
    tutorials_init_state();
    tutorials_init_info();
    if (Bink_info_main.handle != 0)
    {
        video_player_pause(Bink_info_main.handle);
    }
    multi_player_menu_system_shell_load_doc(Mp_tutorials_info.doc_handle, false);
    Mp_tutorials_info.control_id = ui_request_control_id();
    Mp_tutorials_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
}

// 0x008D5BF0 (:1254)
void ui_document_mp_tutorials_exit(game_state new_state)
{
    // Original bug: the document stays loaded on exit.
    tutorials_unload_ui_information();
    ui_relinquish_control_id(Mp_tutorials_info.control_id);
    ui_relinquish_input_id(Mp_tutorials_info.input_id);
    tutorials_stop_video();
    if (!game_booted_to_main_menu() && Bink_info_main.handle != 0)
    {
        video_player_unpause(Bink_info_main.handle);
    }
    human_voice_persona* persona = human_voice_persona_from_handle(human_voice_persona_handle_from_name("DAN"));
    if (persona != nullptr)
    {
        persona->request_destream(true);
    }
}

// 0x009157F0 (:1279)
void ui_document_mp_tutorials_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Mp_tutorials_info.doc_handle);
    switch (Mp_tutorials_info.state)
    {
    case MMS_T_LOADING:
        tutorials_process_loading();
        break;
    case MMS_T_LOADED:
        tutorials_process_loaded();
        break;
    case MMS_T_STOPPING_VIDEO:
        if (Mp_tutorials_info.bink_info.state != BINK_STATE_TUTORIALS_STOPPED)
        {
            tutorials_stop_video();
        }
        tutorials_set_state(MMS_T_UNLOADING);
        break;
    case MMS_T_UNLOADED:
        tutorials_set_state(Mp_tutorials_info.doc_handle != -1 ? MMS_T_LOADING : MMS_T_UNLOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_tutorials_info.control_id, false);
}

// 0x008E5E90 (:1319)
void ui_document_mp_tutorials_handshake(rl_renderer*)
{
    Mp_tutorials_info.prev_state = Mp_tutorials_info.state;
    if (Mp_tutorials_info.pending_state != Mp_tutorials_info.state)
    {
        tutorials_do_state_change();
    }
}

// 0x008FC960 (:1327)
void ui_document_mp_tutorials_render(rl_renderer*)
{
    if (Mp_tutorials_info.doc_handle == -1 || Mp_tutorials_info.control_id == 0)
    {
        return;
    }
    if (Mp_tutorials_info.state == MMS_T_LOADED && Mp_tutorials_info.prev_state == MMS_T_LOADED)
    {
        ui_add_for_rendering(Mp_tutorials_info.doc_handle);
    }
    ui_render(Mp_tutorials_info.control_id, false);
}
