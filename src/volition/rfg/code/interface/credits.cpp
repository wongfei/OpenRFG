// Original: volition/rfg/code/interface/credits.cpp
//
// The credits: credits.xtbl read into sections, headings and entities, then two pages of rows that scroll in turn; the
// document asks for the next page through credits_set_up_credits_page.

#include "volition/rfg/code/interface/credits.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x10
struct credits_entity
{
    const char* name; // 0x00
    const char* description; // 0x04
    credits_entity* next; // 0x08
    credits_entity* prev; // 0x0C
};
static_assert(sizeof(credits_entity) == 0x10, "credits_entity");

enum credits_type
{
    CREDITS_TYPE_INVALID = -1,
    CREDITS_TYPE_ROLE_NAME = 0,
    CREDITS_TYPE_NAME = 1,
    CREDITS_TYPE_LOGO = 2,
    CREDITS_NUM_CREDITS_TYPES = 3,
};

// size 0x14
struct credits_heading
{
    credits_entity* credits_entites; // 0x00
    const char* heading_text; // 0x04
    credits_type credit_type; // 0x08
    credits_heading* next; // 0x0C
    credits_heading* prev; // 0x10
};
static_assert(sizeof(credits_heading) == 0x14, "credits_heading");

// size 0x10
struct credits_section
{
    const char* section_title; // 0x00
    credits_heading* credits_headings; // 0x04
    credits_section* next; // 0x08
    credits_section* prev; // 0x0C
};
static_assert(sizeof(credits_section) == 0x10, "credits_section");

// ---- file-static globals
static bool Credits_finished = true; // 0x01662CD1
static const char* credits_type_strings[CREDITS_NUM_CREDITS_TYPES] = {"Name - Role", "Name", "Logo"}; // 0x01662E3C
static int Music_cue_id = -1; // 0x01662E48
static int Music_play_id = -1; // 0x01662E4C
static int Last_page = 2; // 0x01662E50
static unsigned int Credits_warp_to_obj = 0xFFFFFFFF; // 0x01662E54
static virtual_mempool* Credits_mempool; // 0x02C03368
static credits_entity* Credits_entities; // 0x02C0336C
static unsigned int Credits_num_entities; // 0x02C03370
static credits_section* Credits_sections; // 0x02C03374
static unsigned int Credits_num_sections; // 0x02C03378
static credits_heading* Credits_headings; // 0x02C0337C
static unsigned int Credits_num_headings; // 0x02C03380
static virtual_mempool* Credits_string_mempool; // 0x02C03384
static char* Credits_string_memory; // 0x02C03388
static int Credits_doc_handle; // 0x02C0338C
static int Page_data_handle; // 0x02C03390
static int Section_handle[2]; // 0x02C03394
static int Num_section_headers[2]; // 0x02C0339C
static int Num_headers[2]; // 0x02C033A4
static int Num_entities[2]; // 0x02C033AC
static bool Loaded_credits; // 0x02C033B4
static bool Credits_table_loading; // 0x02C033B5
static bool Exit_credits; // 0x02C033B6
static bool Done; // 0x02C033B7
static unsigned int Control_id_credits; // 0x02C033B8
static unsigned int Input_id_credits; // 0x02C033BC
static int Pages_created; // 0x02C033C0
static bool End_game; // 0x02C033C4
static bool Data_items_created; // 0x02C033C5
static bool Credits_saving; // 0x02C033C6
static credits_section* Credits; // 0x02C033C8
static credits_section* Current_section; // 0x02C033CC
static credits_heading* Current_heading; // 0x02C033D0
static credits_entity* Current_entity; // 0x02C033D4
static credits_section* Start_section; // 0x02C033D8
static credits_heading* Start_heading; // 0x02C033DC
static credits_entity* Start_entity; // 0x02C033E0
static stream2_os_file Credits_req; // 0x02C10990
static unsigned int Credits_req_handle; // 0x02C3CAFC
static string_pool Credits_string_pool((const char*)nullptr); // 0x02C7E270
static bool just_loaded; // 0x02CA1660
static bool fading; // 0x02CA1661

// Links a new element at the end of a circular list. rfg.exe inlines this three times.
template <typename T>
static void credits_link(T*& head, T* element)
{
    if (head == nullptr)
    {
        element->next = element;
        element->prev = element;
        head = element;
    }
    else
    {
        element->prev = head->prev;
        element->next = head;
        head->prev->next = element;
        head->prev = element;
    }
}

