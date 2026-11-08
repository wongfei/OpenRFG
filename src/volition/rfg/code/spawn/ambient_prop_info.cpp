// Original: volition/rfg/code/spawn/ambient_prop_info.cpp
//
// Ambient props (stub_impl.md S9): Ambient_Prop_Info.xtbl, the props NPCs hold and the effects their animations start.
// Linked by OpenRFG only. Build only: nothing here has run.

#include "volition/rfg/code/spawn/ambient_prop_info.h"

#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

// Magic numbers: rfg.exe's pools and table sizes.
static object_pool<ambient_prop_info,20,1> Ambient_prop_info_pool; // 0x0374AD30
static object_pool<ambient_prop_effect_info,20,1> Ambient_prop_effect_pool; // 0x0374BC40
static farray<prop_effect_tracking_info,20> Ambient_prop_animation_effects; // 0x016B69A0
static const char* AMBIENT_PROP_ATTACH_POINT_NAMES[4] = { "weapon_handle", "r-hand", "l-hand", "destruction" }; // 0x016B6960
// Magic number: a detached prop is deleted after 10 s.
static const int DETACHED_PROP_DELETION_MS = 10000;

// 0x00B40840
static const char* find_local_attach_point_name(const char* new_name)
{
    for (int i = 0; i < 4; i++)
    {
        if (v_stricmp(new_name, AMBIENT_PROP_ATTACH_POINT_NAMES[i]) == 0)
        {
            return AMBIENT_PROP_ATTACH_POINT_NAMES[i];
        }
    }
    return nullptr;
}

// 0x00B40880 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
// 0x00B408C0 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
// 0x00B40900 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
static bool start_trigger_elapsed(const human* hp, const ambient_prop_effect_info* effect_info)
{
    if (effect_info->anim_action != ANIMATION_ACTION_NONE)
    {
        return hp->char_inst->is_playing_action(effect_info->anim_action) && (effect_info->start_tag_id < 0 || vanim_has_trigger_just_elapsed(hp->char_inst->vp, effect_info->start_tag_id, effect_info->anim_action, -1));
    }
    if (effect_info->anim_state != ANIMATION_STATE_NONE && hp->char_inst->is_in_state(effect_info->anim_state))
    {
        return effect_info->start_tag_id < 0 || vanim_has_trigger_just_elapsed(hp->char_inst->vp, effect_info->start_tag_id, effect_info->anim_state, -1);
    }
    return false;
}

// 0x00B40980 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
// 0x00B409C0 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
// 0x00B40A00 (LTCG: hp in EDI, effect_info in ESI; the state and action forms inlined here)
static bool end_trigger_elapsed(const human* hp, const ambient_prop_effect_info* effect_info)
{
    if (effect_info->anim_action != ANIMATION_ACTION_NONE)
    {
        if (!hp->char_inst->is_playing_action(effect_info->anim_action))
        {
            return true;
        }
        return effect_info->end_tag_id > -1 && vanim_has_trigger_just_elapsed(hp->char_inst->vp, effect_info->end_tag_id, effect_info->anim_action, -1);
    }
    if (effect_info->anim_state == ANIMATION_STATE_NONE)
    {
        return true;
    }
    // kept: an effect on a state does not end when the state ends, only at its end tag.
    return hp->char_inst->is_in_state(effect_info->anim_state) && effect_info->end_tag_id > -1 && vanim_has_trigger_just_elapsed(hp->char_inst->vp, effect_info->end_tag_id, effect_info->anim_state, -1);
}

// 0x00B46C60 (:19)
ambient_prop_effect_info::ambient_prop_effect_info()
{
    reset();
}

// 0x00B40A80 (:358)
void ambient_prop_effect_info::reset()
{
    anim_state = ANIMATION_STATE_NONE;
    anim_action = ANIMATION_ACTION_NONE;
    effect_handle = 0xFFFFFFFF;
    attach_point_name = nullptr;
    start_tag_id = -1;
    end_tag_id = -1;
    effect_flags.loop_effect = 0;
    effect_flags.attach_effct = 0;
}

