// Original: volition/rfg/code/interface/ui_character.cpp
//
// The main menu's 3D Mason (final_push.md step 6): the character rfg_PC with his head (mason_head on
// base_hires_head), his coat (mason_coat, a cloth simulation) and the sledgehammer, streamed in from the main menu's
// enter, drawn into the "player_model" frame buffer of the uichar document, lit by a directional light of its own,
// swinging the hammer (with the mm_hammer_hit_a effect) when a main menu or options item is chosen.
//
// rfg.exe passes the model names and the document handles in registers (LTCG); here they are parameters.
// Not ported (no caller in rfg.exe): ui_character_init, ui_character_shutdown, ui_character_handshake,
// ui_character_is_loaded, ui_character_release_effect, ui_character_update_effect_render, ui_character_play_effect.

#include "volition/rfg/code/interface/ui_character.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_types.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/geom/static_mesh_shared.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/gamewide/game_handshake.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_render_thread.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <string.h>

// The hammer hit effect of the menu character. Placement inferred: rfg.pdb gives no file; only this file uses it.
// size 0x4C
struct menu_effect
{
    vfx_effect* vfx_effect_p; // 0x00
    float current_time; // 0x04
    object_effect::visual_effect_object visual_effect; // 0x08
};

// ---- globals of ui_menu.cpp used here (file statics in rfg.exe's interface unity, external here)
extern int Doc_handle_main; // 0x016631AC
extern animation_action Play_action; // 0x016642BC
extern int Fx_delay; // 0x016642C8
extern timestamp Fx_delay_timer; // 0x01666FD8

// ---- globals
static bool Update_camera = true; // 0x0166411E
int Doc_handle_uichar = -1; // 0x016642A8 (external since UI4, main_menu.cpp)
static const char* cFramebuffername = {"player_model"}; // 0x016642AC
static const char* cPropframebuftex = {"texture_handle"}; // 0x016642B0
static float Obj_y_orient = -50.0f; // 0x016642B4
static animation_state Anim_state = (animation_state)0x4B; // 0x016642B8, initial data 4b000000
static float Wind_speed = 400.0f; // 0x016642C0
static float Wind_dir = 180.0f; // 0x016642C4
// The initial data of the positions (rfg.exe's .data). Until step 6 the port left them zero.
static vector Light_pos(-200.0f, 500.0f, -500.0f); // 0x01666F90
vector Obj_pos(-1.87f, 6.27f, 0.88f); // 0x01666F9C (external since UI4, main_menu.cpp)
vector Off_screen_pos(0.0f, 10.0f, 0.0f); // 0x01666FA8 (external since UI4, main_menu.cpp)
vector On_screen_pos(0.0f, 5.0f, 0.25f); // 0x01666FB4 (external since UI6, controller_options.cpp)
static vector Cam_pos(-1.97f, 6.25f, 0.92f); // 0x01666FC0
static vector Fx_pos(0.0f, 5.0f, 1.0f); // 0x01666FCC
bool Active_uichar; // 0x02C07FDF (external since UI4, main_menu.cpp)
rl_light* Render_light; // 0x02C07FE0 (a public symbol of rfg.pdb with no file; among this file's globals)
static float Rotation_delta; // 0x02C07FE4
static float Dtime; // 0x02C07FE8
static bool Modify_camera_info; // 0x02C07FEC
static bool Stop_emitting; // 0x02C07FED
menu_effect Hammer_hit_fx; // 0x02C08290 (a public symbol of rfg.pdb with no file)
model_info Player_info; // 0x02C3AA00 (external since UI4, main_menu.cpp)
model_info Coat_info; // 0x02C3CA70 (external since UI4, main_menu.cpp)
static matrix Obj_orient; // 0x02C3CAD0
static matrix Fx_orient; // 0x02C3CB5C
model_info Head_info; // 0x02C54118 (external since UI4, main_menu.cpp)
static_mesh_info Weapon_info; // 0x02C54178 (external since UI4, main_menu.cpp)

// Magic number: degrees to radians, the float 0.017453292 widened to double (0x012BFA28, as rl_camera.cpp).
static const double Deg_to_rad = 0.01745329238474369;

// 0x00893180 (:109)
void model_info::clear()
{
    memset(model_name, 0, sizeof(model_name));
    srid = 0;
    char_inst = nullptr;
    cloth_sim = nullptr;
    h_info = nullptr;
    i_info = nullptr;
    parent_attach_bone = 0;
    attach_bone = 0;
    requested = false;
    loaded = false;
}

// 0x008931B0 (:121)
model_info::model_info()
{
    clear();
}

// 0x008931E0 (:137)
void static_mesh_info::clear()
{
    memset(model_name, 0, sizeof(model_name));
    srid = 0;
    smesh = nullptr;
    smesh_instance = nullptr;
    i_info = nullptr;
    parent_attach_bone = 0;
    attach_bone = 0;
    requested = false;
    loaded = false;
}

