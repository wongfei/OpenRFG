// Original: volition/rfg/code/objects/humans/human_heads_and_props.cpp
//
// Partial (player_minimal.md P3): the heads of the human types (parsed with character.xtbl) and the head managers the
// human containers fill when they load. Compiled in rfg_objects_unity.cpp. stub_misc.md group 3: the head morphing
// (blinking, lip sync).
// The external interface is declared in human_heads_and_props.h.

#include "volition/rfg/code/objects/humans/human_heads_and_props.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/game_rig.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

// ---- globals and file statics compiled since P3
char_def_head Character_heads[140]; // 0x0302B970
int Num_character_heads; // 0x0302F024
string_pool Human_hap_string_pool("Human hap strings"); // 0x035507E0, initialized in human_hap_system_init
static const char HUMAN_HAP_STRING_POOL_NAME[18] = "Human hap strings"; // 0x01310C94
static char Human_hap_string_pool_data[2048]; // 0x030291D8
// head_manager Head_managers[18] (0x030299D8) is used here too; placed in human.cpp (earlier in the unity TU).
static bool Human_hap_init; // 0x0302F066

// ---- functions
// 0x00A0A5A0 (:83)
// A human container loaded: each head of the container's human types gets its morph targets, its LOD mesh and a head
// manager with its character (the base head's mesh on the head rig, or the base head item's character in this
// container) and its material.
void human_hap_stream_load(unsigned int srid)
{
    for (int i = 0; i < Num_character_heads; i++)
    {
        char_def_head* head = &Character_heads[i];
        if (head->morph_mat_srid != srid)
        {
            continue;
        }
        head->head_morph = mesh_morph_find_by_name(head->name, head->morph_mat_srid);
        // Original: base_head is read without a check (it is null when the base head's item was not loaded at parse).
        head->base_head_morph = mesh_morph_find_by_name(head->base_head->name, head->base_head->srid);
        if (head->lod_mesh == nullptr && v_stricmp(head->lod_name, "None") != 0)
        {
            head->lod_mesh = character_mesh_find(head->lod_name, head->morph_mat_srid);
        }
        for (int manager = 0; manager < 18; manager++)
        {
            head_manager& hm = Head_managers[manager];
            if (hm.used)
            {
                continue;
            }
            hm.used = true;
            hm.srid = srid;
            hm.loaded = true;
            hm.head = head;
            character* cp;
            if (head->base_head_info == nullptr)
            {
                character_load(head->rig->name, srid, head->base_head->m_data, head->rig, head->rig->name);
                cp = character_find(head->rig->name, srid);
            }
            else
            {
                cp = character_find(head->base_head_info->name, srid);
                head->base_head = cp;
            }
            hm.cp = cp;
            hm.mat_p = material_find(head->name, srid);
            break;
        }
    }
}

// 0x00A0A6D0 (:125)
void human_hap_stream_unload(unsigned int srid)
{
    for (int manager = 0; manager < 18; manager++)
    {
        head_manager& hm = Head_managers[manager];
        if (hm.srid != srid || !hm.used)
        {
            continue;
        }
        char_def_head* head = hm.head;
        if (head->lod_mesh != nullptr && v_stricmp(head->lod_name, "None") != 0)
        {
            character_mesh_unload(head->lod_mesh);
            head->lod_mesh = nullptr;
        }
        if (head->base_head_info == nullptr)
        {
            character_unload(hm.cp);
        }
        else
        {
            head->base_head = nullptr;
            hm.cp = nullptr;
        }
        hm.used = false;
        hm.loaded = false;
    }
}

