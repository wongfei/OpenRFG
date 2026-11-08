// Original: volition/rfg/code/geom/vanim.cpp
//
// The animation state machine of a character (v_animate) over anim_lib. The tables (vanim_parse_anim_tables):
// anim_files.xtbl (Anims: one per animation with its triggers and variants), anim_groups.xtbl (weapon groups with a
// parent group), anim_transitions.xtbl (the action between two states), anim_blend_trees.xtbl (Blend_trees), and the
// anim set files (Anim_sets: per set and group the animation of each state and action, Group_lookup_*). A v_animate
// keeps its v_anims (states, actions, poses, blend trees, variants) in a page of V_anim_data_pool; every frame
// vanim_process_states blends the states towards the desired state and plays them looping, blend trees blend their
// states by a control input, vanim_process_movement_adjustments moves the character towards action end positions and
// orientations, vanim_process_completed_actions removes the finished actions. Part of rfg_geom_unity.cpp
// (player_minimal.md P2).
//
// Index space of states and actions (index_alias.index): states 0..321, actions 322..982 (ANIMATION_ACTION_IDLE_A);
// 983..1032 are aliases (vanim_aliases), 0x409 / 0x40A the desired / secondary state, larger values anim_lib instance
// handles.
// Precision: scalar chains are evaluated in double and rounded once, as rfg.exe does; vector operators round per step.
// Not reached yet: vanim_attach_actions (NPC.md: the two actors of the EDF ID check), vanim_refresh_all, vanim_dlc_reset,
// the blend helpers and vanim_get_state / _set_start_time / _get_cur_time / _set_cur_time_and_speed / _clear_states and
// the string pool marks.
// The external interface is declared in vanim.h.
//
// Split TU (SFX.md SX4 item 16): compiled twice. The rfg library builds everything but the prop and sound triggers; the
// audio library (rfg_audio, OPENRFG_AUDIO) builds only execute_prop_trigger, vanim_fast_forward_prop_triggers and
// vanim_check_prop_handles (the anim-triggered foley, voice lines and prop transfers). The programs without it keep
// their stand-ins: the triggers play sounds and create, move and destroy items, which would change the deterministic
// programs' results (SFX.md 2.5).

#ifndef OPENRFG_AUDIO

#include "volition/rfg/code/geom/vanim.h"

#include "keen/base/trace.hpp"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

extern float Frametime;

// ---- TU-private types (rfg.pdb; no header names them)
// One entry of a state's transition list (State_transitions[state][10]): the action that takes it to a state.
// size 0x8
struct state_transition
{
    animation_state destination_state; // 0x00
    animation_action action; // 0x04
};

// The state blends collected while a v_animate is processed (vanim_process_blend_tree, vanim_modify_state_percent).
// size 0xA4
struct vanim_internal_process_data
{
    int internal_num_state_blends; // 0x00
    int internal_state_blend_handles[20]; // 0x04
    float internal_state_blend_percents[20]; // 0x54
};

static const int VANIM_DESIRED_STATE = 0x409;
static const int VANIM_SECONDARY_STATE = 0x40A;
static const int VANIM_FIRST_ALIAS = 0x3D7;
static const int VANIM_FIRST_INSTANCE = 0x408;
static const float PI_F = 3.1415927f;
static const float TWO_PI_F = 6.2831855f;

// v_anim bits at +0x11C (the bit fields of v_anim, as the original's word tests)
static const unsigned int VA_USED = 0x1;
static const unsigned int VA_IS_STATE = 0x2;
static const unsigned int VA_IS_POSE = 0x4;
static const unsigned int VA_IS_BLEND_TREE = 0x8;
static const unsigned int VA_IS_TRANSITION_ACTION = 0x10;
static const unsigned int VA_IS_EXPLICIT_TRANSITION_ACTION = 0x200;
static const unsigned int VA_MOVE_TO_END_POS = 0x800;
static const unsigned int VA_MOVE_TO_END_POS_POST_PROCESS = 0x1000;
static const unsigned int VA_MOVE_TO_END_ORIENT = 0x2000;
static const unsigned int VA_MOVE_TO_STARTING_ORIENT = 0x4000;
static const unsigned int VA_MOVE_RELATIVE_TO_OBJ = 0x8000;
static const unsigned int VA_TWO_OBJECT_ANIM = 0x10000;
static const unsigned int VA_IGNORE_Y = 0x20000;

static unsigned int& va_bits(v_anim& va)
{
    return *(unsigned int*)((char*)&va + 0x11C);
}

// ---- globals
// Animation_state_tags and Animation_action_tags (vanim_tags.inl) are included by anim.cpp, first in the same unity
// file: anim_tag_state_from_enum and anim_tag_action_from_enum read them too.
static const char* const Ik_type_names[4] = {"Left Hand", "Right Hand", "Left Foot", "Right Foot"}; // 0x012F7618
static const char* const Prop_transfer_owner_names[6] = {"Item", "Offhand-Item", "Self", "Parent", "World", "Code Designated"}; // 0x012F7628
static const char* const Prop_transfer_action_names[3] = {"Create", "Delete", "Move"}; // 0x012F7690
static const char* const Prop_transfer_tag_names[3] = {"Mainhand", "Offhand", "(Named)"}; // 0x012F769C
static const char* const Anim_set_variant_names[12] = {"personal shield", "wounded", "wounded back", "wounded left leg", "wounded right leg", "drunk", "cold", "afraid", "oppressed", "tired", "confident", "on fire"}; // 0x012F76A8
// anim Anims[4096] (0x0253D4C8) and int Num_anims (0x025EF8A8) are anim.cpp's (same unity file).
static sound_trigger Sound_array[2000]; // 0x025694D0
static state_transition State_transitions[322][10]; // 0x0256D350
static unsigned char V_anim_data_buffer[195000]; // 0x025737F8
static anim_group Anim_groups[26]; // 0x025A31B8
static int Group_lookup_anim_set[6144]; // 0x025B5DF0
static voice_trigger Voice_array[100]; // 0x025BBDF0
anim_foley_set Anim_foley_sets[64]; // 0x025BC118
unsigned int Num_anim_foley_sets; // 0x025EF8E8
anim_set Anim_sets[70]; // 0x025BCF28
int Num_anim_sets; // 0x025EF8EC
static v_animate* vanim_free_list; // 0x025BD394
static char Anim_string_pool_data[112640]; // 0x025C1580
static ik_trigger Ik_array[1000]; // 0x025DCE80
anim_blend_tree_info Blend_trees[300]; // 0x025E03C0
int Num_blend_trees; // 0x025EF8F0
static prop_trigger Prop_trigger_array[500]; // 0x025E3940
anim_variant Anim_variants[256]; // 0x025E8098
int Num_anim_variants; // 0x025EF8AC
static int Group_lookup_anim_handle[6144]; // 0x025E98A0
int Num_free_vanims; // 0x025EF8F4
static int Num_ik_triggers; // 0x025EF8F8
static int Num_prop_triggers; // 0x025EF8FC
static int Num_sound_triggers; // 0x025EF900
static int Num_voice_triggers; // 0x025EF904
static unsigned int* Action_tag_crcs; // 0x025EF908, set while the tables are parsed
static unsigned int* State_tag_crcs; // 0x025EF90C
static page_mempool V_anim_data_pool(V_anim_data_buffer, 0x2F9B8, 300, "v_anim data pool", 1, 0); // 0x025EF950, pages of 300 bytes
static v_animate vanims[296]; // 0x025F6920
static string_pool Anim_string_pool(Anim_string_pool_data, 0x1B800, "Anim strings"); // 0x02616BE0

// 0x007F48D0
v_animate::v_animate()
{
}

// 0x00B62F40 (not reached; vanim_play_parameters is the human's).

static int anim_set_index(const anim_set* set)
{
    return (int)(set - Anim_sets);
}

static anim* anim_by_handle(int handle)
{
    return (unsigned int)handle < 4096 ? &Anims[handle] : nullptr;
}

// ---- table parsing

// 0x007F48E0. The anim_lib triggers of an animation (by name and 30 fps frame).
static void vanim_parse_trigger(xml_element* root, anim* animp)
{
    xml_element* triggers = xml_table_find_req(root, "Triggers");
    for (xml_element* trigger = xml_table_find_opt(triggers, "Trigger"); trigger != nullptr; trigger = xml_table_find_next(triggers, trigger, "Trigger")) {
        char name[512];
        int frame;
        xml_table_get_req_string(name, sizeof(name), trigger, "Name");
        const int trigger_id = animlib_get_trigger_id(name);
        xml_table_get_req_int(frame, trigger, "Frame");
        animlib_add_trigger_to_animation(animp->animlib_handle, trigger_id, frame);
    }
}

// 0x007FC060; LTCG: root on the stack, animp ECX. Item transfers (create / delete / move a prop) at a frame.
static void vanim_parse_prop_trigger(xml_element* root, anim* animp)
{
    xml_element* props = xml_table_find_opt(root, "Props");
    animp->num_prop_triggers = 0;
    if (props != nullptr) {
        for (xml_element* prop = xml_table_find_opt(props, "Prop"); prop != nullptr; prop = xml_table_find_next(props, prop, "Prop")) {
            prop_trigger& trigger = Prop_trigger_array[animp->num_prop_triggers + Num_prop_triggers];
            const char* owner = xml_table_get_req_string_ref(prop, "Owner");
            for (int i = 0; i < 6; i++) {
                if (strcmp(owner, Prop_transfer_owner_names[i]) == 0) {
                    trigger.owner = (prop_transfer_owner)i;
                    break;
                }
                if (i == PROP_TRANSFER_OWNER_CODE_DESIGNATED) {
                    keen::debug::nativePrintArgs("prop trigger owner: %s doesn't exist", owner);
                }
            }
            const char* owner_child = xml_table_get_opt_string_ref(prop, "Owner_Child");
            trigger.owner_child = PROP_TYPE_NONE;
            if (owner_child != nullptr) {
                trigger.owner_child = vanim_get_prop_type_from_name((char*)owner_child);
            }
            trigger.type = vanim_get_prop_type_from_name((char*)xml_table_get_req_string_ref(prop, "Type"));
            const char* action = xml_table_get_req_string_ref(prop, "Action");
            for (int i = 0; i < 3; i++) {
                if (strcmp(action, Prop_transfer_action_names[i]) == 0) {
                    trigger.action = (prop_transfer_action)i;
                    break;
                }
                if (i == PROP_TRANSFER_ACTION_MOVE) {
                    keen::debug::nativePrintArgs("prop trigger action: %s doesn't exist", action);
                }
            }
            xml_table_get_req_int(trigger.frame, prop, "Frame");
            const char* dest_owner = xml_table_get_opt_string_ref(prop, "Dest_Owner");
            trigger.dest_owner = PROP_TRANSFER_OWNER_NONE;
            if (dest_owner != nullptr) {
                for (int i = 0; i < 6; i++) {
                    if (strcmp(dest_owner, Prop_transfer_owner_names[i]) == 0) {
                        trigger.dest_owner = (prop_transfer_owner)i;
                        break;
                    }
                    if (i == PROP_TRANSFER_OWNER_CODE_DESIGNATED) {
                        keen::debug::nativePrintArgs("prop trigger owner: %s doesn't exist", dest_owner);
                    }
                }
            }
            const char* dest_owner_child = xml_table_get_opt_string_ref(prop, "Dest_Owner_Child");
            trigger.dest_owner_child = PROP_TYPE_NONE;
            if (dest_owner_child != nullptr) {
                trigger.dest_owner_child = vanim_get_prop_type_from_name((char*)dest_owner_child);
            }
            const char* dest_tag = xml_table_get_opt_string_ref(prop, "Dest_Tag");
            trigger.dest_tag = PROP_TRANSFER_TAG_NONE;
            if (dest_tag != nullptr) {
                for (int i = 0; i < 3; i++) {
                    if (strcmp(dest_tag, Prop_transfer_tag_names[i]) == 0) {
                        trigger.dest_tag = (prop_transfer_tag)i;
                        break;
                    }
                    if (i == PROP_TRANSFER_TAG_NAMED) {
                        keen::debug::nativePrintArgs("prop trigger tag: %s doesn't exist", dest_tag);
                    }
                }
            }
            const char* dest_tag_name = xml_table_get_opt_string_ref(prop, "Dest_Tag_Name");
            trigger.dest_tag_name = nullptr;
            if (dest_tag_name != nullptr) {
                trigger.dest_tag_name = Anim_string_pool.add_unique(dest_tag_name);
            }
            animp->num_prop_triggers++;
        }
    }
    if (animp->num_prop_triggers != 0) {
        animp->prop_triggers = Prop_trigger_array + Num_prop_triggers;
        Num_prop_triggers += animp->num_prop_triggers;
    } else {
        animp->prop_triggers = nullptr;
    }
}

// 0x007F49A0. IK enable / disable per limb at a frame, with a blend time in frames.
static void vanim_parse_ik_trigger(xml_element* root, anim* animp)
{
    xml_element* iks = xml_table_find_opt(root, "IKs");
    animp->num_ik_triggers = 0;
    if (iks != nullptr) {
        for (xml_element* ik = xml_table_find_opt(iks, "IK"); ik != nullptr; ik = xml_table_find_next(iks, ik, "IK")) {
            const int slot = animp->num_ik_triggers + Num_ik_triggers;
            Ik_array[slot].enable = v_stricmp(xml_table_get_req_string_ref(ik, "Enable"), "True") == 0;
            const char* location = xml_table_get_req_string_ref(ik, "Location");
            for (int i = 0; i < 4; i++) {
                if (v_stricmp(Ik_type_names[i], location) == 0) {
                    Ik_array[slot].location = (unsigned char)i;
                    break;
                }
                if (i == 3) {
                    keen::debug::nativePrintArgs("ik location %s doesn't exist", location);
                }
            }
            int frame = 0;
            xml_table_get_req_int(frame, ik, "Frame");
            Ik_array[slot].frame = (unsigned short)frame;
            if (animp->num_ik_triggers != 0 && (unsigned short)frame < Ik_array[slot - 1].frame) {
                keen::debug::nativePrintArgs("IK entry for %s are not ordered based on frames", xml_table_get_req_string_ref(root, "ID"));
            }
            int blend;
            xml_table_get_req_int(blend, ik, "Blend");
            Ik_array[slot].blend_rate = blend == 0 ? 0.0f : 30.0f / (float)blend;
            animp->num_ik_triggers++;
        }
    }
    if (animp->num_ik_triggers != 0) {
        animp->ik_triggers = Ik_array + Num_ik_triggers;
        Num_ik_triggers += animp->num_ik_triggers;
    } else {
        animp->ik_triggers = nullptr;
    }
}

// 0x007F4BA0; LTCG: root EAX, animp on the stack. Foley sounds at a frame.
static void vanim_parse_sound_trigger(xml_element* root, anim* animp)
{
    xml_element* sounds = xml_table_find_opt(root, "Sounds");
    xml_element* sound = sounds != nullptr ? xml_table_find_opt(sounds, "Sound") : nullptr;
    animp->num_sound_triggers = 0;
    while (sound != nullptr) {
        sound_trigger& trigger = Sound_array[animp->num_sound_triggers + Num_sound_triggers];
        int frame = 0;
        xml_table_get_req_int(frame, sound, "Frame");
        trigger.frame = (unsigned short)frame;
        bool looping = false;
        xml_table_get_req_bool(looping, sound, "Looping");
        trigger.looping = looping;
        const char* cue_name = xml_table_get_opt_string_ref(sound, "Foley");
        if (cue_name != nullptr) {
            audiolib_cue_get_id(cue_name, &trigger.foley_ref);
        }
        sound = xml_table_find_next(sounds, sound, "Sound");
        animp->num_sound_triggers++;
    }
    if (animp->num_sound_triggers != 0) {
        animp->sound_triggers = Sound_array + Num_sound_triggers;
        Num_sound_triggers += animp->num_sound_triggers;
    } else {
        animp->sound_triggers = nullptr;
    }
}

// 0x007F4CB0; LTCG: root EAX, animp EDI. Voice lines at a frame.
static void vanim_parse_voice_trigger(xml_element* root, anim* animp)
{
    xml_element* lines = xml_table_find_opt(root, "Voice_Lines");
    xml_element* line = lines != nullptr ? xml_table_find_opt(lines, "Voice_Line") : nullptr;
    animp->num_voice_triggers = 0;
    while (line != nullptr) {
        voice_trigger& trigger = Voice_array[animp->num_voice_triggers + Num_voice_triggers];
        xml_table_get_req_int(trigger.frame, line, "Frame");
        const char* line_name = xml_table_get_opt_string_ref(line, "Line");
        if (line_name != nullptr) {
            trigger.voice_handle = voice_line_handle_from_name(line_name);
        }
        line = xml_table_find_next(lines, line, "Voice_Line");
        animp->num_voice_triggers++;
    }
    if (animp->num_voice_triggers != 0) {
        animp->voice_triggers = Voice_array + Num_voice_triggers;
        Num_voice_triggers += animp->num_voice_triggers;
    } else {
        animp->voice_triggers = nullptr;
    }
}

// 0x007FC3E0. Counts the group's animations per state and action (the second pass fills the lookup).
static int vanim_parse_group(xml_element* root, anim_group* group, bool refresh)
{
    int found = 0;
    xml_element* states = xml_table_find_req(root, "States");
    for (xml_element* state = xml_find(states, "State"); state != nullptr; state = xml_find_next(states, state, "State")) {
        const char* id = xml_table_get_req_string_ref(state, "ID");
        anim* animp = anim_find(xml_table_get_req_filename_ref(state, "Animation"));
        const animation_state index = vanim_lookup_state(id);
        if (index != ANIMATION_STATE_NONE) {
            group->num_anims[index]++;
        }
        if (animp != nullptr) {
            found++;
        }
    }
    xml_element* actions = xml_table_find_req(root, "Actions");
    for (xml_element* action = xml_find(actions, "Action"); action != nullptr; action = xml_find_next(actions, action, "Action")) {
        const char* id = xml_table_get_req_string_ref(action, "ID");
        anim* animp = anim_find(xml_table_get_req_filename_ref(action, "Animation"));
        const animation_action index = vanim_lookup_action(id);
        if (index != ANIMATION_ACTION_NONE) {
            group->num_anims[index]++;
        }
        if (animp != nullptr) {
            found++;
        }
    }
    return found;
}

static int group_tag_index(const char* name)
{
    int i = 0;
    for (; i < 26; i++) {
        if (v_stricmp(name, Animation_group_tags[i]) == 0) {
            break;
        }
    }
    return i;
}

