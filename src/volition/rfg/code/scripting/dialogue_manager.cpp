// Original: volition/rfg/code/scripting/dialogue_manager.cpp
//
// The dialogue manager (decompiled): a dialogue is a tree of lines (elements); a child waits for its parent to finish
// and for its time offset, then plays through its human (human::play_voice_line) or its persona (radio, public address
// or a vehicle's megaphone). One dialogue plays at a time: the highest priority one is moved to the current category
// (5) and replaces a lower one; ambient dialogues (0) play alongside. A playing radio line ducks the VOICE_GUR_RADIO
// category. The multiplayer announcer plays its lines through it. The external interface is declared in
// dialogue_manager.h. SFX.md SX6: ported whole (the radio, subtitle and public address paths reach the HUD and public
// address stand-ins).

#include "volition/rfg/code/scripting/dialogue_manager.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

// ---- file-static globals
// The cue priority of each dialogue priority (0x016B0CC4, read from rfg.exe): ambient, radio, normal, high, briefing.
static const audiolib_cue_priority DIALOGUE_PRIORITY_CUE_PRIORITIES[5] = {AUDIOLIB_CUE_PRIORITY_DIALOGUE_AMBIENT, AUDIOLIB_CUE_PRIORITY_DIALOGUE_RADIO, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, AUDIOLIB_CUE_PRIORITY_DIALOGUE_HIGH, AUDIOLIB_CUE_PRIORITY_DIALOGUE_BRIEFING};
static cs_wrapper dialog_cs(0); // 0x036D1418
// The dialogues by priority (categories 0-4) and the current one (5); the elements (dynamic initializers: release
// callbacks dialogue_info::clear and dialogue_element::clear).
static object_pool<dialogue_info,20,6> Dialogue_pool(nullptr, &dialogue_info::clear); // 0x036D15F0
static object_pool<dialogue_element,100,1> Dialogue_element_pool(nullptr, &dialogue_element::clear); // 0x036F4270
static short duck_id = -1; // 0x016B67E8, the VOICE_GUR_RADIO category, looked up once

// Set by the scripts (script_definitions.cpp in rfg.exe, not ported): no new dialogue starts.
bool Dialogue_disable_dialogue_start; // 0x0361141D

// Magic number: the current dialogue's category (above the five priorities).
static const int CURRENT_DIALOGUE_CATEGORY = 5;

// ---- dialogue_element / dialogue_info

// 0x00B2D640 (:82)
dialogue_element::dialogue_element()
{
    clear();
}

// 0x00B23890 (:150)
void dialogue_element::clear()
{
    radio_instance = 0;
    owner = nullptr;
    parent = nullptr;
    child_ptr = nullptr;
    child_prev = nullptr;
    child_next = nullptr;
    line_to_play = INVALID_VOICE_LINE_HANDLE;
    target_human = 0xFFFFFFFF;
    target_automobile = 0xFFFFFFFF;
    target_persona = nullptr;
    pa_group = INVALID_PA_HANDLE;
    time_offset = 0;
    time_to_play.invalidate();
    play_status = DEPS_INACTIVE;
    play_instance = -1;
    play_time = 0;
    message_icon_name = nullptr;
    play_intro = true;
    play_outro = true;
}

// 0x00B2D690 (:99)
dialogue_info::dialogue_info()
{
    root = nullptr;
    priority = 0xFF;
}

// 0x00B23860 (:101)
void dialogue_info::clear()
{
    root = nullptr;
    priority = 0xFF;
}

// ---- internal functions

// The element's human when it is a living human (inlined in every function that targets one).
static human* dialogue_target_human(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->is_killed())
    {
        return nullptr;
    }
    return (human*)objp;
}

// 0x00B1F720 (global_event_data.h:103): the event type of a dialogue line starting or finishing.
dialogue_event::dialogue_event()
    : event_data(EVENT_DIALOGUE_FINISHED)
{
}

// 0x00B39DE0 (:221), inlined in process_dialogue_element.
static void post_dialogue_event(voice_line_handle line_handle, bool is_start)
{
    GAME_TRACE("dialogue", "%s #%d %s", is_start ? "starts" : "ends", (int)line_handle, voice_line_handle_to_name(line_handle));
    dialogue_event event;
    event.voice_line = line_handle;
    event.is_start = is_start;
    gem_post_event(event);
}