// 0x00A2FE40 (:151)
// A Head of character.xtbl (at most 140): its base head item's character (when the item is loaded, else the item is
// kept to look the character up in the human's container), the LOD head mesh of a loaded item, and the rig named
// after the head or its base head.
char_def_head* human_hap_parse_head(xml_element* xml_head)
{
    const char* name = xml_table_get_req_string_ref(xml_head, "Name");
    // Magic number: 140, the size of Character_heads.
    if (Num_character_heads > 139)
    {
        return nullptr;
    }
    char_def_head* head = &Character_heads[Num_character_heads++];
    head->name = Human_hap_string_pool.add_unique(name);
    const char* base_head_name = xml_table_get_req_string_ref(xml_head, "Base_Head");
    // Original: the base head and LOD head items are required (not checked).
    item_info* base_head_info = object_item_get_info(base_head_name);
    head->base_head = character_find(base_head_info->name, base_head_info->srid);
    head->base_head_info = head->base_head != nullptr ? nullptr : base_head_info;
    const char* lod_name = xml_table_get_req_string_ref(xml_head, "Lod_Head");
    head->lod_name = Human_hap_string_pool.add_unique(lod_name);
    head->lod_mesh = nullptr;
    if (v_stricmp(lod_name, "None") != 0)
    {
        item_info* lod_info = object_item_get_info(lod_name);
        if (lod_info->srid != 0)
        {
            // The LOD head is an item of its own: its mesh is used and not unloaded with the human.
            head->lod_mesh = character_mesh_find(lod_info->cmesh_name, lod_info->srid);
            head->lod_name = Human_hap_string_pool.add_unique("None");
        }
    }
    char rig_name[64];
    cf_add_ext(rig_name, sizeof(rig_name), name, ".rig_pc", false);
    head->rig = anim_rig_find(rig_name);
    if (head->rig == nullptr)
    {
        cf_add_ext(rig_name, sizeof(rig_name), base_head_name, ".rig_pc", false);
        head->rig = anim_rig_find(rig_name);
    }
    return head;
}

// 0x00A0A750 (:198): empty in the release build.
void human_hap_parse_prop_table()
{
}

// 0x00A0A760 (:210)
void human_hap_system_init()
{
    if (!Human_hap_init)
    {
        Human_hap_init = true;
        Human_hap_string_pool.init(Human_hap_string_pool_data, sizeof(Human_hap_string_pool_data), HUMAN_HAP_STRING_POOL_NAME, 0, 0);
        Human_hap_string_pool.clear();
        Num_character_heads = 0;
        for (int manager = 0; manager < 18; manager++)
        {
            Head_managers[manager].used = false;
            Head_managers[manager].loaded = false;
        }
    }
}

// 0x00A0A7D0 (:232)
void human_hap_system_shutdown()
{
    if (Human_hap_init)
    {
        Human_hap_init = false;
        for (int manager = 0; manager < 18; manager++)
        {
            if (Head_managers[manager].used)
            {
                stream2_release(Head_managers[manager].srid);
                Head_managers[manager].used = false;
                Head_managers[manager].loaded = false;
            }
        }
    }
}

// 0x00A0A810 (:253): the manager of a random head of the human's type, if its container is loaded.
int human_hap_request_head(human* hp)
{
    const human_info* h_info = hp->h_info;
    if (h_info->num_heads > 0)
    {
        const int head = int_rand_range(0, h_info->num_heads - 1);
        for (int manager = 0; manager < 18; manager++)
        {
            if (Head_managers[manager].used && Head_managers[manager].srid == h_info->heads[head]->morph_mat_srid)
            {
                return manager;
            }
        }
    }
    return -1;
}

// 0x00A0A870 (:273): empty in the release build.
void human_hap_do_frame()
{
}

// 0x00A0A880 (:277)
head_manager* human_hap_get_head(int head_manager_idx)
{
    if (head_manager_idx != -1 && Head_managers[head_manager_idx].loaded)
    {
        return &Head_managers[head_manager_idx];
    }
    return nullptr;
}

// 0x00A0A8B0 (:293)
// The low nibble of a phoneme byte picks the morph target. Magic numbers: 26 to 34, the head's phoneme targets.
static int lipsync_phn_to_index(unsigned char phn)
{
    const int nibble = phn & 0xF;
    return nibble >= 1 && nibble <= 8 ? 26 + nibble : 26;
}

// 0x00A0A920 (:333): the header's two bytes hold the data's size in bytes, 2 of them the header itself, and each
// phoneme is two bytes.
static int get_number_lipsync_phonemes(const unsigned char* lipsync_data)
{
    if (lipsync_data == nullptr)
    {
        return 0;
    }
    return ((lipsync_data[0] & 0xF) * 0x100 + lipsync_data[1] - 2) / 2;
}