// 0x007FC500. All animation tables (see the file comment); dlc_id != -1 reads the DLC's tables ("dlc%02d_...").
static void vanim_parse_anim_tables(bool refresh, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    unsigned int state_crcs[322];
    unsigned int action_crcs[661];
    Action_tag_crcs = action_crcs;
    State_tag_crcs = state_crcs;
    for (int i = 0; i < 322; i++) {
        State_tag_crcs[i] = crc_stri(Animation_state_tags[i], 0);
    }
    for (int i = 0; i < 661; i++) {
        Action_tag_crcs[i] = crc_stri(Animation_action_tags[i], 0);
    }
    char filename[256];

    // groups
    memset(Anim_groups, 0, sizeof(Anim_groups));
    xml_element* groups = xml_table_parse("anim_groups.xtbl");
    for (xml_element* group = xml_table_find_opt(groups, "Anim_Group"); group != nullptr; group = xml_table_find_next(groups, group, "Anim_Group")) {
        const int index = group_tag_index(xml_table_get_req_string_ref(group, "Name"));
        const char* parent = xml_table_get_opt_string_ref(group, "Parent");
        if (parent != nullptr) {
            Anim_groups[index].parent = &Anim_groups[group_tag_index(parent)];
        }
    }
    xml_table_close(groups);

    // transitions
    for (int s = 0; s < 322; s++) {
        for (int t = 0; t < 10; t++) {
            State_transitions[s][t].destination_state = ANIMATION_STATE_NONE;
        }
    }
    xml_element* transitions = xml_table_parse("anim_transitions.xtbl");
    for (xml_element* transition = xml_table_find_opt(transitions, "Anim_Transition"); transition != nullptr; transition = xml_table_find_next(transitions, transition, "Anim_Transition")) {
        const char* from = xml_table_get_req_string_ref(transition, "From_State");
        int from_state = 0;
        for (; from_state < 322; from_state++) {
            if (v_stricmp(from, Animation_state_tags[from_state]) == 0) {
                break;
            }
        }
        const char* to = xml_table_get_req_string_ref(transition, "To_State");
        int to_state = 0;
        for (; to_state < 322; to_state++) {
            if (v_stricmp(to, Animation_state_tags[to_state]) == 0) {
                break;
            }
        }
        const char* action_name = xml_table_get_req_string_ref(transition, "Action");
        int action = 0;
        for (; action < 661; action++) {
            if (v_stricmp(action_name, Animation_action_tags[action]) == 0) {
                break;
            }
        }
        for (int t = 0; t < 10; t++) {
            if (State_transitions[from_state][t].destination_state == ANIMATION_STATE_NONE) {
                State_transitions[from_state][t].destination_state = (animation_state)to_state;
                State_transitions[from_state][t].action = (animation_action)(action + ANIMATION_ACTION_IDLE_A);
                break;
            }
        }
    }
    xml_table_close(transitions);

    // foley sets
    xml_element* foley = xml_table_parse("anim_foley.xtbl");
    Num_anim_foley_sets = 0;
    for (xml_element* set = xml_table_find_opt(foley, "AnimFoley"); set != nullptr; set = xml_table_find_next(foley, set, "AnimFoley")) {
        anim_foley_set& foley_set = Anim_foley_sets[Num_anim_foley_sets];
        foley_set.chcksum = crc_str(xml_table_get_opt_string_ref(set, "Name"), 0);
        xml_element* foleys = xml_table_find_opt(set, "Foleys");
        foley_set.num_foleys = 0;
        for (xml_element* entry = xml_table_find_opt(foleys, "Foley"); entry != nullptr; entry = xml_table_find_next(foleys, entry, "Foley")) {
            anim_foley& f = foley_set.foleys[foley_set.num_foleys];
            audiolib_cue_get_id(xml_table_get_req_string_ref(entry, "FoleyRef"), &f.foley_id);
            xml_table_get_req_bool(f.looping, entry, "Looping");
            xml_table_get_req_int(f.frame, entry, "Frame");
            foley_set.num_foleys++;
        }
        Num_anim_foley_sets++;
    }
    xml_table_close(foley);

    // the anim set file names (32 characters each, at most 70)
    char set_files[70][64];
    int num_set_files = 0;
    xml_element* set_names;
    if (!dlc) {
        set_names = xml_table_parse("anim_set_filenames.xtbl");
    } else {
        sprintf(filename, "dlc%02d_%s", dlc_id, "anim_set_filenames.xtbl");
        set_names = xml_table_parse(filename);
    }
    for (xml_element* name = xml_table_find_opt(set_names, "Anim_Set_Filename"); name != nullptr; name = xml_table_find_next(set_names, name, "Anim_Set_Filename")) {
        xml_table_get_req_string(set_files[num_set_files], 0x20, name, "Name");
        num_set_files++;
    }
    xml_table_close(set_names);

    // animation files
    xml_element* files_root;
    if (!dlc) {
        files_root = xml_table_parse("anim_files.xtbl");
    } else {
        sprintf(filename, "dlc%02d_%s", dlc_id, "anim_files.xtbl");
        files_root = xml_table_parse(filename);
    }
    xml_element* files = xml_table_find_req(xml_table_find_req(files_root, "Files"), "Anim_files");
    for (xml_element* file = xml_find(files, "Anim_file"); file != nullptr; file = xml_find_next(files, file, "Anim_file")) {
        const char* animation = xml_table_get_req_filename_ref(file, "Animation");
        char name[64];
        cf_drop_ext(name, sizeof(name), animation);
        anim* animp = nullptr;
        if (Num_anims < 4096) {
            animp = &Anims[Num_anims];
            memset(animp, 0, sizeof(anim));
            Num_anims++;
        }
        animp->editor_flags = 0;
        cf_add_ext(name, sizeof(name), animation, ".anim", false);
        animp->filename = Anim_string_pool.add(name);
        animp->animlib_handle = animlib_get_anim_slot(animp->filename);
        cf_drop_ext(name, sizeof(name), animation);
        animp->name_crc = crc_stri(name, 0);
        vanim_parse_trigger(file, animp);
        vanim_parse_prop_trigger(file, animp);
        vanim_parse_ik_trigger(file, animp);
        vanim_parse_sound_trigger(file, animp);
        vanim_parse_voice_trigger(file, animp);
        xml_element* flags = xml_table_find_opt(file, "Flags");
        if (flags != nullptr) {
            animp->editor_flags |= xml_table_get_flags(Animation_editor_anim_flags, 11, flags, false);
        }
        if (refresh) {
            bool preload;
            xml_table_get_req_bool(preload, xml_find(file, "Animation"), "Preload");
            if (preload) {
                cf_add_ext(filename, 0x80, animp->filename, ".anim_pc", false);
                cf_refresh_file_size(filename);
                anim_load_animation(animp->filename);
            }
        }
    }
    // variants (a second pass: the variant animations are Anims too)
    for (xml_element* file = xml_find(files, "Anim_file"); file != nullptr; file = xml_find_next(files, file, "Anim_file")) {
        xml_element* variants = xml_table_find_opt(file, "Variants");
        if (variants == nullptr) {
            continue;
        }
        anim* animp = anim_find(xml_table_get_req_filename_ref(file, "Animation"));
        float value;
        // seconds to timestamp units of 180 ms; percent to 0..255
        xml_table_get_req_float(value, variants, "Freq_Min");
        animp->variant_frequency_min = (unsigned char)(int)((double)value * 1000.0 / 180.0);
        xml_table_get_req_float(value, variants, "Freq_Max");
        animp->variant_frequency_max = (unsigned char)(int)((double)value * 1000.0 / 180.0);
        xml_table_get_req_float(value, variants, "Cyclic_Chance");
        animp->variant_cyclic_percent = (unsigned char)(int)((double)value / 100.0 * 255.0);
        animp->num_variants = 0;
        animp->variants = Anim_variants + Num_anim_variants;
        for (xml_element* variant = xml_table_find_req(variants, "Variant"); variant != nullptr; variant = xml_table_find_next(variants, variant, "Variant")) {
            anim_variant& v = animp->variants[animp->num_variants];
            const char* tag = xml_table_get_opt_string_ref(variant, "Anim_Tag");
            if (tag != nullptr) {
                for (int i = 0; i < 12; i++) {
                    if (v_stricmp(tag, Variant_tags[i]) == 0) {
                        v.variant_id = (vanim_variants)i;
                    }
                }
            }
            anim* variant_anim = anim_find(xml_table_get_req_string_ref(variant, "File"));
            v.anim_handle = (int)(variant_anim - Anims);
            v.playstyle = VARIANT_PLAYSTYLE_STANDARD;
            const char* playstyle = xml_table_get_opt_string_ref(variant, "Playstyle");
            if (playstyle != nullptr) {
                if (v_stricmp(playstyle, "Fullbody") == 0) {
                    v.playstyle = VARIANT_PLAYSTYLE_FULLBODY;
                } else if (v_stricmp(playstyle, "Priority") == 0) {
                    v.playstyle = VARIANT_PLAYSTYLE_INCREASED_PRIORITY;
                }
            }
            xml_table_get_req_float(v.cyclic_percentage, variant, "Cyclic_Percentage");
            v.cyclic_percentage = (float)((double)v.cyclic_percentage / 100.0);
            xml_table_get_req_float(v.percentage, variant, "Percentage");
            v.percentage = (float)((double)v.percentage / 100.0);
            xml_table_get_req_float(value, variant, "Time");
            v.time = (int)((double)value * 1000.0);
            animp->num_variants++;
        }
        Num_anim_variants += animp->num_variants;
    }
    xml_table_close(files_root);

    // blend trees: the tree's Anims entry gets editor flag 0x40000000 and the tree index as its animlib handle
    xml_element* trees_root;
    if (!dlc) {
        trees_root = xml_table_parse("anim_blend_trees.xtbl");
    } else {
        sprintf(filename, "dlc%02d_%s", dlc_id, "anim_blend_trees.xtbl");
        trees_root = xml_table_parse(filename);
    }
    int tree_index = 0;
    for (xml_element* tree = xml_table_find_opt(trees_root, "Blend_trees"); tree != nullptr; tree = xml_table_find_next(trees_root, tree, "Blend_trees")) {
        Num_blend_trees++;
        anim* animp = anim_find(xml_table_get_req_string_ref(tree, "Name"));
        if (animp == nullptr) {
            // the original does not advance tree_index for a tree without an animation
            continue;
        }
        anim_blend_tree_info& info = Blend_trees[tree_index];
        info.name = animp->filename;
        xml_table_get_req_float(info.control_input_low, tree, "Control_input_low");
        xml_table_get_req_float(info.control_input_high, tree, "Control_input_high");
        xml_table_get_req_float(info.control_ramp_speed, tree, "Ramp_speed");
        animp->editor_flags |= 0x40000000;
        animp->animlib_handle = tree_index;
        info.num_states = 0;
        info.num_actions = 0;
        xml_element* states = xml_table_find_opt(tree, "States");
        if (states != nullptr) {
            for (xml_element* state = xml_table_find_opt(states, "State"); state != nullptr; state = xml_table_find_next(states, state, "State")) {
                info.num_states++;
            }
            info.states = (anim_blend_tree_info::blend_tree_state*)Anim_blend_tree_mempool.alloc(info.num_states * sizeof(anim_blend_tree_info::blend_tree_state), 0);
            int s = 0;
            for (xml_element* state = xml_table_find_opt(states, "State"); state != nullptr;) {
                const char* animation = xml_table_get_req_filename_ref(state, "Animation");
                anim* state_anim = anim_find(animation);
                if (state_anim == nullptr) {
                    // the original keeps looping on this element (no next), as the decompile shows
                    continue;
                }
                anim_blend_tree_info::blend_tree_state& bts = info.states[s];
                bts.anim_filename = state_anim->filename;
                char name[64];
                cf_add_ext(name, sizeof(name), animation, ".anim", false);
                bts.animlib_handle = animlib_get_anim_slot(bts.anim_filename);
                bts.anim_handle = (int)(state_anim - Anims);
                if (refresh) {
                    cf_add_ext(filename, 0x80, name, ".anim_pc", false);
                    cf_refresh_file_size(filename);
                    anim_load_animation(bts.anim_filename);
                }
                xml_table_get_req_float(bts.blend_time, state, "Blend_time");
                xml_element* points = xml_table_find_req(state, "Control_points");
                bts.num_control_points = 0;
                for (xml_element* point = xml_table_find_req(points, "Control_point"); point != nullptr; point = xml_table_find_next(points, point, "Control_point")) {
                    xml_table_get_req_float(bts.control_point_range[bts.num_control_points], point, "Range");
                    xml_table_get_req_float(bts.control_point_value[bts.num_control_points], point, "Value");
                    bts.num_control_points++;
                }
                state = xml_table_find_next(states, state, "State");
                s++;
            }
        }
        xml_element* actions = xml_table_find_opt(tree, "Actions");
        if (actions != nullptr) {
            for (xml_element* action = xml_table_find_opt(actions, "Action"); action != nullptr; action = xml_table_find_next(actions, action, "Action")) {
                info.num_actions++;
            }
            info.actions = (anim_blend_tree_info::blend_tree_action*)Anim_blend_tree_mempool.alloc(info.num_actions * sizeof(anim_blend_tree_info::blend_tree_action), 0);
            int a = 0;
            for (xml_element* action = xml_table_find_opt(actions, "Action"); action != nullptr;) {
                const char* animation = xml_table_get_req_filename_ref(action, "Animation");
                anim* action_anim = anim_find(animation);
                if (action_anim == nullptr) {
                    continue; // as above
                }
                anim_blend_tree_info::blend_tree_action& bta = info.actions[a];
                char name[64];
                cf_add_ext(name, sizeof(name), animation, ".anim", false);
                bta.anim_filename = action_anim->filename;
                bta.anim_handle = animlib_get_anim_slot(bta.anim_filename);
                if (refresh) {
                    cf_add_ext(filename, 0x80, name, ".anim_pc", false);
                    cf_refresh_file_size(filename);
                    anim_load_animation(bta.anim_filename);
                }
                bta.use_ramped_input = v_stricmp(xml_table_get_req_string_ref(action, "Control_type"), "Ramped input") == 0;
                xml_table_get_req_float(bta.start_range_min, action, "Start_range_min");
                xml_table_get_req_float(bta.start_range_max, action, "Start_range_max");
                xml_table_get_req_float(bta.end_range_min, action, "End_range_min");
                xml_table_get_req_float(bta.end_range_max, action, "End_range_max");
                xml_table_get_req_float(bta.time_variance, action, "Time_variance");
                bta.cap_ramp_min = info.control_input_low;
                xml_table_get_opt_float(bta.cap_ramp_min, action, "Cap_ramp_min");
                bta.cap_ramp_max = info.control_input_high;
                xml_table_get_opt_float(bta.cap_ramp_max, action, "Cap_ramp_max");
                xml_table_get_req_bool(bta.always_cap, action, "Always_cap");
                bta.end_state_percent = -1.0f;
                xml_table_get_opt_float(bta.end_state_percent, action, "End_state_percent");
                bta.start_state_percent_min = 0.0f;
                xml_table_get_opt_float(bta.start_state_percent_min, action, "Start_state_percent_min");
                bta.start_state_percent_max = 1.0f;
                xml_table_get_opt_float(bta.start_state_percent_max, action, "Start_state_percent_max");
                bta.cancel_range_min = (float)((double)info.control_input_low - 4000.0);
                xml_table_get_opt_float(bta.cancel_range_min, action, "Cancel_range_min");
                bta.cancel_range_max = (float)((double)info.control_input_high + 4000.0);
                xml_table_get_opt_float(bta.cancel_range_max, action, "Cancel_range_max");
                xml_table_get_req_bool(bta.speed_throttling, action, "Speed_throttling");
                action = xml_table_find_next(actions, action, "Action");
                a++;
            }
        }
        tree_index++;
    }
    xml_table_close(trees_root);

    // anim sets, first pass: names, parents, variant types and the per group animation counts
    for (int f = 0; f < num_set_files; f++) {
        xml_element* set_root = xml_table_parse(set_files[f]);
        if (set_root == nullptr) {
            keen::debug::nativePrintArgs("unable to open %s", set_files[f]);
            continue;
        }
        for (xml_element* set = xml_find(set_root, "Skeleton_Set"); set != nullptr; set = xml_find_next(set_root, set, "Skeleton_Set")) {
            const char* name = xml_table_get_opt_string_ref(set, "Name");
            if (name == nullptr) {
                keen::debug::nativePrintArgs("Anim Set #%i in %s has no name", Num_anim_sets + 1, set_files[f]);
                continue;
            }
            anim_set& anim_set_entry = Anim_sets[Num_anim_sets];
            anim_set_entry.name = Anim_string_pool.add_unique(name);
            const char* parent = xml_table_get_opt_string_ref(set, "Parent_Skeleton");
            anim_set_entry.parent_name = parent == nullptr ? nullptr : Anim_string_pool.add_unique(parent);
            anim_set_entry.variant_type = ANIM_SET_VARIANT_NONE;
            const char* variant = xml_table_get_opt_string_ref(set, "Variant_Type");
            if (variant != nullptr) {
                anim_set_entry.variant_type = vanim_get_animation_set_variant_from_name(variant);
            }
            xml_element* set_groups = xml_table_find_req(set, "Groups");
            for (xml_element* group = xml_find(set_groups, "Group"); group != nullptr; group = xml_find_next(set_groups, group, "Group")) {
                const int index = group_tag_index(xml_table_get_req_string_ref(group, "Group"));
                if (index < 26) {
                    vanim_parse_group(group, &Anim_groups[index], refresh);
                } else {
                    keen::debug::nativePrintArgs("We should have found the group in the group names array");
                }
            }
            Num_anim_sets++;
        }
        xml_table_close(set_root);
    }
    for (int i = 0; i < Num_anim_sets; i++) {
        if (Anim_sets[i].parent_name != nullptr) {
            int p = 0;
            for (; p < Num_anim_sets; p++) {
                if (v_stricmp(Anim_sets[i].parent_name, Anim_sets[p].name) == 0) {
                    break;
                }
            }
            Anim_sets[i].parent = &Anim_sets[p];
        }
    }
    // the lookup ranges from the counts (counts reset for the second pass)
    unsigned short next = 0;
    for (anim_group& group : Anim_groups) {
        for (int i = 0; i < 983; i++) {
            if (group.num_anims[i] == 0) {
                group.lookup_index[i] = 0xFFFF;
            } else {
                group.lookup_index[i] = next;
                next = (unsigned short)(next + group.num_anims[i]);
            }
            group.num_anims[i] = 0;
        }
    }
    // second pass: the anim handle and set of every entry
    Num_anim_sets = 0;
    for (int f = 0; f < num_set_files; f++) {
        xml_element* set_root = xml_table_parse(set_files[f]);
        if (set_root == nullptr) {
            keen::debug::nativePrintArgs("unable to open %s", set_files[f]);
        }
        for (xml_element* set = xml_find(set_root, "Skeleton_Set"); set != nullptr; set = xml_find_next(set_root, set, "Skeleton_Set")) {
            const int set_index = Num_anim_sets;
            xml_element* set_groups = xml_table_find_req(set, "Groups");
            for (xml_element* group = xml_find(set_groups, "Group"); group != nullptr; group = xml_find_next(set_groups, group, "Group")) {
                const int g = group_tag_index(xml_table_get_req_string_ref(group, "Group"));
                xml_element* states = xml_table_find_req(group, "States");
                for (xml_element* state = xml_find(states, "State"); state != nullptr; state = xml_find_next(states, state, "State")) {
                    const char* id = xml_table_get_req_string_ref(state, "ID");
                    const char* animation = xml_table_get_req_filename_ref(state, "Animation");
                    const animation_state index = vanim_lookup_state(id);
                    if (index != ANIMATION_STATE_NONE) {
                        const int slot = Anim_groups[g].lookup_index[index] + Anim_groups[g].num_anims[index];
                        Anim_groups[g].num_anims[index]++;
                        Group_lookup_anim_handle[slot] = (int)(anim_find(animation) - Anims);
                        Group_lookup_anim_set[slot] = set_index;
                    }
                }
                xml_element* actions = xml_table_find_req(group, "Actions");
                for (xml_element* action = xml_find(actions, "Action"); action != nullptr; action = xml_find_next(actions, action, "Action")) {
                    const char* id = xml_table_get_req_string_ref(action, "ID");
                    const char* animation = xml_table_get_req_filename_ref(action, "Animation");
                    const animation_action index = vanim_lookup_action(id);
                    if (index != ANIMATION_ACTION_NONE) {
                        const int slot = Anim_groups[g].lookup_index[index] + Anim_groups[g].num_anims[index];
                        Anim_groups[g].num_anims[index]++;
                        Group_lookup_anim_handle[slot] = (int)(anim_find(animation) - Anims);
                        Group_lookup_anim_set[slot] = set_index;
                    }
                }
            }
            Num_anim_sets++;
        }
        xml_table_close(set_root);
    }
    State_tag_crcs = nullptr;
    Action_tag_crcs = nullptr;
}

// 0x008097D0 (:218): a debug console command, empty in rfg.exe.
void unused_dcf_Print_item_list()
{
}

// 0x007F4D80
static void parse_triggers()
{
    xml_element* root = xml_table_parse("anim_triggers.xtbl");
    for (xml_element* trigger = xml_table_find_opt(root, "Trigger"); trigger != nullptr; trigger = xml_table_find_next(root, trigger, "Trigger")) {
        char name[32];
        xml_table_get_req_string(name, sizeof(name), trigger, "Name");
        animlib_register_trigger_name(name);
    }
    xml_table_close(root);
    character_set_up_trigger_ids();
}

// ---- v_anim lookup

// 0x007F4E00; LTCG: vp on the stack, idx EAX, index_alias_instance ECX.
static bool vanim_matches_index_alias(const v_animate* vp, int idx, int index_alias_instance)
{
    const v_anim* va = &vp->anims[idx];
    if (!va->used) {
        return false;
    }
    if (index_alias_instance == VANIM_DESIRED_STATE) {
        return vp->desired_state == va;
    }
    if (index_alias_instance == VANIM_SECONDARY_STATE) {
        return vp->secondary_state == va;
    }
    if (index_alias_instance < VANIM_FIRST_ALIAS) {
        return va->index == index_alias_instance;
    }
    if (index_alias_instance < VANIM_FIRST_INSTANCE) {
        return va->alias == index_alias_instance;
    }
    return va->instance == (unsigned int)index_alias_instance;
}