// 0x00B40AA0 (:369)
bool ambient_prop_effect_info::load(xml_element* xml_root)
{
    reset();
    xml_element* state = xml_find(xml_root, "anim_state_name");
    xml_element* action = xml_find(xml_root, "anim_action_name");
    if (state != nullptr)
    {
        anim_state = vanim_lookup_state(state->text);
        if (anim_state == ANIMATION_STATE_NONE)
        {
            return false;
        }
    }
    else if (action != nullptr)
    {
        anim_action = vanim_lookup_action(action->text);
        if (anim_action == ANIMATION_ACTION_NONE)
        {
            return false;
        }
    }
    else
    {
        // Original: Int3 critical function here.
        return false;
    }
    xml_element* element = xml_find(xml_root, "anim_effect_type");
    if (element == nullptr)
    {
        return false;
    }
    effect_handle = effects_get_handle(element->text);
    if (effect_handle == 0xFFFFFFFF || (element = xml_find(xml_root, "anim_effect_attach_name")) == nullptr)
    {
        return false;
    }
    attach_point_name = find_local_attach_point_name(element->text);
    if (attach_point_name == nullptr)
    {
        return false;
    }
    if ((element = xml_find(xml_root, "anim_start_tag")) != nullptr)
    {
        start_tag_id = animlib_get_trigger_id(element->text);
    }
    if ((element = xml_find(xml_root, "anim_end_tag")) != nullptr)
    {
        end_tag_id = animlib_get_trigger_id(element->text);
    }
    if ((element = xml_find(xml_root, "looping_effect")) == nullptr)
    {
        return false;
    }
    effect_flags.loop_effect = xml_get_bool(element, false);
    if ((element = xml_find(xml_root, "attach_effect")) == nullptr)
    {
        return false;
    }
    effect_flags.attach_effct = xml_get_bool(element, false);
    return true;
}

// 0x00B46C80 (:51)
prop_effect_tracking_info::prop_effect_tracking_info()
{
    reset();
}

// 0x00B40C40 (:432)
void prop_effect_tracking_info::reset()
{
    human_handle = 0xFFFFFFFF;
    item_handle = 0xFFFFFFFF;
    effect_handle = 0xFFFFFFFF;
    effect_info = nullptr;
}

// 0x00B4B9B0 (.h:38)
ambient_prop_info::ambient_prop_info()
{
    reset();
}

// 0x00B40C60 (:460)
bool ambient_prop_info::is_streamed() const
{
    if (prop_item_info == nullptr)
    {
        return false;
    }
    const stream2_status_val status = stream2_get_status(prop_item_info->srid);
    return status == ST_LOADING || status == ST_LOADED;
}

// 0x00B46D70 (:444)
void ambient_prop_info::reset()
{
    prop_item_info = nullptr;
    human_attach_point_name = nullptr;
    child_attach_point_name = nullptr;
    prop_walk_state = -1;
    prop_stand_state = -1;
    prop_exit_anim = ANIMATION_ACTION_NONE;
    exit_anim_trigger_id = -1;
    perm_effect_attach_name = nullptr;
    perm_effect_handle = 0xFFFFFFFF;
    effect_info_list.clear();
    prop_flags.delete_on_exit = 0;
}

// An animation name in the table as an anim handle; false when the anim is missing.
static bool load_prop_anim(xml_element* xml_root, const char* tag, int& state)
{
    xml_element* element = xml_find(xml_root, tag);
    if (element == nullptr)
    {
        return true;
    }
    const anim* a = anim_find(element->text);
    if (a == nullptr)
    {
        return false;
    }
    state = anim_get_handle(a);
    return state != -1;
}