// 0x00B33030 (:234): an unfinished element of the line, the last allocated first.
static dialogue_element* find_dialogue_element(voice_line_handle line_handle)
{
    csspinlock_scope lock(Dialogue_element_pool.thread_access);
    for (dialogue_element* dep = Dialogue_element_pool.get_last(0); dep != nullptr; dep = Dialogue_element_pool.get_prev(dep))
    {
        if (dep->line_to_play == line_handle && (int)dep->play_status < DEPS_DONE)
        {
            return dep;
        }
    }
    return nullptr;
}

// 0x00B39E00 (:240): the element and its children are freed; with immediate_stop a queued or playing line stops.
static void stop_dialogue_parts(dialogue_element* dep, bool immediate_stop)
{
    dialogue_element* child = dep->child_ptr;
    while (child != nullptr)
    {
        dialogue_element* next = child->child_next != dep->child_ptr ? child->child_next : nullptr;
        if (dep->child_ptr == child)
        {
            if (child->child_next == child)
            {
                child->child_next = nullptr;
                child->child_prev = nullptr;
                dep->child_ptr = nullptr;
                stop_dialogue_parts(child, immediate_stop);
                child = next;
                continue;
            }
            dep->child_ptr = child->child_next;
        }
        child->child_prev->child_next = child->child_next;
        child->child_next->child_prev = child->child_prev;
        child->child_next = nullptr;
        child->child_prev = nullptr;
        stop_dialogue_parts(child, immediate_stop);
        child = next;
    }
    if (immediate_stop && dep->play_status >= DEPS_QUEUED && dep->play_status <= DEPS_DONE)
    {
        if (dep->radio_instance != 0)
        {
            hud_ui_clear_current_radio_message(dep->radio_instance);
        }
        else if (dep->play_instance != -1)
        {
            audiolib_play_stop(dep->play_instance);
        }
        else if (human* hp = dialogue_target_human(dep->target_human))
        {
            hp->stop_voice(VOICE_LINE_PRIORITY_FORCE_ALWAYS);
        }
    }
    Dialogue_element_pool.free(dep);
}

// 0x00B39F10 (:262). LTCG: dip in ESI, immediate_stop on the stack.
static void stop_dialogue(dialogue_info* dip, bool immediate_stop)
{
    cs_scope lock(dialog_cs);
    stop_dialogue_parts(dip->root, immediate_stop);
    Dialogue_pool.free(dip);
    ui_kill_subtitle();
}

// 0x00B2D6C0 (:271): a queued or playing line stops and waits again (the first one found).
bool pause_dialogue_elements(dialogue_element* dep)
{
    if (dep == nullptr)
    {
        return false;
    }
    if (dep->play_status == DEPS_QUEUE_ME)
    {
        dep->play_status = DEPS_WAITING;
    }
    else if (dep->play_status == DEPS_QUEUED || dep->play_status == DEPS_PLAYING)
    {
        if (human* hp = dialogue_target_human(dep->target_human))
        {
            hp->stop_voice(VOICE_LINE_PRIORITY_FORCE_ALWAYS);
        }
        else if (dep->radio_instance != 0)
        {
            hud_ui_clear_current_radio_message(dep->radio_instance);
        }
        else if (dep->play_instance != -1)
        {
            audiolib_play_stop(dep->play_instance);
        }
        dep->radio_instance = 0;
        dep->play_instance = -1;
        dep->play_status = DEPS_WAITING;
        return true;
    }
    for (dialogue_element* child = dep->child_ptr; child != nullptr;)
    {
        dialogue_element* next = child->child_next;
        dialogue_element* first = dep->child_ptr;
        pause_dialogue_elements(child);
        child = next != first ? next : nullptr;
    }
    return false;
}

// 0x00B2D790 (:320)
void pause_dialogue(dialogue_info* dip)
{
    if (dip != nullptr)
    {
        pause_dialogue_elements(dip->root);
        dip->paused = true;
    }
}

// 0x00B238E0 (:330)
void unpause_dialogue(dialogue_info* dip)
{
    if (dip != nullptr)
    {
        dip->paused = false;
    }
}

