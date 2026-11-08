// Original: volition/rfg/code/geom/character.cpp
//
// Characters (a character mesh and its rig, registered with anim_lib as an entity bone set) and the animation system
// init. Part of rfg_geom_unity.cpp (player_minimal.md P2).
// The external interface is declared in character.h.

#include "volition/rfg/code/geom/character.h"

#include "keen/base/trace.hpp"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_comp.h"

// ---- globals
int Trigger_combo_start;
int Trigger_no_interrupt;
int Trigger_left_foot;
int Trigger_right_foot;
int Trigger_left_foot_release;
int Trigger_right_foot_release;
int Trigger_critical_end;
int Trigger_combo_window_start;
int Trigger_combo_window_end;
int Trigger_impact_start;
int Trigger_unlock_ai;
int Trigger_secondary_impact;
int Trigger_impact_end;
int Trigger_release_movement;
int Trigger_warning_shot;
int Trigger_ragdoll;
int Trigger_ragdoll_stop;
int Trigger_tag_start;
int Trigger_tag_end;
int Trigger_enable_spinebend;
int Trigger_camera;
int Trigger_relative_space;
int Trigger_reload_give_ammo;
int Trigger_door_open_start;
int Trigger_door_open_end;
int Trigger_door_close_start;
int Trigger_door_close_end;
int Trigger_gravity_start;
int Num_triggers;

// ---- file-static globals
static int Character_base_unique_id = 12345; // 0x0165F240, the entity bone set ids of anim_lib
static bool Character_system_init; // 0x025EF8B2
static object_pool<character, 137, 1> Characters; // 0x02608860, no init / release callbacks

// ---- internal functions
// 0x007FB6D0 (:98); LTCG: c in EAX. The mesh's bones become the character's entity bone set under a new id.
static void get_bones_used_in_mesh(character* c)
{
    c->bones_used_info.unique_id = Character_base_unique_id++;
    c->bones_used_info.num_bones = c->m_data->m_num_bones;
    c->bones_used_info.bone_indices = c->m_data->m_bone_indices.to_pointer();
}

// 0x007F1EC0 (:77)
void character::init()
{
    name[0] = '\0';
    rig = nullptr;
    bones_used_info.num_bones = 0;
    m_data = nullptr;
}

// 0x007F1ED0
void character_set_up_trigger_ids()
{
    Trigger_combo_start = animlib_get_trigger_id("start combo");
    Trigger_no_interrupt = animlib_get_trigger_id("no interrupt");
    Trigger_left_foot = animlib_get_trigger_id("left-foot");
    Trigger_right_foot = animlib_get_trigger_id("right-foot");
    Trigger_left_foot_release = animlib_get_trigger_id("left-foot release");
    Trigger_right_foot_release = animlib_get_trigger_id("right-foot release");
    Trigger_critical_end = animlib_get_trigger_id("critical end");
    Trigger_combo_window_start = animlib_get_trigger_id("combo window start");
    Trigger_combo_window_end = animlib_get_trigger_id("combo window end");
    Trigger_impact_start = animlib_get_trigger_id("impact start");
    Trigger_unlock_ai = animlib_get_trigger_id("unlock ai");
    Trigger_secondary_impact = animlib_get_trigger_id("secondary impact");
    Trigger_impact_end = animlib_get_trigger_id("impact end");
    Trigger_release_movement = animlib_get_trigger_id("release movement");
    Trigger_warning_shot = animlib_get_trigger_id("warning shot");
    Trigger_ragdoll = animlib_get_trigger_id("ragdoll start");
    Trigger_ragdoll_stop = animlib_get_trigger_id("ragdoll stop");
    Trigger_tag_start = animlib_get_trigger_id("tag start");
    Trigger_tag_end = animlib_get_trigger_id("tag end");
    Trigger_enable_spinebend = animlib_get_trigger_id("enable spinebend");
    Trigger_camera = animlib_get_trigger_id("camera");
    Trigger_relative_space = animlib_get_trigger_id("relative space");
    Trigger_reload_give_ammo = animlib_get_trigger_id("reload give ammo");
    Trigger_door_open_start = animlib_get_trigger_id("door start");
    Trigger_door_open_end = animlib_get_trigger_id("door end");
    Trigger_door_close_start = animlib_get_trigger_id("door close start");
    Trigger_door_close_end = animlib_get_trigger_id("door close end");
    Trigger_gravity_start = animlib_get_trigger_id("gravity start");
    Num_triggers = 28;
}

// 0x00801CE0. string_pool_size is not used.
void character_system_init(int string_pool_size)
{
    if (Character_system_init) {
        return;
    }
    Character_system_init = true;
    animlib_system_init();
    anim_system_init();
    vanim_system_init();
    character_instance_system_init(-1);
}

// 0x007FB6F0 (:139)
void character_system_shutdown()
{
    if (Character_system_init) {
        Character_system_init = false;
    }
}

// 0x00801D10. A character for a loaded character mesh: its bones become an anim_lib entity bone set.
// (A null rig leaves the allocated character in the pool, as the original.)
bool character_load(const char* name, unsigned int srid, character_mesh* cmesh, anim_rig* rig, const char* mesh_name)
{
    character* cp = Characters.alloc(0);
    if (cp == nullptr) {
        keen::debug::nativePrintArgs("character_load: Out of character slots!\n");
        return false;
    }
    cp->name[0] = '\0';
    cp->bones_used_info.num_bones = 0;
    cp->m_data = cmesh;
    cp->srid = srid;
    cp->rig = rig;
    if (rig == nullptr) {
        return false;
    }
    get_bones_used_in_mesh(cp);
    animlib_register_entity_base_bone_set(rig, cp->bones_used_info);
    cf_drop_ext(cp->name, 0x40, name);
    if (mesh_name != nullptr) {
        char mesh[64];
        cf_drop_ext(mesh, sizeof(mesh), mesh_name);
        cp->morph_name_hash = crc_stri(mesh, 0);
        return true;
    }
    cp->morph_name_hash = 0;
    return true;
}

// 0x0080A800
void character_unload(character* cp)
{
    animlib_unregister_entity_base_bone_set(cp->bones_used_info.unique_id);
    Characters.free(cp);
}

// 0x00801E20. By name (without extension) and stream container.
character* character_find(const char* name, unsigned int srid)
{
    char base[68];
    cf_drop_ext(base, 0x40, name);
    character* found = nullptr;
    csspinlock_scope lock(Characters.thread_access);
    for (character* cp = Characters.get_first(0); cp != nullptr; cp = Characters.get_next(cp)) {
        if (cp->srid == srid && v_stricmp(base, cp->name) == 0) {
            found = cp;
            break;
        }
    }
    return found;
}

// 0x00801F10 (:368): every character's entity bone set again, after the rigs were reloaded.
void character_reload()
{
    csspinlock_scope lock(Characters.thread_access);
    for (character* cp = Characters.get_first(0); cp != nullptr; cp = Characters.get_next(cp)) {
        if (cp->rig != nullptr) {
            animlib_register_entity_base_bone_set(cp->rig, cp->bones_used_info);
        }
    }
}
