// Original: volition/rfg/code/scripting/objective_highlight.cpp
//
// Objective highlights (gameflow.md GF1). Scripts and activities put objects in Highlight_objects_table with a purpose
// (the color) and a style. objective_highlight_do_frame walks the table and starts or stops the marker effects (arrow,
// cylinder), objective_highlight_do_frame_handshake sets the glow outlines on the render instances and pulses their
// color. Ground and activity markers are separate effects on a host object, updated by their render callbacks.
//
// Floating point: the scalar statements below are double chains rounded on assignment, as in the asm (stub_impl.md S3).

#include "volition/rfg/code/scripting/objective_highlight.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_deformable_mesh_instance.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/target_outline.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/hash_table.inl"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <cmath>
#include <cstring>

// The table holds 64 entries; from this many on, objects that are not fully in the world lose their highlight.
static const int Highlight_table_nearly_full = 63;

// Scale of a ground marker's diameter, inlined at its one use in rfg.exe.
static const float Highlight_ground_marker_size_mul = 0.65f; // 0x01332060

static float Highlight_current_opacity_change_sign = -1.0f; // 0x016B0CD8
static float Highlight_max_color_multiplier = 0.5f; // 0x016B0CDC
static tweak_table_item Tweak_table_item_Highlight_max_color_multiplier("Highlight_max_color_multiplier", &Highlight_max_color_multiplier); // 0x036F61D8
static float Highlight_min_color_multiplier = 0.05f; // 0x016B0CE0
static tweak_table_item Tweak_table_item_Highlight_min_color_multiplier("Highlight_min_color_multiplier", &Highlight_min_color_multiplier); // 0x036F3818
static float Highlight_color_multiplier_change_per_sec = 2.0f; // 0x016B0CE4
static tweak_table_item Tweak_table_item_Highlight_color_multiplier_change_per_sec("Highlight_color_multiplier_change_per_sec", &Highlight_color_multiplier_change_per_sec); // 0x036D1338
static unsigned int Highlight_ground_marker_effect = 0xFFFFFFFF; // 0x016B0CE8
static float Highlight_arrow_marker_size = 0.3f; // 0x016B0CEC
static float Highlight_arrow_marker_far_size = 3.0f; // 0x016B0CF0
static unsigned int Highlight_arrow_marker_effect = 0xFFFFFFFF; // 0x016B0CF4
static unsigned int Highlight_cylinder_marker_effect = 0xFFFFFFFF; // 0x016B0CF8

// Indexed by highlight_purpose: destroy, neutral, EDF, activity.
vector Highlight_colors[4] = // 0x016B2660
{
    vector(0.78f, 0.0f, 0.0f),
    vector(0.9f, 0.3f, 0.0f),
    vector(0.01f, 0.03f, 0.78f),
    vector(0.2f, 1.0f, 0.5f),
};

data_hash_table<unsigned int,highlight_data,&uint_hash,64,unsigned char> Highlight_objects_table(false); // 0x016B2690
static farray<unsigned int,128> Highlight_objects_w_glow_prev; // 0x016B2B18

// Never written in rfg.exe: a debug switch without its console command.
static bool Highlight_glow_disable = false; // 0x0361141E
static float Highlight_current_opacity = 0.0f; // 0x03611424

static void clear_effect(unsigned int handle);

// 0x00B2DA70 (:182), LTCG: op in EDX, idx on the stack.
// Puts the object's main render instances on outline layer idx. Answers whether it has any. Not the objects'
// enable_object_outline: that answers true without an instance, and this one tells the caller to keep a layer on.
// rfg.exe does not check a human's char_inst for null here.
// Deviation from rfg.exe: external, not file-local, for the object_effect and object_mover friend
// declarations (clear_glow_effect too). The mover paths call the virtual get_render_model_instance where it inlines
// a read per mover subtype.
bool glow_set_rl_outline(object* op, int idx)
{
    switch (op->get_obj_type())
    {
    case OT_HUMAN:
    {
        human* hp = static_cast<human*>(op);
        if (!hp->char_inst->rdd.is_loaded() || hp->char_inst->rdd.get_data()->renderable_instance_p == nullptr)
        {
            return false;
        }
        if ((unsigned int)idx < 4)
        {
            hp->char_inst->rdd.get_data()->renderable_instance_p->set_outline_enabled(idx);
        }
        if (hp->rdd.is_loaded() && hp->rdd.get_data()->head_renderable_instance_p != nullptr && (unsigned int)idx < 4)
        {
            hp->rdd.get_data()->head_renderable_instance_p->set_outline_enabled(idx);
        }
        return true;
    }
    case OT_ITEM:
    {
        item* ip = static_cast<item*>(op);
        rl_renderable_instance* ri = nullptr;
        if (ip->char_inst != nullptr)
        {
            if (ip->char_inst->rdd.is_loaded())
            {
                ri = ip->char_inst->rdd.get_data()->renderable_instance_p;
            }
        }
        else if (ip->rdd.is_loaded())
        {
            ri = ip->rdd.get_data()->smesh_instance_p;
        }
        if (ri == nullptr)
        {
            return false;
        }
        if ((unsigned int)idx < 4)
        {
            ri->set_outline_enabled(idx);
        }
        return true;
    }
    case OT_MOVER:
    {
        rl_renderable_instance* ri = static_cast<object_mover*>(op)->get_render_model_instance();
        if (ri == nullptr)
        {
            return false;
        }
        if ((unsigned int)idx < 4)
        {
            ri->set_outline_enabled(idx);
        }
        return true;
    }
    case OT_VEHICLE:
    {
        vehicle* vp = static_cast<vehicle*>(op);
        if (!vp->rdd.is_loaded() || vp->rdd.get_data()->m_renderable_instance_p == nullptr)
        {
            return false;
        }
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        if ((unsigned int)idx < 4)
        {
            data->m_renderable_instance_p->set_outline_enabled(idx);
        }
        for (unsigned int i = 0; i < data->m_num_components; i++)
        {
            if (data->m_components[i].m_renderable_instance_p != nullptr && (unsigned int)idx < 4)
            {
                data->m_components[i].m_renderable_instance_p->set_outline_enabled(idx);
            }
        }
        return true;
    }
    default:
        return false;
    }
}