// 0x007F4E70; LTCG: vp ECX, index_alias_instance EAX. The v_anims are packed (used ones first). File static in rfg.exe;
// external linkage here for the rfg_audio half of this split TU (vanim_fast_forward_prop_triggers).
int vanim_get_index(v_animate* vp, int index_alias_instance)
{
    if (index_alias_instance == VANIM_DESIRED_STATE || index_alias_instance == VANIM_SECONDARY_STATE) {
        v_anim* target = index_alias_instance == VANIM_DESIRED_STATE ? vp->desired_state : vp->secondary_state;
        if (target != nullptr) {
            for (int i = 0; i < vp->max_simultaneous_anims; i++) {
                if (&vp->anims[i] == target) {
                    return i;
                }
            }
        }
        return -1;
    }
    if (index_alias_instance == 0) {
        return -1;
    }
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        const v_anim& va = vp->anims[i];
        if (!va.used) {
            return -1;
        }
        if (index_alias_instance < VANIM_FIRST_ALIAS) {
            if (va.index == index_alias_instance) {
                return i;
            }
        } else if (index_alias_instance < VANIM_FIRST_INSTANCE) {
            if (va.alias == index_alias_instance) {
                return i;
            }
        } else if (va.instance == (unsigned int)index_alias_instance) {
            return i;
        }
    }
    return -1;
}

// 0x007FDB60. Removes a v_anim: the last used one moves into its slot (keeping the list packed) and the state and
// variant links follow it.
static void vanim_internal_remove_anim(v_animate* vp, int index)
{
    if (vp->anims[index].two_object_anim) {
        vanim_detach_actions(vp);
    }
    v_anim* variant = vp->anims[index].variant;
    if (variant != nullptr && !vp->anims[index].is_variant) {
        variant->variant = nullptr;
        if (vp->anims[index].variant != nullptr) {
            vp->anims[index].variant->blend = 0.0f;
        }
    }
    v_anim* removed = &vp->anims[index];
    if (removed->move_relative_to_obj) {
        vp->flags.move_relative_to_obj = 0;
    }
    int last = index;
    while (last < vp->max_simultaneous_anims - 1 && vp->anims[last + 1].used) {
        last++;
    }
    if (last > index) {
        v_anim* moved = &vp->anims[last];
        if (!moved->is_variant) {
            if (moved->variant != nullptr) {
                moved->variant->variant = removed;
            }
        } else {
            for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
                if (vp->anims[i].variant == moved) {
                    vp->anims[i].variant = removed;
                }
            }
        }
        memcpy(removed, moved, sizeof(v_anim));
        if (vp->secondary_state == removed) {
            vp->secondary_state = nullptr;
        }
        if (vp->desired_state == removed) {
            vp->desired_state = nullptr;
        } else if (vp->desired_state == moved) {
            vp->desired_state = removed;
        } else if (vp->secondary_state == moved) {
            vp->secondary_state = removed;
        }
    } else if (vp->desired_state == removed) {
        vp->desired_state = nullptr;
    }
    if (vp->secondary_state == &vp->anims[last]) {
        vp->secondary_state = nullptr;
    }
    memset(&vp->anims[last], 0, sizeof(v_anim));
    vp->anims[last].anim_handle = -1;
}

// 0x007F6B00; LTCG: vp ESI. One more v_anim: a new page from V_anim_data_pool, the pointers into the old one moved.
static void vanim_reallocate(v_animate* vp)
{
    const int old_count = vp->max_simultaneous_anims;
    const int new_count = old_count + 1;
    if (!V_anim_data_pool.can_alloc(new_count * sizeof(v_anim), 0)) {
        return;
    }
    v_anim* anims = (v_anim*)V_anim_data_pool.alloc(new_count * sizeof(v_anim), 0);
    if (vp->anims != nullptr) {
        memcpy(anims, vp->anims, old_count * sizeof(v_anim));
        if (vp->desired_state != nullptr) {
            vp->desired_state = anims + (vp->desired_state - vp->anims);
        }
        if (vp->secondary_state != nullptr) {
            vp->secondary_state = anims + (vp->secondary_state - vp->anims);
        }
        for (int i = 0; i < old_count; i++) {
            if (vp->anims[i].variant != nullptr) {
                anims[i].variant = anims + (vp->anims[i].variant - vp->anims);
            }
        }
        V_anim_data_pool.release_bytes(vp->anims);
    }
    vp->anims = anims;
    vp->max_simultaneous_anims = new_count;
    for (int i = old_count; i < new_count; i++) {
        memset(&vp->anims[i], 0, sizeof(v_anim));
        vp->anims[i].anim_handle = -1;
    }
}

// 0x007F5E30; LTCG: vp EDI, state_list ESI. The state v_anims, by blend (largest first).
static int vanim_get_state_list(v_animate* vp, v_anim** state_list)
{
    int count = 0;
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim* va = &vp->anims[i];
        if (!va->used) {
            return count;
        }
        if (!va->is_state) {
            continue;
        }
        int slot = 0;
        while (slot < count && !(state_list[slot]->blend < va->blend)) {
            slot++;
        }
        for (int j = count; j > slot; j--) {
            state_list[j] = state_list[j - 1];
        }
        state_list[slot] = va;
        count++;
    }
    return count;
}

// 0x007F6C40. The anim of a state or action for an anim set and group: the set, then its parents, then the parent
// group with the original set again.
int vanim_find_anim_handle(int set_index, int group_index, int anim_index)
{
    const anim_group* group = &Anim_groups[group_index];
    do {
        int set = set_index;
        for (;;) {
            const unsigned int first = group->lookup_index[anim_index];
            for (int i = 0; i < group->num_anims[anim_index]; i++) {
                if (Group_lookup_anim_set[first + i] == set) {
                    return Group_lookup_anim_handle[first + i];
                }
            }
            if (Anim_sets[set].parent == nullptr) {
                break;
            }
            set = anim_set_index(Anim_sets[set].parent);
        }
        group = group->parent;
    } while (group != nullptr);
    return -1;
}

// 0x007F6CC0 (:1950)
char* vanim_add_string(const char* string)
{
    return Anim_string_pool.add_unique(string);
}

// 0x007F6CD0 (:1955)
void vanim_mark_anim_string_pool()
{
    Anim_string_pool.mark();
}

// 0x007F6CE0 (:1960)
void vanim_restore_anim_string_pool_to_mark()
{
    Anim_string_pool.restore_to_mark(0);
}

// 0x007FDD10 (:1965): every table parsed again from the files on disk. Magic number: 0x2580 bytes, all of Blend_trees.
void vanim_refresh_all()
{
    cf_refresh_file_size_ext(".xtbl");
    Anim_string_pool.clear();
    Num_ik_triggers = 0;
    Num_prop_triggers = 0;
    Num_blend_trees = 0;
    Num_sound_triggers = 0;
    Num_voice_triggers = 0;
    Num_anim_sets = 0;
    parse_triggers();
    memset(Blend_trees, 0, 0x2580);
    vanim_parse_anim_tables(true, -1);
}

// 0x007FDD80 (:1984): every table parsed again with a DLC's files. Magic number: as vanim_refresh_all.
void vanim_dlc_reset(int dlc_id)
{
    Anim_string_pool.set_thread_ownership();
    Anim_string_pool.clear();
    Num_ik_triggers = 0;
    Num_prop_triggers = 0;
    Num_blend_trees = 0;
    Num_sound_triggers = 0;
    Num_voice_triggers = 0;
    Num_anim_sets = 0;
    parse_triggers();
    memset(Blend_trees, 0, 0x2580);
    vanim_parse_anim_tables(false, dlc_id);
}

// 0x007F6CF0
int vanim_get_action_handle(const v_animate* vp, int group_index, int action)
{
    if (action != -1 && vp->animation_set != nullptr) {
        return vanim_find_anim_handle(anim_set_index(vp->animation_set), group_index, action);
    }
    return -1;
}

// 0x007F6D20
int vanim_get_state_handle(v_animate* vp, int group_index, int state)
{
    if (state != -1 && vp->animation_set != nullptr) {
        return vanim_find_anim_handle(anim_set_index(vp->animation_set), group_index, state);
    }
    return -1;
}

static int vanim_find_handle_for(const v_animate* vp, int index)
{
    if (index == -1 || vp->animation_set == nullptr) {
        return -1;
    }
    return vanim_find_anim_handle(anim_set_index(vp->animation_set), vp->default_group_index, index);
}

// 0x007FDDE0. A v_animate from the free list (no v_anims yet).
v_animate* vanim_create(const char* anim_set_name)
{
    v_animate* vp = vanim_free_list;
    if (vp == nullptr) {
        return nullptr;
    }
    v_animate* next = vp->next;
    vp->max_simultaneous_anims = 0;
    vp->anims = nullptr;
    vp->state_blend_rate = -1.0f;
    vp->desired_state = nullptr;
    vp->secondary_state = nullptr;
    vp->default_group_index = ANIMATION_GROUP_DEFAULT;
    if (next == vp) {
        vp->next = nullptr;
        vp->prev = nullptr;
        vanim_free_list = nullptr;
    } else {
        vanim_free_list = next;
        if (vp->prev != nullptr) {
            vp->prev->next = vp->next;
        }
        if (vp->next != nullptr) {
            vp->next->prev = vp->prev;
        }
        vp->next = nullptr;
        vp->prev = nullptr;
    }
    anim_set* set = anim_set_find(anim_set_name);
    vp->base_animation_set = set;
    vp->animation_set = set;
    vp->base_anim_set_variant = ANIM_SET_VARIANT_NONE;
    vp->temp_anim_set_variant = ANIM_SET_VARIANT_NONE;
    vp->aim_handle = -1;
    return vp;
}

// 0x007F6D50
void vanim_delete(v_animate* vp)
{
    if (vp == nullptr) {
        return;
    }
    if (vp->anims != nullptr) {
        V_anim_data_pool.release_bytes(vp->anims);
        vp->anims = nullptr;
        vp->max_simultaneous_anims = 0;
    }
    if (vanim_free_list == nullptr) {
        vp->next = vp;
        vp->prev = vp;
        vanim_free_list = vp;
    } else {
        vp->prev = vanim_free_list->prev;
        vp->next = vanim_free_list;
        vanim_free_list->prev->next = vp;
        vanim_free_list->prev = vp;
    }
}

// 0x007FDE70
void vanim_system_init()
{
    for (v_animate& vp : vanims) {
        if (vanim_free_list == nullptr) {
            vp.next = &vp;
            vp.prev = &vp;
            vanim_free_list = &vp;
        } else {
            vp.prev = vanim_free_list->prev;
            vp.next = vanim_free_list;
            vanim_free_list->prev->next = &vp;
            vanim_free_list->prev = &vp;
        }
    }
    parse_triggers();
    vanim_parse_anim_tables(false, -1);
}

// 0x007F6DB0. While the tables are parsed by crc, else by name.
animation_state vanim_lookup_state(const char* name)
{
    if (State_tag_crcs == nullptr) {
        for (int i = 0; i < 322; i++) {
            if (v_stricmp(name, Animation_state_tags[i]) == 0) {
                return (animation_state)i;
            }
        }
        return ANIMATION_STATE_NONE;
    }
    const unsigned int crc = crc_stri(name, 0);
    for (int i = 0; i < 322; i++) {
        if (State_tag_crcs[i] == crc) {
            return (animation_state)i;
        }
    }
    return ANIMATION_STATE_NONE;
}

// 0x007F6E20
animation_action vanim_lookup_action(const char* name)
{
    if (Action_tag_crcs == nullptr) {
        for (int i = 0; i < 661; i++) {
            if (v_stricmp(name, Animation_action_tags[i]) == 0) {
                return (animation_action)(i + ANIMATION_ACTION_IDLE_A);
            }
        }
        return ANIMATION_ACTION_NONE;
    }
    const unsigned int crc = crc_stri(name, 0);
    for (int i = 0; i < 661; i++) {
        if (Action_tag_crcs[i] == crc) {
            return (animation_action)(i + ANIMATION_ACTION_IDLE_A);
        }
    }
    return ANIMATION_ACTION_NONE;
}

// 0x007F6EA0
animation_group vanim_lookup_group(const char* name)
{
    for (int i = 0; i < 26; i++) {
        if (v_stricmp(name, Animation_group_tags[i]) == 0) {
            return (animation_group)i;
        }
    }
    return ANIMATION_GROUP_NONE;
}

// 0x007F6EE0
const char* vanim_get_group_name(animation_group group_index)
{
    return Animation_group_tags[group_index];
}

// The common part of vanim_play_action and vanim_play_custom_action once the slot and anim are known.
static unsigned int vanim_start_action(v_animate* vp, int slot, unsigned int animlib_flag, float speed_multiplier, float max_mix_percent, bool set_freq_ts_invalid)
{
    v_anim& va = vp->anims[slot];
    const unsigned int handle = (unsigned int)va.anim_handle;
    if (handle < 4096) {
        const anim& a = Anims[handle];
        if ((animlib_flag & 0x4000) != 0 || (a.editor_flags & 8) != 0) {
            va.is_explicit_transition_action = 1;
        }
        if (a.editor_flags & 1) {
            animlib_flag |= 0x2000; // additive
        }
        if (a.num_variants == 0) {
            if (set_freq_ts_invalid) {
                va.variant_freq_ts.invalidate();
            }
        } else {
            va.variant_freq_ts.set(int_rand_range(a.variant_frequency_min * 180, a.variant_frequency_max * 180));
        }
        if (a.editor_flags & 0x40) {
            animlib_flag |= 0x8000; // snap to frames
        }
        va.flags = animlib_flag;
        va.instance = animlib_play_animation(vp->aim_handle, a.animlib_handle, animlib_flag, 0.0f, max_mix_percent, speed_multiplier, -1.0f);
    }
    return va.instance;
}

// 0x007F6EF0. A slot (reusing one playing the same action unless flags & 0x200) for the action of the current set and
// group.
unsigned int vanim_play_action(v_animate* vp, index_alias action_index, unsigned int animlib_flag, float speed_multiplier, float max_mix_percent)
{
    if (vp->aim_handle == -1) {
        return 0xFFFFFFFF;
    }
    const int count = vp->max_simultaneous_anims;
    int slot = 0;
    while (slot < count && vp->anims[slot].used && !((animlib_flag & 0x200) == 0 && vp->anims[slot].index == action_index.index)) {
        slot++;
    }
    if (count <= slot) {
        vanim_reallocate(vp);
    }
    if (slot >= vp->max_simultaneous_anims) {
        return 0xFFFFFFFF;
    }
    v_anim& va = vp->anims[slot];
    va.anim_handle = vanim_find_handle_for(vp, action_index.index);
    if (va.anim_handle < 0) {
        return 0xFFFFFFFF;
    }
    va.used = 1;
    va.move_to_end_pos = 0;
    va.move_to_end_orient = 0;
    va.is_explicit_transition_action = 0;
    va.index = action_index.index;
    va.alias = action_index.alias;
    return vanim_start_action(vp, slot, animlib_flag, speed_multiplier, max_mix_percent, true);
}

// 0x007F7100. As vanim_play_action with an Anims handle.
unsigned int vanim_play_custom_action(v_animate* vp, index_alias action_handle, unsigned int animlib_flag, float speed_multiplier, float max_mix_percent)
{
    if (vp->aim_handle == -1) {
        return 0xFFFFFFFF;
    }
    const int count = vp->max_simultaneous_anims;
    int slot = 0;
    while (slot < count && vp->anims[slot].used && vp->anims[slot].anim_handle != action_handle.index) {
        slot++;
    }
    if (count <= slot) {
        vanim_reallocate(vp);
    }
    if (slot >= vp->max_simultaneous_anims) {
        return 0xFFFFFFFF;
    }
    v_anim& va = vp->anims[slot];
    va.used = 1;
    va.move_to_end_pos = 0;
    va.move_to_end_orient = 0;
    va.is_explicit_transition_action = 0;
    va.anim_handle = action_handle.index;
    va.index = -1;
    va.alias = action_handle.alias;
    return vanim_start_action(vp, slot, animlib_flag, speed_multiplier, max_mix_percent, false);
}

// 0x008097E0
void vanim_change_group(v_animate* vp, animation_group group_index, index_alias state_index, float blend_time, bool start_at_zero);

// 0x007FDF50. The state of the current set and group; without an explicit action the transition action between the
// states is played (unless an explicit transition action is already playing).
void vanim_change_state(v_animate* vp, index_alias state_index, index_alias action_index, float blend_time, bool start_at_zero)
{
    if (vp->desired_state != nullptr && action_index.index == -1) {
        bool explicit_transition = false;
        for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
            if (vp->anims[i].is_explicit_transition_action) {
                explicit_transition = true;
                break;
            }
        }
        if (!explicit_transition) {
            const int from = vp->desired_state->index;
            for (int t = 0; t < 10; t++) {
                const animation_state destination = State_transitions[from][t].destination_state;
                if (destination == ANIMATION_STATE_NONE) {
                    break;
                }
                if (destination == state_index.index) {
                    action_index.index = State_transitions[from][t].action;
                    break;
                }
            }
        }
    }
    const int state_handle = vanim_find_handle_for(vp, state_index.index);
    const int action_handle = vanim_find_handle_for(vp, action_index.index);
    if (state_handle == -1) {
        return;
    }
    vanim_change_custom_state(vp, index_alias(state_handle, state_index.alias), index_alias(action_handle, action_index.alias), blend_time, start_at_zero);
    vp->desired_state->index = state_index.index;
    if (action_handle > -1) {
        for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
            if (vp->anims[i].anim_handle == action_handle) {
                vp->anims[i].index = action_index.index;
                return;
            }
        }
    }
}

// 0x007F72B0. A new desired state (an Anims handle): a slot (when the v_anims are full, the weakest state is
// replaced), the secondary state (the strongest other one), playing transition actions stopped, and the optional
// transition action whose length becomes the blend time. A blend tree state that only changes alias keeps playing.
void vanim_change_custom_state(v_animate* vp, index_alias state_handle, index_alias action_handle, float blend_time, bool start_at_zero)
{
    if (state_handle.index == -1) {
        return;
    }
    v_anim* desired = vp->desired_state;
    if (desired != nullptr && state_handle.index == desired->anim_handle) {
        return;
    }
    anim* new_anim = anim_by_handle(state_handle.index);
    anim* old_anim = desired != nullptr ? anim_by_handle(desired->anim_handle) : nullptr;
    if (new_anim != nullptr && old_anim != nullptr && new_anim->animlib_handle == old_anim->animlib_handle && (new_anim->editor_flags & 0x40000000) != 0) {
        desired->alias = state_handle.alias;
        return;
    }
    int count = vp->max_simultaneous_anims;
    int slot = 0;
    while (slot < count && vp->anims[slot].used && vp->anims[slot].anim_handle != state_handle.index) {
        slot++;
    }
    if (count <= slot) {
        vanim_reallocate(vp);
    }
    count = vp->max_simultaneous_anims;
    if (count <= slot) {
        float weakest = 50.0f;
        int weakest_slot = -1;
        for (int i = 0; i < count; i++) {
            if (vp->anims[i].is_state && vp->anims[i].blend < weakest) {
                weakest_slot = i;
                weakest = vp->anims[i].blend;
            }
        }
        if (weakest_slot != -1) {
            slot = weakest_slot;
        }
        if (count <= slot) {
            return;
        }
    }
    vp->desired_state = &vp->anims[slot];
    vp->anims[slot].used = 1;
    vp->desired_state->is_state = 1;
    vp->desired_state->alias = state_handle.alias;
    vp->flags.next_state_start_at_zero = start_at_zero ? 1 : 0;
    if (vp->desired_state->anim_handle != state_handle.index) {
        vp->desired_state->blend = 0.0f;
        vp->desired_state->anim_handle = state_handle.index;
    }
    anim* state_anim = anim_get_anim_from_handle(state_handle.index);
    if (state_anim != nullptr) {
        if (state_anim->editor_flags & 1) {
            vp->desired_state->flags |= 0x2000;
        }
        if (state_anim->editor_flags & 0x40000000) {
            vp->desired_state->is_blend_tree = 1;
            vp->desired_state->blend_tree_info = &Blend_trees[state_anim->animlib_handle];
        }
    }
    // (the original reads num_variants through a null state_anim for an invalid handle)
    if (state_anim->num_variants == 0) {
        vp->desired_state->variant_freq_ts.invalidate();
    } else {
        vp->desired_state->variant_freq_ts.set(int_rand_range(state_anim->variant_frequency_min * 180, state_anim->variant_frequency_max * 180));
    }
    v_anim* states[20] = {};
    vanim_get_state_list(vp, states);
    vp->secondary_state = states[0] == vp->desired_state ? states[1] : states[0];
    for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
        if (vp->anims[i].is_transition_action && vp->aim_handle > -1) {
            animlib_stop_animation(vp->aim_handle, vp->anims[i].instance, 0.2f);
        }
    }
    if (action_handle.index > -1) {
        const unsigned int instance = vanim_play_custom_action(vp, action_handle, 0x10, 1.0f, 1.0f);
        const int index = vanim_get_index(vp, (int)instance);
        vp->anims[index].is_transition_action = 1;
        const int handle = vp->anims[index].anim_handle;
        if (handle > -1) {
            blend_time = (float)((double)animlib_get_animation_length(anim_get_anim_from_handle(handle)->animlib_handle) - (double)0.1f);
            if (0.0f <= 0.0f - blend_time) {
                blend_time = 0.0f;
            }
        }
    }
    vp->state_blend_rate = blend_time;
    vp->parametric_blend_percent = vp->desired_state->blend;
}