// 0x008838F0 (:206)
static void parse_credits_table()
{
    Credits = nullptr;
    if (Credits_req.dest == nullptr)
    {
        keen::debug::nativePrintArgs("Memory has not been allocated! How did this happen!?");
        return;
    }
    xml_element* table = xml_table_parse_from_memory(Credits_req.dest);
    for (xml_element* section_xml = xml_table_find_opt(table, "Credits"); section_xml != nullptr; section_xml = xml_table_find_next(table, section_xml, "Credits"), Credits_num_sections++)
    {
        credits_section* section = Credits_sections + Credits_num_sections;
        section->credits_headings = nullptr;
        credits_link(Credits, section);
        const char* title = xml_table_get_opt_string_ref(section_xml, "section_title");
        section->section_title = Credits_string_pool.add_unique_case_sensitive(title != nullptr ? title : "");
        xml_element* headings_xml = xml_table_find_req(section_xml, "headings");
        for (xml_element* heading_xml = xml_table_find_req(headings_xml, "headings"); heading_xml != nullptr; heading_xml = xml_table_find_next(headings_xml, heading_xml, "headings"), Credits_num_headings++)
        {
            credits_heading* heading = Credits_headings + Credits_num_headings;
            heading->credits_entites = nullptr;
            credits_link(section->credits_headings, heading);
            const char* heading_text = xml_table_get_opt_string_ref(heading_xml, "heading");
            heading->heading_text = Credits_string_pool.add_unique_case_sensitive(heading_text != nullptr ? heading_text : "");
            const char* type_name = xml_table_get_req_string_ref(heading_xml, "type");
            heading->credit_type = CREDITS_TYPE_INVALID;
            for (int type = CREDITS_TYPE_ROLE_NAME; type < CREDITS_NUM_CREDITS_TYPES; type++)
            {
                if (v_stricmp(type_name, credits_type_strings[type]) == 0)
                {
                    heading->credit_type = (credits_type)type;
                    break;
                }
            }
            xml_element* items_xml = xml_table_find_req(heading_xml, "items");
            for (xml_element* item_xml = xml_table_find_req(items_xml, "items"); item_xml != nullptr; item_xml = xml_table_find_next(items_xml, item_xml, "items"), Credits_num_entities++)
            {
                credits_entity* entity = Credits_entities + Credits_num_entities;
                credits_link(heading->credits_entites, entity);
                const char* name = xml_table_get_opt_string_ref(item_xml, "name");
                const char* description = xml_table_get_opt_string_ref(item_xml, "desc");
                if (name == nullptr)
                {
                    entity->name = Credits_string_pool.add_unique_case_sensitive("");
                }
                else
                {
                    // Each byte of the name becomes one UTF-8 character.
                    char utf8_name[256];
                    char* target = utf8_name;
                    unsigned int capacity = 0x100;
                    const unsigned int length = keen::getStringLength(name);
                    for (unsigned int i = 0; i < length; i++)
                    {
                        const unsigned int written = keen::writeUTF8Character(target, capacity, (unsigned char)name[i]);
                        capacity -= written;
                        target += written;
                    }
                    *target = '\0';
                    entity->name = Credits_string_pool.add_unique_case_sensitive(utf8_name);
                }
                entity->description = Credits_string_pool.add_unique_case_sensitive(description != nullptr ? description : "");
            }
        }
    }
    xml_table_close(table);
}

// 0x008CE8A0 (:322)
static void credits_create_data_items()
{
    vint_data_group* group = vint_data_group::create_new("SectionGroup1");
    if (group != nullptr)
    {
        Section_handle[0] = group->get_handle();
        ui_add_data_group(Credits_doc_handle, Section_handle[0]);
    }
    group = vint_data_group::create_new("SectionGroup2");
    if (group != nullptr)
    {
        Section_handle[1] = group->get_handle();
        ui_add_data_group(Credits_doc_handle, Section_handle[1]);
    }
    vint_data_item* item = vint_data_item::create_new("PageData");
    if (item != nullptr)
    {
        const vint_variant_type types[9] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING};
        item->set_template(types, 9);
        Page_data_handle = item->get_handle();
        ui_add_data_item(Credits_doc_handle, Page_data_handle);
    }
}