// 0x00B33710 (:272)
// glow_cleanup inlines this in rfg.exe. A vehicle's detached component is drawn by its debris object.
bool clear_glow_effect(object* op)
{
    if (op == nullptr)
    {
        return true;
    }
    highlight_data* data = Highlight_objects_table.get_ptr(op->get_handle());
    if (data != nullptr)
    {
        data->effect_obj_handle = 0xFFFFFFFF;
    }
    switch (op->get_obj_type())
    {
    case OT_HUMAN:
    {
        human* hp = static_cast<human*>(op);
        // Original bug kept: a human's char_inst is not checked for null, as in glow_set_rl_outline.
        if (!hp->char_inst->rdd.is_loaded() || hp->char_inst->rdd.get_data()->renderable_instance_p == nullptr)
        {
            return false;
        }
        hp->char_inst->rdd.get_data()->renderable_instance_p->set_outline_disabled();
        if (hp->rdd.is_loaded() && hp->rdd.get_data()->head_renderable_instance_p != nullptr)
        {
            hp->rdd.get_data()->head_renderable_instance_p->set_outline_disabled();
        }
        return true;
    }
    case OT_ITEM:
    {
        item* ip = static_cast<item*>(op);
        rl_renderable_instance* ri = nullptr;
        if (ip->char_inst != nullptr)
        {
            if (ip->char_inst->rdd.is_loaded())
            {
                ri = ip->char_inst->rdd.get_data()->renderable_instance_p;
            }
        }
        else if (ip->rdd.is_loaded())
        {
            ri = ip->rdd.get_data()->smesh_instance_p;
        }
        if (ri == nullptr)
        {
            return false;
        }
        ri->set_outline_disabled();
        return true;
    }
    case OT_MOVER:
    {
        rl_renderable_instance* ri = static_cast<object_mover*>(op)->get_render_model_instance();
        if (ri == nullptr)
        {
            return false;
        }
        ri->set_outline_disabled();
        return true;
    }
    case OT_VEHICLE:
    {
        vehicle* vp = static_cast<vehicle*>(op);
        if (!vp->rdd.is_loaded() || vp->rdd.get_data()->m_renderable_instance_p == nullptr)
        {
            return false;
        }
        vehicle::resource_dependent_data* vdata = vp->rdd.get_data();
        vdata->m_renderable_instance_p->set_outline_disabled();
        for (unsigned int i = 0; i < vdata->m_num_components; i++)
        {
            vehicle_component& component = vdata->m_components[i];
            if (component.m_renderable_instance_p != nullptr)
            {
                component.m_renderable_instance_p->set_outline_disabled();
            }
            else if (component.m_debris_object != 0xFFFFFFFF)
            {
                object_debris* debris = World.object_from_handle<object_debris>(component.m_debris_object);
                if (debris != nullptr && debris->m_renderable_instance_p != nullptr)
                {
                    debris->m_renderable_instance_p->set_outline_disabled();
                }
            }
        }
        return true;
    }
    default:
        return false;
    }
}

// 0x00B33930 (:376)
// Outlines the object and its children on layer idx, and remembers them for glow_cleanup next frame.
static bool create_glow_effect(object* op, int idx)
{
    if (Highlight_glow_disable || Highlight_objects_w_glow_prev.size() >= 128) // Magic number: the array's capacity
    {
        return false;
    }
    if (op == nullptr)
    {
        return false;
    }
    bool child_glows = false;
    object* child = op->child_ptr;
    while (child != nullptr)
    {
        child_glows |= create_glow_effect(child, idx);
        child = child->child_next;
        if (child == op->child_ptr)
        {
            break;
        }
    }
    highlight_data* data = Highlight_objects_table.get_ptr(op->get_handle());
    if (data != nullptr)
    {
        data->effect_obj_handle = 0;
    }
    const bool glows = glow_set_rl_outline(op, idx);
    if (glows)
    {
        // The children can fill the array after the check above; rfg.exe prints an Int3 message then and drops this one.
        Highlight_objects_w_glow_prev.add(op->get_handle());
    }
    return child_glows || glows;
}