// 0x00B2D7B0 (:338). LTCG: dep in ESI. Through the human, or the persona (at the megaphone of a living automobile);
// a line that cannot play is done, or skipped when it has no play time.
static bool start_playing_dialogue_line(dialogue_element* dep)
{
    dep->play_status = DEPS_QUEUED;
    if (dep->target_human == 0xFFFFFFFF)
    {
        if (dep->target_persona != nullptr)
        {
            if (dep->pa_group != INVALID_PA_HANDLE)
            {
                public_address_set_active_group(dep->pa_group);
            }
            if (dep->message_icon_name != nullptr)
            {
                hud_ui_set_radio_message_icon(dep->message_icon_name);
            }
            vector pos = ZERO_VECTOR;
            if (dep->target_automobile != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(dep->target_automobile);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_AUTO && !vehicle_is_corpse((vehicle*)objp))
                {
                    automobile_get_horn_world_pos(pos, (vehicle*)objp);
                }
            }
            const bool played = dep->target_persona->persona_play_voice_line(dep->line_to_play, pos, DIALOGUE_PRIORITY_CUE_PRIORITIES[dep->owner->priority], dep->play_instance, dep->radio_instance, nullptr, false, false, dep->play_intro, dep->play_outro, nullptr);
            public_address_unset_active_group();
            hud_ui_unset_radio_message_icon();
            if (played)
            {
                dep->play_status = DEPS_QUEUED;
                return true;
            }
        }
    }
    else if (human* hp = dialogue_target_human(dep->target_human))
    {
        if (hp->play_voice_line(dep->line_to_play, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false, dep->play_intro, dep->play_outro) == PLAY_VOICE_SUCCESS)
        {
            dep->play_status = DEPS_QUEUED;
            dep->play_instance = hp->voice_instance;
            return true;
        }
    }
    if (dep->play_time == 0)
    {
        dep->play_status = DEPS_PLAYING;
        return true;
    }
    dep->play_status = DEPS_DONE;
    return false;
}

// 0x00B2D950 (:422): the queued line's remaining play time; it is playing once audio_lib or the radio says so, done
// when no time is left.
bool check_queued_line_status(dialogue_element* dep)
{
    if (human* hp = dialogue_target_human(dep->target_human))
    {
        dep->radio_instance = hp->radio_instance;
        dep->play_instance = hp->voice_instance;
    }
    bool playing = false;
    dep->play_time = 0;
    if (dep->radio_instance == 0)
    {
        if (dep->play_instance != -1 && audiolib_play_get_state(dep->play_instance, nullptr) == AUDIOLIB_ERR_NONE)
        {
            audiolib_play_get_play_time(dep->play_instance, &dep->play_time, nullptr);
            playing = audiolib_is_playing(dep->play_instance, true);
        }
    }
    else
    {
        bool visible = false;
        if (hud_ui_is_radio_message_valid(dep->radio_instance, &visible, nullptr))
        {
            dep->play_time = hud_ui_get_radio_message_remaining_time(dep->radio_instance);
            playing = true;
        }
        else
        {
            playing = visible;
        }
    }
    if (dep->play_time > 0)
    {
        if (playing)
        {
            dep->play_status = DEPS_PLAYING;
        }
        return true;
    }
    dep->play_time = 0;
    dep->play_status = DEPS_DONE;
    return false;
}