// 0x00893210 (:148)
static_mesh_info::static_mesh_info()
{
    clear();
}

// 0x00893250 (:251)
static void ui_character_release_model()
{
    if (Player_info.char_inst != nullptr)
    {
        character_instance_resources_unloaded(Player_info.char_inst);
        character_instance_delete(Player_info.char_inst);
    }
    if (Player_info.requested && Player_info.srid != 0)
    {
        stream2_release(Player_info.srid);
    }
    Player_info.clear();
}

// 0x008932D0 (:271)
static void ui_character_release_head()
{
    if (Head_info.char_inst != nullptr)
    {
        character_instance_resources_unloaded(Head_info.char_inst);
        character_instance_delete(Head_info.char_inst);
    }
    if (Head_info.requested && Head_info.srid != 0)
    {
        stream2_release(Head_info.srid);
    }
    Head_info.clear();
}

// 0x00893350 (:306)
static void ui_character_release_weapon()
{
    if (Weapon_info.smesh_instance != nullptr)
    {
        Weapon_info.smesh_instance->release();
    }
    if (Weapon_info.requested && Weapon_info.srid != 0)
    {
        stream2_release(Weapon_info.srid);
    }
    Weapon_info.clear();
}

// 0x008933C0 (:321)
static void ui_character_release_coat()
{
    if (Coat_info.char_inst != nullptr)
    {
        cloth_sim_delete(Coat_info.cloth_sim);
        character_instance_resources_unloaded(Coat_info.char_inst);
        character_instance_delete(Coat_info.char_inst);
    }
    if (Coat_info.requested && Coat_info.srid != 0)
    {
        stream2_release(Coat_info.srid);
    }
    Coat_info.clear();
}

// 0x008C48E0 (:343)
rl_mesh_instance* ui_create_static_mesh(static_mesh* base_mesh)
{
    rl_mesh_instance* instance = rl_mesh_instance::create();
    if (instance != nullptr)
    {
        instance->init(base_mesh->m_shared_header.m_mesh.to_pointer(), base_mesh->m_shared_header.m_material_map.to_pointer(), (unsigned int)base_mesh->m_lod_submesh_id.to_pointer()[0]);
        instance->calculate_local_aabb();
    }
    return instance;
}

// 0x00893450 (:360)
character_instance* ui_create_character_instance(character* base_character, const char* anim_set_name, unsigned char skin_category)
{
    character_instance* char_inst = nullptr;
    if (base_character != nullptr && anim_set_name != nullptr)
    {
        char_inst = character_instance_create(1.0f, anim_set_name);
        if (char_inst != nullptr)
        {
            character_instance_resources_loaded(char_inst, base_character, skin_category);
        }
    }
    return char_inst;
}

// 0x008934A0 (:379); LTCG: the name in ESI. Requests the human's container, true once it is loaded.
static bool ui_character_load_model(const char* model_name)
{
    if (model_name == nullptr)
    {
        return false;
    }
    if (Player_info.requested)
    {
        if (stream2_get_status(Player_info.srid) == ST_LOADED)
        {
            Player_info.loaded = true;
            return true;
        }
        return false;
    }
    Player_info.h_info = human_info_find(model_name);
    if (Player_info.h_info != nullptr)
    {
        stream2_request(Player_info.h_info->srid);
        _snprintf_safe(Player_info.model_name, sizeof(Player_info.model_name), model_name); // Original: as a format string.
        Player_info.srid = Player_info.h_info->srid;
        Player_info.requested = true;
    }
    return false;
}