enum human_emotion_encoding
{
    HUMAN_EMOTION_NORMAL = 0,
    HUMAN_EMOTION_HAPPY = 1,
    HUMAN_EMOTION_DEAD = 2,
    HUMAN_EMOTION_FRIGHTENED = 3,
    HUMAN_EMOTION_PAIN = 4,
    HUMAN_EMOTION_SCREAM = 5,
};

// 0x00A0A940 (:343): the high nibble of the first byte.
static human_emotion_encoding get_lipsync_emotion(const unsigned char* lipsync_data)
{
    if (lipsync_data == nullptr)
    {
        return HUMAN_EMOTION_NORMAL;
    }
    return (human_emotion_encoding)(lipsync_data[0] >> 4);
}

// The strength of a phoneme from the high nibble of its byte, at most 1. Inlined three times in do_lipsync.
static float lipsync_phn_strength(unsigned char phn)
{
    // Magic numbers: rfg.exe's (3 n + 3) * 2 percent.
    const float strength = (float)((double)(float)(((phn >> 4) * 3 + 3) * 2) / 100.0);
    return strength > 1.0f ? 1.0f : strength;
}

// A blend through the two ramps as a float, computed in x87 double in rfg.exe: the differences are unsigned.
static float lipsync_ramp(unsigned int num, unsigned int den)
{
    return (float)((double)num / (double)den);
}

// Short strong phonemes are held back. Magic numbers: 70 ms (a duration unit is 30 ms) and strength 0.6, factor 0.4.
static float lipsync_scale_short(float value, unsigned int duration, float strength)
{
    if (duration <= 0x46 && strength >= 0.6f)
    {
        return (float)((double)value * (double)0.4f);
    }
    return value;
}