// 0x00B33A50 (:405)
static void glow_cleanup()
{
    for (int i = Highlight_objects_w_glow_prev.size() - 1; i >= 0; i--)
    {
        object* op = World.object_from_handle(Highlight_objects_w_glow_prev[i]);
        if (op != nullptr)
        {
            clear_glow_effect(op);
        }
    }
    Highlight_objects_w_glow_prev.remove_all();
}

// The renderable's size along x: the local bounding volume's max minus its min.
static float highlight_instance_width(const rl_renderable_instance* ri)
{
    const rl_aabb box = ri->get_local_aabb();
    return (float)((double)box.max.x - (double)box.min.x);
}

// 0x00B3DA60 (:534)
// Keeps an arrow or cylinder over its object, sized for the camera distance, and in the highlight's color.
// Deviation from rfg.exe: this and the activity and ground marker callbacks are external, not file-local,
// for the friend declarations.
void objective_highlight_target_marker_update_render_cb(object_effect* oep)
{
    if (!oep->rdd.is_loaded())
    {
        return;
    }
    vfx_effect* vfx = oep->rdd.get_data()->vfx_effect_p;
    if (vfx == nullptr)
    {
        return;
    }
    const int color_idx = (int)oep->get_custom_cb_data();
    float opacity = 1.0f;

    object* host = World.object_from_handle(oep->get_parent());
    if (host == nullptr || host->is_killed())
    {
        objective_highlight_remove(oep->get_parent());
        return;
    }
    highlight_data* data = Highlight_objects_table.get_ptr(oep->get_parent());
    if (data == nullptr)
    {
        objective_highlight_remove(oep->get_parent());
        return;
    }

    vector bmin;
    vector bmax;
    host->get_local_bbox(bmin, bmax);
    float size = vec_xz_dist(bmin, bmax);
    const float half_width = (float)((double)size * 0.5);
    // Magic numbers: the marker's base size stays between 1 m and 6 m.
    if ((float)((double)size - 1.0) < 0.0f)
    {
        size = 1.0f;
    }
    if ((float)((double)size - 6.0) >= 0.0f)
    {
        size = 6.0f;
    }

    host->get_bbox(bmin, bmax);
    // Left unset by rfg.exe when the style has no marker (Int3 there), which do_frame never lets through.
    vector marker_pos(0.0f, 0.0f, 0.0f);
    if (data->style == HIGHLIGHT_STYLE_ARROW || data->style == HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER)
    {
        size = (float)((double)Highlight_arrow_marker_size * (double)size);
        // The arrow goes above the children, and for a plain arrow above their children too (a driver's weapon).
        vector child_bmin;
        vector child_bmax;
        object* child = host->child_ptr;
        while (child != nullptr)
        {
            if (child->havok_handle != 0xFFFFFFFF)
            {
                child->get_bbox(child_bmin, child_bmax);
                if (bmax.y < child_bmax.y)
                {
                    bmax.y = child_bmax.y;
                }
                if (data->style == HIGHLIGHT_STYLE_ARROW)
                {
                    object* grandchild = child->child_ptr;
                    while (grandchild != nullptr)
                    {
                        if (grandchild->havok_handle != 0xFFFFFFFF)
                        {
                            grandchild->get_bbox(child_bmin, child_bmax);
                            if (bmax.y < child_bmax.y)
                            {
                                bmax.y = child_bmax.y;
                            }
                        }
                        grandchild = grandchild->child_next;
                        if (grandchild == child->child_ptr)
                        {
                            break;
                        }
                    }
                }
            }
            child = child->child_next;
            if (child == host->child_ptr)
            {
                break;
            }
        }
        marker_pos.x = (float)(((double)bmax.x + (double)bmin.x) * 0.5);
        marker_pos.y = bmax.y;
        marker_pos.z = (float)(((double)bmax.z + (double)bmin.z) * 0.5);

        vector to_camera;
        to_camera.x = (float)((double)Rfg_camera.real_pos.x - (double)marker_pos.x);
        to_camera.y = (float)((double)Rfg_camera.real_pos.y - (double)marker_pos.y);
        to_camera.z = (float)((double)Rfg_camera.real_pos.z - (double)marker_pos.z);
        const float camera_dist = to_camera.mag_and_normalize();
        // Magic numbers: over a tall, narrow object (more than 20 m high, under 5 m across) the arrow comes toward the
        // camera and down, so it does not float out of view.
        if ((double)bmax.y - (double)bmin.y > 20.0 && half_width < 5.0f)
        {
            marker_pos.x = (float)((double)to_camera.x * (double)half_width + (double)marker_pos.x);
            marker_pos.y = (float)((double)(float)((double)to_camera.y * (double)half_width + (double)marker_pos.y) - (double)half_width);
            marker_pos.z = (float)((double)to_camera.z * (double)half_width + (double)marker_pos.z);
        }
        // Magic number: the on-screen size is computed per 50 m of distance, and grows up to far_size as it shrinks.
        const float dist_scale = (float)((double)camera_dist / 50.0);
        float screen_size = (float)((double)size / (double)dist_scale);
        screen_size = fl_parameter(screen_size, size, 0.0f, 1.0f, Highlight_arrow_marker_far_size, true) * screen_size;
        size = (float)((double)screen_size * (double)dist_scale);
        // Magic numbers: fades out as the on-screen size goes from 7 up to 30.
        opacity = fl_parameter(screen_size, 30.0f, 7.0f, 0.0f, 0.9f, true);
    }
    else if (data->style == HIGHLIGHT_STYLE_CYLINDER)
    {
        size = (float)((double)size * (double)1.1f);
        marker_pos.x = (float)(((double)bmax.x + (double)bmin.x) * 0.5);
        marker_pos.y = bmin.y;
        marker_pos.z = (float)(((double)bmax.z + (double)bmin.z) * 0.5);
        // rfg.exe also calls vec_dist(camera, marker_pos) here and drops the result.
    }
    else
    {
        // Original: Int3 critical function here.
    }

    rl_renderable_instance* head = vfx->get_mesh_instances().first();
    const float scale = (float)((double)size / (double)highlight_instance_width(head));
    rl_renderable_instance* ri = head;
    while (ri != nullptr)
    {
        matrix rotation(ri->get_rotation());
        const vector old_pos = ri->get_position();
        vector new_pos;
        new_pos.x = marker_pos.x;
        new_pos.y = (float)((((double)old_pos.x - (double)oep->pos.x) * (double)rotation.uvec.x
            + (double)rotation.uvec.y * ((double)old_pos.y - (double)oep->pos.y)
            + (double)rotation.uvec.z * ((double)old_pos.z - (double)oep->pos.z)) * (double)scale + (double)marker_pos.y);
        new_pos.z = marker_pos.z;
        ri->set_position(new_pos);

        const float up_len_squared = (float)((double)rotation.uvec.y * (double)rotation.uvec.y
            + (double)rotation.uvec.x * (double)rotation.uvec.x
            + (double)rotation.uvec.z * (double)rotation.uvec.z);
        // Only the heading of the old rotation is kept.
        rotation.set(0.0f, 0.0f, rotation.extract_heading());
        const float axis = (float)((double)(float)sqrt((double)up_len_squared) * (double)scale);
        const matrix scale_matrix(vector(axis, 0.0f, 0.0f), vector(0.0f, axis, 0.0f), vector(0.0f, 0.0f, axis));
        rotation.unrotate(scale_matrix);
        ri->set_rotation(rotation);

        ri->set_tint(Highlight_colors[color_idx]);
        ri->set_opacity(opacity);
        ri = ri->next;
        if (ri == head)
        {
            break;
        }
    }
    oep->update_pos_and_orient(marker_pos, oep->orient, true);
    vfx->set_position(marker_pos);
    rl_color_float tint;
    tint.set(Highlight_colors[color_idx].x, Highlight_colors[color_idx].y, Highlight_colors[color_idx].z, 1.0f);
    vfx->set_effect_material_tint(tint);
}