// 0x00B39F50 (:531): advances the element and its children; true when all of them are done. A radio line that plays
// on the radio channel ducks (duck_me).
static bool process_dialogue_element(dialogue_element* dep, bool& duck_me)
{
    if (dep->play_status == DEPS_INACTIVE)
    {
        return true;
    }
    if (dep->play_status == DEPS_WAITING)
    {
        if (dep->parent != nullptr && (int)dep->parent->play_status < DEPS_DONE)
        {
            return false;
        }
        if (dep->time_offset > 0)
        {
            if (!dep->time_to_play.valid())
            {
                dep->time_to_play.set(dep->time_offset);
                return false;
            }
            if (!dep->time_to_play.elapsed())
            {
                return false;
            }
        }
        dep->play_status = DEPS_QUEUE_ME;
    }
    const dialogue_element_play_status old_status = dep->play_status;
    bool done = false;
    bool finished = false;
    switch (old_status)
    {
    case DEPS_QUEUE_ME:
        finished = !start_playing_dialogue_line(dep);
        break;
    case DEPS_QUEUED:
        finished = !check_queued_line_status(dep);
        break;
    case DEPS_PLAYING:
        if (dep->radio_instance == 0)
        {
            audiolib_play_state state;
            finished = !((int)audiolib_play_get_state(dep->play_instance, &state) >= 0 && (int)state < 6);
        }
        else
        {
            bool radio_line = false;
            if (hud_ui_is_radio_message_valid(dep->radio_instance, nullptr, &radio_line))
            {
                if (radio_line)
                {
                    duck_me = true;
                }
            }
            else
            {
                finished = true;
            }
        }
        break;
    case DEPS_DONE:
        done = true;
        break;
    default:
        break;
    }
    if (finished)
    {
        dep->play_status = DEPS_DONE;
        done = true;
    }
    // The line started (reached playing) and finished (reached done) this pass: inlined post_dialogue_event.
    if ((int)old_status < DEPS_PLAYING && (int)dep->play_status >= DEPS_PLAYING)
    {
        post_dialogue_event(dep->line_to_play, true);
    }
    if ((int)old_status < DEPS_DONE && (int)dep->play_status >= DEPS_DONE)
    {
        post_dialogue_event(dep->line_to_play, false);
    }
    for (dialogue_element* child = dep->child_ptr; child != nullptr;)
    {
        dialogue_element* next = child->child_next;
        dialogue_element* first = dep->child_ptr;
        done = process_dialogue_element(child, duck_me) && done;
        child = next != first ? next : nullptr;
    }
    return done;
}

// 0x00B3A0E0 (:591)
void process_dialogue(dialogue_info* dip, bool& duck_me)
{
    if (dip != nullptr && !dip->paused && process_dialogue_element(dip->root, duck_me))
    {
        stop_dialogue(dip, false);
    }
}

// 0x00B330E0 (:613): the first dialogue of the highest priority that has one.
dialogue_info* find_next_dialogue_to_play()
{
    cs_scope lock(dialog_cs);
    for (int priority = DIALOGUE_PRIORITY_BRIEFING; priority >= 0; priority--)
    {
        if (Dialogue_pool.get_count(priority) != 0)
        {
            return Dialogue_pool.get_first(priority);
        }
    }
    return nullptr;
}

// 0x00B33160 (:642). LTCG: parent_line in EAX, line_handle and time_offset on the stack. A child of the parent line's
// unfinished element, last in its children.
static dialogue_element* create_new_child_element(voice_line_handle line_handle, voice_line_handle parent_line, int time_offset)
{
    dialogue_element* parent = find_dialogue_element(parent_line);
    if (parent == nullptr)
    {
        return nullptr;
    }
    dialogue_element* dep = Dialogue_element_pool.alloc(0);
    if (dep == nullptr)
    {
        return nullptr;
    }
    dep->line_to_play = line_handle;
    dep->time_offset = time_offset;
    dep->time_to_play.invalidate();
    dep->parent = parent;
    dep->owner = parent->owner;
    if (parent->child_ptr == nullptr)
    {
        dep->child_next = dep;
        dep->child_prev = dep;
        parent->child_ptr = dep;
        return dep;
    }
    dep->child_prev = parent->child_ptr->child_prev;
    dep->child_next = parent->child_ptr;
    parent->child_ptr->child_prev->child_next = dep;
    parent->child_ptr->child_prev = dep;
    return dep;
}

// 0x00B3A110 (:663): a dialogue of the priority (not the current category) with its root element.
static dialogue_info* create_new_dialogue(voice_line_handle line_handle, dialogue_priorities priority)
{
    if (priority > DIALOGUE_PRIORITY_BRIEFING)
    {
        return nullptr;
    }
    dialogue_info* dip = Dialogue_pool.alloc(priority);
    if (dip == nullptr)
    {
        GAME_TRACE("dialogue", "no room to queue #%d %s", (int)line_handle, voice_line_handle_to_name(line_handle));
        return nullptr;
    }
    GAME_TRACE("dialogue", "queue #%d %s priority=%d", (int)line_handle, voice_line_handle_to_name(line_handle), (int)priority);
    cs_scope lock(dialog_cs);
    dip->root = Dialogue_element_pool.alloc(0);
    if (dip->root == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        Dialogue_pool.free(dip);
        return nullptr;
    }
    dip->priority = (unsigned char)priority;
    dip->root->line_to_play = line_handle;
    dip->root->owner = dip;
    return dip;
}

