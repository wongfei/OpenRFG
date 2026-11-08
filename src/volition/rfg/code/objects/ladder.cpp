// Original: volition/rfg/code/objects/ladder.cpp
//
// ladder (header comment in ladder.h). Compiled in rfg_objects_unity.cpp (after human.cpp and human_move.cpp, whose
// file statics get_default_collision_filter and human_move_set_mode it uses).
//
// A ladder's rungs are 0.25 m apart; rung 6 is the one at the ladder's origin (the computed rung of a human is
// 6 + 4 * (its height above the ladder + 0.3), clamped at 0). The climb moves 3 rungs a cycle (the left and right idle
// states alternate), the slide down stops at the rung picked when the stick lets go. In multiplayer the human's
// ladder state and rung go to the other peers (player::mp_ladder_state, set_mp_ladder_state).

#include "volition/rfg/code/objects/ladder.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

#include <math.h>

static const int LADDER_POOL_SIZE = 128;
static const int LADDER_POOL_USAGE = 0x56C0; // memory footprint registered with the instantiator
// Magic numbers: the rung spacing, the rung at the ladder's origin and the rungs of a climb cycle.
static const float LADDER_RUNG_HEIGHT = 0.25f;
static const int LADDER_BASE_RUNG = 6;
static const int LADDER_CLIMB_RUNGS = 3;

// ---- file-static globals
// const float PI (0x01310C80) is used here too; placed in human_spinebend.cpp.
// const float min_correction_diff (0x01315520) is used here too; placed in human.cpp.
static float Ladder_move_speed = 2.0f; // 0x016944F8
static float Ladder_mp_move_speed = 4.0f; // 0x016944FC
static float Ladder_destroyed_threshold = 0.025f; // 0x01694500
static vector Ladder_rc_offset(0.0f, -0.125f, -0.555f); // 0x0169563C, the hands on a rung relative to the ladder
static vector Ladder_rc_slide_offset(0.005f, -0.037f, -0.35f); // 0x01695648, the same while sliding
// player* Player (0x03023874) is used here too; placed in player.h.
// int Ladder_slide_loop_cue (0x03024E70) is used here too; placed in human.cpp.
// int Ladder_slide_land_cue (0x030269C0) is used here too; placed in human.cpp.
static object_pool<ladder, LADDER_POOL_SIZE, 1> ladderpool; // 0x03169690
static obj_class_instantiator ladder_instantiator("ladder", alloc_ladder, types_ladder, bpo_index_ladder, classname_ladder, ladder::pool_remaining, LADDER_POOL_USAGE, LADDER_POOL_SIZE); // 0x0304266C (0x01075BA0)
// tweak_table.xtbl overrides the ladder speeds (not read by the ported ladder code).
static tweak_table_item Tweak_table_item_Ladder_move_speed("Ladder_move_speed", &Ladder_move_speed); // 0x0306E540
static tweak_table_item Tweak_table_item_Ladder_mp_move_speed("Ladder_mp_move_speed", &Ladder_mp_move_speed); // 0x0328B734

// ---- internal functions
static void ladder_start_slide_sound(human* hp); // 0x00A369F0 (:71)
static int ladder_compute_current_rung(const human* hp, const ladder* lp); // 0x00A36A70 (:98)
static void set_mp_ladder_state(human* hp, ladder_state_enum state, int cur_rung); // 0x00A12740 (:106)
static float get_stop_dist(float speed); // 0x00A128D0 (:468)
static bool mp_should_force_dismount(human* hp, ladder* lp); // 0x00A5E4F0 (:482)

// The ladder of a handle (nullptr unless a ladder), as the original's inlined object_from_handle checks it.
static ladder* ladder_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_LADDER ? (ladder*)objp : nullptr;
}

// The character controller's phantom collision filter after a ladder state change (inlined in the original).
static void ladder_update_collision_filter(human* hp)
{
    if (!hp->rdd.is_loaded())
    {
        return;
    }
    const unsigned int filter = get_default_collision_filter(hp);
    hkpShapePhantom* phantom;
    if (hp->rdd.is_loaded() && (phantom = hp->rdd.get_data()->controller->m_phantom) != nullptr)
    {
        phantom->getCollidableRw()->getBroadPhaseHandle()->m_collisionFilterInfo = filter;
        havok_phantom_update_collision_filter(phantom, true);
    }
}

// 0x00A369F0 (:71): the slide loop, at the human unless it is the local player.
static void ladder_start_slide_sound(human* hp)
{
    audiolib_params params;
    params.volume = 1.1754944e-38f; // Original: FLT_MIN as the volume (the slide raises it by speed)
    if (hp == nullptr || hp != Player)
    {
        params.pos = hp->pos;
        params.flags |= 1; // 3D
    }
    hp->ladder_slide_play_id = audiolib_cue_play(Ladder_slide_loop_cue, &params, nullptr);
}

// 0x00A36A70 (:98): the rung the human is at (see the file comment), at least 0.
static int ladder_compute_current_rung(const human* hp, const ladder* lp)
{
    // Magic numbers: 0.3 m above the rung, 4 rungs a meter (the sign as in the original).
    const int rung = LADDER_BASE_RUNG - (int)(((hp->pos.y - lp->pos.y) + 0.3f) * -4.0f);
    return rung < 0 ? 0 : rung;
}

// 0x00A12740 (:106)
static void set_mp_ladder_state(human* hp, ladder_state_enum state, int cur_rung)
{
    if (Multiplayer)
    {
        // Original: the human is a player in multiplayer (no npcs there).
        player* pp = (player*)hp;
        pp->mp_ladder_state.l_state = (unsigned char)state;
        pp->mp_ladder_state.l_rung = (unsigned char)cur_rung;
    }
}