// 0x00B547E0 (:474)
bool ambient_prop_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr || (prop_item_info = object_item_get_info(element->text)) == nullptr)
    {
        return false;
    }
    if ((element = xml_find(xml_root, "human_attach_point")) == nullptr || (human_attach_point_name = find_local_attach_point_name(element->text)) == nullptr)
    {
        return false;
    }
    if ((element = xml_find(xml_root, "obj_attach_point")) == nullptr || (child_attach_point_name = find_local_attach_point_name(element->text)) == nullptr)
    {
        return false;
    }
    if (!load_prop_anim(xml_root, "walk_anim_name", prop_walk_state) || !load_prop_anim(xml_root, "stand_anim_name", prop_stand_state))
    {
        return false;
    }
    if ((element = xml_find(xml_root, "exit_animation")) != nullptr)
    {
        prop_exit_anim = vanim_lookup_action(element->text);
        if (prop_exit_anim == ANIMATION_ACTION_NONE)
        {
            return false;
        }
        if ((element = xml_find(xml_root, "exit_anim_trigger")) != nullptr)
        {
            exit_anim_trigger_id = animlib_get_trigger_id(element->text);
        }
    }
    if ((element = xml_find(xml_root, "delete_on_exit")) == nullptr)
    {
        return false;
    }
    prop_flags.delete_on_exit = xml_get_bool(element, false);
    xml_element* always_on = xml_find(xml_root, "always_on_effect");
    if (always_on != nullptr)
    {
        if ((element = xml_find(always_on, "perm_effect_type")) == nullptr)
        {
            return false;
        }
        perm_effect_handle = effects_get_handle(element->text);
        if (perm_effect_handle == 0xFFFFFFFF)
        {
            return false;
        }
        // kept: perm_effect_attach_name is not checked for null.
        perm_effect_attach_name = find_local_attach_point_name(xml_find(always_on, "perm_effect_attach_name")->text);
        if (perm_effect_attach_name == nullptr)
        {
            return false;
        }
    }
    xml_element* list = xml_find(xml_root, "anim_effect_list");
    if (list == nullptr)
    {
        return true;
    }
    for (xml_element* entry = xml_find(list, "anim_effect_info"); entry != nullptr; entry = xml_find_next(list, entry, "anim_effect_info"))
    {
        ambient_prop_effect_info* effect = Ambient_prop_effect_pool.alloc(0);
        if (effect == nullptr)
        {
            break;
        }
        if (effect->load(entry))
        {
            const ambient_prop_effect_info* loaded = effect;
            effect_info_list.add(loaded);
        }
        else
        {
            effect->reset();
            Ambient_prop_effect_pool.free(effect);
        }
    }
    return effect_info_list.size() > 0;
}

// 0x00B40C90 (:586)
void create_prop_effect(item* ip, const ambient_prop_info* amb_prop_info)
{
    if (ip == nullptr || amb_prop_info == nullptr)
    {
        return;
    }
    const int host_tag = ip->get_tag_index_from_name(amb_prop_info->perm_effect_attach_name);
    if (host_tag > -1)
    {
        effects_play(amb_prop_info->perm_effect_handle, ip->get_handle(), host_tag, true, 0xFFFFFFFF, 1.0f, false);
    }
}