// The dialogue of the current category (the one playing), or null.
static dialogue_info* current_dialogue()
{
    return Dialogue_pool.get_first(CURRENT_DIALOGUE_CATEGORY);
}

// A whole dialogue stops and is freed (inlined stop_dialogue in the loops below).
static dialogue_info* stop_and_free_dialogue(dialogue_info* dip, bool immediate_stop)
{
    cs_scope lock(dialog_cs);
    stop_dialogue_parts(dip->root, immediate_stop);
    dialogue_info* next = Dialogue_pool.free(dip);
    ui_kill_subtitle();
    return next;
}

// ---- external functions

// 0x00B3D760 (:700)
void dialogue_manager_level_init_pre()
{
    dialogue_manager_cancel_all();
    {
        csspinlock_scope lock(Dialogue_element_pool.thread_access);
        if (Dialogue_element_pool.get_count(0) != 0)
        {
            for (dialogue_element* dep = Dialogue_element_pool.get_first(0); dep != nullptr; dep = Dialogue_element_pool.free(dep))
            {
            }
        }
    }
    Dialogue_pool.clear();
}

// 0x00B3D7C0 (:716): single player cancels everything when the local player is dead. A briefing (category 4) plays
// as soon as it exists; otherwise the highest priority dialogue becomes current, putting a lower current one back.
// The current dialogue and every ambient one advance (not while the game is paused, unless a briefing waits), and the
// radio ducks while a radio line plays.
void dialogue_manager_do_frame()
{
    player* local = player_get_local_player();
    if (!Multiplayer && (local == nullptr || local->is_killed()))
    {
        dialogue_manager_cancel_all();
    }
    const int briefings = Dialogue_pool.get_count(DIALOGUE_PRIORITY_BRIEFING);
    bool duck_me = false;
    cs_scope lock(dialog_cs);
    dialogue_info* current = Dialogue_pool.get_first(DIALOGUE_PRIORITY_BRIEFING);
    if (current == nullptr)
    {
        current = current_dialogue();
        dialogue_info* next = find_next_dialogue_to_play();
        if (current == nullptr)
        {
            if (next != nullptr)
            {
                Dialogue_pool.move(next, CURRENT_DIALOGUE_CATEGORY);
            }
        }
        else if (next != nullptr && next != current && current->priority < next->priority)
        {
            Dialogue_pool.move(current, current->priority);
            Dialogue_pool.move(next, CURRENT_DIALOGUE_CATEGORY);
        }
        current = current_dialogue();
    }
    if (current != nullptr && (!game_is_paused() || briefings > 0))
    {
        if (!current->paused && process_dialogue_element(current->root, duck_me))
        {
            stop_dialogue(current, false);
        }
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(DIALOGUE_PRIORITY_AMBIENT); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            if (!dip->paused && process_dialogue_element(dip->root, duck_me))
            {
                stop_and_free_dialogue(dip, false);
            }
            dip = next;
        }
    }
    if (!duck_me)
    {
        audiolib_category_duck_off();
        return;
    }
    if (duck_id == -1)
    {
        audiolib_category_get_id("VOICE_GUR_RADIO", &duck_id);
    }
    if (!audiolib_ducking() && duck_id != -1)
    {
        audiolib_category_duck(duck_id, -1.0f);
    }
}

// 0x00B238F0 (:802)
// The persona of a line: the given one, else the local player's voice; false when the line is unknown or no persona
// is found (0x00B238F0, do_persona_line_check_common).
static bool do_persona_line_check_common(const human_voice_persona*& hvpp, voice_line_handle line_handle)
{
    if (human_voice_line_get_info(line_handle) == nullptr)
    {
        return false;
    }
    if (hvpp == nullptr)
    {
        player* local = player_get_local_player();
        hvpp = local != nullptr ? local->get_voice() : nullptr;
    }
    return hvpp != nullptr;
}