// 0x00A7F3A0 (:360)
// Plays the current voice line's phonemes on morph targets 2 (this phoneme) and 3 (the phoneme it blends with), the
// line's emotion on target 4. Times come from the timer set when the line started playing, 50 ms ahead.
void do_lipsync(human* hp, human_head_morphing* hmorph)
{
    if (hp->voice == nullptr)
    {
        return;
    }
    const unsigned char* data = hp->voice->get_lipsync_data(hmorph->current_lipsync_handle);
    if (data == nullptr)
    {
        hmorph->target[2].blend = 0.0f;
        hmorph->target[3].blend = 0.0f;
        hmorph->target[4].blend = 0.0f;
        return;
    }
    if (hmorph->has_new_data)
    {
        if (hp->voice_instance == -1 && (hp->radio_instance == 0 || hud_ui_radio_get_play_id_from_msg(hp->radio_instance) == -1))
        {
            hmorph->has_new_data = 0;
            return;
        }
        audiolib_play_state state;
        if (audiolib_play_get_state(hp->get_voice_line_play_id(), &state) != AUDIOLIB_ERR_NONE || state != AUDIOLIB_PLAY_STATE_PLAYING)
        {
            return;
        }
        hmorph->current_phn = 0;
        hmorph->emotion_scale = fl_rand_range(0.5f, 1.0f);
        if (audiolib_play_get_play_time(hp->get_voice_line_play_id(), &hmorph->total_duration, nullptr) != AUDIOLIB_ERR_NONE)
        {
            hmorph->total_duration = 1000; // Magic number: 1 s when the line's length is unknown.
        }
        hmorph->timer.set(-50);
        hmorph->has_new_data = 0;
        hmorph->current_phn_start = 0;
        hmorph->initial_phn_start = 0;
    }
    const int num_bytes = get_number_lipsync_phonemes(data) * 2;
    if (num_bytes <= 0)
    {
        return;
    }
    const unsigned int emotion = (unsigned int)get_lipsync_emotion(data);
    unsigned char phn = data[hmorph->current_phn + 2];
    // Magic number: a phoneme's duration is in units of 30 ms.
    unsigned int duration = (unsigned int)data[hmorph->current_phn + 3] * 30;
    hmorph->current_phn_end = hmorph->current_phn_start + duration;
    const unsigned int time = (unsigned int)hmorph->timer.time_since();
    bool finished = hmorph->current_phn >= num_bytes;
    while (!finished && time > hmorph->current_phn_end)
    {
        hmorph->current_phn += 2;
        if (hmorph->current_phn >= num_bytes)
        {
            finished = true;
            break;
        }
        phn = data[hmorph->current_phn + 2];
        duration = (unsigned int)data[hmorph->current_phn + 3] * 30;
        hmorph->current_phn_start = hmorph->current_phn_end;
        hmorph->current_phn_end += duration;
    }
    if (finished)
    {
        hmorph->current_phn = 0;
        hmorph->target[2].blend = 0.0f;
        hmorph->target[3].blend = 0.0f;
        hmorph->current_lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
        hmorph->target[4].blend = 0.0f;
        return;
    }

    const unsigned int cur = hmorph->current_phn;
    const float strength = lipsync_phn_strength(data[cur + 2]);
    const unsigned int mid = (duration >> 1) + hmorph->current_phn_start;
    bool has_prev = false;
    bool has_next = false;
    unsigned char prev_phn = 0;
    unsigned char next_phn = 0;
    unsigned int prev_duration = 0;
    unsigned int next_duration = 0;
    unsigned int prev_mid = 0;
    unsigned int next_mid = 0;
    float blend;
    if (time < mid)
    {
        // Rising to the middle of this phoneme, from the middle of the one before it.
        if (cur != 0)
        {
            prev_phn = data[cur];
            prev_duration = (unsigned int)data[cur + 1] * 30;
            prev_mid = hmorph->current_phn_start - (prev_duration >> 1);
            has_prev = true;
        }
        else
        {
            prev_mid = hmorph->current_phn_start;
        }
        blend = lipsync_scale_short(lipsync_ramp(time - prev_mid, mid - prev_mid), duration, strength);
    }
    else if (time > hmorph->current_phn_end)
    {
        blend = 0.0f;
    }
    else
    {
        // Falling from the middle of this phoneme to the middle of the next one.
        if ((int)(cur + 2) < num_bytes)
        {
            next_phn = data[cur + 4];
            next_duration = (unsigned int)data[cur + 5] * 30;
            next_mid = (next_duration >> 1) + hmorph->current_phn_end;
            has_next = true;
        }
        else
        {
            next_mid = hmorph->current_phn_end;
        }
        blend = lipsync_scale_short(lipsync_ramp(next_mid - time, next_mid - mid), duration, strength);
    }
    if (blend > 1.0f)
    {
        blend = 1.0f;
    }
    else if (0.0f > blend)
    {
        blend = 0.0f;
    }
    const int index = lipsync_phn_to_index(phn);
    const float cur_blend = (float)((double)blend * (double)strength);
    hmorph->target[2].target_idx = index;
    hmorph->target[2].blend = cur_blend;

    if (has_next || has_prev)
    {
        // The neighbouring phoneme on target 3, or added to target 2's when they share a morph.
        float other = has_next ? lipsync_ramp(time - mid, next_mid - mid) : lipsync_ramp(mid - time, mid - prev_mid);
        if (other > 1.0f)
        {
            other = 1.0f;
        }
        else if (0.0f > other)
        {
            other = 0.0f;
        }
        const float other_strength = lipsync_phn_strength(has_next ? data[cur + 4] : data[cur]);
        other = lipsync_scale_short((float)((double)other_strength * (double)other), has_next ? next_duration : prev_duration, other_strength);
        const int other_index = lipsync_phn_to_index(has_next ? next_phn : prev_phn);
        hmorph->target[3].target_idx = other_index;
        if (other_index == index)
        {
            const float sum = (float)((double)other + (double)cur_blend);
            hmorph->target[3].blend = sum > 1.0f ? 1.0f : sum;
            hmorph->target[2].blend = 0.0f;
        }
        else
        {
            hmorph->target[3].blend = other;
        }
    }
    else
    {
        hmorph->target[3].blend = 0.0f;
    }

    if (emotion != 0)
    {
        // Fades in over the line's first 20%, out over its last 20%. Magic number: 0x22, the targets before the emotions.
        const int elapsed = (int)(time - hmorph->initial_phn_start);
        hmorph->target[4].target_idx = (int)emotion + 0x22;
        const double elapsed_d = (double)(float)elapsed;
        const double duration_d = (double)(float)hmorph->total_duration;
        const double ramp = duration_d * (double)0.2f;
        float scale;
        if (ramp >= elapsed_d)
        {
            scale = (float)(elapsed_d / ramp);
        }
        else if (elapsed_d >= duration_d * (double)0.8f)
        {
            scale = (float)((double)(float)(hmorph->total_duration - elapsed) / ramp);
        }
        else
        {
            scale = 1.0f;
        }
        const float value = (float)((double)hmorph->emotion_scale * (double)scale);
        if (value > 1.0f)
        {
            hmorph->target[4].blend = 1.0f;
        }
        else
        {
            hmorph->target[4].blend = 0.0f > value ? 0.0f : value;
        }
    }
}

