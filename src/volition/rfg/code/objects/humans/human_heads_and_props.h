// Original: volition/rfg/code/objects/humans/human_heads_and_props.h
// Implementation: volition/rfg/code/objects/humans/human_heads_and_props.cpp
//
// Heads and props of humans: head morph targets (blinking, emotion, lipsync phonemes; human_head_morphing) and the
// props humans carry. Placement of the types inferred.
#pragma once

#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_pool.h"

class anim_rig;
class character;
struct character_mesh;
struct item_info;
struct material;

class human;
struct xml_element;

// A head of a human type (character.xtbl Heads, human_hap_parse_head): the base head item's character, the LOD head
// mesh, the head rig, and the morph targets found when the human's container loads. Placement inferred.
// size 0x64
struct char_def_head
{
    char* name; // 0x00
    char* lod_name; // 0x04
    item_info* base_head_info; // 0x08, set while the base head's character is not loaded
    character* base_head; // 0x0C
    character_mesh* lod_mesh; // 0x10
    anim_rig* rig; // 0x14
    unsigned int morph_mat_srid; // 0x18, the human type's container
    int head_morph; // 0x1C
    int base_head_morph; // 0x20
    int num_skin_shader_params; // 0x24
    float skin_shader_params[3][5]; // 0x28
};

// A loaded head (one per human container, Head_managers). Placement inferred.
// size 0x14
struct head_manager
{
    bool used; // 0x00
    bool loaded; // 0x01
    char_def_head* head; // 0x04
    unsigned int srid; // 0x08
    character* cp; // 0x0C
    material* mat_p; // 0x10
};

extern char_def_head Character_heads[140]; // 0x0302B970
extern int Num_character_heads; // 0x0302F024
extern string_pool Human_hap_string_pool; // 0x035507E0

// size 0x10
struct morph_target
{
    float blend; // 0x00
    float blend_target; // 0x04
    float blend_rate; // 0x08
    int target_idx; // 0x0C
};

// size 0x7C
struct human_head_morphing
{
    human_head_morphing(); // 0x00A0F250

    int num_targets; // 0x00
    morph_target target[5]; // 0x04
    timestamp blink_ts; // 0x54
    float blink_blend; // 0x58
    lipsync_data_handle current_lipsync_handle; // 0x5C
    float emotion_scale; // 0x60
    unsigned int current_phn_start; // 0x64
    unsigned int initial_phn_start; // 0x68
    unsigned int current_phn_end; // 0x6C
    int total_duration; // 0x70
    timestamp timer; // 0x74
    unsigned short current_phn; // 0x78
    unsigned char do_blink_close : 1; // 0x7A:0
    unsigned char do_blink_open : 1; // 0x7A:1
    unsigned char do_blink_wait : 1; // 0x7A:2
    unsigned char has_new_data : 1; // 0x7A:3
};

void human_hap_stream_load(unsigned int srid); // 0x00A0A5A0 (:83)
void human_hap_stream_unload(unsigned int srid); // 0x00A0A6D0 (:125)
char_def_head* human_hap_parse_head(xml_element* xml_head); // 0x00A2FE40 (:151)
void human_hap_parse_prop_table(); // 0x00A0A750 (:198)
void human_hap_system_init(); // 0x00A0A760 (:210)
void human_hap_system_shutdown(); // 0x00A0A7D0 (:232)
int human_hap_request_head(human* hp); // 0x00A0A810 (:253)
void human_hap_do_frame(); // 0x00A0A870 (:273)
head_manager* human_hap_get_head(int head_manager_idx); // 0x00A0A880 (:277)
void do_lipsync(human* hp, human_head_morphing* hmorph); // 0x00A7F3A0 (:360)
void do_blinking(human_head_morphing* hmorph); // 0x00A0A950 (:645)
void human_hap_process_morphs(human* hp); // 0x00A7F9E0 (:689)

// 0x00A0F250: the timestamps construct themselves.
inline human_head_morphing::human_head_morphing()
{
}
