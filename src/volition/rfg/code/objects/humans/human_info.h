// Original: volition/rfg/code/objects/humans/human_info.h
// Implementation: volition/rfg/code/objects/humans/human_info.cpp
//
// Human (character) definitions from humans.xtbl (parse_human_table): health, speed, model, rig and animation set,
// heads, voice personas, default inventory, props and the bone indices the game looks up once. The master list holds
// up to 165 entries (human_info_get_master_list). Each human_info names a GSC_HUMAN streamed container (srid, slot_id);
// human_info_stream_load / _unload are the container load callbacks, which resolve the rig, mesh and voice data.
// foot_ground_effects maps physical material, shape type and footstep variant to effect handles.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/string_pool.h"

class mempool;
struct xml_element;
struct inv_item_info;
struct char_def_head;
class human_voice_persona;

enum footstep_version
{
    FOOTSTEP_LEFT = 0,
    FOOTSTEP_RIGHT = 1,
    NUM_FOOTSTEP_VERSIONS = 2,
};

enum footstep_variation
{
    FOOTSTEP_WALKING = 0,
    FOOTSTEP_RUNNING = 1,
    NUM_FOOTSTEP_VARIATIONS = 2,
};

enum jump_version
{
    JUMP_TAKEOFF = 0,
    JUMP_LAND = 1,
    NUM_JUMP_VERSIONS = 2,
};

// size 0x2C
struct foot_ground_effects
{
    unsigned int get_footstep_effect_handle(unsigned char mat_index, material_shape_type shape_type, footstep_version version, footstep_variation variation); // 0x00A0AC30 (:1571)
    void set_footstep_effect_handle(unsigned int handle, unsigned char mat_index, material_shape_type shape_type, footstep_version version, footstep_variation variation); // 0x00A0ACB0 (:1590)
    unsigned int get_jump_effect_handle(unsigned char mat_index, material_shape_type shape_type, jump_version version); // 0x00A0AD90 (:1622)
    void set_jump_effect_handle(unsigned int handle, unsigned char mat_index, material_shape_type shape_type, jump_version version); // 0x00A0AE00 (:1641)

    char name[20]; // 0x00
    int walk_foley; // 0x14
    int run_foley; // 0x18
    int jump_foley; // 0x1C
    int land_foley; // 0x20
    unsigned int (** footstep)[5][2][2]; // 0x24
    unsigned int (** jump)[5][2]; // 0x28
};

// size 0x10
class voice_persona_list
{
    // player::serialize picks the multiplayer voice (friends are not recorded in rfg.pdb).
    friend class player;
    // So does the multiplayer level init of the human voice (inferred, as above).
    friend void human_voice_persona_level_init_pre(bool multiplayer);
public:
    voice_persona_list(); // 0x00970690 (.h:160)
    bool add_voice_name(const char* new_name); // 0x00A55C20 (:1366)
    human_voice_persona* pick_voice(bool needs_hostage_lines, bool needs_raid_lines, bool needs_raid_intro, bool needs_miner_persona) const; // 0x00A55E70 (:1481)
    int get_voice_count(); // 0x00A2C590 (.h:169)
    void reset(); // 0x00A30B80 (:1357)
    void load(mempool* pool, xml_element* xml_root); // 0x00A55D60 (:1437)
private:
    human_voice_persona* pick_voice_generic(const farray<human_voice_persona*,64>& filtered_voice_list) const; // 0x00A55C80 (:1391)
public:
    ~voice_persona_list(); // 0x0097A7E0
    voice_persona_list& operator=(const voice_persona_list& __that); // 0x007D7150

    base_array<human_voice_persona*> voice_list; // 0x00
};

// size 0xC
struct human_info_inventory_info
{
    const inv_item_info* i_info; // 0x00
    int count; // 0x04
    int slot; // 0x08
};

// size 0x180
class human_info
{
public:
    // size 0x1C
    struct human_prop_info
    {
        item_info* item; // 0x00
        char* item_prop_tag; // 0x04
        char* character_prop_tag; // 0x08
        int character_prop_index; // 0x0C
        float appearance_percent; // 0x10
        int num_sub_props; // 0x14
        human_prop_info* sub_props; // 0x18
    };

    // size 0x14
    struct human_prop_general_info
    {
        char* prop_name; // 0x00
        float appearance_percent; // 0x04
        prop_type m_prop_type; // 0x08
        int num_props; // 0x0C
        human_prop_info* props; // 0x10
    };

    // size 0x4
    struct human_info_flags
    {
        bool female : 1; // 0x00:0
        bool coin : 1; // 0x00:1
        bool preload : 1; // 0x00:2
        bool officer : 1; // 0x00:3
        bool unarmed : 1; // 0x00:4
        bool vip : 1; // 0x00:5
        bool use_tech_level : 1; // 0x00:6
        bool sniper : 1; // 0x00:7
        bool elite_dodge : 1; // 0x01:0
        bool riot_shield : 1; // 0x01:1
        bool body_armor : 1; // 0x01:2
        bool nonflammable : 1; // 0x01:3
        bool no_attaching_projectiles : 1; // 0x01:4
        bool guerrilla_tech : 1; // 0x01:5
        bool guerrilla_rfc : 1; // 0x01:6
        bool old_coot : 1; // 0x01:7
        bool dan : 1; // 0x02:0
        bool jenkins : 1; // 0x02:1
        bool driverless_exit_only; // 0x03, not a bit field
    };