// 0x00893510 (:408)
static bool ui_character_set_model()
{
    if (Player_info.loaded && Player_info.h_info != nullptr)
    {
        character* base_character = character_find(Player_info.model_name, Player_info.srid);
        if (base_character != nullptr)
        {
            // Magic number: render category 0x12.
            Player_info.char_inst = ui_create_character_instance(base_character, Player_info.h_info->anim_set_name, 0x12);
            if (Player_info.char_inst != nullptr)
            {
                // Magic numbers: state 1, no action, a 0.3 s blend.
                Player_info.char_inst->set_state(index_alias(1, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                return true;
            }
        }
    }
    ui_character_release_model();
    return false;
}

// 0x008935A0 (:438); LTCG: the name in ESI.
static bool ui_character_load_weapon(const char* model_name)
{
    if (model_name == nullptr)
    {
        return false;
    }
    if (Weapon_info.requested)
    {
        if (stream2_get_status(Weapon_info.srid) == ST_LOADED)
        {
            Weapon_info.loaded = true;
            return true;
        }
        return false;
    }
    Weapon_info.i_info = object_item_get_info(model_name);
    if (Weapon_info.i_info != nullptr)
    {
        stream2_request(Weapon_info.i_info->srid);
        _snprintf_safe(Weapon_info.model_name, sizeof(Weapon_info.model_name), model_name); // Original: as a format string.
        Weapon_info.srid = Weapon_info.i_info->srid;
        Weapon_info.requested = true;
    }
    return false;
}

// 0x008C4920 (:465)
static bool ui_character_set_weapon()
{
    if (Weapon_info.loaded && Weapon_info.i_info != nullptr)
    {
        // Magic numbers: the character's tag 0x39 (the hand), the weapon's tag 2.
        Weapon_info.parent_attach_bone = 0x39;
        Weapon_info.attach_bone = 2;
        Weapon_info.smesh = static_mesh_find(Weapon_info.i_info->name, Weapon_info.srid);
        if (Weapon_info.smesh != nullptr)
        {
            Weapon_info.smesh_instance = ui_create_static_mesh(Weapon_info.smesh);
            if (Weapon_info.smesh_instance != nullptr)
            {
                return true;
            }
        }
    }
    ui_character_release_weapon();
    return false;
}

// 0x00893610 (:499); LTCG: model_name in ESI, base_name in EAX. The head is in the character's container: the base
// item names its anim set, the model its character.
static bool ui_character_load_head(const char* model_name, const char* base_name)
{
    if (model_name == nullptr)
    {
        return false;
    }
    if (Head_info.requested)
    {
        if (stream2_get_status(Head_info.srid) == ST_LOADED)
        {
            Head_info.loaded = true;
            return true;
        }
        return false;
    }
    Head_info.i_info = object_item_get_info(base_name);
    if (Head_info.i_info != nullptr && Player_info.srid != 0)
    {
        stream2_request(Player_info.srid);
        _snprintf_safe(Head_info.model_name, sizeof(Head_info.model_name), model_name); // Original: as a format string.
        Head_info.srid = Player_info.srid;
        Head_info.requested = true;
    }
    return false;
}

// 0x008C4990 (:528). The head's "head" bone, and the character's bone of the same name as the parent.
static bool ui_character_set_head()
{
    character* head_character = (Head_info.loaded && Head_info.i_info != nullptr) ? character_find(Head_info.model_name, Head_info.srid) : nullptr;
    if (head_character != nullptr)
    {
        // Magic number: tag 2 unless a "head" bone is found.
        Head_info.attach_bone = 2;
        // Magic number: render category 0x10.
        Head_info.char_inst = ui_create_character_instance(head_character, Head_info.i_info->anim_set_name, 0x10);
        if (Head_info.char_inst != nullptr)
        {
            animlib_set_parent_rig(Head_info.char_inst->aim_handle, Player_info.char_inst->aim_handle);
            const character_instance::resource_dependent_data* head_data = Head_info.char_inst->rdd.get_data();
            if (head_data != nullptr && head_data->renderable_instance_p != nullptr)
            {
                // Original: the character's data is read without a check that it is loaded.
                const character* body = Player_info.char_inst->rdd.get_data()->base_character;
                for (int i = 0; i < head_data->base_character->bones_used_info.num_bones; i++)
                {
                    if (v_stricmp(head_data->base_character->rig->bones[i].name.to_pointer(), "head") == 0)
                    {
                        Head_info.attach_bone = i;
                        break;
                    }
                }
                const char* bone_name = head_data->base_character->rig->bones[Head_info.attach_bone].name.to_pointer();
                for (int i = 0; i < body->bones_used_info.num_bones; i++)
                {
                    if (v_stricmp(body->rig->bones[i].name.to_pointer(), bone_name) == 0)
                    {
                        Head_info.parent_attach_bone = i;
                        break;
                    }
                }
                return true;
            }
        }
    }
    ui_character_release_head();
    return false;
}

// 0x00893690 (:649); LTCG: the name in ESI. The coat is in the character's container too.
static bool ui_character_load_coat(const char* model_name)
{
    if (model_name == nullptr)
    {
        return false;
    }
    if (Coat_info.requested)
    {
        if (stream2_get_status(Coat_info.srid) == ST_LOADED)
        {
            Coat_info.loaded = true;
            return true;
        }
        return false;
    }
    Coat_info.i_info = object_item_get_info(model_name);
    if (Coat_info.i_info != nullptr && Player_info.srid != 0)
    {
        stream2_request(Player_info.srid);
        _snprintf_safe(Coat_info.model_name, sizeof(Coat_info.model_name), model_name); // Original: as a format string.
        Coat_info.srid = Player_info.srid;
        Coat_info.requested = true;
    }
    return false;
}

// 0x00893710 (:679)
static bool ui_character_set_coat()
{
    if (Coat_info.loaded && Coat_info.i_info != nullptr)
    {
        character* base_character = character_find(Coat_info.model_name, Coat_info.srid);
        if (base_character != nullptr)
        {
            // Magic number: render category 0x10.
            Coat_info.char_inst = ui_create_character_instance(base_character, Coat_info.i_info->anim_set_name, 0x10);
            if (Coat_info.char_inst != nullptr)
            {
                animlib_set_parent_rig(Coat_info.char_inst->aim_handle, Player_info.char_inst->aim_handle);
                Coat_info.cloth_sim = cloth_sim_create(Coat_info.model_name, &Obj_pos, &Obj_orient, Coat_info.char_inst->aim_handle);
                return true;
            }
        }
    }
    ui_character_release_coat();
    return false;
}

// The render update of an attached part (process_head, process_coat), with a 20 m box around the character.
static bool ui_character_render_part(character_instance* char_inst)
{
    if (char_inst == nullptr)
    {
        return false;
    }
    const character_instance::resource_dependent_data* data = char_inst->rdd.get_data();
    if (data == nullptr || data->renderable_instance_p == nullptr)
    {
        return false;
    }
    // Magic numbers: a box of +-10 m.
    const vector bmin(-10.0f, -10.0f, -10.0f);
    const vector bmax(10.0f, 10.0f, 10.0f);
    ((rl_renderable_instance*)data->renderable_instance_p)->set_is_visible(true);
    char_inst->update_render(Obj_pos, Obj_orient, bmin, bmax, 0, false);
    return true;
}

// 0x008AFE40 (:717)
static bool process_head()
{
    return ui_character_render_part(Head_info.char_inst);
}

// 0x008AFED0 (:771)
static bool process_coat()
{
    return ui_character_render_part(Coat_info.char_inst);
}

// The animation of one instance into the bone matrices (scratch).
static void ui_character_animate(int aim_handle, matrix43* scratch)
{
    animlib_process_instance(aim_handle, Dtime);
    animlib_allocate_bones(aim_handle);
    animlib_transform_all_bones_pre(aim_handle, scratch, nullptr, 0);
    animlib_transform_all_bones_post(aim_handle, scratch, nullptr, 0);
}

// 0x008AFF60 (:791). The hammer group in the idle state, the queued action (with the swing sound), the animation and
// its root motion, then the head and the coat on the character's bones and the coat's cloth.
static bool process_character()
{
    character_instance* char_inst = Player_info.char_inst;
    if (char_inst == nullptr || !char_inst->rdd.is_loaded())
    {
        return false;
    }
    if (char_inst->get_animation_group() != ANIMATION_GROUP_HAMMER)
    {
        char_inst->set_animation_group(ANIMATION_GROUP_HAMMER, index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    if (char_inst->get_animation_state() != Anim_state)
    {
        char_inst->set_state(index_alias(Anim_state, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    if (Play_action != ANIMATION_ACTION_NONE)
    {
        char_inst->play_action(Play_action, ANIM_ALIAS_NONE, 1.0f, 0);
        Play_action = ANIMATION_ACTION_NONE;
        int cue_id;
        audiolib_cue_get_id("SYS_MENU_HAMMER_SWING", &cue_id);
        audiolib_cue_play(cue_id, nullptr, nullptr);
    }
    // Magic number: 64 bone matrices of scratch (the 64-matrix buffer of item::update_animlib, S9 note).
    matrix43 scratch[64];
    vanim_process_states(char_inst->vp);
    animlib_clear_bone_info(char_inst->aim_handle);
    ui_character_animate(char_inst->aim_handle, scratch);
    vanim_process_completed_actions(char_inst->vp);
    const character_instance::resource_dependent_data* data = char_inst->rdd.get_data();
    if (data != nullptr && data->renderable_instance_p != nullptr)
    {
        Obj_orient.rotate_around_local_y(animlib_get_heading_change(char_inst->vp->aim_handle, 0));
        vector action_delta;
        vector state_delta;
        vector local_delta;
        animlib_get_position_change(action_delta, state_delta, local_delta, char_inst->vp->aim_handle);
        vector delta(state_delta.x + action_delta.x, state_delta.y + action_delta.y, state_delta.z + action_delta.z);
        delta.unrotate(Obj_orient);
        Obj_pos.x = Obj_pos.x + delta.x;
        Obj_pos.y = Obj_pos.y + delta.y;
        Obj_pos.z = delta.z + Obj_pos.z;
        // Magic numbers: a box of +-10 m, LOD 1.
        const vector bmin(-10.0f, -10.0f, -10.0f);
        const vector bmax(10.0f, 10.0f, 10.0f);
        ((rl_renderable_instance*)data->renderable_instance_p)->set_is_visible(true);
        char_inst->update_render(Obj_pos, Obj_orient, bmin, bmax, 1, false);
        if (Head_info.char_inst != nullptr)
        {
            vanim_process_states(Head_info.char_inst->vp);
            ui_character_animate(Head_info.char_inst->aim_handle, scratch);
            vanim_process_completed_actions(Head_info.char_inst->vp);
        }
        if (Coat_info.char_inst != nullptr)
        {
            ui_character_animate(Coat_info.char_inst->aim_handle, scratch);
            matrix43* coat_matrices = nullptr;
            matrix43* body_matrices = nullptr;
            animlib_get_instanced_model_matrices(Coat_info.char_inst->aim_handle, &coat_matrices);
            animlib_get_instanced_model_matrices(Player_info.char_inst->aim_handle, &body_matrices);
            Coat_info.cloth_sim->pre_simulate(nullptr, coat_matrices, body_matrices, &Obj_pos, &Obj_orient);
            cloth_sim_batch_process_start();
            cloth_sim_batch_process_end();
        }
    }
    return true;
}

// 0x008B03A0 (:883). The hammer in the character's hand, or spinning on its own while the character is missing.
static bool process_weapon()
{
    rl_mesh_instance* instance = Weapon_info.smesh_instance;
    if (instance == nullptr)
    {
        return true;
    }
    vector pos = Obj_pos;
    matrix orient(Obj_orient);
    if (Player_info.char_inst == nullptr || Player_info.char_inst->aim_handle == 0)
    {
        // Magic number: 30 degrees a second about the heading.
        static vector heading(0.0f, 0.0f, 0.0f); // 0x02CA14D0 (guard 0x02CA14DC)
        heading.z = (float)((double)Dtime * 30.0 + (double)heading.z);
        orient.set(heading.x, heading.y, (float)((double)heading.z * Deg_to_rad));
        Obj_orient = orient;
    }
    else
    {
        vector tag_pos;
        matrix tag_orient;
        animlib_get_tag_world_coords(Player_info.char_inst->aim_handle, Weapon_info.parent_attach_bone, Obj_pos, Obj_orient, tag_pos, &tag_orient);
        pos = tag_pos;
        orient = tag_orient;
    }
    instance->set_is_visible(true);
    instance->set_position(pos);
    instance->set_rotation(orient);
    instance->set_lod_level(0);
    game_render_scene_update(instance);
    instance->set_opacity(1.0f);
    return true;
}

// 0x008B0670 (:928). The parts, the light, the wind and the effect go; the character leaves the screen.
void ui_character_shutdown_internal()
{
    game_render_thread_wait_for_prev_done(false);
    ui_character_release_weapon();
    ui_character_release_model();
    ui_character_release_head();
    ui_character_release_coat();
    game_render_set_TOD_light(nullptr);
    if (Render_light != nullptr)
    {
        Render_light->release();
        Render_light = nullptr;
    }
    weather_wind_stop();
    cloth_sim_set_always_windy(false);
    // rfg.exe reads Scene_main_p inline. Magic number: scene renderer flag 4 (the main scene into its temp target).
    game_render_get_main_scene()->get_scene_renderer()->m_flags &= ~4u;
    if (Hammer_hit_fx.vfx_effect_p != nullptr)
    {
        Hammer_hit_fx.vfx_effect_p->release();
        Hammer_hit_fx.vfx_effect_p = nullptr;
    }
    Obj_pos = Off_screen_pos;
}

// 0x008937C0 (:959). mm_hammer_hit_a at the effect position, white, started half a second late.
void ui_character_create_effect()
{
    const unsigned int handle = effects_get_handle("mm_hammer_hit_a");
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const effect_info* info = effects_get_effect_info(handle);
    if (info != nullptr)
    {
        Hammer_hit_fx.visual_effect.handle = info->visual_handle;
        Hammer_hit_fx.visual_effect.kill_particles_on_stop = info->flags.vfx_kill_particles_on_stop;
        Hammer_hit_fx.visual_effect.kill_particles_fade_time_s = info->vfx_kill_particles_fade_time_s;
        Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s = 0.0f;
    }
    // Original: the visual info is used without a null check.
    const visual_effect_info* visual = effects_get_visual_effect_info(Hammer_hit_fx.visual_effect.handle);
    Hammer_hit_fx.vfx_effect_p = vfx_effect_create(effects_template_find(visual->vfx_filename));
    vfx_effect* effect = Hammer_hit_fx.vfx_effect_p;
    if (effect == nullptr)
    {
        return;
    }
    // Magic number: visual flags 3.
    Hammer_hit_fx.visual_effect.flags |= 3;
    effect->set_effect_material_tint(rl_color_float(rl_color::WHITE));
    // Magic number: half a second before the start.
    effect->set_elapsed_time(-0.5f);
    effect->enable_emission(true);
    effect->enable_coronas(true);
    effect->set_position(Fx_pos);
    effect->set_rotation(Fx_orient);
    // Magic number: 1e-7 past the view dependent minimum time.
    effect->update(visual->m_lod.view_dependent_minimum_time + 1e-07f);
    Stop_emitting = false;
}

// 0x008939A0 (:992). Without the vfx a cube of the visual's radius (1 without one) at the effect position; with it the
// larger of the template's and the visual's radius in the effect's transform.
void ui_character_get_effect_bbox(vector& bmin, vector& bmax)
{
    const visual_effect_info* visual = effects_get_visual_effect_info(Hammer_hit_fx.visual_effect.handle);
    if (Hammer_hit_fx.vfx_effect_p == nullptr)
    {
        const float radius = visual != nullptr ? visual->radius : 1.0f;
        bmin.x = -radius;
        bmin.y = -radius;
        bmin.z = -radius;
        bmax.x = radius;
        bmax.z = radius;
        bmax.y = radius;
        bmin.x = bmin.x + Fx_pos.x;
        bmin.y = bmin.y + Fx_pos.y;
        bmin.z = bmin.z + Fx_pos.z;
        bmax.x = bmax.x + Fx_pos.x;
        bmax.y = Fx_pos.y + bmax.y;
        bmax.z = bmax.z + Fx_pos.z;
        return;
    }
    float radius = 0.0f;
    if (visual != nullptr)
    {
        radius = Hammer_hit_fx.vfx_effect_p->get_template()->get_bounding_radius();
        if (radius <= visual->radius)
        {
            radius = visual->radius;
        }
    }
    const vector box_max(radius, radius, radius);
    const vector box_min(radius * -1.0f, radius * -1.0f, radius * -1.0f);
    bbox_instance(bmin, bmax, Fx_orient, Fx_pos, box_min, box_max);
}

// 0x00893B80 (:1024). The first frame sets the bounds; the effect runs to its template's duration, stops emitting once
// (or fades its particles out).
void ui_character_update_effect(float d_time)
{
    Hammer_hit_fx.current_time = Hammer_hit_fx.current_time + d_time;
    Hammer_hit_fx.visual_effect.do_update_render = false;
    if (Hammer_hit_fx.vfx_effect_p == nullptr)
    {
        return;
    }
    if (Hammer_hit_fx.current_time - d_time == 0.0f)
    {
        bbox& box = Hammer_hit_fx.visual_effect.bbox_initial;
        ui_character_get_effect_bbox(box.min, box.max);
        Hammer_hit_fx.visual_effect.bbox_current = box;
        rl_aabb world_aabb;
        world_aabb.min = box.min;
        world_aabb.max = box.max;
        Hammer_hit_fx.vfx_effect_p->set_world_aabb(world_aabb);
        rl_sphere world_sphere;
        world_sphere.m_position.x = (box.max.x + box.min.x) * 0.5f;
        world_sphere.m_position.y = (box.max.y + box.min.y) * 0.5f;
        world_sphere.m_position.z = (box.max.z + box.min.z) * 0.5f;
        const float dx = box.max.x - box.min.x;
        const float dy = box.max.y - box.min.y;
        const float dz = box.max.z - box.min.z;
        world_sphere.m_radius = sqrtf(dx * dx + dy * dy + dz * dz) * 0.5f;
        Hammer_hit_fx.vfx_effect_p->set_world_bounding_sphere(world_sphere);
    }
    Hammer_hit_fx.visual_effect.do_update_render = true;
    Hammer_hit_fx.vfx_effect_p->update(d_time);
    if (Hammer_hit_fx.vfx_effect_p->get_elapsed_time() + d_time <= Hammer_hit_fx.vfx_effect_p->get_template()->get_duration())
    {
        Hammer_hit_fx.vfx_effect_p->set_elapsed_time(Hammer_hit_fx.vfx_effect_p->get_elapsed_time() + d_time);
    }
    else
    {
        Hammer_hit_fx.vfx_effect_p->release();
        Hammer_hit_fx.vfx_effect_p = nullptr;
    }
    // Original: the effect can be gone here (released above); rfg.exe uses it without a check. The effect starts at
    // -0.5 s, so it is still there on the one frame that stops it.
    if (!Stop_emitting)
    {
        Stop_emitting = true;
        if (!Hammer_hit_fx.visual_effect.kill_particles_on_stop)
        {
            Hammer_hit_fx.vfx_effect_p->enable_emission(false);
            Hammer_hit_fx.vfx_effect_p->enable_coronas(false);
        }
        else
        {
            Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s = Hammer_hit_fx.visual_effect.kill_particles_fade_time_s;
        }
    }
    if (Hammer_hit_fx.visual_effect.kill_particles_on_stop)
    {
        Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s = Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s - d_time;
        if (0.0f < Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s)
        {
            Hammer_hit_fx.vfx_effect_p->set_alpha_fade(Hammer_hit_fx.visual_effect.kill_particles_rem_fade_time_s / Hammer_hit_fx.visual_effect.kill_particles_fade_time_s);
            return;
        }
        if (Hammer_hit_fx.vfx_effect_p != nullptr)
        {
            Hammer_hit_fx.vfx_effect_p->release();
            Hammer_hit_fx.vfx_effect_p = nullptr;
        }
    }
}

// 0x008B0710 (:1095). The character on screen turned by Obj_y_orient, the wind for the coat, a white directional light
// at Light_pos (pointed from there at the character) and the scene's light, colour and blur for the menu.
void ui_character_init_resources()
{
    Obj_pos = On_screen_pos;
    Obj_orient.make_identity();
    Obj_orient.rotate_around_local_y((float)((double)Obj_y_orient * Deg_to_rad));
    weather_wind_start();
    // Magic number: -1, the wind's transition rate (at once).
    weather_wind_set_speed(Wind_speed, Wind_dir, -1.0f);
    weather_wind_update();
    cloth_sim_set_always_windy(true);
    Fx_orient.make_identity();
    rl_camera* camera = camera_get_main_render_camera();
    const vector to_character((float)((double)Obj_pos.x - (double)Light_pos.x), (float)((double)Obj_pos.y - (double)Light_pos.y), (float)((double)Obj_pos.z - (double)Light_pos.z));
    if (Render_light == nullptr)
    {
        Render_light = rl_light::create();
    }
    static rl_color_float light_color(1.0f, 1.0f, 1.0f, 1.0f); // 0x02CA14F0 (guard 0x02CA1500 bit 0)
    // Magic number: 0.75 grey.
    static rl_color_float ambient_color(0.75f, 0.75f, 0.75f, 1.0f); // 0x02CA14E0 (guard bit 1)
    Render_light->set_position(Light_pos);
    // Original: the light direction is the normalized light position, not to_character.
    vector direction = Light_pos;
    direction.normalize_safe();
    Render_light->set_light_direction(direction);
    Render_light->set_light_type(rl_light::LIGHT_TYPE_DIRECTIONAL);
    Render_light->set_color(light_color);
    // Magic number: 1500 m attenuation.
    Render_light->set_attenuation_start(1500.0f);
    Render_light->set_attenuation_end(1500.0f);
    // rfg.exe reads Scene_main_p inline.
    rl_scene* scene = game_render_get_main_scene();
    scene->set_ambient_color(ambient_color);
    // Magic numbers: a dark blue back ambient.
    scene->set_back_ambient_color(rl_color_float(0.05f, 0.05f, 0.075f, 1.0f));
    rl_particle_lighting_state particle_lighting = scene->get_particle_lighting_state();
    particle_lighting.m_ambient_color = rl_color_float(rl_color::WHITE);
    particle_lighting.m_tod_color = rl_color_float(rl_color::WHITE);
    particle_lighting.m_tod_light_dir = vector(1.0f, 0.0f, 0.0f);
    scene->set_particle_lighting_state(particle_lighting);
    // The gamma and levels are written back unchanged.
    rl_color_correct_state color_correct = scene->get_color_correct_state();
    color_correct.set_tint(rl_color_float(rl_color::WHITE));
    color_correct.set_saturation(1.0f);
    scene->set_color_correct_state(color_correct);
    camera->set_frame_override_blur(0.0f);
    camera->set_motion_blur_enabled(false);
    game_render_set_TOD_light(Render_light);
    matrix light_orient;
    light_orient.make(to_character);
    Render_light->set_rotation(light_orient);
    // Magic number: scene renderer flag 4 (the main scene into its temp target, the frame buffer the document shows).
    scene->get_scene_renderer()->m_flags |= 4;
    Obj_pos = On_screen_pos;
}

// 0x008D7100 (:1168); LTCG: the document in EAX. The main menu's background video dims, the menu's own models hide.
void ui_character_fade_main_menu(int dhandle)
{
    vint_doc_info* info = ui_get_doc_info(dhandle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_element_base* bg_video = (vint_element_base*)vint_object_base::find_object("bg_video", info->m_doc);
    vint_element_base* model = (vint_element_base*)vint_object_base::find_object("model", info->m_doc);
    vint_element_base* model2 = (vint_element_base*)vint_object_base::find_object("model2", info->m_doc);
    if (bg_video != nullptr)
    {
        // Magic number: alpha 0.4.
        bg_video->set_alpha(vint_variant(0.4f), 0);
    }
    if (model != nullptr)
    {
        model->set_visible(vint_variant(false), 0);
    }
    if (model2 != nullptr)
    {
        model2->set_visible(vint_variant(false), 0);
    }
}

// 0x00893F90 (:1222). The animation frame for the character, the delayed hammer hit effect, the effect.
void ui_character_process(float dtime)
{
    if (!Active_uichar)
    {
        return;
    }
    if (Player_info.char_inst != nullptr || Weapon_info.smesh_instance != nullptr)
    {
        Sim_snapshot.in_gameplay = true;
        Dtime = dtime;
        animlib_do_frame_pre_transform(true);
    }
    if (Fx_delay_timer.valid() && Fx_delay_timer.elapsed())
    {
        Hammer_hit_fx.current_time = 0.0f;
        if (Hammer_hit_fx.vfx_effect_p != nullptr)
        {
            Hammer_hit_fx.vfx_effect_p->release();
            Hammer_hit_fx.vfx_effect_p = nullptr;
        }
        ui_character_create_effect();
        Fx_delay_timer.invalidate();
    }
    ui_character_update_effect(dtime);
}

// 0x008C4B10 (:1248). Until all four parts are loaded: their requests, then their instances (all released when one
// fails). Then each frame: the parts, the debug camera input (Modify_camera_info, never set), the menu camera and a
// clear black fog; and the effect's render data.
void ui_character_process_internal()
{
    if (Player_info.loaded && Coat_info.loaded && Weapon_info.loaded && Head_info.loaded)
    {
        character_instance_animlib_matrices_do_frame();
        process_character();
        process_head();
        process_weapon();
        process_coat();
        Update_camera = true;
        if (Modify_camera_info)
        {
            // Magic numbers: 30 degrees a second.
            Rotation_delta = (float)((double)controls_get_analog(CAA_CAMERA_ROTATE, Player_controller_index) * 30.0);
            Obj_y_orient = Rotation_delta + Obj_y_orient;
            Obj_orient.rotate_around_local_y((float)((double)(Rotation_delta * Dtime) * Deg_to_rad));
            const float forward = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
            const float elevate = controls_get_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
            const float turn = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
            Cam_pos.x = Dtime * forward + Cam_pos.x;
            Cam_pos.y = Dtime * elevate + Cam_pos.y;
            Cam_pos.z = Dtime * turn + Cam_pos.z;
        }
        camera_set_pos(Cam_pos);
        matrix cam_orient;
        cam_orient.make(vector(1.0f, 0.0f, 0.0f));
        camera_set_orient(cam_orient);
        camera_do_frame_post();
        rl_camera* camera = camera_get_main_render_camera();
        // Magic numbers: clip planes 0.1 m and 200 m, a 48 degree view.
        camera->set_near_clip(0.1f);
        camera->set_far_clip(200.0f);
        camera->set_fov(48.0f);
        camera->set_position(Cam_pos);
        camera->set_rotation(cam_orient);
        // rfg.exe reads Scene_main_p inline. No fog: height density 0, atmosphere scale 1, density 0, offset 0, black.
        rl_fog_state fog;
        fog.set_density(0.0f);
        fog.set_color(rl_color_float(rl_color::BLACK));
        game_render_get_main_scene()->set_fog_state(fog);
        Update_camera = false;
    }
    else
    {
        const bool model = ui_character_load_model("rfg_PC");
        const bool head = ui_character_load_head("mason_head", "base_hires_head");
        const bool coat = ui_character_load_coat("mason_coat");
        const bool weapon = ui_character_load_weapon("sledgehammer");
        if (model && head && coat && weapon)
        {
            const bool model_set = ui_character_set_model();
            const bool head_set = ui_character_set_head();
            const bool coat_set = ui_character_set_coat();
            const bool weapon_set = ui_character_set_weapon();
            if (!(model_set && head_set && coat_set && weapon_set))
            {
                ui_character_release_model();
                ui_character_release_head();
                ui_character_release_coat();
                ui_character_release_weapon();
            }
        }
    }
    if (Hammer_hit_fx.vfx_effect_p != nullptr)
    {
        Hammer_hit_fx.vfx_effect_p->update_render(Frametime, Hammer_hit_fx.visual_effect.do_update_render, nullptr);
    }
}

// 0x00894050 (:1370)
void ui_character_set_visible(bool visible)
{
    Obj_pos = visible ? On_screen_pos : Off_screen_pos;
}

// 0x008D7220 (:1381). The uichar document's frame buffer element shows the main scene's temp target (-1 while the
// scene does not render into it; 0 when hidden).
void ui_character_set_frame_visible(bool visible)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_uichar);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* frame = vint_object_base::find_object(cFramebuffername, info->m_doc);
    if (frame == nullptr)
    {
        return;
    }
    frame->set_property_value("visible", vint_variant(visible), 0);
    vint_variant texture;
    if (visible)
    {
        // rfg.exe reads Scene_main_p inline.
        rl_scene_renderer* renderer = game_render_get_main_scene()->get_scene_renderer();
        if ((renderer->m_flags >> 2 & 1) == 0)
        {
            texture = 0xFFFFFFFFu;
        }
        else
        {
            texture = (unsigned int)renderer->m_main_scene_temp_target[0].get_texture_handle();
        }
    }
    else
    {
        texture = 0;
    }
    frame->set_property_value(cPropframebuftex, texture, 0);
}

// 0x008D7310 (:1402). A choice swings the hammer (the hit effect after Fx_delay ms): the main menu's second item with
// no utility document or message box up, or any options item but the fifth.
void ui_character_trigger(bool select, int menu_option)
{
    if (!select)
    {
        return;
    }
    const bool main_menu = gameseq_get_state() == GS_MAINMENU && (Utility_doc_handle == -1 || Message_box_info_set.msgbox_info_pool.get_capacity() == (unsigned int)Message_box_info_set.msgbox_info_pool.get_free()) && menu_option == 1;
    if (main_menu || (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1 && gameseq_get_state() == GS_MAINMENU_OPTIONS && menu_option != 4))
    {
        Play_action = ANIMATION_ACTION_MELEE2;
        Fx_delay_timer.set(Fx_delay);
    }
}