// 0x00B3E3D0 (:687), LTCG: handle in ESI, eih on the stack.
// Starts the marker effect eih on a highlighted object unless it already has a live one.
static void create_visual_effect(unsigned int handle, unsigned int eih)
{
    highlight_data* data = Highlight_objects_table.get_ptr(handle);
    if (data->effect_obj_handle != 0xFFFFFFFF && World.object_from_handle<object_effect>(data->effect_obj_handle) != nullptr)
    {
        return;
    }
    const vector pos = World.object_from_handle(handle)->pos;
    data->effect_obj_handle = effects_play(eih, pos, IDENTITY_MATRIX, true, handle, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    object_effect* effect = nullptr;
    if (data->effect_obj_handle != 0xFFFFFFFF)
    {
        effect = World.object_from_handle<object_effect>(data->effect_obj_handle);
    }
    if (effect == nullptr)
    {
        data->effect_obj_handle = 0xFFFFFFFF;
        return;
    }
    effect->set_post_update_render_cb(objective_highlight_target_marker_update_render_cb);
    effect->set_custom_cb_data((float)(int)data->color_idx);
}

// 0x00B33CC0 (:711)
static void clear_visual_effect(unsigned int handle)
{
    highlight_data* data = Highlight_objects_table.get_ptr(handle);
    if (data->effect_obj_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = World.object_from_handle<object_effect>(data->effect_obj_handle);
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
    data->effect_obj_handle = 0xFFFFFFFF;
}

// 0x00B23970 (:725): empty in rfg.exe, and not called.
static void glow_do_frame(unsigned int handle)
{
}

// 0x00B3E4D0 (:730)
// Only reached from the unused debug command below. Removes the highlight of every host of the effect.
static void remove_all_effects(unsigned int highlight_effect)
{
    const effect_info* info = effects_get_effect_info(highlight_effect);
    const int num_effects = World.get_num_objects<object_effect>();
    for (int i = 0; i < num_effects; i++)
    {
        object_effect* effect = World.get_object<object_effect>(i);
        if (effect->get_visual_effect_info_handle() == info->visual_handle)
        {
            objective_highlight_remove(effect->get_parent());
        }
    }
}

// 0x00B2DC40 (:741), LTCG: host on the stack, highlight_effect in EAX.
static void remove_effect(unsigned int host, unsigned int highlight_effect)
{
    const effect_info* info = effects_get_effect_info(highlight_effect);
    if (info == nullptr || info->visual_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* host_objp = World.object_from_handle(host);
    if (host_objp == nullptr)
    {
        return;
    }
    object* child = host_objp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_EFFECT)
        {
            object_effect* effect = static_cast<object_effect*>(child);
            if (effect->get_visual_effect_info_handle() == info->visual_handle)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        child = child->child_next;
        if (child == host_objp->child_ptr)
        {
            break;
        }
    }
}

// 0x00B2DCA0 (:767)
void objective_highlight_activity_marker_update_render_cb(object_effect* oep)
{
    if (!oep->rdd.is_loaded())
    {
        return;
    }
    vfx_effect* vfx = oep->rdd.get_data()->vfx_effect_p;
    if (vfx == nullptr)
    {
        return;
    }
    rl_renderable_instance* head = vfx->get_mesh_instances().first();
    rl_renderable_instance* ri = head;
    while (ri != nullptr)
    {
        ri->set_tint(Highlight_colors[HIGHLIGHT_PURPOSE_ACTIVITY]);
        ri = ri->next;
        if (ri == head)
        {
            break;
        }
    }
}

// 0x00B2DD00 (:783)
// Keeps a ground marker under its host at the diameter given when it was added. It fades with the camera distance and
// within 30 m follows the host's action node or the ground.
void objective_highlight_ground_marker_update_render_cb(object_effect* oep)
{
    float size = (float)((double)oep->get_custom_cb_data() * (double)Highlight_ground_marker_size_mul);
    if (!oep->rdd.is_loaded())
    {
        return;
    }
    vfx_effect* vfx = oep->rdd.get_data()->vfx_effect_p;
    if (vfx == nullptr)
    {
        return;
    }
    object* host = World.object_from_handle(oep->get_parent());
    if (host == nullptr)
    {
        oep->stop(object_effect::SS_STOP_EMITTING);
        return;
    }
    vector marker_pos = oep->pos;
    const float dist_squared = vec_dist_squared(Rfg_camera.real_pos, vfx->get_position());
    // Magic numbers: alpha 0.3 at 0.5 m, 0.75 from 8 m on.
    vfx->set_alpha_fade(fl_parameter(dist_squared, 0.25f, 64.0f, 0.3f, 0.75f, true));
    vfx->set_use_alpha_fade(true);
    if (dist_squared < 900.0f) // Magic number: 30 m
    {
        if (host->get_obj_type() == OT_ACTION_NODE)
        {
            static_cast<object_action_node*>(host)->get_entry_point(nullptr, marker_pos);
        }
        else
        {
            bool found_node = false;
            object* child = host->child_ptr;
            while (child != nullptr)
            {
                if (child->get_obj_type() == OT_ACTION_NODE && static_cast<object_action_node*>(child)->allow_player())
                {
                    static_cast<object_action_node*>(child)->get_entry_point(nullptr, marker_pos);
                    found_node = true;
                    break;
                }
                child = child->child_next;
                if (child == host->child_ptr)
                {
                    break;
                }
            }
            if (!found_node)
            {
                if (host->get_obj_type() == OT_HUMAN || host->get_obj_type() == OT_SQUAD)
                {
                    marker_pos = host->pos;
                }
                else if (host->get_obj_type() == OT_VEHICLE)
                {
                    havok_terrain_height(marker_pos.y, marker_pos.x, marker_pos.z, false, nullptr);
                }
                else
                {
                    // Magic number: sinks the marker by 12% of its size.
                    marker_pos.y = (float)((double)host->pos.y - (double)size * (double)0.12f);
                }
            }
        }
    }
    rl_renderable_instance* head = vfx->get_mesh_instances().first();
    const float scale = (float)((double)size / (double)highlight_instance_width(head));
    rl_renderable_instance* ri = head;
    while (ri != nullptr)
    {
        matrix rotation(ri->get_rotation());
        vector new_pos;
        new_pos.x = marker_pos.x;
        new_pos.y = (float)(((double)ri->get_position().y - (double)oep->pos.y) * (double)scale + (double)marker_pos.y);
        new_pos.z = marker_pos.z;
        ri->set_position(new_pos);

        const matrix scale_matrix(vector(scale, 0.0f, 0.0f), vector(0.0f, scale, 0.0f), vector(0.0f, 0.0f, scale));
        rotation.unrotate(scale_matrix);
        ri->set_rotation(rotation);

        ri->set_tint(Highlight_colors[HIGHLIGHT_PURPOSE_NEUTRAL]);
        ri = ri->next;
        if (ri == head)
        {
            break;
        }
    }
    oep->update_pos_and_orient(marker_pos, oep->orient, true);
}

// 0x00B23980 (:898), LTCG: style in ESI, color_hint and op on the stack.
// The default style: glow in Wrecking Crew, otherwise a neutral arrow. A squad keeps the default, and its members get
// the arrows in do_frame.
static void get_style_by_object(highlight_purpose& color_hint, highlight_style& style, object* op)
{
    if (style != HIGHLIGHT_STYLE_DEFAULT)
    {
        return;
    }
    if (wrecking_crew_is_active())
    {
        style = HIGHLIGHT_STYLE_GLOW;
    }
    else if (op->get_obj_type() != OT_SQUAD)
    {
        style = HIGHLIGHT_STYLE_ARROW;
        color_hint = HIGHLIGHT_PURPOSE_NEUTRAL;
    }
}

// 0x00B2E120 (:1130)
unsigned int objective_highlight_activity_marker_add(const object* host_objp, const vector& effect_offset)
{
    vector pos = effect_offset;
    vec_change_to_global_coords(pos, host_objp->pos, host_objp->orient);
    const unsigned int handle = effects_play(Highlight_ground_marker_effect, pos, host_objp->orient, true, host_objp->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(handle);
        if (effect != nullptr)
        {
            effect->obj_flags.destroy_on_stream = 0;
            effect->set_post_update_render_cb(objective_highlight_activity_marker_update_render_cb);
        }
    }
    return handle;
}

// 0x00B2E1C0 (:1151)
unsigned int objective_highlight_activity_marker_add(unsigned int host, const vector& effect_offset)
{
    const object* host_objp = World.object_from_handle(host);
    if (host_objp == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return objective_highlight_activity_marker_add(host_objp, effect_offset);
}

// 0x00B2E1E0 (:1167)
unsigned int objective_highlight_ground_marker_add(unsigned int host, float diameter, const vector& offset, bool use_action_node, bool stick_to_ground)
{
    // stick_to_ground is not read in rfg.exe; vehicles always go to the ground.
    object* host_objp = World.object_from_handle(host);
    if (host_objp == nullptr)
    {
        return 0xFFFFFFFF;
    }
    vector base_pos = host_objp->pos;
    if (use_action_node)
    {
        if (host_objp->get_obj_type() == OT_ACTION_NODE)
        {
            static_cast<object_action_node*>(host_objp)->get_entry_point(nullptr, base_pos);
        }
        else
        {
            object* child = host_objp->child_ptr;
            while (child != nullptr)
            {
                if (child->get_obj_type() == OT_ACTION_NODE)
                {
                    static_cast<object_action_node*>(child)->get_entry_point(nullptr, base_pos);
                    break;
                }
                child = child->child_next;
                if (child == host_objp->child_ptr)
                {
                    break;
                }
            }
        }
    }
    vector pos = offset;
    vec_change_to_global_coords(pos, base_pos, host_objp->orient);
    if (host_objp->get_obj_type() == OT_VEHICLE)
    {
        havok_terrain_height(pos.y, pos.x, pos.z, false, nullptr);
    }
    pos.y = (float)((double)pos.y + 0.02); // Magic number: 2 cm above the surface, to avoid z-fighting
    const unsigned int handle = effects_play(Highlight_ground_marker_effect, pos, host_objp->orient, true, host_objp->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(handle);
        if (effect != nullptr)
        {
            effect->obj_flags.destroy_on_stream = 0;
            effect->set_post_update_render_cb(objective_highlight_ground_marker_update_render_cb);
            effect->set_custom_cb_data(diameter);
        }
    }
    return handle;
}

// 0x00B3E540 (:1269)
void unused_dcf_objective_highlight_ground_marker_kill()
{
    remove_all_effects(Highlight_ground_marker_effect);
}

// 0x00B3A700 (:1274), LTCG: handle in ESI.
static void clear_effect(unsigned int handle)
{
    highlight_data* data = Highlight_objects_table.get_ptr(handle);
    if (data == nullptr)
    {
        return;
    }
    switch (data->style)
    {
    case HIGHLIGHT_STYLE_GLOW:
        clear_glow_effect(World.object_from_handle(handle));
        break;
    case HIGHLIGHT_STYLE_FRAME:
        target_outline_remove_object(handle); // inlined
        break;
    case HIGHLIGHT_STYLE_ARROW:
    case HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER:
    case HIGHLIGHT_STYLE_CYLINDER:
        clear_visual_effect(handle);
        break;
    default:
        break;
    }
}

// 0x00B2E360 (:1304)
void objective_highlight_ground_marker_remove(unsigned int host)
{
    remove_effect(host, Highlight_ground_marker_effect);
}

// 0x00B3E550 (:1310)
// Walks the table: drops what is gone, moves highlights between passengers and vehicles and from squads to members,
// and starts the marker effects. On the satellite view markers are stopped, and every human except the local player
// glows by team, red for the player's enemies.
void objective_highlight_do_frame()
{
    const bool nearly_full = Highlight_objects_table.get_count() >= Highlight_table_nearly_full;
    // Kept between entries as in rfg.exe: an entry a squad's removal took out of the table reads the previous data.
    highlight_data data = {};
    unsigned int key = 0;
    bool more = Highlight_objects_table.get_first_key(key);

    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE)
    {
        while (more)
        {
            // Taken first: removing the entry would lose the place.
            unsigned int next_key = 0;
            const bool has_next = Highlight_objects_table.get_next_key(next_key, key);
            Highlight_objects_table.get(key, data);
            object* op = World.object_from_handle(key);
            if (op == nullptr || op->is_killed()
                || (nearly_full && (op->obj_flags.dont_transform != 0 || op->obj_flags._dont_use_me == 0))
                || (op->get_obj_type() == OT_VEHICLE && vehicle_get_num_passengers(static_cast<vehicle*>(op)) == 0))
            {
                objective_highlight_remove(key);
            }
            else if (data.style != HIGHLIGHT_STYLE_GLOW && data.effect_obj_handle != 0xFFFFFFFF)
            {
                clear_effect(key);
            }
            key = next_key;
            more = has_next;
        }
        const int num_humans = World.get_num_objects<human>();
        for (int i = 0; i < num_humans; i++)
        {
            human* hp = World.get_object<human>(i);
            if ((nearly_full && (hp->obj_flags.dont_transform != 0 || hp->obj_flags._dont_use_me == 0))
                || hp->is_killed() || hp == player_get_local_player())
            {
                continue;
            }
            const highlight_purpose purpose = human_teams_get_disposition(hp->current_team, HUMAN_TEAM_GUERILLA) > 1
                ? HIGHLIGHT_PURPOSE_NEUTRAL
                : HIGHLIGHT_PURPOSE_DESTROY;
            // A passenger's highlight goes on the vehicle.
            objective_highlight_add(hp->vehicle_handle != 0xFFFFFFFF ? hp->vehicle_handle : hp->get_handle(), purpose, HIGHLIGHT_STYLE_GLOW);
        }
        return;
    }

    while (more)
    {
        unsigned int next_key = 0;
        const bool has_next = Highlight_objects_table.get_next_key(next_key, key);
        Highlight_objects_table.get(key, data);
        const unsigned int cur_key = key;
        key = next_key;
        more = has_next;

        object* op = World.object_from_handle(cur_key);
        if (op == nullptr || op->is_killed()
            || (nearly_full && (op->obj_flags.dont_transform != 0 || op->obj_flags._dont_use_me == 0)))
        {
            objective_highlight_remove(cur_key);
            continue;
        }
        const object_type type = op->get_obj_type();
        if (type == OT_HUMAN)
        {
            human* hp = static_cast<human*>(op);
            // An unreachable passenger or a hostage moving with a vehicle is marked through the vehicle.
            if (human_in_protected_vehicle_pos(hp)
                || (hp->hflags.activity_ha_hostage != 0
                    && (hp->cur_state == HS_VEHICLE_ENTERING || hp->cur_state == HS_VEHICLE_EXITING || human_in_vehicle(hp) != nullptr)))
            {
                objective_highlight_add(hp->vehicle_handle, data.color_idx, HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER);
                clear_effect(cur_key);
                continue;
            }
        }
        else if (type == OT_VEHICLE)
        {
            vehicle* vp = static_cast<vehicle*>(op);
            if (player_get_local_player()->vehicle_handle == vp->get_handle())
            {
                clear_effect(cur_key);
                continue;
            }
            if (data.style == HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER)
            {
                // Stays only while a highlighted human rides. All 8 slots are read, used or not.
                bool passenger_highlighted = false;
                for (int i = 0; i < 8 && !passenger_highlighted; i++)
                {
                    const unsigned int passenger = vp->vii.humans[i].handle;
                    if (passenger == 0xFFFFFFFF)
                    {
                        continue;
                    }
                    const object* passenger_op = World.object_from_handle(passenger);
                    passenger_highlighted = passenger_op != nullptr && passenger_op->get_obj_type() == OT_HUMAN
                        && Highlight_objects_table.exists(passenger_op->get_handle());
                }
                if (!passenger_highlighted)
                {
                    objective_highlight_remove(cur_key);
                    continue;
                }
            }
        }
        else if (type == OT_SQUAD)
        {
            object_squad* squad = static_cast<object_squad*>(op);
            for (int i = squad->member_array.size() - 1; i >= 0; i--)
            {
                objective_highlight_add(squad->member_array[i], data.color_idx, data.style);
            }
            continue;
        }

        switch (data.style)
        {
        case HIGHLIGHT_STYLE_ARROW:
        case HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER:
            create_visual_effect(cur_key, Highlight_arrow_marker_effect);
            break;
        case HIGHLIGHT_STYLE_CYLINDER:
            create_visual_effect(cur_key, Highlight_cylinder_marker_effect);
            break;
        default:
            break;
        }
    }
}

// 0x00B33D30 (:1462)
// Single player only. Takes last frame's outlines off, pulses the color, and outlines every GLOW entry: layer 0 for the
// main purpose (neutral, or EDF in Wrecking Crew), layer 1 for the rest, which always uses the destroy color.
void objective_highlight_do_frame_handshake()
{
    if (Multiplayer)
    {
        return;
    }
    glow_cleanup();
    if (Highlight_glow_disable)
    {
        return;
    }
    rl_outline_state state;
    memset(&state, 0, sizeof(state));
    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE)
    {
        Highlight_current_opacity = (float)(((double)Highlight_max_color_multiplier + (double)Highlight_min_color_multiplier) * 0.5);
    }
    else if (!game_is_paused())
    {
        float opacity = (float)((double)Highlight_color_multiplier_change_per_sec * (double)Highlight_current_opacity_change_sign * (double)Frametime + (double)Highlight_current_opacity);
        if ((float)((double)opacity - (double)Highlight_min_color_multiplier) < 0.0f)
        {
            opacity = Highlight_min_color_multiplier;
        }
        if ((float)((double)opacity - (double)Highlight_max_color_multiplier) >= 0.0f)
        {
            opacity = Highlight_max_color_multiplier;
        }
        Highlight_current_opacity = opacity;
        if (opacity <= Highlight_min_color_multiplier)
        {
            Highlight_current_opacity_change_sign = 1.0f;
        }
        else if (opacity >= Highlight_max_color_multiplier)
        {
            Highlight_current_opacity_change_sign = -1.0f;
        }
    }

    const int main_purpose = wrecking_crew_is_active() ? HIGHLIGHT_PURPOSE_EDF : HIGHLIGHT_PURPOSE_NEUTRAL;
    const vector& main_color = Highlight_colors[main_purpose];
    const vector& other_color = Highlight_colors[HIGHLIGHT_PURPOSE_DESTROY];
    state.colors[0].set((float)((double)main_color.x * (double)Highlight_current_opacity), (float)((double)main_color.y * (double)Highlight_current_opacity), (float)((double)main_color.z * (double)Highlight_current_opacity), 1.0f);
    state.colors[1].set((float)((double)other_color.x * (double)Highlight_current_opacity), (float)((double)other_color.y * (double)Highlight_current_opacity), (float)((double)other_color.z * (double)Highlight_current_opacity), 1.0f);
    state.layer[0] = RLOL_USE_COLOR0;
    state.layer[1] = RLOL_USE_COLOR1;

    bool layer_used[2] = {false, false};
    unsigned int key = 0;
    bool more = Highlight_objects_table.get_first_key(key);
    while (more)
    {
        const highlight_data* data = Highlight_objects_table.get_ptr(key);
        if (data->style == HIGHLIGHT_STYLE_GLOW)
        {
            object* op = World.object_from_handle(key);
            if (op != nullptr)
            {
                bool outline = true;
                if (op->get_obj_type() == OT_HUMAN)
                {
                    human* hp = static_cast<human*>(op);
                    if (human_in_vehicle(hp) != nullptr)
                    {
                        // The passenger's outline goes on a vehicle that is on screen.
                        vehicle* vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
                        if (vp != nullptr)
                        {
                            op = vp;
                            outline = !vp->is_offscreen();
                        }
                    }
                    else
                    {
                        outline = hp->hflags.was_rendered != 0;
                    }
                }
                if (outline)
                {
                    const int layer = data->color_idx != main_purpose ? 1 : 0;
                    layer_used[layer] |= create_glow_effect(op, layer);
                }
            }
        }
        unsigned int next_key = 0;
        more = Highlight_objects_table.get_next_key(next_key, key);
        key = next_key;
    }
    if (!layer_used[0])
    {
        state.layer[0] = RLOL_DISABLED;
    }
    if (!layer_used[1])
    {
        state.layer[1] = RLOL_DISABLED;
    }
    rl_scene* scene = game_render_get_main_scene();
    if (scene != nullptr)
    {
        scene->set_outline_state(state); // inlined
    }
}

// 0x00B2E370 (:1584)
void objective_highlight_level_init()
{
    Highlight_objects_table.clear();
}

// 0x00B239B0 (:1593)
void objective_highlight_system_init()
{
    Highlight_ground_marker_effect = effects_get_handle("gp_marker_target_a");
    Highlight_arrow_marker_effect = effects_get_handle("gp_target_indicator_a");
    Highlight_cylinder_marker_effect = effects_get_handle("gp_marker_human");
}

// 0x00B3A790 (:1605)
void objective_highlight_add(unsigned int handle, highlight_purpose color_hint, highlight_style style)
{
    if (handle == 0xFFFFFFFF || Highlight_objects_table.exists(handle))
    {
        return;
    }
    object* op = World.object_from_handle(handle);
    if (op == nullptr)
    {
        return;
    }
    if ((int)color_hint < 0 || (int)color_hint > HIGHLIGHT_PURPOSE_ACTIVITY)
    {
        color_hint = HIGHLIGHT_PURPOSE_DESTROY;
    }
    get_style_by_object(color_hint, style, op); // inlined
    highlight_data data;
    data.effect_obj_handle = 0xFFFFFFFF;
    data.color_idx = color_hint;
    data.style = style;
    Highlight_objects_table.insert_key_data(handle, data);
}

// 0x00B3A840 (:1639)
// A squad's members go with it.
void objective_highlight_remove(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        Highlight_objects_table.remove(handle);
        return;
    }
    clear_effect(handle);
    Highlight_objects_table.remove(handle);
    object* op = World.object_from_handle(handle);
    if (op != nullptr && op->get_obj_type() == OT_SQUAD)
    {
        object_squad* squad = static_cast<object_squad*>(op);
        for (int i = squad->member_array.size() - 1; i >= 0; i--)
        {
            objective_highlight_remove(squad->member_array[i]);
        }
    }
}

// 0x00B3A8B0 (:1660)
void objective_highlight_remove_all()
{
    unsigned int key = 0;
    bool more = Highlight_objects_table.get_first_key(key);
    while (more)
    {
        unsigned int next_key = 0;
        const bool has_next = Highlight_objects_table.get_next_key(next_key, key);
        objective_highlight_remove(key);
        key = next_key;
        more = has_next;
    }
    glow_cleanup();
}