// 0x007F7600
void vanim_set_transition_ending_percent(v_animate* vp, float percent)
{
    if (vp->desired_state != nullptr) {
        vp->flags.set_state_transition_ending_frame = 1;
        vp->transition_ending_percent = percent;
    }
}

// 0x007FE0B0. A pose (an extra state at a fixed blend): the existing pose of the anim or a new slot; blend 0 removes
// it.
v_anim* vanim_set_state_custom_blend_pose(v_animate* vp, index_alias state_index, float blend_percent, unsigned int flags)
{
    int count = vp->max_simultaneous_anims;
    int slot = 0;
    bool found = false;
    for (; slot < count; slot++) {
        if (!vp->anims[slot].used) {
            break;
        }
        if (vp->anims[slot].is_pose && vp->anims[slot].anim_handle == state_index.index) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (blend_percent < 0.001f) {
            return nullptr;
        }
        slot = 0;
        while (slot < count && vp->anims[slot].used) {
            slot++;
        }
        if (count <= slot) {
            vanim_reallocate(vp);
        }
    }
    if (slot >= vp->max_simultaneous_anims || slot == -1) {
        return nullptr;
    }
    if (blend_percent == 0.0f) {
        vanim_internal_remove_anim(vp, slot);
        return nullptr;
    }
    v_anim& va = vp->anims[slot];
    va.anim_handle = state_index.index;
    va.blend = blend_percent;
    if ((unsigned int)state_index.index < 4096) {
        if (Anims[state_index.index].editor_flags & 1) {
            flags |= 0x2000;
        }
        if (Anims[state_index.index].editor_flags & 0x40000000) {
            va.is_blend_tree = 1;
            va.blend_tree_info = &Blend_trees[Anims[state_index.index].animlib_handle];
        }
    }
    va.index = -1;
    va.flags = flags;
    va.is_pose = 1;
    va.used = 1;
    va.alias = state_index.alias;
    return &va;
}

// 0x00809840. As vanim_set_state_custom_blend_pose with a state of the current set and group.
v_anim* vanim_set_state_blend_pose(v_animate* vp, index_alias state_index, float blend_percent, unsigned int flags)
{
    const int count = vp->max_simultaneous_anims;
    int slot = 0;
    if (count > 0) {
        for (; slot < count; slot++) {
            const v_anim& va = vp->anims[slot];
            if (!va.used) {
                slot = -1;
                break;
            }
            if (va.is_pose && (va.anim_handle == state_index.index || va.index == state_index.index || va.alias == state_index.index)) {
                break;
            }
        }
    }
    if (slot == count) {
        slot = -1;
    }
    if (slot < 0) {
        if (blend_percent != 0.0f && state_index.index < VANIM_FIRST_ALIAS) {
            v_anim* pose = vanim_set_state_custom_blend_pose(vp, index_alias(vanim_find_handle_for(vp, state_index.index), state_index.alias), blend_percent, flags);
            if (pose != nullptr) {
                pose->index = state_index.index;
                return pose;
            }
        }
        return nullptr;
    }
    const int index = vp->anims[slot].index;
    v_anim* pose = vanim_set_state_custom_blend_pose(vp, index_alias(vp->anims[slot].anim_handle, vp->anims[slot].alias), blend_percent, flags);
    if (pose != nullptr) {
        pose->index = index;
    }
    return pose;
}

// ---- blend trees

// 0x007F5EC0. The tree's actions: an inactive action starts when the input goes from its start range into its end
// range within time_variance seconds; an active one is cancelled (blends out) outside its cancel range.
static void vanim_process_blend_tree_input(v_anim* va)
{
    const anim_blend_tree_info* info = va->blend_tree_info;
    for (int i = 0; i < info->num_actions; i++) {
        const anim_blend_tree_info::blend_tree_action& action = info->actions[i];
        anim_blend_tree_instance::action_instance& instance = va->blend_tree_instance.actions[i];
        const float input = action.use_ramped_input ? va->blend_tree_instance.ramped_input : va->blend_tree_instance.control_input;
        if (!instance.is_active) {
            if (input < action.start_range_min || action.start_range_max < input) {
                if (action.end_range_min <= input && input <= action.end_range_max) {
                    if (0.0f < instance.time_variance) {
                        instance.is_blending_out = 0;
                        instance.is_active = 1;
                        instance.instance = 0xFFFFFFFF;
                    }
                } else {
                    instance.time_variance = instance.time_variance - Frametime;
                }
            } else {
                instance.time_variance = (float)((double)action.time_variance + 0.001);
            }
        }
        if (instance.is_active && (input < action.cancel_range_min || action.cancel_range_max < input)) {
            instance.is_blending_out = 1;
        }
    }
}

// 0x007F6000; LTCG: internal_data EAX, animlib_handle EDI, blend_percent on the stack (vp not passed).
static float vanim_modify_state_percent(v_animate* vp, vanim_internal_process_data& internal_data, int animlib_handle, float blend_percent)
{
    for (int i = 0; i < internal_data.internal_num_state_blends; i++) {
        if (animlib_handle == internal_data.internal_state_blend_handles[i]) {
            float percent = internal_data.internal_state_blend_percents[i] + blend_percent;
            if ((float)(1.0 - (double)percent) < 0.0f) {
                percent = 1.0f;
            }
            internal_data.internal_state_blend_percents[i] = percent;
            return percent;
        }
    }
    const int n = internal_data.internal_num_state_blends;
    internal_data.internal_num_state_blends = n + 1;
    internal_data.internal_state_blend_percents[n] = blend_percent;
    internal_data.internal_state_blend_handles[n] = animlib_handle;
    return blend_percent;
}

// The value of a blend tree state's control point curve at an input (0 outside the points).
static float blend_tree_state_value(const anim_blend_tree_info::blend_tree_state& state, float input)
{
    for (int k = 0; k < state.num_control_points - 1; k++) {
        if (state.control_point_range[k] <= input && input <= state.control_point_range[k + 1]) {
            return (float)(((double)state.control_point_value[k + 1] - (double)state.control_point_value[k]) * ((double)input - (double)state.control_point_range[k]) / ((double)state.control_point_range[k + 1] - (double)state.control_point_range[k]) + (double)state.control_point_value[k]);
        }
    }
    return 0.0f;
}

// 0x007F6080; LTCG: vp and va on the stack, internal_data ECX. Ramps the input, runs the actions (ramping in and out,
// capping the input, playing them looping, possibly setting the states' cycle position at their end), then blends the
// states towards their curve values and plays them looping.
static void vanim_process_blend_tree(v_animate* vp, vanim_internal_process_data& internal_data, v_anim* va)
{
    anim_blend_tree_info* info = va->blend_tree_info;
    anim_blend_tree_instance& bti = va->blend_tree_instance;
    if (!bti.input_initialized) {
        float input = info->control_input_low;
        if (0.0f <= 0.0f - input) {
            input = 0.0f;
        }
        if (0.0f <= input - info->control_input_high) {
            input = info->control_input_high;
        }
        bti.input_initialized = true;
        bti.last_control_input = input;
        bti.control_input = input;
        bti.ramped_input = input;
    }
    const float step = info->control_ramp_speed * Frametime;
    const float target = bti.control_input;
    float ramped = bti.ramped_input;
    if (target <= ramped) {
        ramped = ramped - step;
        if (target - ramped < 0.0f) {
            bti.ramped_input = ramped;
        } else {
            bti.ramped_input = target;
        }
    } else {
        ramped = ramped + step;
        if (0.0f <= target - ramped) {
            bti.ramped_input = ramped;
        } else {
            bti.ramped_input = target;
        }
    }
    vanim_process_blend_tree_input(va);
    float end_state_percent = -1.0f;
    float state_blend_time_override = -1.0f;
    for (int i = 0; i < info->num_actions; i++) {
        anim_blend_tree_info::blend_tree_action& action = info->actions[i];
        anim_blend_tree_instance::action_instance& instance = bti.actions[i];
        if (!instance.is_active) {
            continue;
        }
        int ramp_in_ticks;
        int ramp_out_ticks;
        animlib_get_ramp_in_out_times(action.anim_handle, ramp_in_ticks, ramp_out_ticks);
        const float ramp_in = (float)ramp_in_ticks / 4800.0f;
        const float ramp_out = (float)ramp_out_ticks / 4800.0f;
        const float length = animlib_get_animation_length(action.anim_handle);
        const float time_left = (float)((double)length - (double)instance.cur_time_percent * (double)length);
        float speed = 1.0f;
        if (instance.instance != 0xFFFFFFFF) {
            int prev_time;
            int cur_time;
            animlib_get_animation_times(vp->aim_handle, instance.instance, prev_time, cur_time);
            speed = animlib_get_current_speed_multiplier(vp->aim_handle, instance.instance);
            const float percent = (float)(((double)Frametime * (double)speed + (double)((float)cur_time / 4800.0f)) / (double)length);
            if (instance.cur_time_percent < percent) {
                instance.cur_time_percent = percent;
            }
        }
        if (action.always_cap) {
            float input = bti.ramped_input;
            if (input - action.cap_ramp_min < 0.0f) {
                input = action.cap_ramp_min;
            }
            if (0.0f <= input - action.cap_ramp_max) {
                input = action.cap_ramp_max;
            }
            bti.ramped_input = input;
        }
        if (time_left <= ramp_out || instance.is_blending_out) {
            if (time_left < ramp_out) {
                float input = bti.ramped_input;
                if (input - action.cap_ramp_min < 0.0f) {
                    input = action.cap_ramp_min;
                }
                if (0.0f <= input - action.cap_ramp_max) {
                    input = action.cap_ramp_max;
                }
                bti.ramped_input = input;
            }
            float blend = (float)((double)instance.current_blend - (double)Frametime * (double)speed / (double)ramp_out);
            if (0.0f <= 0.0f - blend) {
                blend = 0.0f;
            }
            instance.current_blend = blend;
            if (1.0f < instance.cur_time_percent || blend <= 0.0f) {
                instance.current_blend = 0.0f;
                instance.cur_time_percent = 0.0f;
                instance.is_active = 0;
                instance.is_blending_out = 0;
                instance.instance = 0xFFFFFFFF;
                instance.time_variance = 0.0f;
                continue;
            }
        } else {
            float blend = (float)((double)Frametime * (double)speed / (double)ramp_in + (double)instance.current_blend);
            if (1.0f - blend < 0.0f) {
                blend = 1.0f;
            }
            instance.current_blend = blend;
            // an action ramping in blends the earlier active ones out
            for (int j = 0; j < i; j++) {
                if (bti.actions[j].is_active) {
                    bti.actions[j].is_blending_out = 1;
                }
            }
        }
        if (0.0f < instance.current_blend) {
            const float blend = va->blend * instance.current_blend;
            const float total = vanim_modify_state_percent(vp, internal_data, action.anim_handle, blend);
            const unsigned int handle = animlib_play_animation_looping(vp->aim_handle, action.anim_handle, fl_ramp(total, 1.0f), 0x10, 1.0f, 0.0f);
            instance.instance = handle;
            if (instance.cur_time_percent == 0.0f) {
                animlib_set_looping_anim_start_percent(vp->aim_handle, handle, (float)(((double)action.time_variance - (double)instance.time_variance) / (double)length));
            }
            if (!instance.is_blending_out) {
                if (-1e-05f <= action.end_state_percent) {
                    end_state_percent = action.end_state_percent;
                }
                state_blend_time_override = (float)((double)length - (double)ramp_out - (double)ramp_in);
                if (0.0f <= 0.0f - state_blend_time_override) {
                    state_blend_time_override = 0.0f;
                }
            }
        }
    }
    const float input = bti.ramped_input;
    float strongest = -1.0f;
    unsigned int handles[5];
    for (int s = 0; s < info->num_states; s++) {
        const anim_blend_tree_info::blend_tree_state& state = info->states[s];
        const float target_weight = blend_tree_state_value(state, input);
        float blend_time = state.blend_time;
        if (0.0f < state_blend_time_override) {
            blend_time = state_blend_time_override;
        }
        const float rate = Frametime / blend_time;
        float weight = bti.state_weights[s];
        float new_weight = target_weight;
        if (rate < fabsf(target_weight - weight)) {
            new_weight = target_weight <= weight ? weight - rate : weight + rate;
        }
        bti.state_weights[s] = new_weight;
        const float blend = new_weight * va->blend;
        if (blend <= 0.0f) {
            handles[s] = 0;
            continue;
        }
        const float total = vanim_modify_state_percent(vp, internal_data, state.animlib_handle, blend);
        float mix = total;
        if (va->variant != nullptr && va->variant->is_variant_fullbody) {
            mix = (float)((1.0 - (double)va->variant->blend) * (double)total);
        }
        const unsigned int handle = animlib_play_animation_looping(vp->aim_handle, state.animlib_handle, mix, va->flags, 1.0f, 0.0f);
        handles[s] = handle;
        if (strongest < mix) {
            va->instance = handle;
            strongest = mix;
            if (end_state_percent < 0.0f) {
                bti.global_init_cyclic_percent = animlib_get_init_cyclic_percent(vp->aim_handle, handle);
            }
        }
        if (0.0f <= end_state_percent) {
            animlib_set_looping_anim_start_percent(vp->aim_handle, handle, end_state_percent);
            bti.global_init_cyclic_percent = animlib_get_init_cyclic_percent(vp->aim_handle, handle);
        }
    }
    for (int s = 0; s < info->num_states; s++) {
        if (end_state_percent < 0.0f) {
            animlib_set_init_cyclic_percent(vp->aim_handle, handles[s], bti.global_init_cyclic_percent);
        }
    }
}

// 0x007F91B0; LTCG: anim EAX, control_value XMM1. Sets the input (clamped to the tree's range) at once and the
// state weights to their curve values.
static void vanim_set_anim_blend_tree_init_control_input(v_anim& anim, float control_value)
{
    float input = anim.blend_tree_info->control_input_low;
    if (0.0f <= control_value - input) {
        input = control_value;
    }
    float high = anim.blend_tree_info->control_input_high;
    if (input - high < 0.0f) {
        high = input;
    }
    anim.blend_tree_instance.input_initialized = true;
    anim.blend_tree_instance.last_control_input = high;
    anim.blend_tree_instance.control_input = high;
    anim.blend_tree_instance.ramped_input = high;
    vanim_process_blend_tree_input(&anim);
    const anim_blend_tree_info* info = anim.blend_tree_info;
    for (int s = 0; s < info->num_states; s++) {
        anim.blend_tree_instance.state_weights[s] = blend_tree_state_value(info->states[s], high);
    }
}

// 0x007F9330 (not reached).
static void vanim_set_anim_blend_tree_control_info(v_anim& anim, float control_value);

// 0x007F9390
void vanim_set_blend_tree_init_control_input(v_animate* vp, int anim_index, float control_value)
{
    const int index = vanim_get_index(vp, anim_index);
    if (index > -1 && vp->anims[index].is_blend_tree) {
        vanim_set_anim_blend_tree_init_control_input(vp->anims[index], control_value);
    }
}

static void set_blend_tree_input(v_anim& anim, float control_value)
{
    if (!anim.blend_tree_instance.input_initialized) {
        vanim_set_anim_blend_tree_init_control_input(anim, control_value);
        return;
    }
    float input = anim.blend_tree_info->control_input_low;
    if (0.0f <= control_value - input) {
        input = control_value;
    }
    const float high = anim.blend_tree_info->control_input_high;
    if (0.0f <= input - high) {
        input = high;
    }
    anim.blend_tree_instance.control_input = input;
}

// 0x007F93D0
void vanim_set_blend_tree_control_input(v_animate* vp, int anim_index, float control_value)
{
    const int index = vanim_get_index(vp, anim_index);
    if (index > -1 && vp->anims[index].is_blend_tree) {
        set_blend_tree_input(vp->anims[index], control_value);
    }
}

// 0x007F9460. Every blend tree v_anim that matches.
void vanim_set_all_blend_tree_control_inputs(v_animate* vp, int anim_index, float control_value)
{
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim& va = vp->anims[i];
        if (!va.used) {
            return;
        }
        if (va.is_blend_tree && vanim_matches_index_alias(vp, i, anim_index)) {
            set_blend_tree_input(va, control_value);
        }
    }
}

// 0x007F9520
float vanim_get_blend_tree_ramped_input(v_animate* vp, int anim_index)
{
    const int index = vanim_get_index(vp, anim_index);
    if (index > -1 && vp->anims[index].is_blend_tree) {
        return vp->anims[index].blend_tree_instance.ramped_input;
    }
    return 0.0f;
}

// 0x007F9560
bool vanim_is_blend_tree(v_animate* vp, int anim_index)
{
    const int index = vanim_get_index(vp, anim_index);
    return index > -1 && vp->anims[index].is_blend_tree;
}

// ---- variants

// 0x007FE220 (:2566). The desired state's directional variants as poses, by lean (x left and right, y up and down, z
// forward and back; a direction without a variant does not count). A lean longer than 1 in the sum of its axes is scaled
// to 1. blend_rate > 0 moves each pose from its current blend towards the target by that share (it is the share kept of
// the old blend). Magic number: a pose under 0.005 is off. Null without directional variants.
v_anim* vanim_set_state_directional_blend_pose(v_animate* vp, vector lean, float blend_rate)
{
    if (vp == nullptr || vp->desired_state == nullptr) {
        return nullptr;
    }
    v_anim* state = vp->desired_state;
    const anim* animp = anim_get_anim_from_handle(state->anim_handle);
    if (animp == nullptr || animp->num_variants == 0) {
        return nullptr;
    }
    float x = ZERO_VECTOR.x;
    float y = ZERO_VECTOR.y;
    float z = ZERO_VECTOR.z;
    bool directional = false;
    for (int i = 0; i < animp->num_variants; i++) {
        switch (animp->variants[i].variant_id) {
        case ANIM_VARIANT_FORWARD:
        case ANIM_VARIANT_BACK:
            z = 1.0f;
            directional = true;
            break;
        case ANIM_VARIANT_LEFT:
        case ANIM_VARIANT_RIGHT:
            x = 1.0f;
            directional = true;
            break;
        case ANIM_VARIANT_UP:
        case ANIM_VARIANT_DOWN:
            y = 1.0f;
            directional = true;
            break;
        default:
            break;
        }
    }
    if (!directional) {
        return nullptr;
    }
    x = lean.x * x;
    y = lean.y * y;
    z = lean.z * z;
    const float sum = (float)(fabs((double)y) + fabs((double)x) + fabs((double)z));
    if (1.0f < sum) {
        const float scale = 1.0f / sum;
        x = scale * x;
        y = scale * y;
        z = scale * z;
    }
    const auto clamp01 = [](float value) {
        if (value - 0.0f < 0.0f) {
            value = 0.0f;
        }
        if (0.0f <= value - 1.0f) {
            value = 1.0f;
        }
        return value;
    };
    const float left = clamp01(-x);
    const float right = clamp01(x);
    const float up = clamp01(y);
    const float down = clamp01(-y);
    const float forward = clamp01(z);
    const float back = clamp01(-z);
    for (int i = 0; i < animp->num_variants; i++) {
        const anim_variant& variant = animp->variants[i];
        float blend;
        switch (variant.variant_id) {
        case ANIM_VARIANT_FORWARD:
            blend = forward;
            break;
        case ANIM_VARIANT_BACK:
            blend = back;
            break;
        case ANIM_VARIANT_LEFT:
            blend = left;
            break;
        case ANIM_VARIANT_RIGHT:
            blend = right;
            break;
        case ANIM_VARIANT_UP:
            blend = up;
            break;
        case ANIM_VARIANT_DOWN:
            blend = down;
            break;
        default:
            continue;
        }
        if (0.0f < blend_rate) {
            float current = 0.0f;
            for (int j = 0; j < vp->max_simultaneous_anims; j++) {
                if (vp->anims[j].anim_handle == variant.anim_handle) {
                    current = vp->anims[j].blend;
                    break;
                }
            }
            blend = (float)(((double)current - (double)blend) * (double)blend_rate + (double)blend);
        }
        if (blend < 0.005f) {
            blend = 0.0f;
        }
        vanim_set_state_custom_blend_pose(vp, index_alias(variant.anim_handle, ANIM_ALIAS_NONE), blend, 0);
    }
    return state;
}