// One row of a page. rfg.exe inlines this three times.
static void credits_add_row(vint_data_group* group, int& row, int kind, int sections, int headers, int entities, const char* text, const char* subtext)
{
    vint_data_item* item = group->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    const vint_variant_type types[8] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 8);
    row++;
    item->set_element(0, vint_variant(row));
    item->set_element(1, vint_variant(Last_page));
    item->set_element(2, vint_variant(kind));
    item->set_element(3, vint_variant(sections));
    item->set_element(4, vint_variant(headers));
    item->set_element(5, vint_variant(entities));
    item->set_element(6, vint_variant(text));
    item->set_element(7, vint_variant(subtext));
}

// 0x00883C90 (:368)
// Magic number: a page takes up to 33 rows. Row kinds: 1 section, 2 heading, 3 + credits_type an entity.
static void credits_process_credits_page()
{
    bool first_page = false;
    Last_page = Last_page == 2 ? 1 : (Last_page == 1 ? 2 : Last_page);
    const int page = Last_page - 1;
    vint_data_group* group = vint_data_group::find(Section_handle[page]);
    if (group != nullptr)
    {
        if (Current_section == nullptr)
        {
            first_page = true;
            Current_section = Credits;
        }
        credits_section* section = Current_section;
        credits_heading* heading = Current_heading;
        credits_entity* entity = Current_entity;
        int row = 0;
        int sections = 0;
        int headers = 0;
        int entities = 0;
        bool wrapped = false;
        bool page_full = false;
        do
        {
            if (row > 32)
            {
                Current_heading = nullptr;
                Current_entity = nullptr;
                page_full = true;
                break;
            }
            if (heading == nullptr)
            {
                credits_add_row(group, row, 1, sections, headers, entities, section->section_title, "");
                sections++;
                heading = section->credits_headings;
            }
            do
            {
                if (row > 32)
                {
                    Current_heading = heading;
                    Current_entity = nullptr;
                    page_full = true;
                    break;
                }
                if (entity == nullptr)
                {
                    credits_add_row(group, row, 2, sections, headers, entities, heading->heading_text, "");
                    headers++;
                    entity = heading->credits_entites;
                }
                do
                {
                    if (row > 32)
                    {
                        Current_heading = heading;
                        Current_entity = entity;
                        page_full = true;
                        break;
                    }
                    credits_add_row(group, row, heading->credit_type + CREDITS_NUM_CREDITS_TYPES, sections, headers, entities, entity->name, entity->description);
                    entities++;
                    entity = entity->next;
                } while (entity != heading->credits_entites);
                if (page_full)
                {
                    break;
                }
                heading = heading->next;
                entity = nullptr;
            } while (heading != section->credits_headings);
            if (page_full)
            {
                break;
            }
            section = section->next;
            heading = nullptr;
            wrapped = true;
        } while (section != Credits);
        if (page_full)
        {
            Current_section = section;
        }
        if (!page_full || (wrapped && section == Credits))
        {
            Done = true;
        }
        Num_section_headers[page] = sections;
        Num_headers[page] = headers;
        Num_entities[page] = entities;
    }
    vint_data_item* item = vint_data_item::find(Page_data_handle);
    if (item != nullptr)
    {
        const int other_page = Last_page == 1 ? 1 : 0;
        item->set_element(0, vint_variant(Last_page));
        item->set_element(1, vint_variant(first_page));
        item->set_element(2, vint_variant(Num_section_headers[page]));
        item->set_element(3, vint_variant(Num_headers[page]));
        item->set_element(4, vint_variant(Num_entities[page]));
        item->set_element(5, vint_variant(Num_section_headers[other_page]));
        item->set_element(6, vint_variant(Num_headers[other_page]));
        item->set_element(7, vint_variant(Num_entities[other_page]));
        item->set_element(8, vint_variant("ui_rfg_credits1"));
    }
    Pages_created++;
}

// 0x00884460 (:563)
static void credits_clean_up_last_page()
{
    vint_data_group* group = vint_data_group::find(Section_handle[Last_page == 1 ? 1 : 0]);
    if (group != nullptr)
    {
        group->remove_all_data_items();
    }
    Pages_created--;
}

// 0x008CE980 (:576)
static void credits_process_input()
{
    // Magic number: ui button 11 leaves.
    if (ui_get_button_info(11, (int)Input_id_credits) > 0.0f)
    {
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        Exit_credits = true;
    }
}

// 0x00917170 (:591)
void credits_init()
{
    state_info info;
    info.enter = credits_enter;
    info.exit = credits_exit;
    info.process = credits_process;
    info.handshake = credits_handshake;
    info.render = credits_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_CREDITS, info);
}