// 0x00A0A950 (:645)
// The eyelids, morph targets 0 and 1: shut for 10 ms, open in 0.15 s, wait 1 to 12 s, shut in 0.2 s. Magic number: 0.6,
// the most they blend. Opening stops at the target's blend_target (human_spinebend.cpp sets it from the eyes' pitch).
void do_blinking(human_head_morphing* hmorph)
{
    if (hmorph->do_blink_wait)
    {
        hmorph->target[0].blend = 0.6f;
        hmorph->target[1].blend = 0.6f;
        if (hmorph->blink_ts.elapsed())
        {
            hmorph->do_blink_wait = 0;
            hmorph->do_blink_open = 1;
            hmorph->blink_ts.set(int_rand_range(1000, 12000));
        }
    }
    if (hmorph->do_blink_open)
    {
        float blend = (float)((double)hmorph->blink_blend - (double)Frametime / (double)0.15f);
        if ((float)(0.0 - (double)blend) >= 0.0f)
        {
            blend = 0.0f;
        }
        hmorph->blink_blend = blend;
        if (blend == 0.0f)
        {
            hmorph->do_blink_open = 0;
        }
        if (blend > hmorph->target[0].blend_target)
        {
            hmorph->target[0].blend = blend;
        }
        if (blend > hmorph->target[1].blend_target)
        {
            hmorph->target[1].blend = blend;
        }
    }
    if (hmorph->blink_ts.elapsed())
    {
        hmorph->do_blink_close = 1;
    }
    if (hmorph->do_blink_close)
    {
        float blend = (float)((double)Frametime / (double)0.2f + (double)hmorph->blink_blend);
        if ((float)((double)0.6f - (double)blend) < 0.0f)
        {
            blend = 0.6f;
        }
        hmorph->blink_blend = blend;
        if (blend >= 0.6f)
        {
            hmorph->do_blink_close = 0;
            hmorph->do_blink_wait = 1;
            hmorph->blink_ts.set(10);
        }
        if (hmorph->blink_blend > hmorph->target[0].blend)
        {
            hmorph->target[0].blend = hmorph->blink_blend;
        }
        if (hmorph->blink_blend > hmorph->target[1].blend)
        {
            hmorph->target[1].blend = hmorph->blink_blend;
        }
    }
}

// 0x00A7F9E0 (:689)
void human_hap_process_morphs(human* hp)
{
    if (!hp->rdd.is_loaded() || hp->is_offscreen())
    {
        return;
    }
    // Magic number: 30 m of apparent distance.
    // Deviation from rfg.exe: get_apparent_distance, which it inlines.
    if ((double)hp->render_distance.get_apparent_distance(hp->pos) > 30.0 || hp->is_killed())
    {
        return;
    }
    human_head_morphing* hmorph = &hp->head_morphs;
    for (int i = 0; i < hmorph->num_targets; i++)
    {
        morph_target& t = hmorph->target[i];
        if (t.blend_target > t.blend)
        {
            const float blend = (float)((double)Frametime / (double)t.blend_rate + (double)t.blend);
            t.blend = (float)((double)blend - (double)t.blend_target) >= 0.0f ? t.blend_target : blend;
        }
        else if (t.blend > t.blend_target)
        {
            const float blend = (float)((double)t.blend - (double)Frametime / (double)t.blend_rate);
            // Written as !(>=) because the asm takes the target when the difference is NaN (COMISS, JNC).
            t.blend = !((float)((double)blend - (double)t.blend_target) >= 0.0f) ? t.blend_target : blend;
        }
    }
    do_blinking(hmorph);
    do_lipsync(hp, hmorph);
}