// 0x007FE6A0. Variants of a v_anim (alternative animations played instead of, or blended over, it): a cyclic variant
// may start at the end of a cycle, a timed one when its frequency timestamp elapses (or at once with immediate /
// play_variant); an active variant's blend ramps in and out.
void vanim_process_variant(v_animate* vp, int va_index, vanim_variants play_variant, bool immediate)
{
    v_anim* va = &vp->anims[va_index];
    const unsigned int handle = (unsigned int)va->anim_handle;
    if (handle >= 4096 || Anims[handle].num_variants == 0) {
        return;
    }
    const anim& a = Anims[handle];
    if ((va->variant == nullptr && 0.0 < (double)a.variant_cyclic_percent / 255.0) || (immediate && va->is_state)) {
        int prev_time;
        int cur_time;
        animlib_get_animation_times(vp->aim_handle, va->instance, prev_time, cur_time);
        float length = va->is_blend_tree ? vanim_get_anim_length(vp, (int)va->instance) : animlib_get_animation_length(a.animlib_handle);
        if ((immediate || (int)(((double)length - (double)Frametime * 1.1) * 4800.0) <= cur_time)) {
            const float chance = fl_rand_range(0.0f, 1.0f);
            if (immediate || chance < (float)a.variant_cyclic_percent / 255.0f) {
                float pick = fl_rand_range(0.0f, 1.0f);
                for (int i = 0; i < a.num_variants; i++) {
                    const anim_variant& variant = a.variants[i];
                    const bool chosen = play_variant == ANIM_VARIANT_NONE ? pick < variant.cyclic_percentage : play_variant == variant.variant_id;
                    if (chosen) {
                        v_anim* pose = vanim_set_state_custom_blend_pose(vp, index_alias(variant.anim_handle, ANIM_ALIAS_NONE), 0.001f, 0);
                        va = &vp->anims[va_index];
                        if (pose == nullptr) {
                            return;
                        }
                        if (variant.playstyle == VARIANT_PLAYSTYLE_FULLBODY) {
                            pose->is_variant_fullbody = 1;
                        }
                        pose->is_variant = 1;
                        pose->variant = va;
                        anim* variant_anim = anim_by_handle(pose->anim_handle);
                        // flags 0x20: AP_PRIORITY_MEDIUM (raw float bits 2.24208e-44 in the decompile)
                        const unsigned int flags = variant.playstyle == VARIANT_PLAYSTYLE_INCREASED_PRIORITY ? 0x10u : 0u;
                        const unsigned int instance = animlib_play_animation_looping(vp->aim_handle, variant_anim->animlib_handle, 0.01f, flags, 1.0f, 0.0f);
                        pose->is_variant_cyclic = 1;
                        pose->instance = instance;
                        pose->flags = flags;
                        if (play_variant != ANIM_VARIANT_NONE && 0 < variant.time) {
                            pose->is_variant_cyclic = 0;
                            pose->variant_length_ts.set(variant.time);
                        }
                        if (!immediate) {
                            animlib_set_looping_anim_start_percent(vp->aim_handle, pose->instance, 0.0f);
                        } else {
                            const float cycle = animlib_get_init_cyclic_percent(vp->aim_handle, va->instance);
                            if (va->instance == 0 && vp->flags.next_state_start_at_zero) {
                                animlib_set_looping_anim_start_percent(vp->aim_handle, pose->instance, 0.0f);
                            } else {
                                animlib_set_init_cyclic_percent(vp->aim_handle, pose->instance, cycle);
                            }
                        }
                        if (variant.playstyle != VARIANT_PLAYSTYLE_FULLBODY) {
                            const float variant_length = animlib_get_animation_length(variant_anim->animlib_handle);
                            animlib_set_speed_multiplier(vp->aim_handle, pose->instance, length / variant_length);
                        }
                        va->variant = pose;
                        va->variant_length_ts.set(variant.time);
                        break;
                    }
                    pick = (float)((double)pick - (double)variant.cyclic_percentage);
                }
            }
        }
    }
    if ((va->variant == nullptr && va->variant_freq_ts.elapsed()) || (immediate && !va->is_state)) {
        float pick = fl_rand_range(0.0f, 1.0f);
        for (int i = 0; i < a.num_variants; i++) {
            const anim_variant& variant = a.variants[i];
            const bool chosen = pick < variant.percentage ? (play_variant == ANIM_VARIANT_NONE || play_variant == variant.variant_id) : (play_variant != ANIM_VARIANT_NONE && play_variant == variant.variant_id);
            if (chosen) {
                va->variant_length_ts.set(variant.time);
                if (!va->is_state) {
                    if (variant.playstyle != VARIANT_PLAYSTYLE_FULLBODY) {
                        break;
                    }
                    animlib_stop_animation(vp->aim_handle, va->instance, 0.0f);
                    va->anim_handle = variant.anim_handle;
                    va->instance = animlib_play_animation(vp->aim_handle, anim_get_anim_from_handle(variant.anim_handle)->animlib_handle, va->flags, 0.0f, 1.0f, 1.0f, -1.0f);
                } else {
                    v_anim* pose = vanim_set_state_custom_blend_pose(vp, index_alias(variant.anim_handle, ANIM_ALIAS_NONE), 0.001f, 0);
                    va = &vp->anims[va_index];
                    if (variant.playstyle == VARIANT_PLAYSTYLE_FULLBODY) {
                        pose->is_variant_fullbody = 1;
                    }
                    pose->is_variant = 1;
                    pose->variant = va;
                    va->variant = pose;
                }
                va->variant_length_ts.set(variant.time);
                break;
            }
            pick = (float)((double)pick - (double)variant.percentage);
        }
        if (!va->is_state || (a.variant_frequency_min == 0 && a.variant_frequency_max == 0)) {
            va->variant_freq_ts.invalidate();
        } else {
            const int high = va->variant_length_ts.time_until() + a.variant_frequency_max * 180;
            va->variant_freq_ts.set(int_rand_range(va->variant_length_ts.time_until() + a.variant_frequency_min * 180, high));
        }
    }
    v_anim* variant = va->variant;
    if (variant == nullptr || va->is_variant) {
        return;
    }
    const unsigned int variant_handle = (unsigned int)variant->anim_handle;
    if (variant_handle >= 4096) {
        va->variant = nullptr;
        return;
    }
    int animlib_handle = Anims[variant_handle].animlib_handle;
    if (variant->is_blend_tree) {
        animlib_handle = Blend_trees[animlib_handle].states[0].animlib_handle;
    }
    int ramp_in;
    int ramp_out;
    if (variant->is_variant_cyclic) {
        animlib_get_ramp_in_out_times(animlib_handle, ramp_in, ramp_out);
        int prev_time = 0;
        int cur_time = 0;
        if (variant->instance != 0) {
            animlib_get_animation_times(vp->aim_handle, variant->instance, prev_time, cur_time);
        }
        const int end = (int)(animlib_get_animation_length(animlib_handle) * 4800.0f);
        if ((cur_time < end - ramp_out && !variant->ramping_out && prev_time <= cur_time) || variant->variant_forced) {
            float blend = (float)((double)Frametime * 4800.0 / (double)(float)ramp_in + (double)variant->blend);
            if (1.0f - blend < 0.0f) {
                blend = 1.0f;
            }
            variant->blend = blend;
            variant->variant_forced = 0;
            return;
        }
        variant->ramping_out = 1;
        variant->blend = (float)((double)variant->blend - (double)Frametime * 4800.0 / (double)(float)ramp_out);
        if (0.0f < variant->blend) {
            if (prev_time <= cur_time) {
                return;
            }
            va->variant = nullptr;
            variant->blend = 0.0f;
            return;
        }
    } else {
        if (!variant->is_pose) {
            return;
        }
        animlib_get_ramp_in_out_times(animlib_handle, ramp_in, ramp_out);
        const float ramp_out_f = (float)ramp_out;
        if ((float)((double)ramp_out_f / 4800.0 * 1000.0) < (float)va->variant_length_ts.time_until()) {
            float blend = (float)((double)Frametime * 4800.0 / (double)(float)ramp_in + (double)variant->blend);
            if (1.0f - blend < 0.0f) {
                blend = 1.0f;
            }
            variant->blend = blend;
            return;
        }
        variant->blend = (float)((double)variant->blend - (double)Frametime * 4800.0 / (double)ramp_out_f);
        if (0.0f < variant->blend) {
            return;
        }
    }
    va->variant = nullptr;
    variant->blend = 0.0f;
}

// 0x007FF7F0. Forces a variant (override_anim replaces a playing one with another).
void vanim_set_variant(v_animate* vp, int anim_index, vanim_variants variant, bool immediate, bool override_anim)
{
    const int index = vanim_get_index(vp, anim_index);
    if (index < 0) {
        return;
    }
    v_anim* va = &vp->anims[index];
    v_anim* current = va->variant;
    if (current != nullptr) {
        if (!override_anim) {
            return;
        }
        const anim* a = anim_by_handle(va->anim_handle);
        int wanted = -1;
        for (int i = 0; i < a->num_variants; i++) {
            if (a->variants[i].variant_id == variant) {
                wanted = a->variants[i].anim_handle;
            }
        }
        if (current->anim_handle == wanted) {
            current->ramping_out = 0;
            va->variant->variant_forced = 1;
            return;
        }
        current->ramping_out = 1;
        va->variant->variant = nullptr;
        va->variant->is_variant = 0;
        va->variant->is_variant_cyclic = 0;
        va->variant = nullptr;
    }
    vanim_process_variant(vp, index, variant, immediate);
}

// ---- per frame

// 0x007FEFB0. Blends the states (the desired one up by the frame's share of state_blend_rate, the others down, the
// second strongest at half the rate), processes the variants, collects the editor flags and plays every state, pose
// and blend tree looping.
void vanim_process_states(v_animate* vp)
{
    vanim_internal_process_data internal_data;
    internal_data.internal_num_state_blends = 0;
    if (vp->aim_handle > -1) {
        animlib_clear_looping_animations(vp->aim_handle, false);
    }
    vp->editor_flags = 0;
    vp->ramped_editor_flags = 0;
    v_anim* states[20];
    int num_states = vanim_get_state_list(vp, states);
    if (num_states > 0 && vp->desired_state != nullptr) {
        float rate = vp->state_blend_rate == 0.0f ? 1.0f : Frametime / vp->state_blend_rate;
        bool explicit_transition = false;
        for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
            if (vp->anims[i].is_explicit_transition_action) {
                explicit_transition = true;
                if (1 < num_states) {
                    rate = animlib_get_current_mix_percent(vp->aim_handle, vp->anims[i].instance) < 1.0f ? 0.0001f : 100.0f;
                }
                break;
            }
        }
        if (!explicit_transition) {
            vp->flags.set_state_transition_ending_frame = 0;
        }
        if (2 < num_states) {
            rate = (float)(num_states - 1) * rate;
        }
        float blend = rate + vp->desired_state->blend;
        if (1.0f - blend < 0.0f) {
            blend = 1.0f;
        }
        vp->desired_state->blend = blend;
        for (int i = 0; i < num_states; i++) {
            if (states[i] == vp->desired_state) {
                num_states--;
                for (int j = i; j < num_states; j++) {
                    states[j] = states[j + 1];
                }
                break;
            }
        }
        for (int i = num_states - 1; i > -1; i--) {
            float step = rate;
            if (i == 1) {
                step = (float)((double)rate * 0.5);
            }
            v_anim* state = states[i];
            if ((float)((double)step + 0.001) <= state->blend) {
                state->blend = state->blend - step;
            } else {
                step = state->blend;
                state->blend = 0.0f;
            }
            rate = rate - step;
        }
    }
    if (vp->aim_handle > -1) {
        for (int i = 0; i < vp->max_simultaneous_anims; i++) {
            v_anim* va = &vp->anims[i];
            if (!va->used) {
                break;
            }
            anim* a = anim_by_handle(va->anim_handle);
            if (!va->is_state || va->variant_freq_ts.valid() || va->variant != nullptr) {
                vanim_process_variant(vp, i, ANIM_VARIANT_NONE, false);
            }
            va = &vp->anims[i];
            if (a != nullptr) {
                vp->editor_flags |= a->editor_flags;
                if (!animlib_anim_is_ramping_out(vp->aim_handle, va->instance)) {
                    vp->ramped_editor_flags |= a->editor_flags;
                }
            }
            if (!((va->is_state && 0.0f < va->blend) || va->is_pose)) {
                continue;
            }
            if (va->is_pose && va->ramping_out && !va->is_variant) {
                int ramp_in;
                int ramp_out;
                animlib_get_ramp_in_out_times(a->animlib_handle, ramp_in, ramp_out);
                float blend = (float)((double)va->blend - (double)Frametime * 4800.0 / (double)(float)ramp_out);
                if (blend - 0.0f < 0.0f) {
                    blend = 0.0f;
                }
                va->blend = blend;
            }
            if (va->is_blend_tree) {
                vanim_process_blend_tree(vp, internal_data, va);
                continue;
            }
            if (a == nullptr) {
                continue;
            }
            float mix = vanim_modify_state_percent(vp, internal_data, a->animlib_handle, va->blend);
            v_anim* variant = va->variant;
            if (variant != nullptr) {
                if (!variant->used) {
                    va->blend = 0.0f;
                    continue;
                }
                if (variant->is_variant_fullbody) {
                    mix = (float)((1.0 - (double)variant->blend) * (double)mix);
                }
                if (va->is_variant) {
                    mix = variant->blend * mix;
                }
            }
            if (mix != 1.0f && !va->is_pose) {
                if (mix - 0.0f < 0.0f) {
                    mix = 0.0f;
                }
                if (0.0f <= mix - 1.0f) {
                    mix = 1.0f;
                }
                mix = fl_ramp(mix, 1.0f);
            }
            va->instance = animlib_play_animation_looping(vp->aim_handle, a->animlib_handle, mix, va->flags, 1.0f, 0.0f);
        }
    }
    if (vp->flags.next_state_start_at_zero && vp->desired_state != nullptr) {
        animlib_set_looping_anim_start_percent(vp->aim_handle, vp->desired_state->instance, 0.0f);
        vp->flags.set_state_transition_ending_frame = 0;
        vp->flags.next_state_start_at_zero = 0;
    }
    if (vp->flags.set_state_transition_ending_frame && vp->desired_state != nullptr) {
        animlib_set_looping_anim_start_percent(vp->aim_handle, vp->desired_state->instance, vp->transition_ending_percent);
    }
}

// 0x007FF770. Removes the finished actions and the states and poses blended to 0.
void vanim_process_completed_actions(v_animate* vp)
{
    if (vp->aim_handle < 0) {
        return;
    }
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim& va = vp->anims[i];
        if (!va.used) {
            return;
        }
        if (va.anim_handle == -1) {
            continue;
        }
        if (!va.is_state && !va.is_pose) {
            if (animlib_is_anim_finished(vp->aim_handle, va.instance)) {
                vanim_internal_remove_anim(vp, i);
            }
        } else if (va.blend == 0.0f) {
            vanim_internal_remove_anim(vp, i);
        }
    }
}

// 0x007F7620
void vanim_ramp_out_action(v_animate* vp, int action_alias_index, int num_frames)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index > -1) {
        animlib_stop_animation(vp->aim_handle, vp->anims[index].instance, (float)num_frames / 30.0f);
    }
}

// 0x007F7670
void vanim_pause_and_ramp_out_action(v_animate* vp, int action_alias_index, int num_frames)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index > -1) {
        animlib_set_speed_multiplier(vp->aim_handle, vp->anims[index].instance, 0.0f);
        animlib_stop_animation(vp->aim_handle, vp->anims[index].instance, (float)num_frames / 30.0f);
    }
}

// 0x007F76E0. action_alias_index -1: any animation of the instance.
bool vanim_has_trigger_just_elapsed(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx)
{
    unsigned int handle = 0xFFFFFFFF;
    if (action_alias_index != -1) {
        const int index = vanim_get_index(vp, action_alias_index);
        if (index < 0) {
            return false;
        }
        handle = vp->anims[index].instance;
    }
    return animlib_trigger_just_elapsed(vp->aim_handle, trigger_id, handle, trigger_idx);
}

// 0x007F7730
bool vanim_has_trigger_elapsed(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx)
{
    unsigned int handle = 0xFFFFFFFF;
    if (action_alias_index != -1) {
        const int index = vanim_get_index(vp, action_alias_index);
        if (index < 0) {
            return false;
        }
        handle = vp->anims[index].instance;
    }
    return animlib_trigger_has_elapsed(vp->aim_handle, trigger_id, handle, trigger_idx);
}

// 0x007F7780. Seconds; a negative tick time (no such trigger) is returned as is (-1.0).
float vanim_get_trigger_time(v_animate* vp, int trigger_id, int action_alias_index, int trigger_idx)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return 0.0f;
    }
    const v_anim& va = vp->anims[index];
    int ticks;
    if (!va.is_blend_tree) {
        if ((unsigned int)va.anim_handle >= 4096) {
            return 0.0f;
        }
        ticks = animlib_get_trigger_time(trigger_id, Anims[va.anim_handle].animlib_handle, trigger_idx);
    } else {
        const int animlib_handle = animlib_get_anim_handle_from_instance_handle(vp->aim_handle, va.instance);
        if (animlib_handle < 1) {
            return 0.0f;
        }
        ticks = animlib_get_trigger_time(trigger_id, animlib_handle, trigger_idx);
    }
    const float time = (float)ticks;
    if (0.0f <= time) {
        return (float)((double)time / 4800.0);
    }
    return time;
}

// 0x007F7860. A blend tree's length is the weighted length of its active actions, else of its states.
float vanim_get_anim_length(v_animate* vp, int action_alias_index)
{
    float length = 0.0f;
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return length;
    }
    const v_anim& va = vp->anims[index];
    if (!va.is_blend_tree) {
        // (an invalid handle reads the animlib handle through a null anim, as the original)
        return animlib_get_animation_length(Anims[va.anim_handle].animlib_handle);
    }
    const anim_blend_tree_info* info = va.blend_tree_info;
    bool any = false;
    for (int i = 0; i < info->num_actions; i++) {
        const float blend = va.blend_tree_instance.actions[i].current_blend;
        if (0.0f < blend) {
            length = (float)((double)animlib_get_animation_length(info->actions[i].anim_handle) * (double)blend + (double)length);
            any = true;
        }
    }
    if (any) {
        return length;
    }
    for (int s = 0; s < info->num_states; s++) {
        const float weight = va.blend_tree_instance.state_weights[s];
        if (0.0f < weight) {
            length = (float)((double)animlib_get_animation_length(info->states[s].animlib_handle) * (double)weight + (double)length);
        }
    }
    return length;
}

// 0x007F79C0. A blend tree's strongest state.
int vanim_get_anim_handle(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return -1;
    }
    const v_anim& va = vp->anims[index];
    if (!va.is_blend_tree) {
        return va.anim_handle;
    }
    float strongest = 0.0f;
    int best = -1;
    for (int s = 0; s < va.blend_tree_info->num_states; s++) {
        if ((double)strongest < (double)va.blend_tree_instance.state_weights[s]) {
            best = s;
            strongest = va.blend_tree_instance.state_weights[s];
        }
    }
    return va.blend_tree_info->states[best].anim_handle;
}