// 0x009096B0 (:604)
void credits_enter(game_state prev_state)
{
    (void)prev_state;
    Music_cue_id = -1;
    Music_play_id = -1;
    Credits_doc_handle = ui_load_document("rfg_credits", DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Credits_doc_handle == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Credits_finished = false;
    Done = false;
    Last_page = 2;
    Pages_created = 0;
    // Magic number: 128 KB of strings.
    Credits_string_mempool = Common_page_pool.get_virtual_mempool("Credits string pool", 0x20000);
    Credits_string_memory = (char*)Credits_string_mempool->alloc(0x20000, 0);
    Credits_string_pool.init(Credits_string_memory, 0x20000, nullptr, 0, 0);
    Credits_req_handle = 0xFFFFFFFF;
    // Magic numbers: 32 sections (0x200), 256 headings (0x1400), 3000 entities (48000): 0xD180 in all.
    const int lists_size = 0xD180;
    if (!cf_exist("credits.xtbl"))
    {
        Credits_mempool = Common_page_pool.get_virtual_mempool("Credits", lists_size);
        keen::debug::nativePrintArgs("The credits table file does not exist! WTF!?");
        Common_page_pool.release_virtual_mempool(Credits_mempool);
        Credits_mempool = nullptr;
        Common_page_pool.release_virtual_mempool(Credits_string_mempool);
        Credits_string_mempool = nullptr;
        gameseq_pop_state();
        return;
    }
    unsigned int file_size;
    cf_get_file_size(&file_size, "credits.xtbl", false);
    // Magic number: the pool is rounded up to 16 bytes; the text gets a terminator.
    const int remainder = (int)(file_size + lists_size + 1) % 16;
    const int padding = remainder == 0 ? 0 : 16 - remainder;
    Credits_mempool = Common_page_pool.get_virtual_mempool("Credits", padding + file_size + lists_size + 1);
    strcpy(Credits_req.filename, "credits.xtbl");
    Credits_req.size = file_size;
    Credits_req.offset = 0;
    Credits_req.dest = (unsigned char*)Credits_mempool->alloc(file_size + 1, 0);
    // Original bug, kept: the buffer is cleared before it is tested.
    memset(Credits_req.dest, 0, file_size + 1);
    if (Credits_req.dest == nullptr)
    {
        keen::debug::nativePrintArgs("Failed to allocate necessary memory, I'm am going to die now ok?");
        Common_page_pool.release_virtual_mempool(Credits_mempool);
        Credits_mempool = nullptr;
        Common_page_pool.release_virtual_mempool(Credits_string_mempool);
        Credits_string_mempool = nullptr;
        gameseq_pop_state();
        return;
    }
    Credits_req_handle = stream2_os_read(Credits_req);
    Credits_table_loading = true;
    Credits_sections = (credits_section*)Credits_mempool->alloc(0x200, 0);
    Credits_num_sections = 0;
    Credits_headings = (credits_heading*)Credits_mempool->alloc(0x1400, 0);
    Credits_num_headings = 0;
    Credits_entities = (credits_entity*)Credits_mempool->alloc(48000, 0);
    Credits_num_entities = 0;
    Current_section = nullptr;
    Current_heading = nullptr;
    Current_entity = nullptr;
    Start_section = Credits;
    Start_heading = nullptr;
    Start_entity = nullptr;
    Control_id_credits = ui_request_control_id();
    Input_id_credits = (unsigned int)ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    ui_wait_for_button_ups();
    const unsigned int srid = stream2_container_find("ui_rfg_credits1");
    if (srid != 0)
    {
        stream2_request(srid);
    }
    audiolib_system_set_game_state(AUDIOLIB_GAME_STATE_LOADING);
    gamemusic_frontend_stop(1000); // Magic number: one second.
    if (!End_game)
    {
        main_menu_show_menu(false);
    }
    audiolib_cue_get_id("MUS_MAIN_CREDITS", &Music_cue_id);
    Credits_finished = false;
    Credits_saving = false;
    if (!End_game)
    {
        video_player_pause_all();
    }
}

// 0x009127D0 (:745)
void credits_process()
{
    if (Credits_table_loading)
    {
        if (stream2_os_poll(Credits_req_handle) == SOS_BUSY)
        {
            ui_process(Control_id_credits, false);
            return;
        }
        Credits_table_loading = false;
        parse_credits_table();
        stream2_os_release_handle(Credits_req_handle);
    }
    if (video_cutscene_pending_or_playing())
    {
        ui_process(Control_id_credits, false);
        return;
    }
    if (Exit_credits)
    {
        if (Music_play_id != -1)
        {
            audiolib_play_fade_out(Music_play_id, 500);
            Music_play_id = -1;
        }
        if (!fading)
        {
            ui_fade_to_black_fade_out(true);
            fading = true;
        }
        if (!ui_fade_to_black_is_faded())
        {
            ui_add_for_processing(Credits_doc_handle);
            ui_process(Control_id_credits, false);
            return;
        }
        if (Credits_warp_to_obj != 0xFFFFFFFF)
        {
            World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false);
        }
        else
        {
            // Original bug, kept: leaving without a warp takes two steps off the fade's count, where one was added.
            ui_fade_to_black_fade_in(false);
            gameseq_pop_state();
        }
        ui_fade_to_black_fade_in(false);
        fading = false;
        ui_process(Control_id_credits, false);
        return;
    }
    if (Loaded_credits)
    {
        if (just_loaded)
        {
            credits_process_credits_page();
            just_loaded = false;
            audiolib_all_streaming_sounds_unload();
            Music_play_id = audiolib_cue_play(Music_cue_id, nullptr, nullptr);
        }
        credits_process_input();
        if (multi_core_platform_has_pending_invite())
        {
            Exit_credits = true;
        }
        ui_add_for_processing(Credits_doc_handle);
        ui_process(Control_id_credits, false);
        return;
    }
    const unsigned int srid = stream2_container_find("ui_rfg_credits1");
    if (srid == 0 || stream2_get_status(srid) != ST_LOADED)
    {
        credits_process_input();
        if (multi_core_platform_has_pending_invite())
        {
            Exit_credits = true;
        }
        ui_add_for_processing(Credits_doc_handle);
        ui_process(Control_id_credits, false);
        return;
    }
    switch (ui_get_doc_asset_state(Credits_doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        [[fallthrough]];
    case ST_ERROR:
        ui_unload_document(Credits_doc_handle, DFOT_NONE, 0.5f);
        Credits_doc_handle = -1;
        gameseq_pop_state();
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        Loaded_credits = true;
        if (!Data_items_created)
        {
            credits_create_data_items();
            credits_process_credits_page();
            just_loaded = true;
            Data_items_created = true;
            if (ui_fade_to_black_is_faded())
            {
                ui_fade_to_black_fade_in(false);
            }
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
    ui_process(Control_id_credits, false);
}

// 0x008F09C0 (:848)
void credits_exit(game_state prev_state)
{
    (void)prev_state;
    if (Credits_mempool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(Credits_mempool);
    }
    Credits_mempool = nullptr;
    if (Credits_string_mempool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(Credits_string_mempool);
    }
    Credits_string_mempool = nullptr;
    const unsigned int srid = stream2_container_find("ui_rfg_credits1");
    if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
    {
        stream2_release(srid);
    }
    ui_unload_document(Credits_doc_handle, DFOT_NONE, 0.5f);
    Credits_doc_handle = -1;
    ui_relinquish_control_id(Control_id_credits);
    ui_relinquish_input_id((int)Input_id_credits);
    Data_items_created = false;
    if (Music_play_id != -1)
    {
        audiolib_play_fade_out(Music_play_id, 500);
        Music_play_id = -1;
    }
    if (!End_game)
    {
        gamemusic_frontend_start(multi_core_platform_has_pending_invite());
        main_menu_show_menu(true);
    }
    Loaded_credits = false;
    Exit_credits = false;
    if (!End_game)
    {
        video_player_unpause_all();
    }
}

// 0x008F70C0 (:893)
void credits_render(rl_renderer*)
{
    if (Loaded_credits)
    {
        ui_add_for_rendering(Credits_doc_handle);
    }
    ui_render(Control_id_credits, false);
}

// 0x00884490 (:902)
void credits_handshake(rl_renderer*)
{
}

// 0x008844A0 (:906)
void credits_show(bool end_game, unsigned int warp_to_obj)
{
    End_game = end_game;
    gameseq_push_state(GS_CREDITS, false, end_game);
    Credits_warp_to_obj = warp_to_obj;
}

// 0x008844E0 (:918)
bool credits_are_rolling()
{
    return !Credits_finished;
}

// 0x008844F0 (:923)
void credits_set_up_credits_page()
{
    credits_clean_up_last_page();
    if (!Done)
    {
        credits_process_credits_page();
    }
    if (Pages_created < 1)
    {
        Credits_finished = true;
        Exit_credits = true;
    }
}
