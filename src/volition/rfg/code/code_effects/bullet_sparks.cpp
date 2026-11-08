// Original: volition/rfg/code/code_effects/bullet_sparks.cpp
//
// The impact effect of a bullet (VFX.md FX4 item 13): the effect material's bullet impact effect per shape type and
// bullet group, overridden by a mover's level object properties (whole mover or a script point), or the no-blood flesh
// effect where the localization forbids blood.

#include "volition/rfg/code/code_effects/bullet_sparks.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"

// ---- file-static globals
static unsigned int Human_hit_no_blood_effect = 0xFFFFFFFF; // 0x01648E98
// The impact speed at which the effect plays at full volume (rfg.exe .data 0x0164A98C: 10.0).
static float BULLET_SPARK_MAX_VOLUME_IMPACT = 10.0f; // 0x0164A98C

// 0x006E6C90 (:43)
void bullet_spark_create(const vector& pos, const vector& direction, const weapon_info* w_info, unsigned char material_type, material_shape_type shape_type, const object* hit_obj, unsigned int hit_shape_key, float speed)
{
    const human* hp = nullptr;
    if (hit_obj != nullptr && hit_obj->get_obj_type() == OT_HUMAN)
    {
        hp = (const human*)hit_obj;
        if (hp->is_invulnerable())
        {
            return;
        }
    }

    // The effect faces along the direction with its up and forward axes turned: up = forward, forward = -up.
    matrix orient;
    orient.make(direction);
    const vector fvec = orient.fvec;
    orient.fvec = vector(-orient.uvec.x, -orient.uvec.y, -orient.uvec.z);
    orient.uvec = fvec;

    rfg_effect_material* effect_material = rfg_get_effect_material_by_index(material_type);
    if (effect_material == nullptr)
    {
        return;
    }
    unsigned int effect_handle = effect_material->get_bullet_impact_effect(shape_type, w_info->bullet_group_index);

    int script_point_index = -1;
    const object_mover* mp = nullptr;
    if (hit_obj != nullptr && hit_obj->get_obj_type() == OT_MOVER)
    {
        mp = (const object_mover*)hit_obj;
        if (mp->m_level_obj_properties != nullptr)
        {
            const unsigned int* bullet_impact_override = nullptr;
            bool use_override = true;
            if (mp->has_script_point_effect(hit_shape_key, &script_point_index))
            {
                bullet_impact_override = mp->m_level_obj_properties->m_base_properties->m_script_point_effect_infos[script_point_index]->bullet_impact_override;
            }
            else if (mp->get_obj_type() == OT_MOVER && mp->get_sub_type() == OT_SUB_MOVER_GENERAL && mp->is_killed())
            {
                use_override = false;
            }
            else
            {
                bullet_impact_override = mp->m_level_obj_properties->m_base_properties->m_bullet_impact_override;
            }
            if (use_override && bullet_impact_override != nullptr && bullet_impact_override[w_info->bullet_group_index] != 0xFFFFFFFF)
            {
                effect_handle = bullet_impact_override[w_info->bullet_group_index];
            }
        }
    }

    if (hp != nullptr && !localization_allow_blood())
    {
        effect_handle = Human_hit_no_blood_effect;
    }
    if (effect_handle == 0xFFFFFFFF)
    {
        return;
    }

    // -1 = no speed given: full volume.
    float volume;
    if (speed != -1.0f)
    {
        volume = speed / BULLET_SPARK_MAX_VOLUME_IMPACT;
        if (volume < 0.0f)
        {
            volume = 0.0f;
        }
        if (volume >= 1.0f)
        {
            volume = 1.0f;
        }
    }
    else
    {
        volume = 1.0f;
    }

    const unsigned int host_handle = hit_obj != nullptr ? hit_obj->get_handle() : 0xFFFFFFFF;
    unsigned int handle;
    if (script_point_index == -1 || hit_obj->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        handle = effects_play(effect_handle, pos, orient, false, host_handle, -1, 0xFFFFFFFF, volume, false, EFFECTS_CATEGORY_NONE);
    }
    else
    {
        handle = effects_play_at_script_point(effect_handle, host_handle, mp->m_level_obj_properties->m_script_point_effect_insts[script_point_index]->script_point_idx, false, &pos, &orient);
    }
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = World.object_from_handle<object_effect>(handle);
    if (effect != nullptr)
    {
        effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
        if (effect_material->effect_tint_enabled)
        {
            effect->set_material_tint(effect_material->effect_tint);
        }
    }
}

// 0x006E17E0 (:134)
void bullet_spark_system_init()
{
    Human_hit_no_blood_effect = effects_get_handle("wep_bullet_impact_flesh_no_blood");
}