// 0x007F7AF0. The total root translation (a blend tree's weighted by its actions, else its states).
void vanim_get_anim_translation(v_animate* vp, int index, vector& translation)
{
    const int i = vanim_get_index(vp, index);
    if (i < 0) {
        return;
    }
    const v_anim& va = vp->anims[i];
    if (!va.is_blend_tree) {
        animlib_get_total_translation(anim_by_handle(va.anim_handle)->animlib_handle, translation, -1, -1);
        return;
    }
    translation.zero();
    const anim_blend_tree_info* info = va.blend_tree_info;
    bool any = false;
    for (int a = 0; a < info->num_actions; a++) {
        const float blend = va.blend_tree_instance.actions[a].current_blend;
        if (0.0f < blend) {
            vector t;
            animlib_get_total_translation(info->actions[a].anim_handle, t, -1, -1);
            translation.x = translation.x + t.x * blend;
            translation.y = translation.y + t.y * blend;
            translation.z = translation.z + t.z * blend;
            any = true;
        }
    }
    if (any) {
        return;
    }
    for (int s = 0; s < info->num_states; s++) {
        const float weight = va.blend_tree_instance.state_weights[s];
        if (0.0f < weight) {
            vector t;
            animlib_get_total_translation(info->states[s].animlib_handle, t, -1, -1);
            translation.x = translation.x + t.x * weight;
            translation.y = translation.y + t.y * weight;
            translation.z = translation.z + t.z * weight;
        }
    }
}

// 0x007F7D80. Meters per second of a moving state (or a blend tree's first active action).
float vanim_get_anim_velocity(v_animate* vp, int index)
{
    const int i = vanim_get_index(vp, index);
    if (i < 0) {
        return 0.0f;
    }
    const v_anim& va = vp->anims[i];
    if (!va.is_blend_tree) {
        if (va.is_state) {
            vector t;
            vanim_get_anim_translation(vp, index, t);
            const float length = vanim_get_anim_length(vp, index);
            if (0.01f < length) {
                return (float)sqrt((double)(float)((double)t.z * (double)t.z + (double)t.x * (double)t.x + (double)t.y * (double)t.y)) / length;
            }
        }
        return 0.0f;
    }
    const anim_blend_tree_info* info = va.blend_tree_info;
    for (int a = 0; a < info->num_actions; a++) {
        const anim_blend_tree_instance::action_instance& instance = va.blend_tree_instance.actions[a];
        if (instance.is_active && 0.0f < instance.current_blend && instance.instance != 0xFFFFFFFF) {
            int prev_time = 0;
            int cur_time = 0;
            animlib_get_animation_times(vp->aim_handle, instance.instance, prev_time, cur_time);
            vector vel;
            animlib_get_animation_vel(vel, info->actions[a].anim_handle, prev_time, cur_time);
            return (float)sqrt((double)(float)((double)vel.z * (double)vel.z + (double)vel.x * (double)vel.x + (double)vel.y * (double)vel.y));
        }
    }
    vector t;
    vanim_get_anim_translation(vp, index, t);
    const float length = vanim_get_anim_length(vp, index);
    if (0.01f < length) {
        return (float)sqrt((double)(float)((double)t.z * (double)t.z + (double)t.x * (double)t.x + (double)t.y * (double)t.y)) / length;
    }
    return 0.0f;
}

// 0x007F7F70
int vanim_get_anim_index(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    return index > -1 ? vp->anims[index].index : -1;
}

// 0x007F7FA0
unsigned int vanim_get_anim_instance_handle(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    return index > -1 ? vp->anims[index].instance : 0xFFFFFFFF;
}

// 0x007F7FD0
bool vanim_is_in_state(v_animate* vp, int state_alias_index)
{
    const v_anim* state = vp->desired_state;
    return state != NULL && (state->index == state_alias_index || state->alias == state_alias_index || state->instance == (unsigned int)state_alias_index);
}

// 0x007F8000
bool vanim_is_playing_action(v_animate* vp, int action_alias_index)
{
    return vanim_get_index(vp, action_alias_index) > -1;
}

// 0x007F8020
bool vanim_is_action_ending_this_frame(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return false;
    }
    const unsigned int handle = (unsigned int)vp->anims[index].anim_handle;
    if (handle >= 4096) {
        return false;
    }
    const float time = animlib_get_current_time(vp->aim_handle, vp->anims[index].instance) + Frametime;
    return animlib_get_animation_length(Anims[handle].animlib_handle) < (float)((double)time + 0.0001);
}

// 0x007F80B0
bool vanim_is_moving_animation(v_animate* vp, int index)
{
    const int i = vanim_get_index(vp, index);
    if (i < 0) {
        return false;
    }
    if (!vp->anims[i].is_blend_tree) {
        const unsigned int handle = (unsigned int)vp->anims[i].anim_handle;
        return handle < 4096 && animlib_is_moving_animation(Anims[handle].animlib_handle);
    }
    vector t;
    vanim_get_anim_translation(vp, index, t);
    return !t.is_null();
}

// 0x007F8130. Blending from a moving state into a still one.
bool vanim_is_transitioning_to_still_state(v_animate* vp)
{
    if (vp->desired_state == nullptr || vp->secondary_state == nullptr || !(vp->desired_state->blend < 1.0f)) {
        return false;
    }
    const bool desired_moving = vanim_is_moving_animation(vp, VANIM_DESIRED_STATE);
    const bool secondary_moving = vanim_is_moving_animation(vp, VANIM_SECONDARY_STATE);
    return secondary_moving && !desired_moving;
}

// 0x007F8180
bool vanim_check_editor_flag(v_animate* vp, unsigned int editor_flag)
{
    return (vp->editor_flags & editor_flag) != 0;
}

// 0x007F81A0
bool vanim_check_editor_flag_ramped(v_animate* vp, unsigned int editor_flag)
{
    return (vp->ramped_editor_flags & editor_flag) != 0;
}

// 0x007F81C0
int vanim_get_anim_frame(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index > -1) {
        const int frame = animlib_get_current_frame(vp->aim_handle, vp->anims[index].instance);
        if (frame > -1) {
            return frame;
        }
    }
    return -1;
}

// 0x007F15A0 (vanim.h:34, inline in the original header). The orientation with a new up vector, keeping the forward
// direction as far as it can. The cross products are inline (each component one double expression).
void MAT_CHANGE_UVEC(matrix& orient, const vector up_vec)
{
    orient.uvec = up_vec.copy_normalize();
    const vector u = orient.uvec;
    const vector f = orient.fvec;
    orient.rvec.x = (float)((double)u.y * (double)f.z - (double)u.z * (double)f.y);
    orient.rvec.y = (float)((double)f.x * (double)u.z - (double)u.x * (double)f.z);
    orient.rvec.z = (float)((double)u.x * (double)f.y - (double)f.x * (double)u.y);
    orient.rvec.normalize();
    const vector r = orient.rvec;
    orient.fvec.x = (float)((double)u.z * (double)r.y - (double)u.y * (double)r.z);
    orient.fvec.y = (float)((double)u.x * (double)r.z - (double)u.z * (double)r.x);
    orient.fvec.z = (float)((double)u.y * (double)r.x - (double)u.x * (double)r.y);
    orient.fvec.normalize();
}

// 0x007F4F90; LTCG: vp ESI, pos EAX, i / orient / velocity on the stack. A two-character animation (the master moves
// both towards their relative offset and keeps their heights within vertical_deviation), plus the starting and end
// orientation turns.
static void vanim_two_anim_process(v_animate* vp, int i, const vector& pos, matrix& orient, vector& velocity)
{
    v_animate* other = vp->attached_object;
    v_anim& va = vp->anims[i];
    if (vp->flags.master_object) {
        if (va.move_to_starting_orient) {
            vector movement = vp->local_anim_movement;
            movement.unrotate(vp->starting_orient);
            vp->anim_pos_delta.x = vp->anim_pos_delta.x + movement.x;
            vp->anim_pos_delta.y = movement.y + vp->anim_pos_delta.y;
            vp->anim_pos_delta.z = movement.z + vp->anim_pos_delta.z;
            const double ft = (double)Frametime;
            const float target_x = vp->anim_pos_delta.x + vp->start_pos.x;
            const float target_y = vp->start_pos.y + vp->anim_pos_delta.y;
            const float next_y = (float)((double)pos.y + (double)velocity.y * ft);
            const float next_z = (float)((double)pos.z + (double)velocity.z * ft);
            velocity.x = (float)((double)velocity.x + ((double)target_x - ((double)pos.x + (double)velocity.x * ft)) / ft);
            velocity.y = (float)((double)velocity.y + ((double)target_y - (double)next_y) / ft);
            velocity.z = (float)(((double)vp->start_pos.z + (double)vp->anim_pos_delta.z - (double)next_z) / ft + (double)velocity.z);
            vp->current_pos_time = vp->current_pos_time + Frametime;
        }
        vector to_other(other->hp->pos.x - pos.x, other->hp->pos.y - pos.y, other->hp->pos.z - pos.z);
        to_other.rotate(vp->starting_orient);
        float share = 0.5f;
        if (!va.move_to_starting_orient) {
            vector offset_dir = vp->two_object_offset;
            offset_dir.mag_and_normalize();
            const float wanted = fl_atan2_safe(offset_dir.x, offset_dir.z);
            vector dir(other->hp->pos.x - pos.x, other->hp->pos.y - pos.y, other->hp->pos.z - pos.z);
            dir.mag_and_normalize();
            dir.rotate(vp->starting_orient);
            const float actual = fl_atan2_safe(dir.x, dir.z);
            const float diff = fl_angle_cap_radians(wanted - actual, -PI_F);
            const float max_turn = (float)((double)Frametime * (double)TWO_PI_F);
            float turn = -max_turn;
            if (0.0f <= diff - -max_turn) {
                turn = diff;
            }
            if (0.0f <= turn - max_turn) {
                turn = max_turn;
            }
            orient.rotate_around_local_y(turn);
            other->hp->orient.rotate_around_local_y(turn);
            vp->starting_orient.rotate_around_local_y(-turn);
            other->starting_orient.rotate_around_local_y(-turn);
        }
        vector offset = vp->two_object_offset;
        offset.unrotate(vp->starting_orient);
        vector correction((pos.x + offset.x) - other->hp->pos.x, 0.0f, (pos.z + offset.z) - other->hp->pos.z);
        if (vp->flags.first_process) {
            vp->total_pos_adjustment = (float)sqrt((double)correction.x * (double)correction.x + 0.0 + (double)correction.z * (double)correction.z);
        }
        if (!correction.is_null()) {
            if (vp->current_orient_time < vp->start_orient_time) {
                share = 0.75f;
                const double mag = sqrt((double)correction.x * (double)correction.x + (double)correction.y * (double)correction.y + (double)correction.z * (double)correction.z);
                correction.normalize();
                const float scale = (float)(mag - (((double)vp->start_orient_time - (double)Frametime - (double)vp->current_orient_time) * (double)vp->total_pos_adjustment) / (double)vp->start_orient_time);
                correction.y = scale * correction.y;
                correction.z = correction.z * scale;
                correction.x = scale * correction.x;
            }
            if (!vp->flags.first_process) {
                if (share - 0.25f < 0.0f) {
                    share = 0.25f;
                }
                if (0.0f <= share - 0.75f) {
                    share = 0.75f;
                }
            }
            const float inv_ft = 1.0f / Frametime;
            correction.x = inv_ft * correction.x;
            correction.z = correction.z * inv_ft;
            const float cy = inv_ft * correction.y;
            other->hp->velocity.x = other->hp->velocity.x + correction.x * share;
            other->hp->velocity.y = cy * share + other->hp->velocity.y;
            other->hp->velocity.z = correction.z * share + other->hp->velocity.z;
            share = 1.0f - share;
            velocity.x = velocity.x - correction.x * share;
            velocity.y = velocity.y - share * cy;
            velocity.z = velocity.z - correction.z * share;
            vp->start_pos.x = vp->start_pos.x - correction.x * Frametime;
            vp->start_pos.y = vp->start_pos.y - Frametime * cy;
            vp->start_pos.z = vp->start_pos.z - correction.z * Frametime;
        }
        vp->two_object_offset.x = vp->two_object_offset.x - vp->local_anim_movement.x;
        vp->two_object_offset.y = vp->two_object_offset.y - vp->local_anim_movement.y;
        vp->two_object_offset.z = vp->two_object_offset.z - vp->local_anim_movement.z;
        vector other_movement = other->local_anim_movement;
        other_movement.rotate(vp->starting_orient);
        other_movement.unrotate(other->starting_orient);
        vp->two_object_offset.x = vp->two_object_offset.x + other_movement.x;
        vp->two_object_offset.y = other_movement.y + vp->two_object_offset.y;
        vp->two_object_offset.z = other_movement.z + vp->two_object_offset.z;
        vp->flags.first_process = 0;
        const float low = vp->two_object_offset.y - vp->vertical_deviation;
        const float high = vp->vertical_deviation + vp->two_object_offset.y;
        const float height = other->hp->pos.y - pos.y;
        float clamped = low;
        if (0.0f <= height - low) {
            clamped = height;
        }
        if (0.0f <= clamped - high) {
            clamped = high;
        }
        if (clamped - height <= 0.0f) {
            float vy = (height - clamped) / Frametime;
            if (vy - -10.0f < 0.0f) {
                vy = -10.0f;
            }
            if (0.0f <= vy - 10.0f) {
                vy = 10.0f;
            }
            velocity.y = vy;
            vp->flags.use_velocity = 1;
        } else {
            float vy = (clamped - height) / Frametime;
            if (vy - -10.0f < 0.0f) {
                vy = -10.0f;
            }
            if (0.0f <= vy - 10.0f) {
                vy = 10.0f;
            }
            other->hp->velocity.y = vy;
            other->flags.use_velocity = 1;
        }
    }
    if (va.move_to_starting_orient) {
        float dt = Frametime;
        if (vp->start_orient_time < vp->current_orient_time + Frametime) {
            dt = vp->start_orient_time - vp->current_orient_time;
        }
        orient.rotate_around_local_y((float)((double)vp->start_heading_delta * (double)dt / (double)vp->start_orient_time));
        vp->current_orient_time = vp->current_orient_time + Frametime;
        if (vp->start_orient_time <= vp->current_orient_time) {
            va.move_to_starting_orient = 0;
        }
    }
    if (va.move_to_end_orient) {
        const float turn = (float)((double)vp->end_heading_delta * (double)Frametime / (double)vp->dest_orient_time);
        orient.rotate_around_local_y(turn);
        if (vp->flags.master_object) {
            vp->starting_orient.rotate_around_local_y(turn);
            other->hp->orient.rotate_around_local_y(turn);
            other->starting_orient.rotate_around_local_y(turn);
        }
        if (!va.move_to_starting_orient) {
            vp->current_orient_time = vp->current_orient_time + Frametime;
        }
        if (vp->dest_orient_time < vp->current_orient_time) {
            va.move_to_end_orient = 0;
        }
    }
}

// 0x007F8200 (:3375). Actions in relative space (vanim_set_action_relative_space) drive the object directly: the
// animation's own movement plus the error of the start (fading over start_orient_time) and of the end (growing from
// dest_orient_time) become a velocity and a heading. Stops at the first unused v_anim or two-object animation. Magic
// number: heading errors within 0.01 rad are ignored (double compares).
void vanim_process_obj_movements(v_animate* vp, const vector& pos, matrix& orient, vector& velocity)
{
    vp->flags.adjusting_orient = 0;
    if (vp->aim_handle == -1) {
        return;
    }
    vector action_delta;
    vector state_delta;
    vector local_delta;
    animlib_get_position_change(action_delta, state_delta, local_delta, vp->aim_handle);
    vp->local_anim_movement = local_delta;
    vp->hp->hflags.use_current_velocity = 1;
    vp->hp->hflags.use_as_final_velocity = 1;
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        const v_anim& va = vp->anims[i];
        if (!va.used || va.two_object_anim) {
            return;
        }
        if (!va.move_relative_to_obj) {
            continue;
        }
        // Original: an out of range anim handle reads through a null anim.
        const float length = animlib_get_animation_length(anim_get_anim_from_handle(va.anim_handle)->animlib_handle);
        const float cur_time = animlib_get_current_time(vp->aim_handle, va.instance);
        vp->current_orient_time = cur_time;
        float start_share = (float)(1.0 - (double)cur_time / (double)vp->start_orient_time);
        if (start_share - 0.0f < 0.0f) {
            start_share = 0.0f;
        }
        if (0.0f <= start_share - 1.0f) {
            start_share = 1.0f;
        }
        float end_share = (float)(((double)cur_time - (double)vp->dest_orient_time) / ((double)length - (double)vp->dest_orient_time));
        if (end_share - 0.0f < 0.0f) {
            end_share = 0.0f;
        }
        if (0.0f <= end_share - 1.0f) {
            end_share = 1.0f;
        }
        vp->anim_pos_delta.x = vp->anim_pos_delta.x + vp->local_anim_movement.x;
        vp->anim_pos_delta.y = vp->local_anim_movement.y + vp->anim_pos_delta.y;
        vp->anim_pos_delta.z = vp->local_anim_movement.z + vp->anim_pos_delta.z;
        vector target;
        target.x = vp->two_object_offset.x * end_share + (vp->anim_pos_delta.x - vp->current_pos_adjustment.x * start_share);
        target.y = vp->two_object_offset.y * end_share + (vp->anim_pos_delta.y - vp->current_pos_adjustment.y * start_share);
        target.z = vp->two_object_offset.z * end_share + (vp->anim_pos_delta.z - vp->current_pos_adjustment.z * start_share);
        vec_change_to_global_coords(target, vp->start_pos, vp->starting_orient);
        velocity.x = (target.x - pos.x) / Frametime;
        velocity.y = (target.y - pos.y) / Frametime;
        velocity.z = (target.z - pos.z) / Frametime;
        vp->rotate_speed = animlib_get_heading_change(vp->aim_handle, 0) + vp->rotate_speed;
        // x87 in rfg.exe: each statement one double chain, rounded on the store.
        const float without_start = (float)((double)vp->rotate_speed - (double)vp->start_heading_delta * (double)start_share);
        const float heading = (float)((double)without_start + (double)vp->end_heading_delta * (double)end_share);
        const matrix yaw(0.0f, 0.0f, heading);
        matrix delta(yaw);
        delta.unrotate(vp->starting_orient);
        delta.rotate(orient);
        const float heading_error = delta.extract_heading();
        if ((double)heading_error < -0.01 || 0.01 < (double)heading_error) {
            orient.rotate_around_local_y(heading_error);
        }
    }
}