    int get_max_hit_points() const; // 0x00A0AB00 (:245)
    int get_max_knockdown_hits() const; // 0x00A0AB20 (:254)
    float get_max_speed() const; // 0x00A0AB40 (:263)
    const inv_item_info* get_default_equip_item() const; // 0x00A0AB60 (:272)
    const inv_item_info* get_default_last_equip_item() const; // 0x00A0AB80 (:281)
    void set_max_hit_points(int _max_hit_points); // 0x00A05510 (.h:214)
    void set_max_knockdown_hits(int _max_knockdown_hits); // 0x00A05520 (.h:215)
    void set_max_speed(float _max_speed); // 0x00A05530 (.h:216)
    void set_default_equip_item(inv_item_info* _default_equip_item); // 0x00A05540 (.h:217)
    void set_default_last_equip_item(inv_item_info* _default_last_equip_item); // 0x00A05550 (.h:218)
    human_info(); // 0x00A74370
    ~human_info(); // 0x00A47480
    human_info& operator=(const human_info& __that); // 0x007F03E0

private:
    int max_hit_points; // 0x00
    int max_knockdown_hits; // 0x04
    float max_speed; // 0x08
    inv_item_info* default_equip_item; // 0x0C
    inv_item_info* default_last_equip_item; // 0x10
public:
    char* name; // 0x14
    unsigned int name_crc; // 0x18
    char* model_name; // 0x1C
    char* anim_set_name; // 0x20
    unsigned int anim_set_name_crc; // 0x24
    char* rig_name; // 0x28
    unsigned int srid; // 0x2C
    int slot_id; // 0x30
    float default_height_scale; // 0x34
    float height_scale_variation; // 0x38
    human_teams default_team; // 0x3C
    short hurt_threshold; // 0x40
    int home_district; // 0x44
    float aim_error_pct; // 0x48
    foot_ground_effects* foot_effects; // 0x4C
    int num_heads; // 0x50
    char_def_head* heads[4]; // 0x54
    voice_persona_list voice_personas; // 0x64
    farray<human_info_inventory_info,15> default_inventory; // 0x74
    int num_general_props; // 0x138
    human_prop_general_info* general_props; // 0x13C
    int lower_spine_bone_index; // 0x140
    int upper_spine_bone_index; // 0x144
    int pelvis_bone_index; // 0x148
    int head_bone_index; // 0x14C
    int neck_bone_index; // 0x150
    int eye_l_bone_index; // 0x154
    int eye_r_bone_index; // 0x158
    int calf_l_bone_index; // 0x15C
    int calf_r_bone_index; // 0x160
    int camera_bone_index; // 0x164
    int drop_cash_min; // 0x168
    int drop_cash_max; // 0x16C
    unsigned int light_effect_handle; // 0x170
    char* light_tag_names[2]; // 0x174
    human_info_flags flags; // 0x17C
};

extern farray<human_info,165> Master_human_info_list; // 0x03188080
extern string_pool Human_info_string_pool; // 0x033D4D20
extern foot_ground_effects* Footstep_effects_list; // 0x0302F068
extern int Num_footstep_effect_lists; // 0x0302F06C
extern int INVALID_HUMAN_INFO_ID; // 0x01694394
extern int INVALID_HUMAN_RESOURCE_SLOT_ID; // 0x01694390
extern const char VIP_GUERRILLA_TECH_NAME[8]; // 0x01310E98
extern const char VIP_GUERRILLA_RFC_NAME[12]; // 0x01310EA0
extern const char VIP_OLD_COOT_NAME[8]; // 0x01310EAC
extern const char VIP_DAN_NAME[4]; // 0x01310EB4

human_info* human_info_find_internal(const char* human_name); // 0x00A2FFF0 (:296)
void parse_human_table(bool refresh, mempool* pool, int dlc_id); // 0x00ABD070 (:950)
void human_info_stream_load(unsigned int srid); // 0x00A30470 (:1049)
void human_info_stream_unload(unsigned int srid); // 0x00A30870 (:1156)
void human_info_system_init(); // 0x00ABD250 (:1226)
void human_info_system_shutdown(); // 0x00A30A60 (:1239)
const farray<human_info,165>& human_info_get_master_list(); // 0x00A0AC20 (:1251)
const human_info* human_info_find(const char* human_name); // 0x00A30A80 (:1290)
int human_info_id_find(const char* human_name); // 0x00A30A90 (:1296)
const human_info* human_info_find(int id); // 0x00A30B10 (:1317)
const human_info* human_info_find(unsigned int checksum); // 0x00A30B30 (:1338)