// 0x00B3A1A0 (:883)
bool dialogue_manager_start_dialogue(voice_line_handle line_handle, human* hp, dialogue_priorities priority)
{
    if (Dialogue_disable_dialogue_start || hp == nullptr || line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    const human_voice_persona* voice = hp->voice;
    if (!do_persona_line_check_common(voice, line_handle))
    {
        return false;
    }
    cs_scope lock(dialog_cs);
    dialogue_info* dip = create_new_dialogue(line_handle, priority);
    if (dip == nullptr)
    {
        return false;
    }
    dip->root->target_human = hp->get_handle();
    dip->root->play_status = DEPS_WAITING;
    return true;
}

// 0x00B3A230 (:910)
bool dialogue_manager_start_dialogue(voice_line_handle line_handle, const human_voice_persona* hvpp, dialogue_priorities priority, public_address_group_handle pa_group, const char* message_icon_name)
{
    if (Dialogue_disable_dialogue_start || line_handle == INVALID_VOICE_LINE_HANDLE || !do_persona_line_check_common(hvpp, line_handle))
    {
        return false;
    }
    cs_scope lock(dialog_cs);
    dialogue_info* dip = create_new_dialogue(line_handle, priority);
    if (dip == nullptr)
    {
        return false;
    }
    dip->root->target_persona = hvpp;
    dip->root->pa_group = pa_group;
    dip->root->message_icon_name = message_icon_name;
    dip->root->play_status = DEPS_WAITING;
    return true;
}

// 0x00B3A2C0 (:938)
bool dialogue_manager_start_dialogue_megaphone(voice_line_handle line_handle, const human_voice_persona* hvpp, dialogue_priorities priority, unsigned int automobile_handle)
{
    if (Dialogue_disable_dialogue_start || line_handle == INVALID_VOICE_LINE_HANDLE || !do_persona_line_check_common(hvpp, line_handle))
    {
        return false;
    }
    cs_scope lock(dialog_cs);
    dialogue_info* dip = create_new_dialogue(line_handle, priority);
    if (dip == nullptr)
    {
        return false;
    }
    dip->root->target_persona = hvpp;
    dip->root->target_automobile = automobile_handle;
    dip->root->play_status = DEPS_WAITING;
    return true;
}

// 0x00B331D0 (:964): a line said by the same human less than a second after its parent runs on without the radio
// outro and intro between them.
bool dialogue_manager_add_line(voice_line_handle line_handle, human* hp, voice_line_handle parent_line, int time_offset)
{
    if (Dialogue_disable_dialogue_start || hp == nullptr || line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    const human_voice_persona* voice = hp->voice;
    if (!do_persona_line_check_common(voice, line_handle))
    {
        return false;
    }
    cs_scope lock(dialog_cs);
    dialogue_element* dep = create_new_child_element(line_handle, parent_line, time_offset);
    if (dep == nullptr)
    {
        return false;
    }
    dialogue_element* parent = dep->parent;
    dep->target_human = hp->get_handle();
    dep->play_status = DEPS_WAITING;
    // Magic number: 1000 ms.
    if (parent != nullptr && parent->target_human == dep->target_human && time_offset <= 1000)
    {
        parent->play_outro = false;
        dep->play_intro = false;
    }
    return true;
}

// 0x00B33290 (:1001)
bool dialogue_manager_add_line(voice_line_handle line_handle, const human_voice_persona* hvpp, voice_line_handle parent_line, int time_offset, public_address_group_handle pa_group, const char* message_icon_name)
{
    if (Dialogue_disable_dialogue_start || line_handle == INVALID_VOICE_LINE_HANDLE || !do_persona_line_check_common(hvpp, line_handle))
    {
        return false;
    }
    cs_scope lock(dialog_cs);
    dialogue_element* dep = create_new_child_element(line_handle, parent_line, time_offset);
    if (dep == nullptr)
    {
        return false;
    }
    dep->target_persona = hvpp;
    dep->message_icon_name = message_icon_name;
    dialogue_element* parent = dep->parent;
    dep->pa_group = pa_group;
    dep->play_status = DEPS_WAITING;
    if (parent != nullptr && parent->target_persona == dep->target_persona && time_offset <= 1000)
    {
        parent->play_outro = false;
        dep->play_intro = false;
    }
    return true;
}

// 0x00B33340 (:1046)
void dialogue_manager_pause()
{
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            pause_dialogue_elements(dip->root);
            dip->paused = true;
            dip = next;
        }
    }
}