// 0x007F8700. The movement the actions impose this frame: the up vector interpolation, then per action the move to
// an end position (velocity towards start_pos + the animation's own movement + the adjustment share), the starting
// and end orientation turns, and the two-character animations.
void vanim_process_movement_adjustments(v_animate* vp, const vector& pos, matrix& orient, vector& velocity)
{
    vp->flags.adjusting_orient = 0;
    if (vp->flags.interpolate_orient) {
        const float left = vp->interpolate_time - vp->interpolate_cur_time;
        if (Frametime <= left) {
            float t = Frametime / left;
            if (t - 0.0f < 0.0f) {
                t = 0.0f;
            }
            if (0.0f <= t - 1.0f) {
                t = 1.0f;
            }
            vector up = orient.uvec;
            const vector& dest = vp->interpolate_dest_uvec;
            const vector axis((float)((double)up.y * (double)dest.z - (double)dest.y * (double)up.z),
                              (float)((double)dest.x * (double)up.z - (double)dest.z * (double)up.x),
                              (float)((double)dest.y * (double)up.x - (double)up.y * (double)dest.x));
            up.rotate_around_vector(axis, (float)(((double)up.x * (double)dest.x + (double)dest.y * (double)orient.uvec.y + (double)orient.uvec.z * (double)dest.z) * (double)t));
            MAT_CHANGE_UVEC(orient, up);
            vp->interpolate_cur_time = vp->interpolate_cur_time + Frametime;
        } else {
            MAT_CHANGE_UVEC(orient, vp->interpolate_dest_uvec);
            vp->flags.interpolate_orient = 0;
        }
    }
    if (vp->flags.move_relative_to_obj || vp->aim_handle == -1) {
        return;
    }
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim& va = vp->anims[i];
        if (!va.used) {
            continue;
        }
        if (va.two_object_anim) {
            vanim_two_anim_process(vp, i, pos, orient, velocity);
            continue;
        }
        if (va.move_relative_to_obj) {
            vp->flags.move_relative_to_obj = 1;
            return;
        }
        const float speed = animlib_get_current_speed_multiplier(vp->aim_handle, va.instance);
        if (speed == 0.0f) {
            continue;
        }
        const float dt = Frametime * speed;
        const float inv_dt = (float)(1.0 / (double)Frametime * (double)speed);
        if (va.move_to_end_pos) {
            if (!va.move_to_end_pos_post_process) {
                const float total_time = vp->dest_pos_time;
                float step = dt;
                if (total_time - vp->current_pos_time < dt) {
                    step = total_time - vp->current_pos_time;
                }
                vp->anim_pos_delta.x = (float)((double)step * (double)vp->pos_adjustment_delta.x / (double)total_time + (double)vp->anim_pos_delta.x);
                vp->anim_pos_delta.y = (float)((double)vp->anim_pos_delta.y + (double)vp->pos_adjustment_delta.y * (double)step / (double)total_time);
                vp->anim_pos_delta.z = (float)((double)vp->anim_pos_delta.z + (double)vp->pos_adjustment_delta.z * (double)step / (double)total_time);
            }
            const float vel_y = velocity.y;
            vp->current_pos_adjustment.x = vp->current_pos_adjustment.x + vp->local_anim_movement.x;
            vp->current_pos_adjustment.y = vp->local_anim_movement.y + vp->current_pos_adjustment.y;
            vp->current_pos_adjustment.z = vp->local_anim_movement.z + vp->current_pos_adjustment.z;
            vector movement = vp->local_anim_movement;
            movement.unrotate(vp->starting_orient);
            vp->anim_pos_delta.x = movement.x + vp->anim_pos_delta.x;
            vp->anim_pos_delta.y = vp->anim_pos_delta.y + movement.y;
            vp->anim_pos_delta.z = vp->anim_pos_delta.z + movement.z;
            velocity.x = (float)(((double)vp->start_pos.x + (double)vp->anim_pos_delta.x - (double)pos.x) * (double)inv_dt);
            velocity.y = (float)(((double)vp->start_pos.y + (double)vp->anim_pos_delta.y - (double)pos.y) * (double)inv_dt);
            velocity.z = (float)(((double)vp->start_pos.z + (double)vp->anim_pos_delta.z - (double)pos.z) * (double)inv_dt);
            vp->flags.use_velocity = 1;
            if (va.ignore_y) {
                velocity.y = vel_y;
            }
            vp->current_pos_time = dt + vp->current_pos_time;
            if (vp->dest_pos_time < vp->current_pos_time) {
                va.move_to_end_pos_post_process = 1;
            }
        }
        if (va.move_to_starting_orient) {
            const float vel_y = velocity.y;
            if (!va.move_to_end_pos) {
                vp->current_pos_adjustment.x = vp->current_pos_adjustment.x + vp->local_anim_movement.x;
                vp->current_pos_adjustment.y = vp->local_anim_movement.y + vp->current_pos_adjustment.y;
                vp->current_pos_adjustment.z = vp->local_anim_movement.z + vp->current_pos_adjustment.z;
                vector movement = vp->local_anim_movement;
                movement.unrotate(vp->starting_orient);
                vp->anim_pos_delta.x = movement.x + vp->anim_pos_delta.x;
                vp->anim_pos_delta.y = vp->anim_pos_delta.y + movement.y;
                vp->anim_pos_delta.z = vp->anim_pos_delta.z + movement.z;
                velocity.x = (float)(((double)vp->start_pos.x + (double)vp->anim_pos_delta.x - (double)pos.x) * (double)inv_dt);
                velocity.y = (float)(((double)vp->start_pos.y + (double)vp->anim_pos_delta.y - (double)pos.y) * (double)inv_dt);
                velocity.z = (float)(((double)vp->start_pos.z + (double)vp->anim_pos_delta.z - (double)pos.z) * (double)inv_dt);
                vp->flags.use_velocity = 1;
            }
            if (va.ignore_y) {
                velocity.y = vel_y;
            }
            const float total = vp->start_orient_time;
            float turn;
            if (vp->current_orient_time + dt <= total) {
                turn = vp->start_heading_delta * dt;
            } else {
                turn = (total - vp->current_orient_time) * vp->start_heading_delta;
            }
            orient.rotate_around_local_y(turn / total);
            vp->current_orient_time = vp->current_orient_time + dt;
            if (vp->current_orient_time <= vp->start_orient_time) {
                vp->flags.adjusting_orient = 1;
            } else {
                va.move_to_starting_orient = 0;
            }
        }
        if (va.move_to_end_orient) {
            float turn;
            if (vp->dest_orient_time <= 0.0f) {
                turn = vp->end_heading_delta;
            } else {
                turn = (float)((double)vp->end_heading_delta * (double)dt / (double)vp->dest_orient_time);
            }
            orient.rotate_around_local_y(turn);
            if (!va.move_to_starting_orient) {
                vp->current_orient_time = vp->current_orient_time + dt;
            }
            if (vp->current_orient_time <= vp->dest_orient_time) {
                vp->flags.adjusting_orient = 1;
            } else {
                va.move_to_end_orient = 0;
            }
        }
    }
}

// 0x007F9590. Another action in the v_anim's slot (anim_lib swaps it in place).
void vanim_swap_animation(v_animate* vp, int anim_index, index_alias action_index)
{
    const int index = vanim_get_index(vp, anim_index);
    if (index < 0 || action_index.index == -1 || vp->animation_set == nullptr) {
        return;
    }
    const int handle = vanim_find_anim_handle(anim_set_index(vp->animation_set), vp->default_group_index, action_index.index);
    if (handle < 0) {
        return;
    }
    v_anim& va = vp->anims[index];
    const int old_handle = va.anim_handle;
    va.anim_handle = handle;
    if ((unsigned int)handle < 4096) {
        if (action_index.alias != ANIM_ALIAS_NONE) {
            va.alias = action_index.alias;
        }
        if (old_handle != handle) {
            va.instance = animlib_swap_animation(vp->aim_handle, va.instance, Anims[handle].animlib_handle);
        }
    }
}

// 0x007F9640. Moves the character so the action ends at world_pos (over time seconds, the action's length by default).
void vanim_set_action_dest_position(v_animate* vp, vector& current_pos, matrix& current_orient, vector world_pos, int action_alias_index, float time, bool ignore_y)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    const unsigned int handle = (unsigned int)vp->anims[index].anim_handle;
    if (handle >= 4096) {
        return;
    }
    vp->start_pos = current_pos;
    vp->starting_orient = current_orient;
    vp->anim_pos_delta.zero();
    vp->current_pos_adjustment.zero();
    vp->anims[index].move_to_end_pos = 1;
    vp->anims[index].move_to_end_pos_post_process = 0;
    vp->anims[index].ignore_y = ignore_y ? 1 : 0;
    vp->flags.ignore_y = ignore_y ? 1 : 0;
    vp->current_pos_time = 0.0f;
    if (time <= 0.0f) {
        vp->dest_pos_time = animlib_get_animation_length(Anims[handle].animlib_handle);
        if (vp->dest_pos_time == 0.0f) {
            vp->dest_pos_time = (float)((double)vp->dest_pos_time + 0.05);
        }
    } else {
        vp->dest_pos_time = time;
    }
    vector translation;
    animlib_get_total_translation(Anims[handle].animlib_handle, translation, -1, -1);
    translation.unrotate(current_orient);
    vp->pos_adjustment_delta.x = (float)((double)world_pos.x - ((double)current_pos.x + (double)translation.x));
    vp->pos_adjustment_delta.y = (float)((double)world_pos.y - ((double)current_pos.y + (double)translation.y));
    vp->pos_adjustment_delta.z = (float)((double)world_pos.z - ((double)current_pos.z + (double)translation.z));
}

// 0x007F9890. Turns the character so the action ends facing dest_orient (or sets it at once when close enough).
void vanim_set_action_dest_orient(v_animate* vp, matrix& current_orient, const matrix& dest_orient, int action_alias_index, float time)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    anim* a = anim_by_handle(vp->anims[index].anim_handle);
    if (time < 0.0f) {
        if (a != nullptr) {
            vp->dest_orient_time = animlib_get_animation_length(a->animlib_handle);
        }
    } else {
        vp->dest_orient_time = time;
    }
    vp->current_orient_time = 0.0f;
    float dest_heading;
    dest_orient.extract_heading(dest_heading);
    const float anim_rotation = a != nullptr ? animlib_get_total_rotation(a->animlib_handle) : 0.0f;
    float current_heading;
    (vp->anims[index].move_to_starting_orient ? vp->starting_orient : current_orient).extract_heading(current_heading);
    vp->end_heading_delta = fl_angle_cap_radians((float)((double)dest_heading - ((double)current_heading + (double)anim_rotation)), -PI_F);
    if (0.01f < fabsf(vp->end_heading_delta)) {
        vp->flags.adjusting_orient = 1;
        vp->anims[index].move_to_end_orient = 1;
        return;
    }
    current_orient = dest_orient;
    vp->anims[index].move_to_end_orient = 0;
}

// 0x007FF8E0. Turns and moves the character to the action's starting orientation and position over time seconds.
void vanim_set_action_starting_orient(v_animate* vp, vector& current_pos, matrix& current_orient, vector& desired_starting_pos, matrix& desired_starting_orient, int action_alias_index, float time)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    const unsigned int handle = (unsigned int)vp->anims[index].anim_handle;
    if (handle >= 4096) {
        return;
    }
    vp->start_orient_time = time < 0.0f ? animlib_get_animation_length(Anims[handle].animlib_handle) : time;
    vp->current_orient_time = 0.0f;
    if (vp->hp != nullptr) {
        vanim_interpolate_uvec(vp, vp->hp, desired_starting_orient.uvec, vp->start_orient_time);
    }
    float desired_heading;
    float current_heading;
    desired_starting_orient.extract_heading(desired_heading);
    current_orient.extract_heading(current_heading);
    vp->start_heading_delta = desired_heading - current_heading;
    vp->start_heading_delta = fl_angle_cap_radians(desired_heading - current_heading, -PI_F);
    if (fabsf(vp->start_heading_delta) <= 0.01f) {
        current_orient = desired_starting_orient;
        vp->anims[index].move_to_starting_orient = 0;
    } else {
        vp->flags.adjusting_orient = 1;
        vp->anims[index].move_to_starting_orient = 1;
    }
    vp->start_pos = current_pos;
    vp->anim_pos_delta.zero();
    vp->current_pos_adjustment.zero();
    vp->starting_orient = desired_starting_orient;
    if (desired_starting_pos.x != current_pos.x || desired_starting_pos.y != current_pos.y || desired_starting_pos.z != current_pos.z) {
        vp->anims[index].move_to_end_pos = 1;
        vp->pos_adjustment_delta.x = desired_starting_pos.x - current_pos.x;
        vp->pos_adjustment_delta.y = desired_starting_pos.y - current_pos.y;
        vp->pos_adjustment_delta.z = desired_starting_pos.z - current_pos.z;
        vp->anims[index].move_to_end_pos_post_process = 0;
        vp->anims[index].ignore_y = 0;
        vp->flags.ignore_y = 0;
        vp->current_pos_time = 0.0f;
        vp->dest_pos_time = time;
        return;
    }
    vp->pos_adjustment_delta.zero();
    vp->anims[index].ignore_y = 1;
    vp->flags.ignore_y = 1;
}

// 0x007FFC10 (:3921). Plays the action relative to its ideal start (null parameters end that). The object's error from
// the ideal start and the error of the animation's end from real_end are stored for vanim_process_obj_movements; a
// negative start_err_corr_length means the whole animation, a negative end_err_corr_begin_time counts from its end.
// Magic numbers: pi as rfg.exe's float; with under 0.01 of movement squared and of heading there is no end correction.
// real_end_pos and real_end_orient of the parameters are changed in place. Original: two_object_offset uses
// pos_adjustment_delta before should_initialize reads the animation's total translation into it.
void vanim_set_action_relative_space(v_animate* vp, int action_alias_index, vanim_play_parameters* vpp)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    v_anim& va = vp->anims[index];
    const anim* animp = anim_get_anim_from_handle(va.anim_handle);
    if (animp == nullptr) {
        return;
    }
    if (vpp == nullptr) {
        va.move_relative_to_obj = 0;
        vp->flags.move_relative_to_obj = 0;
        return;
    }
    va.move_relative_to_obj = 1;
    vp->flags.ignore_y = 0;
    vp->flags.move_relative_to_obj = 1;
    vector current_pos = vp->hp->pos;
    matrix current_orient = vp->hp->orient;
    if (vpp->should_initialize) {
        vp->start_orient_time = vpp->start_err_corr_length;
        if (vp->start_orient_time < 0.0f) {
            vp->start_orient_time = animlib_get_animation_length(animp->animlib_handle);
        }
        vp->dest_orient_time = vpp->end_err_corr_begin_time;
        if (vp->dest_orient_time < 0.0f) {
            vp->dest_orient_time = animlib_get_animation_length(animp->animlib_handle) + vp->dest_orient_time;
        }
        vp->rotate_speed = 0.0f;
        vp->anim_pos_delta = ZERO_VECTOR;
        vp->current_orient_time = 0.0f;
        vanim_interpolate_uvec(vp, vp->hp, vpp->ideal_start_orient.uvec, vp->start_orient_time);
    }
    vp->start_pos = vpp->ideal_start_pos;
    vp->starting_orient = vpp->ideal_start_orient;
    vec_change_to_local_coords(vpp->real_end_pos, vp->start_pos, vp->starting_orient);
    vec_change_to_local_coords(current_pos, vp->start_pos, vp->starting_orient);
    const matrix to_local = vp->starting_orient.copy_inverse();
    vpp->real_end_orient = vpp->real_end_orient * to_local;
    current_orient = to_local * current_orient;
    const float current_heading = current_orient.extract_heading();
    const float total_rotation = animlib_get_total_rotation(animp->animlib_handle);
    const float end_heading = vpp->real_end_orient.extract_heading();
    vp->two_object_offset.x = vpp->real_end_pos.x - vp->pos_adjustment_delta.x;
    vp->two_object_offset.y = vpp->real_end_pos.y - vp->pos_adjustment_delta.y;
    vp->two_object_offset.z = vpp->real_end_pos.z - vp->pos_adjustment_delta.z;
    vp->end_heading_delta = fl_angle_cap_radians(end_heading - total_rotation, -PI_F);
    if (vpp->should_initialize) {
        vp->current_pos_adjustment.x = -current_pos.x;
        vp->current_pos_adjustment.y = -current_pos.y;
        vp->current_pos_adjustment.z = -current_pos.z;
        vp->start_heading_delta = fl_angle_cap_radians(0.0f - current_heading, -PI_F);
        animlib_get_total_translation(animp->animlib_handle, vp->pos_adjustment_delta, -1, -1);
        const vector& t = vp->pos_adjustment_delta;
        const float moved = (float)((double)t.y * (double)t.y + (double)t.x * (double)t.x + (double)t.z * (double)t.z);
        if (moved < 0.01f && fabsf(vp->end_heading_delta) < 0.01f) {
            vp->dest_orient_time = 0.0f;
        }
    }
}

// 0x007F9A40. Turns the human's up vector to desired_uvec over time seconds (at once when time is 0 or the
// orientation is not valid).
void vanim_interpolate_uvec(v_animate* vp, human* hp, const vector& desired_uvec, const float time)
{
    if (hp == nullptr) {
        return;
    }
    if (hp->orient.is_valid() && !vec_equal_robust(hp->orient.uvec, desired_uvec, 0.001f) && 0.0f < time) {
        vp->hp = hp;
        vp->interpolate_dest_uvec = desired_uvec;
        vp->interpolate_dest_uvec.normalize();
        vp->flags.interpolate_orient = 1;
        vp->interpolate_time = time;
        vp->interpolate_cur_time = 0.0f;
        return;
    }
    MAT_CHANGE_UVEC(hp->orient, desired_uvec);
    vp->flags.interpolate_orient = 0;
}

// 0x007F9B10 (:4044). Locks two actors' actions together; hp_1 is the master. Original bugs kept: hp_2's starting
// orientation test uses hp_1's action index on hp_2's v_anims, and index_2 is not checked.
void vanim_attach_actions(human* hp_1, human* hp_2, int action_alias_index_1, int action_alias_index_2, const vector& initial_offset, float vertical_deviation)
{
    v_animate* vp_1 = hp_1->char_inst->vp;
    v_animate* vp_2 = hp_2->char_inst->vp;
    vp_1->hp = hp_1;
    vp_2->hp = hp_2;
    const int index_1 = vanim_get_index(vp_1, action_alias_index_1);
    const int index_2 = vanim_get_index(vp_2, action_alias_index_2);
    if (index_1 < 0) {
        return;
    }
    if (!vp_1->anims[index_1].move_to_starting_orient) {
        vp_1->starting_orient = hp_1->orient;
    }
    if (!vp_2->anims[index_1].move_to_starting_orient) {
        vp_2->starting_orient = hp_2->orient;
    }
    vp_1->flags.master_object = 1;
    vp_2->flags.master_object = 0;
    vp_1->attached_object = vp_2;
    vp_2->attached_object = vp_1;
    vp_1->vertical_deviation = vertical_deviation;
    vp_1->anims[index_1].two_object_anim = 1;
    vp_2->anims[index_2].two_object_anim = 1;
    vp_1->two_object_offset = initial_offset;
    vp_1->flags.first_process = 1;
    vp_1->flags.ignore_y = 0;
    vp_2->flags.ignore_y = 0;
    vp_1->rotate_speed = 0.0f;
}

// 0x007F9C80. Ends a two-character animation on both sides.
void vanim_detach_actions(v_animate* vp)
{
    if (vp->attached_object == nullptr) {
        return;
    }
    vp->flags.master_object = 0;
    vp->attached_object->flags.master_object = 0;
    for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++) {
        unsigned int& bits = va_bits(vp->anims[i]);
        if (bits & VA_TWO_OBJECT_ANIM) {
            bits &= ~(VA_TWO_OBJECT_ANIM | VA_MOVE_TO_STARTING_ORIENT | VA_MOVE_TO_END_POS_POST_PROCESS | VA_MOVE_TO_END_POS | VA_MOVE_TO_END_ORIENT);
        }
    }
    // (the attached object's list is walked with this object's count, as the original)
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim* anims = vp->attached_object->anims;
        if (anims == nullptr || !anims[i].used) {
            break;
        }
        unsigned int& bits = va_bits(anims[i]);
        if (bits & VA_TWO_OBJECT_ANIM) {
            bits &= ~(VA_TWO_OBJECT_ANIM | VA_MOVE_TO_STARTING_ORIENT | VA_MOVE_TO_END_POS_POST_PROCESS | VA_MOVE_TO_END_POS | VA_MOVE_TO_END_ORIENT);
        }
    }
    vp->attached_object->attached_object = nullptr;
    vp->attached_object = nullptr;
}

// 0x007F9E20 (:4133): empty in rfg.exe.
void vanim_blend_animations_forming_box(v_animate* vp, int* state_handles, float x, float y)
{
}

// 0x007F9E30 (:4156): empty in rfg.exe.
void vanim_blend_states_linear(v_animate* vp, int state_handle_1, int state_handle_2, float x)
{
}

// 0x007F9E40 (:4169): the state with that rank by blend. Magic number: room for 20 states.
v_anim* vanim_get_state(v_animate* vp, state_enum state)
{
    v_anim* states[20] = {};
    vanim_get_state_list(vp, states);
    switch (state) {
    case VANIM_STATE_HIGHEST_BLEND:
        return states[0];
    case VANIM_STATE_SECOND_HIGHEST_BLEND:
        return states[1];
    case VANIM_STATE_THIRD:
        return states[2];
    case VANIM_STATE_FOURTH:
        return states[3];
    case VANIM_STATE_FIFTH:
        return states[4];
    default:
        return nullptr;
    }
}

// 0x007F9EC0
v_anim* vanim_get_action(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    return index > -1 ? &vp->anims[index] : nullptr;
}

// 0x007F9EF0 (:4212). By anim_percent of the animation, or anim_time seconds with a negative percent. Magic number: 30
// fps frames.
void vanim_set_start_time(v_animate* vp, int action_alias_index, float anim_time, float anim_percent)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    const v_anim& va = vp->anims[index];
    const anim* animp = anim_get_anim_from_handle(va.anim_handle);
    if (animp == nullptr) {
        return;
    }
    if (anim_percent < 0.0f) {
        animlib_set_start_frame(vp->aim_handle, va.instance, (int)((double)anim_time * 30.0));
        return;
    }
    const float length = animlib_get_animation_length(animp->animlib_handle);
    animlib_set_start_frame(vp->aim_handle, va.instance, (int)((double)length * (double)anim_percent * 30.0));
}