// ---- the header inlines (.h:26)
// 0x006C70A0
const object_type ladder::get_type_static()
{
    return OT_LADDER;
}

// 0x006C70B0
const object_sub_type ladder::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06980
const char* ladder::get_classname_static()
{
    return "ladder";
}

// 0x00A06990: the ladder broadphase layer (World.collect_objects<ladder>).
int ladder::get_bpo_index_static()
{
    return 1;
}

// 0x00A127D0
const char* ladder::get_classname() const
{
    return "ladder";
}

// ---- class registration (:120)
// 0x00A12770
void types_ladder(signed char& type, signed char& subtype)
{
    type = OT_LADDER;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A12780
int bpo_index_ladder()
{
    return 1;
}

// 0x00A12790
const char* classname_ladder()
{
    return "ladder";
}

// 0x00A127A0
unsigned int ladder::get_classname_hash()
{
    return ladder_instantiator.this_hash;
}

// 0x00A36AC0
int ladder::pool_remaining()
{
    return ladderpool.get_free();
}

// 0x00A5E4E0
object* alloc_ladder()
{
    return ladderpool.alloc(0);
}

// 0x00A8A6A0
void ladder::pool_free()
{
    ladderpool.free(this);
}

// 0x00A127B0 (:121)
ladder::ladder()
{
}

// 0x00A127E0
ladder::~ladder()
{
}

// 0x00AB34F0 (:133)
// Loaded: the rungs (kept when missing), enabled (true when missing), not in use. Saved: the rungs and enabled.
bool ladder::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(LADDER_PROP_RUNGS, rungs);
        pblock.set(LADDER_ENABLED, ladder_enabled);
        return true;
    }
    pblock.get(LADDER_PROP_RUNGS, rungs);
    if (!pblock.get_optional(LADDER_ENABLED, ladder_enabled))
    {
        ladder_enabled = true;
    }
    in_use = false;
    use_count = 0;
    return true;
}

// 0x00A127F0 (:161): a disabled ladder is saved (its state differs from the level's).
obj_serialize_mode ladder::serialize_mode()
{
    return (obj_serialize_mode)(OSM_RESTORE + (ladder_enabled ? 0 : 1));
}

// 0x00A36AF0 (:171)
bool ladder::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A12800 (:186)
bool ladder::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A12810 (:191)
bool ladder::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    return true;
}

// 0x00A12820 (:196)
void ladder::get_bbox(vector& bmin, vector& bmax) const
{
    vector local_min;
    vector local_max;
    get_local_bbox(local_min, local_max);
    bbox_instance(bmin, bmax, orient, pos, local_min, local_max);
}

// 0x00A12870 (:208): 1.7 m wide and deep, as high as the rungs.
void ladder::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmax.x = 0.85f;
    bmax.z = 0.85f;
    bmax.y = (float)rungs * LADDER_RUNG_HEIGHT;
    bmin.x = -0.85f;
    bmin.y = 0.0f;
    bmin.z = -0.85f;
}

// ---- humans on ladders

// 0x00AC4300 (:215)
// Off the ladder: gravity back on, a player's movement velocity cleared (a remote one may not mount again for 150 ms),
// the ladder's use count down, the velocity overrides off, standing, the default collision filter, the weapon the
// mount holstered equipped again (an npc marks the re-equip), the character scale back and the slide sound stopped.
void human_dismount_ladder(human* hp)
{
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->flags.disable_gravity = 0;
    }
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        player* pp = (player*)hp;
        pp->movement_velocity.zero();
        if (pp != Player)
        {
            // Magic number: 150 ms before a remote player may mount again.
            pp->mp_ladder_state.remote_ladder_mount_delay.set(150);
        }
    }
    ladder* lp = ladder_from_handle(hp->ladder_handle);
    // Original: no null check (a ladder destroyed under the human is written through a null pointer).
    if (lp != nullptr)
    {
        lp->use_count--;
    }
    hp->hflags.use_current_velocity = 0;
    hp->char_inst->vp->flags.use_velocity = 0;
    hp->hflags.is_on_ladder = 0;
    hp->steering_vector.zero();
    hp->ladder_handle = 0xFFFFFFFF;
    hp->move_speed = -1.0f;
    hp->cur_state = HS_STANDARD;
    ladder_update_collision_filter(hp);
    if (hp->hflags.ladder_reequip_weapon)
    {
        human_equip_inventory_item(hp, hp->last_equipped_weapon);
        hp->hflags.ladder_reequip_weapon = 0;
        if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)hp)->npc_flags.reequipping_weapon = 1;
        }
    }
    if (hp->char_inst != nullptr)
    {
        hp->char_inst->character_scale = hp->char_inst->orig_character_scale;
    }
    audiolib_play_stop(hp->ladder_slide_play_id);
    hp->ladder_slide_play_id = -1;
}

// 0x00AC4470 (:258): jumps off to the side the human holds the ladder with (no jump while a ladder action plays),
// then dismounts and equips the item it wants. True when a jump plays.
bool human_ladder_jump(human* hp)
{
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_LADDER_ACTION))
    {
        return false;
    }
    bool jumped = false;
    animation_action action = ANIMATION_ACTION_NONE;
    bool have_action = true;
    if (hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_IDLE_LEFT))
    {
        action = ANIMATION_ACTION_LADDER_JUMP_LEFT;
    }
    else if (hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_IDLE_RIGHT))
    {
        action = ANIMATION_ACTION_LADDER_JUMP_RIGHT;
    }
    else
    {
        have_action = false;
    }
    if (have_action)
    {
        hp->char_inst->play_action(action, ANIM_ALIAS_JUMP, 1.0f, 0);
        jumped = true;
    }
    human_dismount_ladder(hp);
    hp->set_equipped_inv_item(hp->desired_equipped_inv_item);
    return jumped;
}