// 0x00B59470 (:604)
void amb_prop_system_init()
{
    xml_element* root = xml_table_parse("Ambient_Prop_Info.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Ambient_prop_info_pool.clear();
    Ambient_prop_effect_pool.clear();
    for (xml_element* entry = xml_find(root, "amb_prop_info"); entry != nullptr; entry = xml_find_next(root, entry, "amb_prop_info"))
    {
        ambient_prop_info* info = Ambient_prop_info_pool.alloc(0);
        if (info != nullptr && !info->load(entry))
        {
            Ambient_prop_info_pool.free(info);
        }
    }
    xml_table_close(root);
}

// 0x00B46DA0 (:632)
void amb_prop_level_init_post()
{
    Ambient_prop_animation_effects.clear();
}

// 0x00B46DB0 (:640)
item* amb_prop_create(const ambient_prop_info* amb_prop_info, const vector& pos, const matrix& orient, unsigned int parent_handle)
{
    if (amb_prop_info == nullptr)
    {
        return nullptr;
    }
    obj_item_flags flags; // 0x6024: rfg.exe's mask
    flags.simulate = 1;
    flags.no_save = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_item_props props(amb_prop_info->prop_item_info, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* ip = World.create_object<item>(props, parent_handle);
    if (ip != nullptr && amb_prop_info->perm_effect_handle != 0xFFFFFFFF)
    {
        create_prop_effect(ip, amb_prop_info);
    }
    return ip;
}

// 0x00B4BB90 (:668)
bool amb_prop_create(const ambient_prop_info* amb_prop_info, human* hp)
{
    if (amb_prop_info == nullptr || hp == nullptr || !hp->obj_flags.streamed)
    {
        return false;
    }
    if (hp->amb_prop_info != nullptr)
    {
        // kept: a human holding a different prop whose item is gone gets nothing, and keeps the info.
        if (amb_prop_human_find(hp) != nullptr || amb_prop_info != hp->amb_prop_info)
        {
            return false;
        }
        hp->amb_prop_info = nullptr;
    }
    const int parent_prop_point = hp->get_tag_index_from_name(amb_prop_info->human_attach_point_name);
    if (parent_prop_point < 0)
    {
        return false;
    }
    obj_item_flags flags;
    flags.no_physics = 1;
    flags.simulate = 1;
    flags.item_held = 1;
    flags.no_save = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_item_props props(amb_prop_info->prop_item_info, hp->pos, hp->orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* ip = World.create_object<item>(props, 0xFFFFFFFF);
    if (ip == nullptr)
    {
        return false;
    }
    if (!ip->obj_flags.streamed)
    {
        World.destroy_object(ip, false);
        return false;
    }
    if (amb_prop_info->perm_effect_handle != 0xFFFFFFFF)
    {
        create_prop_effect(ip, amb_prop_info);
    }
    const int child_prop_point = ip->get_tag_index_from_name(amb_prop_info->child_attach_point_name);
    ip->set_owner(hp->get_handle(), true);
    hp->set_child(ip, parent_prop_point, child_prop_point, false, true);
    hp->amb_prop_info = amb_prop_info;
    return true;
}

// 0x00B40CE0 (:742)
bool amb_prop_mount(human* hp, const ambient_prop_info* amb_prop_info, item* ip)
{
    if (hp == nullptr || amb_prop_info == nullptr || ip == nullptr)
    {
        return false;
    }
    const int parent_prop_point = hp->get_tag_index_from_name(amb_prop_info->human_attach_point_name);
    if (parent_prop_point < 0)
    {
        return false;
    }
    const int child_prop_point = ip->get_tag_index_from_name(amb_prop_info->child_attach_point_name);
    ip->item_flags.no_physics = 1;
    ip->item_flags.no_save = 1;
    ip->item_flags.item_held = 1;
    if (ip->get_parent() != 0xFFFFFFFF)
    {
        ip->remove_from_parent();
    }
    ip->set_owner(hp->get_handle(), true);
    hp->set_child(ip, parent_prop_point, child_prop_point, false, true);
    hp->amb_prop_info = amb_prop_info;
    return true;
}

// 0x00B4BCF0 (:782)
const ambient_prop_info* amb_prop_info_find(const char* prop_name)
{
    const ambient_prop_info* found = nullptr;
    Ambient_prop_info_pool.thread_access.enter();
    for (ambient_prop_info* info = Ambient_prop_info_pool.get_first(0); info != nullptr; info = Ambient_prop_info_pool.get_next(info))
    {
        if (v_stricmp(prop_name, info->prop_item_info->name) == 0)
        {
            found = info;
            break;
        }
    }
    Ambient_prop_info_pool.thread_access.leave();
    return found;
}

// 0x00B46E60 (:797)
item* amb_prop_human_find(const human* hp)
{
    if (hp == nullptr || hp->amb_prop_info == nullptr)
    {
        return nullptr;
    }
    object* first = hp->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ITEM && hp->amb_prop_info->prop_item_info == ((item*)child)->info)
        {
            return (item*)child;
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00B46EA0 (:828)
void amb_prop_detach_props(human* hp, bool delete_prop)
{
    item* ip = amb_prop_human_find(hp);
    if (ip != nullptr)
    {
        hp->remove_child(ip);
        ip->item_flags.item_held = 0;
        ip->unset_owner(true);
        ip->deletion_timer.set(DETACHED_PROP_DELETION_MS);
        if (delete_prop || hp->amb_prop_info->prop_flags.delete_on_exit)
        {
            World.destroy_object(ip, false);
        }
    }
    hp->amb_prop_info = nullptr;
}

// 0x00B46CC0 (LTCG: hp in ECX, anim_effect_info in EAX)
static bool should_cancel_animation_effct(const human* hp, const prop_effect_tracking_info& anim_effect_info)
{
    if (hp->get_handle() != anim_effect_info.human_handle)
    {
        return false;
    }
    if (anim_effect_info.item_handle == 0xFFFFFFFF || World.object_from_handle<item>(anim_effect_info.item_handle) == nullptr)
    {
        return true;
    }
    if (anim_effect_info.effect_handle == 0xFFFFFFFF || World.object_from_handle<object_effect>(anim_effect_info.effect_handle) == nullptr)
    {
        return true;
    }
    return end_trigger_elapsed(hp, anim_effect_info.effect_info);
}

// 0x00B46D20 (LTCG: anim_effect_info in ESI)
static void cancel_animation_effct(prop_effect_tracking_info& anim_effect_info)
{
    if (anim_effect_info.effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(anim_effect_info.effect_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    anim_effect_info.reset();
}

// 0x00B4BA10 (LTCG: hp in EAX and EDI, prop_anim_info on the stack)
static bool should_start_animation_effect(const human* hp, const ambient_prop_effect_info* prop_anim_info)
{
    return start_trigger_elapsed(hp, prop_anim_info) && amb_prop_human_find(hp) != nullptr;
}

// 0x00B4BA30 (LTCG: prop_anim_info in EDI)
static void start_animation_effect(human* hp, const ambient_prop_effect_info* prop_anim_info)
{
    if (Ambient_prop_animation_effects.full())
    {
        return;
    }
    for (int i = 0; i < Ambient_prop_animation_effects.size(); i++)
    {
        if (hp->get_handle() == Ambient_prop_animation_effects[i].human_handle)
        {
            return;
        }
    }
    item* ip = amb_prop_human_find(hp);
    if (ip == nullptr)
    {
        return;
    }
    const int tag = ip->get_tag_index_from_name(prop_anim_info->attach_point_name);
    if (tag < 0)
    {
        return;
    }
    const bool looping = prop_anim_info->effect_flags.loop_effect;
    prop_effect_tracking_info tracking;
    tracking.human_handle = hp->get_handle();
    tracking.item_handle = ip->get_handle();
    tracking.effect_info = prop_anim_info;
    if (prop_anim_info->effect_flags.attach_effct)
    {
        tracking.effect_handle = effects_play(prop_anim_info->effect_handle, ip->get_handle(), tag, looping, 0xFFFFFFFF, 1.0f, false);
    }
    else
    {
        vector tag_pos;
        matrix tag_orient;
        ip->get_tag_world_coords(tag, tag_pos, &tag_orient, nullptr, nullptr);
        tracking.effect_handle = effects_play(prop_anim_info->effect_handle, tag_pos, tag_orient, looping, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    if (tracking.effect_handle != 0xFFFFFFFF)
    {
        Ambient_prop_animation_effects.add(tracking);
    }
}

// 0x00B4BD70 (:860)
void amb_prop_do_frame()
{
    for (int i = 0; i < Ambient_prop_animation_effects.size(); i++)
    {
        prop_effect_tracking_info& tracking = Ambient_prop_animation_effects[i];
        const human* hp = tracking.human_handle != 0xFFFFFFFF ? World.object_from_handle<human>(tracking.human_handle) : nullptr;
        if (hp == nullptr || hp->is_killed() || should_cancel_animation_effct(hp, tracking))
        {
            cancel_animation_effct(tracking);
        }
    }
    for (int i = 0; i < Ambient_prop_animation_effects.size();)
    {
        if (Ambient_prop_animation_effects[i].human_handle == 0xFFFFFFFF)
        {
            Ambient_prop_animation_effects.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->amb_prop_info == nullptr)
        {
            continue;
        }
        for (int j = 0; j < hp->amb_prop_info->effect_info_list.size(); j++)
        {
            const ambient_prop_effect_info* effect_info = hp->amb_prop_info->effect_info_list[j];
            if (!should_start_animation_effect(hp, effect_info))
            {
                continue;
            }
            for (int k = 0; k < Ambient_prop_animation_effects.size(); k++)
            {
                if (hp->get_handle() == Ambient_prop_animation_effects[k].human_handle)
                {
                    cancel_animation_effct(Ambient_prop_animation_effects[k]);
                    break;
                }
            }
            start_animation_effect(hp, effect_info);
            break; // one effect per human per frame
        }
    }
}