// 0x007F9FB0 (:4229): seconds, -1 when the action is not playing.
float vanim_get_cur_time(v_animate* vp, int action_alias_index)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index > -1 && anim_get_anim_from_handle(vp->anims[index].anim_handle) != nullptr) {
        return animlib_get_current_time(vp->aim_handle, vp->anims[index].instance);
    }
    return -1.0f;
}

// 0x007FA000 (:4243): a negative anim_time keeps the time.
void vanim_set_cur_time_and_speed(v_animate* vp, int action_alias_index, float anim_time, float anim_speed)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0 || anim_get_anim_from_handle(vp->anims[index].anim_handle) == nullptr) {
        return;
    }
    if (0.0f <= anim_time) {
        animlib_set_animation_time(vp->aim_handle, vp->anims[index].instance, anim_time);
    }
    animlib_set_speed_multiplier(vp->aim_handle, vp->anims[index].instance, anim_speed);
}

// 0x007FA080
prop_type vanim_get_prop_type_from_name(char* prop_type_name)
{
    for (int i = 0; i < 20; i++) {
        if (strcmp(prop_type_name, Prop_type_names[i]) == 0) {
            return (prop_type)i;
        }
        if (i == PROP_TYPE_BACK) {
            // "prop trigger type: %s doesn't exist" is formatted and not shown
            return PROP_TYPE_NONE;
        }
    }
    return PROP_TYPE_NONE;
}

// 0x007FA120
anim_set* anim_set_find(const char* name)
{
    for (int i = 0; i < Num_anim_sets; i++) {
        if (v_stricmp(Anim_sets[i].name, name) == 0) {
            return &Anim_sets[i];
        }
    }
    return nullptr;
}

// 0x007FA170; LTCG: base_anim_set EAX, variant_type on the stack. The variant set of a set or its parents.
static anim_set* vanim_get_variant_anim_set(const anim_set* base_anim_set, anim_set_variants variant_type)
{
    if (variant_type > ANIM_SET_VARIANT_ON_FIRE) {
        return nullptr;
    }
    for (; base_anim_set != nullptr; base_anim_set = base_anim_set->parent) {
        for (int i = 0; i < Num_anim_sets; i++) {
            if (Anim_sets[i].parent == base_anim_set && Anim_sets[i].variant_type == variant_type) {
                return &Anim_sets[i];
            }
        }
    }
    return nullptr;
}

// 0x007FA1C0 (:4682); LTCG: vp ESI, variant_type EAX. No callers left in rfg.exe (vanim_set_temp_animation_set_variant_internal
// has its own copy). Magic number: 12, the last variant.
static void vanim_set_base_animation_set_variant_internal(v_animate* vp, anim_set_variants variant_type)
{
    if ((unsigned int)(variant_type + 1) > 12) {
        return;
    }
    vp->base_anim_set_variant = variant_type;
    if (variant_type == ANIM_SET_VARIANT_NONE) {
        vp->animation_set = vp->base_animation_set;
        return;
    }
    anim_set* set = vanim_get_variant_anim_set(vp->base_animation_set, variant_type);
    if (set == nullptr) {
        vp->animation_set = vp->base_animation_set;
        vp->base_anim_set_variant = ANIM_SET_VARIANT_NONE;
        return;
    }
    vp->animation_set = set;
}

// 0x007FA200; LTCG: vp ESI, variant_type EAX. A temporary set variant over the base variant.
static void vanim_set_temp_animation_set_variant_internal(v_animate* vp, anim_set_variants variant_type)
{
    vp->temp_anim_set_variant = variant_type;
    anim_set_variants base_variant;
    if (variant_type == ANIM_SET_VARIANT_NONE) {
        base_variant = vp->base_anim_set_variant;
    } else {
        anim_set* set = vanim_get_variant_anim_set(vp->animation_set, variant_type);
        if (set != nullptr) {
            vp->animation_set = set;
            return;
        }
        base_variant = vp->base_anim_set_variant;
        vp->temp_anim_set_variant = ANIM_SET_VARIANT_NONE;
    }
    if ((unsigned int)(base_variant + 1) > 12) {
        return;
    }
    if (base_variant == ANIM_SET_VARIANT_NONE) {
        vp->animation_set = vp->base_animation_set;
        return;
    }
    anim_set* set = vanim_get_variant_anim_set(vp->base_animation_set, base_variant);
    if (set == nullptr) {
        vp->animation_set = vp->base_animation_set;
        vp->base_anim_set_variant = ANIM_SET_VARIANT_NONE;
        return;
    }
    vp->animation_set = set;
}

// 0x007FA270
void vanim_set_base_animation_set_variant(v_animate* vp, anim_set_variants variant_type)
{
    if (vp->base_anim_set_variant == variant_type) {
        return;
    }
    if ((unsigned int)(variant_type + 1) < 13) {
        vp->base_anim_set_variant = variant_type;
        if (variant_type == ANIM_SET_VARIANT_NONE) {
            vp->animation_set = vp->base_animation_set;
        } else {
            anim_set* set = vanim_get_variant_anim_set(vp->base_animation_set, variant_type);
            if (set == nullptr) {
                vp->base_anim_set_variant = ANIM_SET_VARIANT_NONE;
                vp->animation_set = vp->base_animation_set;
            } else {
                vp->animation_set = set;
            }
        }
    }
    if (vp->temp_anim_set_variant != ANIM_SET_VARIANT_NONE) {
        vanim_set_temp_animation_set_variant_internal(vp, vp->temp_anim_set_variant);
    }
}

// 0x007FA2D0
void vanim_set_temp_animation_set_variant(v_animate* vp, anim_set_variants variant_type)
{
    if ((unsigned int)(variant_type + 1) <= 12 && vp->temp_anim_set_variant != variant_type) {
        vanim_set_temp_animation_set_variant_internal(vp, variant_type);
    }
}

// 0x007FA2F0
anim_set_variants vanim_get_base_animation_set_variant(const v_animate* vp)
{
    return vp->base_anim_set_variant;
}

// 0x007FA300
anim_set_variants vanim_get_temp_animation_set_variant(const v_animate* vp)
{
    return vp->temp_anim_set_variant;
}

// 0x007FA310
anim_set_variants vanim_get_animation_set_variant_from_name(const char* name)
{
    for (int i = 0; i < 12; i++) {
        if (v_stricmp(name, Anim_set_variant_names[i]) == 0) {
            return (anim_set_variants)i;
        }
    }
    return ANIM_SET_VARIANT_NONE;
}

// 0x007FA350
void vanim_set_thread_ownership()
{
    V_anim_data_pool.set_thread_ownership();
}

// 0x00800B60 (:4611): every state anim is removed (vanim_internal_remove_anim inlined); the anim moved into a freed
// slot is not looked at again.
void vanim_clear_states(v_animate* vp)
{
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        if (vp->anims[i].is_state) {
            vanim_internal_remove_anim(vp, i);
        }
    }
}

#else // OPENRFG_AUDIO

#include "volition/rfg/code/geom/vanim.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/weapons/weapons.h"

int vanim_get_index(v_animate* vp, int index_alias_instance); // 0x007F4E70, the rfg library's half

// Inlined four times in execute_prop_trigger (helper inferred): the first child of objp that is an item prop of the
// type, or null. The children are a ring.
static item* vanim_find_prop_child(object* objp, prop_type type)
{
    object* first = objp->child_ptr;
    for (object* child = first; child != nullptr;) {
        if (child->get_obj_type() == OT_ITEM && ((item*)child)->m_prop_type == type) {
            return (item*)child;
        }
        child = child->child_next;
        if (child == first) {
            break;
        }
    }
    return nullptr;
}

// 0x00800070 (:4259). A prop transfer: the source (the owner, or its child prop of owner_child) creates props of the
// trigger's type on the destination (dest_owner, or its child prop of dest_owner_child), deletes its prop of that type,
// or moves it to the destination or drops it into the world. The destination tag is the human's main or off hand tag
// or a named tag (an item's tag, or a character tag or bone).
// Original: the owners other than item, self and parent leave the source null, and a world destination leaves the
// destination null; the child searches, the tags and the actions dereference them without a check (the tables never
// combine them).
void execute_prop_trigger(prop_trigger* ptrig, object* objp)
{
    object* src = nullptr;
    if (ptrig->owner == PROP_TRANSFER_OWNER_ITEM) {
        src = ((human*)objp)->get_equipped_weapon();
        if (src == nullptr) {
            return;
        }
    } else if (ptrig->owner == PROP_TRANSFER_OWNER_SELF) {
        src = objp;
    } else if (ptrig->owner == PROP_TRANSFER_OWNER_PARENT) {
        src = World.object_from_handle(objp->get_parent());
        if (src == nullptr) {
            return;
        }
    }
    if (ptrig->owner_child != PROP_TYPE_NONE) {
        item* child = vanim_find_prop_child(src, ptrig->owner_child);
        if (child != nullptr) {
            src = child;
        }
    }

    object* dest = nullptr;
    bool to_world = false;
    switch (ptrig->dest_owner) {
    case PROP_TRANSFER_OWNER_ITEM:
        dest = ((human*)objp)->get_equipped_weapon();
        if (dest == nullptr) {
            return;
        }
        break;
    case PROP_TRANSFER_OWNER_SELF:
        dest = objp;
        break;
    case PROP_TRANSFER_OWNER_PARENT:
        dest = World.object_from_handle(objp->get_parent());
        if (dest == nullptr) {
            return;
        }
        break;
    case PROP_TRANSFER_OWNER_WORLD:
        to_world = true;
        break;
    default:
        break;
    }
    if (ptrig->dest_owner_child != PROP_TYPE_NONE) {
        item* child = vanim_find_prop_child(dest, ptrig->dest_owner_child);
        if (child != nullptr) {
            dest = child;
        }
    }

    int parent_prop_point = -1;
    if (ptrig->dest_tag == PROP_TRANSFER_TAG_MAINHAND) {
        parent_prop_point = ((human*)dest)->equip_tag_index;
    } else if (ptrig->dest_tag == PROP_TRANSFER_TAG_OFFHAND) {
        parent_prop_point = ((human*)dest)->equip_offhand_tag_index;
    } else if (ptrig->dest_tag == PROP_TRANSFER_TAG_NAMED) {
        if (dest->get_obj_type() == OT_ITEM) {
            parent_prop_point = dest->get_tag_index_from_name(ptrig->dest_tag_name);
        } else if (dest->get_obj_type() == OT_HUMAN) {
            const int aim_handle = ((human*)dest)->char_inst->vp->aim_handle;
            parent_prop_point = animlib_find_character_tag(aim_handle, ptrig->dest_tag_name);
            if (parent_prop_point == -1) {
                parent_prop_point = animlib_find_character_bone(((human*)dest)->char_inst->vp->aim_handle, ptrig->dest_tag_name);
            }
        }
    }

    if (ptrig->action == PROP_TRANSFER_ACTION_CREATE) {
        if (src->get_obj_type() == OT_ITEM) {
            // every prop of the source's item info with the trigger's type (the info is read again each pass)
            for (int i = 0; i < ((item*)src)->info->num_props; i++) {
                const item_info::item_prop& prop = ((item*)src)->info->props[i];
                if (prop.type != ptrig->type) {
                    continue;
                }
                item_info* prop_info = object_item_get_info(prop.prop_name);
                obj_item_flags flags;
                obj_item_props props(prop_info, dest->pos, dest->orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
                item* prop_item = World.create_object<item>(props, 0xFFFFFFFF);
                if (prop_item != nullptr) {
                    prop_item->m_prop_type = ptrig->type;
                    dest->set_child(prop_item, parent_prop_point, -1, false, true);
                }
            }
        } else if (src->get_obj_type() == OT_HUMAN && ptrig->type == PROP_TYPE_CODE_DESIGNATED) {
            // the human's code designated prop (anim_prop_info, set by whoever plays the animation)
            obj_item_flags flags;
            obj_item_props props(((human*)src)->anim_prop_info, dest->pos, dest->orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
            item* prop_item = World.create_object<item>(props, 0xFFFFFFFF);
            if (prop_item != nullptr) {
                prop_item->m_prop_type = ptrig->type;
                dest->set_child(prop_item, parent_prop_point, -1, false, true);
            }
        }
    }
    if (ptrig->action == PROP_TRANSFER_ACTION_DELETE) {
        item* child = vanim_find_prop_child(src, ptrig->type);
        if (child != nullptr) {
            World.destroy_object(child, false);
        }
    }
    if (ptrig->action == PROP_TRANSFER_ACTION_MOVE) {
        item* child = vanim_find_prop_child(src, ptrig->type);
        if (child == nullptr) {
            return;
        }
        src->remove_child(child);
        if (!to_world) {
            dest->set_child(child, parent_prop_point, -1, false, true);
            return;
        }
        if (child->rdd.is_loaded()) {
            child->unset_owner(true);
        }
    }
}

// 0x00800460 (:4464). The prop triggers of an action that have not passed yet run now (a cancelled reload leaves the
// magazine where the whole action would), and the action's prop triggers are done.
void vanim_fast_forward_prop_triggers(v_animate* vp, int action_alias_index, object* objp)
{
    const int index = vanim_get_index(vp, action_alias_index);
    if (index < 0) {
        return;
    }
    v_anim& va = vp->anims[index];
    // Original: the Anims bounds and null check of anim_get_anim_from_handle, inlined.
    const anim* animp = anim_get_anim_from_handle(va.anim_handle);
    if (animp == nullptr || animp->num_prop_triggers == 0) {
        return;
    }
    va.prop_triggers_done = 1;
    int prev_time;
    int cur_time;
    animlib_get_animation_times(vp->aim_handle, va.instance, prev_time, cur_time);
    for (int i = 0; i < animp->num_prop_triggers; i++) {
        prop_trigger* ptrig = &animp->prop_triggers[i];
        // Magic numbers: trigger frames are 30 fps frames, anim_lib times 4800 ticks a second.
        if (prev_time < (int)((double)(float)ptrig->frame / 30.0 * 4800.0)) {
            // Original: a byte mask, so a ninth trigger and later run again every time.
            const unsigned char bit = (unsigned char)(1 << (i & 0x1F));
            if ((va.prop_triggers_elapsed & bit) == 0) {
                va.prop_triggers_elapsed |= bit;
                execute_prop_trigger(ptrig, objp);
            }
        }
    }
}

// 0x007F6AA0 (:1851); LTCG: line_handle ESI, hp on the stack. Inlined by its only caller in rfg.exe (vanim_check_prop_handles
// has the body, the out-of-line copy has no callers). A voice line that is flagged as dialogue goes through the dialogue
// manager.
static void play_anim_trigger_voice(human* hp, voice_line_handle line_handle)
{
    if (line_handle == INVALID_VOICE_LINE_HANDLE) {
        return;
    }
    human_voice_line_info* info = human_voice_line_get_info(line_handle);
    if (info == nullptr) {
        return;
    }
    const human_voice_persona_groups group = human_voice_persona_group_from_team(hp->current_team);
    if ((unsigned int)group >= NUM_HUMAN_VOICE_PERSONA_GROUPS) {
        return;
    }
    if (info->group_descriptors[group].flags.force_dialogue) {
        dialogue_manager_start_dialogue(line_handle, hp, DIALOGUE_PRIORITY_HIGH);
    } else {
        hp->play_voice_line(line_handle, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_ANIMATION, false, false, true, true);
    }
}

// Inlined twice in vanim_check_prop_handles (helper inferred): the sound triggers (cross-linked to objp, unless muted)
// and the voice triggers (objp's voice, when it is a human) whose time lies in (prev_time, cur_time].
static void vanim_play_audio_triggers(const anim* animp, int prev_time, int cur_time, bool muted, object* objp)
{
    for (int i = 0; i < animp->num_sound_triggers; i++) {
        const sound_trigger& trigger = animp->sound_triggers[i];
        // Magic numbers: trigger frames are 30 fps frames, anim_lib times 4800 ticks a second.
        const int time = (int)((double)trigger.frame / 30.0 * 4800.0);
        if (prev_time < time && time <= cur_time && !muted) {
            gameaudio_play_cross_linked_sound(trigger.foley_ref, objp != nullptr ? objp->get_handle() : 0xFFFFFFFF, nullptr, nullptr);
        }
    }
    human* hp = World.object_cast<human>(objp);
    if (hp == nullptr) {
        return;
    }
    for (int i = 0; i < animp->num_voice_triggers; i++) {
        const voice_trigger& trigger = animp->voice_triggers[i];
        const int time = (int)((double)(float)trigger.frame / 30.0 * 4800.0);
        if (prev_time < time && time <= cur_time) {
            play_anim_trigger_voice(hp, trigger.voice_handle);
        }
    }
}

// 0x00800570 (:4492). Each frame after the human's transform (human_process_post_transform): the sound and voice
// triggers that passed this frame play, for every action and every state blended in to 30% (a blend tree: each of its
// states above 30%, on the tree's own times); then every anim's prop triggers that passed run, once each.
void vanim_check_prop_handles(v_animate* vp, object* objp)
{
    bool transitioning = false;
    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        const v_anim& va = vp->anims[i];
        if (!va.used || va.anim_handle == -1) {
            break;
        }
        if (va.is_transition_action) {
            transitioning = true;
            break;
        }
    }
    // Magic number: editor flag 9, Disable_state_audio.
    const bool disable_state_audio = ((vp->editor_flags >> 9) & 1) != 0;

    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim& va = vp->anims[i];
        if (!va.used || va.anim_handle == -1) {
            break;
        }
        // Magic number: a state below 30% blend stays silent, and so does every state during a transition action.
        if (va.is_state && (0.3f > va.blend || transitioning || disable_state_audio)) {
            continue;
        }
        if (!va.is_blend_tree) {
            // Original: the Anims bounds and null check of anim_get_anim_from_handle, inlined.
            const anim* animp = anim_get_anim_from_handle(va.anim_handle);
            if (animp == nullptr || (animp->num_sound_triggers == 0 && animp->num_voice_triggers == 0)) {
                continue;
            }
            int prev_time = -1;
            int cur_time = -1;
            // the sounds wait for a multiplayer match's gameplay and for the player's turn in wrecking crew
            const bool muted = (Multiplayer && !multi_game_mode_is_gameplay_or_later()) || (wrecking_crew_is_active() == true && !wrecking_crew_can_player_move());
            animlib_get_animation_times(vp->aim_handle, va.instance, prev_time, cur_time);
            vanim_play_audio_triggers(animp, prev_time, cur_time, muted, objp);
        } else {
            // Original: the tree's states are never muted (no multiplayer or wrecking crew check here).
            for (int state = 0; state < va.blend_tree_info->num_states; state++) {
                if (!(va.blend_tree_instance.state_weights[state] > 0.3f)) {
                    continue;
                }
                const anim* animp = anim_get_anim_from_handle(va.blend_tree_info->states[state].anim_handle);
                if (animp == nullptr || (animp->num_sound_triggers == 0 && animp->num_voice_triggers == 0)) {
                    continue;
                }
                int prev_time = -1;
                int cur_time = -1;
                animlib_get_animation_times(vp->aim_handle, va.instance, prev_time, cur_time);
                vanim_play_audio_triggers(animp, prev_time, cur_time, false, objp);
            }
        }
    }

    for (int i = 0; i < vp->max_simultaneous_anims; i++) {
        v_anim& va = vp->anims[i];
        if (!va.used || va.anim_handle == -1) {
            return;
        }
        const anim* animp = anim_get_anim_from_handle(va.anim_handle);
        if (animp == nullptr || animp->num_prop_triggers == 0 || va.prop_triggers_done) {
            continue;
        }
        int prev_time;
        int cur_time;
        animlib_get_animation_times(vp->aim_handle, va.instance, prev_time, cur_time);
        for (int j = 0; j < animp->num_prop_triggers; j++) {
            prop_trigger* ptrig = &animp->prop_triggers[j];
            const int time = (int)((double)(float)ptrig->frame / 30.0 * 4800.0);
            if (prev_time < time && time <= cur_time) {
                // Original: a byte mask, so a ninth trigger and later run every frame they pass.
                const unsigned char bit = (unsigned char)(1 << (j & 0x1F));
                if ((va.prop_triggers_elapsed & bit) == 0) {
                    va.prop_triggers_elapsed |= bit;
                    execute_prop_trigger(ptrig, objp);
                }
            }
        }
    }
}

#endif // OPENRFG_AUDIO