// 0x00AB8AE0 (:275)
// On the ladder (nothing while reloading). A player leaves its zoom (the local one its scope and the camera view) and
// cover. Gravity off, the ladder state, the ladder idle animation state, the ladder collision filter; an enabled
// ladder holsters the weapon (re-equipped on the dismount). Direct movement, no fall, no aim. Then the entry, by
// where the human is:
// - at the top (above the middle, or not on the ground, and within 0.4 m of the top): onto the top facing down (a
//   disabled ladder jumps down instead), sliding;
// - above the middle or in the air: grabs the nearest climb rung from the side it comes from (front, left, right or
//   the diagonals), the rung picked so the climb cycle lines up;
// - from the ground: one of three entries by rung, facing the ladder.
// The action moves the human to the entry's place over 0.2 s; the humans next to the ladder are asked to make room.
void human_mount_ladder(human* hp, ladder* lp)
{
    if (vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_RELOAD))
    {
        return;
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        player* pp = (player*)hp;
        turret* tp;
        if (pp->zoom_state != PZS_OFF && ((tp = human_combat_get_manned_turret(pp)) == nullptr || !tp->t_info->flags.force_fine_aim))
        {
            pp->previous_zoom_state = pp->zoom_state;
            pp->zoom_state = PZS_OFF;
            if (pp == Player)
            {
                pp->get_equipped_weapon(); // Original: the result is not used.
                if (pp->previous_zoom_state == PZS_SCOPE)
                {
                    pp->set_opacity_plus_child_items(1.0f);
                    pp->set_casts_transparent_shadows_plus_child_items(false);
                    hud_ui_rail_driver_set_visible(false);
                    hud_ui_sniper_set_visible(false);
                }
                camera_set_view_player(PZS_OFF, false);
            }
        }
        if (hp->cur_state == HS_COVER)
        {
            player_exit_cover(pp, false);
        }
    }
    const bool was_running = hp->cur_movement_state > HMS_WALK;
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->flags.disable_gravity = 1;
    }
    hp->hflags.is_on_ladder = 1;
    hp->hflags.ladder_force_exit = 0;
    hp->hflags.ladder_force_slide = 0;
    hp->ladder_grab_rung = -1;
    hp->ladder_handle = lp->get_handle();
    hp->move_speed = -1.0f;
    hp->cur_state = HS_LADDER;
    hp->cur_movement_state = HMS_IDLE;
    hp->char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    ladder_update_collision_filter(hp);
    hp->hflags.bashed_character_controller = 0;
    hp->hflags.ladder_reequip_weapon = 0;
    if (human_weapons_get_weapon(hp) != nullptr && lp->is_enabled())
    {
        hp->hflags.ladder_reequip_weapon = 1;
        human_weapons_immediately_holster_weapon(hp);
    }
    animlib_clear_non_looping_animations(hp->char_inst->aim_handle);
    set_mp_ladder_state(hp, MP_LADDER_STATE_IDLE, ladder_compute_current_rung(hp, lp)); // inlined
    human_move_set_mode(hp, HMM_DIRECT, HMSM_NONE, true); // inlined
    if (hp->char_inst != nullptr)
    {
        hp->char_inst->orig_character_scale = hp->char_inst->character_scale;
        hp->char_inst->character_scale = 1.0f;
    }
    hp->hflags.is_falling = 0;
    hp->combat.combat_aim_timestamp.invalidate();
    hp->combat.combat_ready_timestamp.invalidate();
    const vector& lpos = lp->pos;
    const float top_offset = lp->pos.y - hp->pos.y;
    hp->steering_vector.zero();
    vector dest_pos;
    matrix dest_orient;
    const int rungs = lp->rungs;
    if (lp->pos.y + (float)rungs * LADDER_RUNG_HEIGHT * 0.5f <= hp->pos.y || hp->ground_obj_handle == 0xFFFFFFFF)
    {
        // Magic number: within 0.4 m of the top.
        if (fabsf(top_offset + (float)rungs * LADDER_RUNG_HEIGHT) < 0.4f)
        {
            if (!lp->is_enabled())
            {
                hp->char_inst->play_action(was_running ? ANIMATION_ACTION_JUMP_FROM_BUILDING : ANIMATION_ACTION_JUMP_DOWN, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
            }
            else
            {
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_ENTER_TOP, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                ladder_start_slide_sound(hp);
            }
            hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_SLIDE_DOWN, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            set_mp_ladder_state(hp, MP_LADDER_STATE_SLIDE_DOWN, lp->rungs);
            // Magic numbers: the top entry 0.48 m in front of the ladder, 0.05 m to its left, facing it.
            vector p;
            p.x = (lp->orient.fvec.x * 0.48f + lpos.x) - lp->orient.rvec.x * 0.05f;
            p.y = (lpos.y + lp->orient.fvec.y * 0.48f) - lp->orient.rvec.y * 0.05f;
            p.z = (lpos.z + lp->orient.fvec.z * 0.48f) - lp->orient.rvec.z * 0.05f;
            dest_pos.x = p.x;
            dest_pos.y = (float)lp->rungs * LADDER_RUNG_HEIGHT + p.y;
            dest_pos.z = p.z;
            dest_orient = lp->orient;
            dest_orient.rotate_around_local_y(3.1415927f);
        }
        else
        {
            // Magic numbers: the hands' rung (0.001 against rounding down), lined up with the climb cycle (+3000 keeps
            // the remainder's operand positive), at least the 9th rung.
            int rung = (int)(((hp->pos.y - lp->pos.y) - Ladder_rc_offset.y) * 4.0f + 6.0f + 0.001f);
            rung = rung - ((rung - rungs) + 3000) % LADDER_CLIMB_RUNGS;
            float grab_rung = (float)rungs;
            if (rung <= rungs)
            {
                grab_rung = (float)rung;
                if (rung < LADDER_BASE_RUNG)
                {
                    grab_rung = (float)(rung + LADDER_CLIMB_RUNGS);
                }
            }
            vector hand = Ladder_rc_offset;
            hand.unrotate(lp->orient);
            const float up = (float)((int)grab_rung - LADDER_BASE_RUNG);
            vector grab;
            grab.x = up * lp->orient.uvec.x * LADDER_RUNG_HEIGHT + lpos.x + hand.x;
            grab.z = lpos.z + lp->orient.uvec.z * up * LADDER_RUNG_HEIGHT + hand.z;
            grab.y = lp->orient.uvec.y * up * LADDER_RUNG_HEIGHT + lpos.y + hand.y + 0.15f;
            const float facing = hp->orient.fvec.y * lp->orient.rvec.y + lp->orient.rvec.x * hp->orient.fvec.x + hp->orient.fvec.z * lp->orient.rvec.z;
            vector rel = hp->pos - lpos;
            rel.rotate(lp->orient);
            dest_orient = lp->orient;
            // Magic numbers: the sides 0.3 m off the ladder's axis, 45 degree facings (0.707), the grab offsets of
            // each entry animation.
            enum
            {
                GRAB_LEFT,
                GRAB_RIGHT,
                GRAB_FRONT,
                GRAB_FRONT_LEFT,
                GRAB_FRONT_RIGHT,
            } grab_kind;
            if (rel.x <= 0.3f)
            {
                if (rel.x < -0.3f)
                {
                    grab_kind = rel.y < -0.3f ? GRAB_FRONT_LEFT : GRAB_RIGHT;
                }
                else if (facing > 0.707f)
                {
                    grab_kind = GRAB_LEFT;
                }
                else if (facing < -0.707f)
                {
                    grab_kind = GRAB_RIGHT;
                }
                else
                {
                    grab_kind = GRAB_FRONT;
                }
            }
            else
            {
                grab_kind = rel.y < -0.3f ? GRAB_FRONT_RIGHT : GRAB_LEFT;
            }
            switch (grab_kind)
            {
            case GRAB_FRONT_LEFT:
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_AIR_GRAB_FRONT_LEFT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                dest_orient.rotate_around_local_y(0.7853982f);
                grab.x = (-lp->orient.rvec.x * 0.437f - lp->orient.fvec.x * 0.056f) + grab.x;
                grab.y = (-lp->orient.rvec.y * 0.437f - lp->orient.fvec.y * 0.056f) + grab.y;
                grab.z = (-lp->orient.rvec.z * 0.437f - lp->orient.fvec.z * 0.056f) + grab.z;
                break;
            case GRAB_RIGHT:
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_AIR_GRAB_RIGHT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                grab.x = -lp->orient.rvec.x * 0.5f + grab.x;
                grab.y = -lp->orient.rvec.y * 0.5f + grab.y;
                grab.z = -lp->orient.rvec.z * 0.5f + grab.z;
                grab.y = grab.y - 0.15f;
                break;
            case GRAB_FRONT:
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_AIR_GRAB_FRONT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                grab.x = grab.x - lp->orient.fvec.x * 0.153f;
                grab.y = grab.y - lp->orient.fvec.y * 0.153f;
                grab.z = grab.z - lp->orient.fvec.z * 0.153f;
                break;
            case GRAB_FRONT_RIGHT:
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_AIR_GRAB_FRONT_RIGHT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                dest_orient.rotate_around_local_y(-0.7853982f);
                grab.x = (lp->orient.rvec.x * 0.458f - lp->orient.fvec.x * 0.056f) + grab.x;
                grab.y = (lp->orient.rvec.y * 0.458f - lp->orient.fvec.y * 0.056f) + grab.y;
                grab.z = (lp->orient.rvec.z * 0.458f - lp->orient.fvec.z * 0.056f) + grab.z;
                break;
            case GRAB_LEFT:
                hp->char_inst->play_action(ANIMATION_ACTION_LADDER_AIR_GRAB_LEFT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                grab.x = lp->orient.rvec.x * 0.5f + grab.x;
                grab.y = lp->orient.rvec.y * 0.5f + grab.y;
                grab.z = lp->orient.rvec.z * 0.5f + grab.z;
                grab.y = grab.y - 0.15f;
                break;
            }
            hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_IDLE_LEFT, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            dest_pos = grab;
        }
    }
    else
    {
        // From the ground: the entry by rung (Original: the rung goes through rungs - (int)(...) and back).
        const float entry_rung = (float)(rungs - (int)(((hp->pos.y - lp->pos.y) + 0.125f) * 4.0f));
        const int entry = ((int)entry_rung - LADDER_BASE_RUNG) % LADDER_CLIMB_RUNGS;
        if (entry == 0)
        {
            hp->char_inst->play_action(ANIMATION_ACTION_LADDER_ENTER_A, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
        }
        else if (entry == 1)
        {
            hp->char_inst->play_action(ANIMATION_ACTION_LADDER_ENTER_B, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
        }
        else if (entry == 2)
        {
            hp->char_inst->play_action(ANIMATION_ACTION_LADDER_ENTER_C, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
        }
        // Magic number: 0.59 m in front of the ladder.
        vector p;
        p.x = lpos.x - lp->orient.fvec.x * 0.59f;
        p.y = lp->pos.y - lp->orient.fvec.y * 0.59f;
        p.z = lp->pos.z - lp->orient.fvec.z * 0.59f;
        dest_pos.x = p.x;
        dest_pos.y = (float)(lp->rungs - (int)entry_rung) * LADDER_RUNG_HEIGHT + p.y;
        dest_pos.z = p.z;
        dest_orient = lp->orient;
        hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_IDLE_LEFT, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    hp->char_inst->set_action_starting_orient(hp->pos, hp->orient, dest_pos, dest_orient, ANIM_ALIAS_LADDER_ACTION, 0.2f);
    hp->steering_vector.zero();
    lp->use_count++;
    // The humans around the ladder (its box 1 m larger, up to 0.9 m high) make room.
    vector bmin;
    vector bmax;
    lp->get_local_bbox(bmin, bmax);
    bmin.x -= 1.0f;
    bmin.y -= 1.0f;
    bmin.z -= 1.0f;
    bmax.x += 1.0f;
    bmax.z += 1.0f;
    bmax.y = 0.9f;
    unsigned int handles[5];
    const int num = World.collect_objects<human>(bmin, bmax, lpos, lp->orient, handles, 5, nullptr, nullptr);
    for (int i = 0; i < num; i++)
    {
        if (handles[i] == 0xFFFFFFFF)
        {
            continue;
        }
        human* other = (human*)World.object_from_handle(handles[i]);
        if (other != nullptr && other->get_obj_type() == OT_HUMAN && other != hp)
        {
            other->request_avoidance(hp, other->pos - hp->pos, ZERO_VECTOR);
        }
    }
}

// 0x00A128D0 (:468): the stopping distance of a slide at a speed (none up to 0.625 m/s).
// Original: LTCG compiled it with the speed in XMM0 and the result on the x87 stack; ladder_process_human inlines it.
static float get_stop_dist(float speed)
{
    // Magic numbers: 0.390625 = 0.625^2, 40 = twice the 20 m/s^2 slide deceleration.
    if (speed > 0.625f)
    {
        return (speed * speed - 0.390625f) / 40.0f;
    }
    return 0.0f;
}

// 0x00A5E4F0 (:482): the local player gives way when another player on the same ladder is less than 1.6 m above it.
// Original: LTCG passes the ladder in EAX (the decompile shows the arguments swapped).
static bool mp_should_force_dismount(human* hp, ladder* lp)
{
    if (lp->get_use_count() > 1 && Multiplayer && hp != nullptr && hp == Player)
    {
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            player* other = World.get_object<player>(i);
            if (other != hp && other->ladder_handle == lp->get_handle() && other->hflags.is_on_ladder)
            {
                // Magic number: 1.6 m (a body height) above.
                const float above = other->pos.y - hp->pos.y;
                if (above > 0.0f && above < 1.6f)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// 0x00AC4520 (:505)
// Every frame on a ladder (the vertical stick: up climbs, down slides). Off when the ladder is gone, disabled (its
// building gone or moving) or an exit finished; then a non-climbing npc falls. A local multiplayer player jumps off
// under another one. The human turns to the ladder (a turn a second). While a ladder action plays, the human is held
// at its rung (the climb steps also pull it onto the ladder's line); otherwise:
// - not sliding: up climbs one cycle (3 rungs) or exits at the top, down starts a slide (from the 8th rung up) or
//   exits at the bottom when moving down, nothing idles; held at the hands' rung;
// - sliding: the speed (0 to 5 m/s) follows the stick or, when it lets go, slows to stop at the next climb rung within
//   the stopping distance (stopping there with the idle state); the slide sound follows the speed; at the bottom (or
//   on the ground) the human lands or jumps off. Held on the ladder's line.
void ladder_process_human(human* hp, float vertical_stick_val)
{
    ladder* lp = ladder_from_handle(hp->ladder_handle);
    hp->hflags.pushes_other_humans = 1;
    // Magic number: supported for 300 ms.
    hp->last_supported.set(300);
    // A ladder without a building, or on a moving one, is disabled. Original: lp is not checked before its parent is
    // read (a null pointer read when the ladder is gone); checked here.
    object* parent = lp != nullptr ? World.object_from_handle(lp->get_parent()) : nullptr;
    if (lp != nullptr && (parent == nullptr || (parent->get_obj_type() == OT_MOVER && parent->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)parent)->get_move_type() != RFGM_FIXED)))
    {
        lp->set_enabled(false);
    }
    if (!hp->char_inst->is_playing_action(ANIM_ALIAS_LADDER_ACTION) && hp->hflags.ladder_force_exit)
    {
        lp = nullptr;
    }
    // Magic number: the first 30 frames of a ladder action (the entry) keep a non-climbing npc on.
    const bool entering = hp->char_inst->is_playing_action(ANIM_ALIAS_LADDER_ACTION) && vanim_get_anim_frame(hp->char_inst->vp, ANIM_ALIAS_LADDER_ACTION) < 30;
    const bool not_climbing_npc = !hp->pf.flags.climbing_up && hp->get_sub_type() != OT_SUB_HUMAN_PLAYER;
    const bool entering_npc = entering && not_climbing_npc;
    if (lp == nullptr || (!lp->is_enabled() && !entering_npc))
    {
        if (hp->char_inst->is_playing_action(ANIM_ALIAS_LADDER_ACTION))
        {
            hp->char_inst->stop_action(ANIM_ALIAS_LADDER_ACTION, 3, false);
        }
        human_dismount_ladder(hp);
        if (lp == nullptr || !not_climbing_npc)
        {
            return;
        }
        hp->hflags.is_falling = 1;
        hp->move_speed = -1.0f;
        hp->cur_state = HS_FALL;
        return;
    }
    if (mp_should_force_dismount(hp, lp))
    {
        human_ladder_jump(hp);
        // Magic number: 500 ms before the local player may mount again.
        ((player*)hp)->mp_ladder_state.local_ladder_mount_delay.set(500);
    }
    // Turn towards the ladder, a full turn a second at most.
    const float heading = hp->orient.extract_heading();
    const float delta = fl_angle_cap_radians(lp->orient.extract_heading() - heading, -3.1415927f);
    float turn = 0.0f;
    if (!hp->char_inst->vp->flags.adjusting_orient)
    {
        if (delta > 0.0f)
        {
            turn = Frametime * 6.2831855f;
            if (!(turn < delta) && turn != delta)
            {
                turn = delta;
            }
        }
        else if (delta < 0.0f)
        {
            turn = -Frametime * 6.2831855f;
            if (!(delta < turn) && turn != delta)
            {
                turn = delta;
            }
        }
        hp->orient.rotate_around_local_y(turn);
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_LADDER_ACTION))
    {
        if (hp->char_inst->is_playing_action(ANIMATION_ACTION_LADDER_SLIDE_STOP))
        {
            // Settle onto the rung: a quarter of the way a frame.
            const int rung = ladder_compute_current_rung(hp, lp);
            const float to_rung = (((float)(rung - LADDER_BASE_RUNG) * LADDER_RUNG_HEIGHT + lp->pos.y + Ladder_rc_offset.y) - hp->pos.y) / Frametime;
            hp->velocity.x = 0.0f;
            hp->velocity.y = to_rung * 0.25f;
            hp->velocity.z = 0.0f;
            hp->hflags.use_current_velocity = 1;
        }
        hp->char_inst->vp->flags.use_velocity = 1;
        hp->ladder_slide_speed = -hp->actual_velocity.y;
        if (!hp->char_inst->is_playing_action(ANIMATION_ACTION_LADDER_CLIMB_LEFT) && !hp->char_inst->is_playing_action(ANIMATION_ACTION_LADDER_CLIMB_RIGHT))
        {
            return;
        }
        // A climb step: a fifth of the way onto the ladder's line a frame.
        vector hand = Ladder_rc_offset;
        hand.unrotate(lp->orient);
        const float pull = 0.2f / Frametime;
        hp->hflags.use_current_velocity = 1;
        hp->velocity.x = pull * ((lp->pos.x + hand.x) - hp->pos.x);
        hp->velocity.y = pull * 0.0f;
        hp->velocity.z = ((lp->pos.z + hand.z) - hp->pos.z) * pull;
        return;
    }
    const int rung = ladder_compute_current_rung(hp, lp); // inlined
    if (!hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_SLIDE_DOWN))
    {
        // Held at the hands' rung: a fifth of the way a frame.
        vector hold = Ladder_rc_offset;
        hold.unrotate(lp->orient);
        const float up = (float)(rung - LADDER_BASE_RUNG);
        hold.x = lp->pos.x + up * lp->orient.uvec.x * LADDER_RUNG_HEIGHT + hold.x;
        hold.y = hold.y + lp->pos.y + lp->orient.uvec.y * up * LADDER_RUNG_HEIGHT;
        hold.z = hold.z + lp->pos.z + lp->orient.uvec.z * up * LADDER_RUNG_HEIGHT;
        hp->velocity.x = ((hold.x - hp->pos.x) * 0.2f) / Frametime;
        hp->velocity.y = ((hold.y - hp->pos.y) * 0.2f) / Frametime;
        hp->velocity.z = ((hold.z - hp->pos.z) * 0.2f) / Frametime;
        // Magic number: a 0.05 stick dead zone.
        if (vertical_stick_val > 0.05f)
        {
            if (Multiplayer)
            {
                set_mp_ladder_state(hp, MP_LADDER_STATE_CLIMB, rung); // inlined
            }
            if (rung < lp->rungs)
            {
                if (hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_IDLE_LEFT))
                {
                    hp->char_inst->play_action(ANIMATION_ACTION_LADDER_CLIMB_LEFT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                    hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_IDLE_RIGHT, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                }
                else
                {
                    hp->char_inst->play_action(ANIMATION_ACTION_LADDER_CLIMB_RIGHT, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                    hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_IDLE_LEFT, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                }
            }
            else
            {
                hp->char_inst->play_action(hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_IDLE_LEFT) ? ANIMATION_ACTION_LADDER_EXIT_TOP_LEFT : ANIMATION_ACTION_LADDER_EXIT_TOP_RIGHT,
                                           ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                hp->char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                hp->hflags.ladder_force_exit = 1;
            }
        }
        else if (vertical_stick_val < -0.05f)
        {
            set_mp_ladder_state(hp, MP_LADDER_STATE_SLIDE_DOWN, rung);
            // Magic number: a slide from the 8th rung up.
            if (rung > 7)
            {
                hp->char_inst->play_action(hp->char_inst->is_in_state(ANIMATION_STATE_LADDER_IDLE_LEFT) ? ANIMATION_ACTION_LADDER_SLIDE_START_LEFT : ANIMATION_ACTION_LADDER_SLIDE_START_RIGHT,
                                           ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                hp->ladder_slide_speed = 0.0f;
                hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_SLIDE_DOWN, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                hp->hflags.ladder_force_slide = 1;
                hp->ladder_grab_rung = -1;
                ladder_start_slide_sound(hp);
            }
            else
            {
                // The bottom rungs exit; the exit animation only plays when moving down faster than 0.5 m/s.
                if (hp->actual_velocity.y < -0.5f)
                {
                    hp->char_inst->play_action(ANIMATION_ACTION_LADDER_EXIT_BOTTOM, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
                }
                hp->char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                hp->hflags.ladder_force_exit = 1;
            }
        }
        else
        {
            set_mp_ladder_state(hp, MP_LADDER_STATE_IDLE, rung);
        }
        hp->steering_vector.zero();
        hp->hflags.use_current_velocity = 1;
        hp->char_inst->vp->flags.use_velocity = 1;
        return;
    }
    // Sliding. Magic numbers: 5 m/s at most, 20 m/s^2 towards the target speed.
    float slide_speed = hp->ladder_slide_speed;
    float target_speed = 5.0f;
    if (slide_speed - 0.0f < 0.0f)
    {
        slide_speed = 0.0f;
    }
    if (0.0f <= slide_speed - 5.0f)
    {
        slide_speed = 5.0f;
    }
    hp->ladder_slide_speed = slide_speed;
    if (hp->hflags.ladder_force_slide || vertical_stick_val < -0.05f)
    {
        set_mp_ladder_state(hp, MP_LADDER_STATE_SLIDE_DOWN, rung);
        hp->ladder_grab_rung = -1;
    }
    else
    {
        const float hands_rung = (((hp->pos.y - lp->pos.y) - Ladder_rc_offset.y) * 4.0f) + 6.0f;
        int grab_rung = hp->ladder_grab_rung;
        if (grab_rung == -1)
        {
            // The climb rung at or below the hands, one cycle lower when the slide cannot stop before it.
            const int at = (int)(hands_rung + 0.001f);
            grab_rung = at - ((at - lp->rungs) + 3000) % LADDER_CLIMB_RUNGS;
            if (lp->rungs < grab_rung)
            {
                grab_rung = lp->rungs;
            }
            if ((hands_rung - (float)grab_rung) * LADDER_RUNG_HEIGHT < get_stop_dist(slide_speed)) // inlined
            {
                grab_rung -= LADDER_CLIMB_RUNGS;
            }
            hp->ladder_grab_rung = grab_rung;
        }
        // Slower near the grab rung, stopped within 5 cm (the idle state again).
        const float to_grab = (hands_rung - (float)grab_rung) * LADDER_RUNG_HEIGHT;
        float speed;
        if (to_grab > 0.25f)
        {
            speed = 2.5f;
        }
        else if (to_grab > 0.125f)
        {
            speed = 1.25f;
        }
        else if (to_grab > 0.05f)
        {
            speed = 0.625f;
        }
        else
        {
            hp->char_inst->play_action(ANIMATION_ACTION_LADDER_SLIDE_STOP, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
            hp->char_inst->set_state(index_alias(ANIMATION_STATE_LADDER_IDLE_LEFT, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            hp->ladder_slide_speed = 0.0f;
            audiolib_play_stop(hp->ladder_slide_play_id);
            speed = 0.0f;
            hp->ladder_slide_play_id = -1;
        }
        // Magic number: 1/30 s (the speed for a 30 Hz frame).
        target_speed = (speed * 0.033333335f) / Frametime;
        set_mp_ladder_state(hp, MP_LADDER_STATE_SLIDE_STOP, hp->ladder_grab_rung);
    }
    hp->hflags.ladder_force_slide = 0;
    slide_speed = hp->ladder_slide_speed;
    if (target_speed < slide_speed)
    {
        slide_speed = slide_speed - Frametime * 20.0f;
        hp->ladder_slide_speed = slide_speed;
        if (0.0f <= slide_speed - target_speed)
        {
            target_speed = slide_speed;
        }
        hp->ladder_slide_speed = target_speed;
    }
    else if (slide_speed < target_speed)
    {
        slide_speed = Frametime * 20.0f + slide_speed;
        hp->ladder_slide_speed = slide_speed;
        if (slide_speed - target_speed < 0.0f)
        {
            target_speed = slide_speed;
        }
        hp->ladder_slide_speed = target_speed;
    }
    if (hp->ladder_slide_play_id != -1)
    {
        if (!object_is_local_player(hp))
        {
            audiolib_play_set_3d_position(hp->ladder_slide_play_id, &hp->pos, nullptr, nullptr);
        }
        // Magic number: full volume at 5 m/s, over 100 ms.
        audiolib_play_set_volume(hp->ladder_slide_play_id, hp->ladder_slide_speed / 5.0f, 100);
    }
    float side = 0.0f;
    // Magic number: the bottom 5 cm.
    if (hp->pos.y < lp->pos.y + 0.05f || hp->ground_obj_handle != 0xFFFFFFFF)
    {
        // Ground within a meter below the human (0.2 m up): it lands, otherwise it jumps off.
        raycast_input in;
        in.start.x = hp->pos.x;
        in.start.y = hp->pos.y + 0.2f;
        in.start.z = hp->pos.z;
        in.end.x = hp->pos.x - hp->orient.uvec.x;
        in.end.y = (hp->pos.y + 0.2f) - hp->orient.uvec.y;
        in.end.z = in.start.z - hp->orient.uvec.z;
        in.radius = 0.0f;
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        in.m_filter_info = COLLIDE_RAY_DEFAULT;
        raycast_output out;
        if (havok_raycast(in, out))
        {
            hp->char_inst->play_action(ANIMATION_ACTION_LADDER_EXIT_BOTTOM, ANIM_ALIAS_LADDER_ACTION, 1.0f, 0);
            hp->char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            hp->hflags.ladder_force_exit = 1;
            hp->ladder_slide_speed = 0.0f;
            audiolib_play_stop(hp->ladder_slide_play_id);
            hp->ladder_slide_play_id = -1;
            if (object_is_local_player(hp))
            {
                audiolib_cue_play(Ladder_slide_land_cue, nullptr, nullptr);
            }
            else
            {
                audiolib_cue_play_3d(Ladder_slide_land_cue, &hp->pos, nullptr, nullptr, nullptr);
            }
        }
        else
        {
            human_ladder_jump(hp);
        }
    }
    hp->velocity.x = side;
    hp->velocity.y = -hp->ladder_slide_speed;
    hp->velocity.z = side;
    hp->steering_vector.zero();
    hp->hflags.use_current_velocity = 1;
    hp->char_inst->vp->flags.use_velocity = 1;
    // On the slide line: a fifth of the way a frame.
    vector slide = Ladder_rc_slide_offset;
    slide.unrotate(lp->orient);
    const float pull = 0.2f / Frametime;
    hp->velocity.x = pull * ((lp->pos.x + slide.x) - hp->pos.x) + hp->velocity.x;
    hp->velocity.y = pull * 0.0f + hp->velocity.y;
    hp->velocity.z = ((lp->pos.z + slide.z) - hp->pos.z) * pull + hp->velocity.z;
}

// 0x00A12900 (:827)
// Disabling a ladder also breaks its pathfinding link: the closer of the two ladder navpoints (types 12 and 13) with
// a link to a ladder navpoint becomes a jump-down point (19) over its partner, which becomes the landing (20).
// Magic numbers: the navpoint types (no recovered enum).
void ladder::set_enabled(bool enable)
{
    ladder_enabled = enable;
    if (enable)
    {
        return;
    }
    navpoint* other = nullptr;
    navpoint* np = rfg_pf_find_closest_navpoint_to_pos(pos, other, 12, 13);
    if (np == nullptr)
    {
        return;
    }
    int i = 0;
    for (; i < np->num_links; i++)
    {
        if (np->links[i]->type == 13 || np->links[i]->type == 12)
        {
            break;
        }
    }
    if (i >= np->num_links)
    {
        return;
    }
    if (np->type != 13 || np->links[i]->type != 13)
    {
        return;
    }
    navpoint* partner = np->links[i];
    if (partner->pos.y < np->pos.y)
    {
        np->type = 19;
        np->pos.x = partner->pos.x;
        np->pos.z = partner->pos.z;
        partner->type = 20;
        return;
    }
    partner->type = 19;
    partner->pos.x = np->pos.x;
    partner->pos.z = np->pos.z;
    np->type = 20;
}

// 0x00A36B10 (:884)
// A damaged building's ladder: enabled while the building is undamaged, disabled once more than
// Ladder_destroyed_threshold of it is gone or it moves (a non-fixed rfg_mover); in between, unchanged.
event_result ladder_building_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const building_damaged_event& damaged = (const building_damaged_event&)in_event;
    if (damaged.building == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* objp = World.object_from_handle(damaged.building);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return EVENT_IGNORED;
    }
    rfg_mover* building = (rfg_mover*)objp;
    const bool undamaged = building->percent_destroyed() == 0.0f;
    const bool destroyed = building->percent_destroyed() > Ladder_destroyed_threshold;
    if (!undamaged && !destroyed)
    {
        return EVENT_IGNORED;
    }
    object* first = building->child_ptr;
    object* child = first;
    if (child == nullptr)
    {
        return EVENT_OK;
    }
    while (child->get_obj_type() != OT_LADDER)
    {
        child = child->child_next;
        if (child == first || child == nullptr)
        {
            return EVENT_OK;
        }
    }
    bool enable = undamaged;
    rfg_mover* mover = World.object_cast<rfg_mover>(building);
    if (mover != nullptr && mover->get_move_type() != RFGM_FIXED)
    {
        enable = false;
    }
    ((ladder*)child)->set_enabled(enable);
    return EVENT_OK;
}

// 0x00A36C10 (:962): the rung of a human on a ladder by handle (0 without either).
int ladder_compute_current_rung(const human* hp, unsigned int ladder_handle)
{
    const ladder* lp = ladder_from_handle(ladder_handle);
    if (hp != nullptr && lp != nullptr)
    {
        return ladder_compute_current_rung(hp, lp); // inlined
    }
    return 0;
}

// 0x00A12A10 (:974): inlined into human_system_init (human.cpp), which the game calls instead.
void ladder_system_init()
{
    audiolib_cue_get_id("MOV_FOLEY_LADDER_LOOP_01", &Ladder_slide_loop_cue);
    audiolib_cue_get_id("MOV_FOLEY_LADDER_RELEASE_01", &Ladder_slide_land_cue);
}

// 0x00A36C80 (:983): inlined into human_level_init (human.cpp), which the game calls instead.
void ladder_level_init()
{
    notify_info ni;
    ni.data = nullptr;
    ni.type = EVENT_BUILDING_DAMAGED;
    ni.callback = ladder_building_damaged_callback;
    gem_register_for_event(ni);
}