// 0x00B333F0 (:1060)
void dialogue_manager_unpause()
{
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            dip->paused = false;
            dip = next;
        }
    }
}

// 0x00B3A340 (:1081)
void dialogue_manager_cancel(voice_line_handle line_handle, bool immediate_stop)
{
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            if (dip->root->line_to_play == line_handle)
            {
                stop_and_free_dialogue(dip, immediate_stop);
            }
            dip = next;
        }
    }
}

// 0x00B3A430 (:1096)
void dialogue_manager_cancel_vehicle(unsigned int vehicle_handle)
{
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            if (dip->root != nullptr && dip->root->target_automobile == vehicle_handle)
            {
                stop_and_free_dialogue(dip, true);
            }
            dip = next;
        }
    }
}

// 0x00B3A520 (:1119)
void dialogue_manager_cancel_all()
{
    GAME_TRACE("dialogue", "cancel all");
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr;)
        {
            dialogue_info* next = Dialogue_pool.get_next(dip);
            stop_and_free_dialogue(dip, true);
            dip = next;
        }
    }
}

// 0x00B3A5F0 (:1131)
void dialogue_manager_cancel_briefing()
{
    cs_scope lock(dialog_cs);
    csspinlock_scope pool_lock(Dialogue_pool.thread_access);
    for (dialogue_info* dip = Dialogue_pool.get_first(DIALOGUE_PRIORITY_BRIEFING); dip != nullptr;)
    {
        dialogue_info* next = Dialogue_pool.get_next(dip);
        stop_and_free_dialogue(dip, true);
        dip = next;
    }
}

// 0x00B334A0 (:1151): a dialogue whose root line is the line.
bool dialogue_manager_is_dialogue_playing(voice_line_handle line_handle)
{
    cs_scope lock(dialog_cs);
    for (int category = 0; category < 6; category++)
    {
        csspinlock_scope pool_lock(Dialogue_pool.thread_access);
        for (dialogue_info* dip = Dialogue_pool.get_first(category); dip != nullptr; dip = Dialogue_pool.get_next(dip))
        {
            if (dip->root->line_to_play == line_handle)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00B33560 (:1175): of a priority, or (UNDEFINED) all but the ambient ones.
int dialogue_manager_get_num_dialogues(dialogue_priorities which)
{
    if ((int)which > 5)
    {
        return 0;
    }
    if ((int)which >= 0)
    {
        return Dialogue_pool.get_count(which);
    }
    return (int)Dialogue_pool.get_capacity() - Dialogue_pool.get_count(DIALOGUE_PRIORITY_AMBIENT) - Dialogue_pool.get_free();
}

// 0x00B2DA40 (:1190)
int dialogue_manager_get_num_free_dialogues()
{
    const int free_dialogues = Dialogue_pool.get_free();
    const int free_elements = Dialogue_element_pool.get_free();
    return free_elements <= free_dialogues ? free_elements : free_dialogues;
}

// 0x00B335A0 (:1205): an element of the human that waits (or, with include_playing_lines, is queued or playing).
bool dialogue_manager_is_any_line_queued(const human* hp, bool include_playing_lines)
{
    if (hp == nullptr)
    {
        return false;
    }
    const unsigned int handle = hp->get_handle();
    cs_scope lock(dialog_cs);
    csspinlock_scope pool_lock(Dialogue_element_pool.thread_access);
    const int last_status = include_playing_lines ? DEPS_PLAYING : DEPS_WAITING;
    for (dialogue_element* dep = Dialogue_element_pool.get_first(0); dep != nullptr; dep = Dialogue_element_pool.get_next(dep))
    {
        if (dep->target_human == handle && (int)dep->play_status <= last_status)
        {
            return true;
        }
    }
    return false;
}
