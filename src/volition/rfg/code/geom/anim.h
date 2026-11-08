// Original: volition/rfg/code/geom/anim.h
// Implementation: volition/rfg/code/geom/anim.cpp
//
// Game-side animations: an anim record per animation file (name, anim_lib handle, variants and the prop, IK, sound and
// voice triggers), allocated from the anim pool and looked up by name or handle. anim_file_loaded_callback is the
// load callback of the GSP_ANIM_FILE primitive type: it overlays the streamed .anim_pc data on the animation's anim_lib
// slot (animlib_overlay_anim); anim_file_unloaded_callback unloads it. anim_load_animation / anim_load_packfile load
// animations outside the streaming system. index_alias names an animation by table index or alias. Placement of the
// trigger and variant types inferred.
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/object_item.h"

class rfg_split_mempool;

enum variant_playstyle
{
    VARIANT_PLAYSTYLE_STANDARD = 0,
    VARIANT_PLAYSTYLE_FULLBODY = 1,
    VARIANT_PLAYSTYLE_INCREASED_PRIORITY = 2,
    NUM_VARIANT_PLAYSTYLES = 3,
};

enum prop_transfer_owner
{
    PROP_TRANSFER_OWNER_NONE = -1,
    PROP_TRANSFER_OWNER_ITEM = 0,
    PROP_TRANSFER_OWNER_OFFHAND_ITEM = 1,
    PROP_TRANSFER_OWNER_SELF = 2,
    PROP_TRANSFER_OWNER_PARENT = 3,
    PROP_TRANSFER_OWNER_WORLD = 4,
    PROP_TRANSFER_OWNER_CODE_DESIGNATED = 5,
    NUM_PROP_TRANSFER_OWNERS = 6,
};

enum prop_transfer_action
{
    PROP_TRANSFER_ACTION_CREATE = 0,
    PROP_TRANSFER_ACTION_DELETE = 1,
    PROP_TRANSFER_ACTION_MOVE = 2,
    NUM_PROP_TRANSFER_ACTIONS = 3,
};

enum prop_transfer_tag
{
    PROP_TRANSFER_TAG_NONE = -1,
    PROP_TRANSFER_TAG_MAINHAND = 0,
    PROP_TRANSFER_TAG_OFFHAND = 1,
    PROP_TRANSFER_TAG_NAMED = 2,
    NUM_PROP_TRANSFER_TAGS = 3,
};

// size 0x18
struct anim_variant
{
    vanim_variants variant_id; // 0x00
    int anim_handle; // 0x04
    variant_playstyle playstyle; // 0x08
    float cyclic_percentage; // 0x0C
    float percentage; // 0x10
    int time; // 0x14
};

// size 0x24
struct prop_trigger
{
    prop_transfer_owner owner; // 0x00
    prop_type owner_child; // 0x04
    prop_type type; // 0x08
    prop_transfer_action action; // 0x0C
    int frame; // 0x10
    prop_transfer_owner dest_owner; // 0x14
    prop_type dest_owner_child; // 0x18
    prop_transfer_tag dest_tag; // 0x1C
    char* dest_tag_name; // 0x20
};

// size 0x8
struct ik_trigger
{
    unsigned char enable; // 0x00
    unsigned char location; // 0x01
    unsigned short frame; // 0x02
    float blend_rate; // 0x04
};

// size 0x8
struct sound_trigger
{
    unsigned short frame; // 0x00
    unsigned char looping; // 0x02
    int foley_ref; // 0x04
};

// size 0x8
struct voice_trigger
{
    int frame; // 0x00
    voice_line_handle voice_handle; // 0x04
};

// size 0x2C
class anim
{
public:
    char* filename; // 0x00
    unsigned int name_crc; // 0x04
    int animlib_handle; // 0x08
    unsigned char variant_frequency_min; // 0x0C
    unsigned char variant_frequency_max; // 0x0D
    unsigned char variant_cyclic_percent; // 0x0E
    unsigned char num_variants; // 0x0F
    unsigned char num_prop_triggers; // 0x10
    unsigned char num_ik_triggers; // 0x11
    unsigned char num_sound_triggers; // 0x12
    unsigned char num_voice_triggers; // 0x13
    unsigned int editor_flags; // 0x14
    anim_variant* variants; // 0x18
    prop_trigger* prop_triggers; // 0x1C
    ik_trigger* ik_triggers; // 0x20
    sound_trigger* sound_triggers; // 0x24
    voice_trigger* voice_triggers; // 0x28
};

// size 0x8
class index_alias
{
public:
    index_alias(int anim_index, vanim_aliases anim_alias); // 0x005DBEF0 (.h:119)

    int index; // 0x00
    vanim_aliases alias; // 0x04
};

// 0x005DBEF0 (.h:119)
inline index_alias::index_alias(int anim_index, vanim_aliases anim_alias)
    : index(anim_index)
    , alias(anim_alias)
{
}

bool anim_file_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x007F1710 (:1360)
void anim_file_unloaded_callback(const char* name, unsigned int srid); // 0x007F17D0 (:1412)
anim* anim_allocate(); // 0x007F1840 (:1427)
anim* anim_get_anim_from_handle(int handle); // 0x007F1880 (:1436)
int anim_get_handle(const anim* s); // 0x007F18A0 (:1446)
anim* anim_find(const char* filename); // 0x007F18C0 (:1464)
void anim_system_init(); // 0x007FB680 (:1505)
void anim_init(); // 0x007F19D0 (:1521)
void anim_close(); // 0x007F19E0 (:1528)
void anim_reset(); // 0x007F19F0 (:1533)
int anim_load_animation(const char* filename); // 0x007F1A00 (:1541)
const char* anim_tag_state_from_enum(animation_state anim_state); // 0x007F1B20 (:1583)
const char* anim_tag_action_from_enum(animation_action anim_action); // 0x007F1B40 (:1592)
int anim_load_packfile(const char* filename); // 0x007F1B60 (:1614)
rfg_split_mempool* __anim_pool_lock(); // 0x007F1D20 (:1684)
void __anim_pool_unlock_async(); // 0x007F1DC0 (:1719)
bool __anim_pool_unlock_async_poll(); // 0x007F1E30 (:1743)
void __anim_pool_unlock(); // 0x007F1E70 (:1760)
