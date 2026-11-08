// Original: volition/rfg/code/code_effects/material_effects.cpp
//
// Material effects (VFX.md FX4 item 15): timed material overrides on render instances (the dropped weapon shine, the
// nano rifle and reconstructor fades, the backpack looks). Applying an effect to an instance gives the instance a
// tracker (its original material map and the effects applied to it, the last one shown); the effect gets a map library
// entry, shared per original map and effect, holding one rl_layered_material per material of the original map (the
// effect's layers over that material) in a new material map. The entry's materials are registered in the material
// library on the render side (pre-stream task, or at once when render_lib is not locked); the update-render task then
// swaps the instance's map for the entry's, feeds the effect's time into the instance parameters' w and ends the
// expired or removed effects, restoring the original map once an instance has none left.

#include "volition/rfg/code/code_effects/material_effects.h"

#include "volition/render_lib/code/rl_layered_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/hash_table.h"

#include <malloc.h>
#include <new>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0xC
struct ri_effects_tracker
{
    const rl_material_map* orig_mat_map_p; // 0x00
    material_effect* applied_effects_list; // 0x04
    unsigned int flags; // 0x08
};
static_assert(sizeof(ri_effects_tracker) == 0xC, "ri_effects_tracker");

// material_effect_parameters::flags (names inferred): 1 = fade out (time runs backwards), 2 = partial fade (up and down
// to Partial_fade_percentage), 4 = remove at the next update render.
static const unsigned int MATFX_FADE_OUT = 0x1;
static const unsigned int MATFX_PARTIAL_FADE = 0x2;
static const unsigned int MATFX_REMOVE = 0x4;
// ri_effects_tracker::flags: the shown effect changed, the instance's map must be swapped.
static const unsigned int RI_TRACKER_MAP_DIRTY = 0x1;
// rl_material_layer::m_flags of a layer (rl_layered_material.cpp): drawn in the depth pass, additive, base surface.
static const unsigned int LAYER_DEPTH_PASS = 0x1;
static const unsigned int LAYER_ADDITIVE = 0x8;
static const unsigned int LAYER_BASE_SURFACE = 0x10;

// ---- globals
material_map_library_entry* Finalize_map_list; // 0x01DEE4BC

// ---- file-static globals
static float Partial_fade_percentage = 0.5f; // 0x01648FEC
static data_hash_table<unsigned __int64,material_map_library_entry*,&hash_uint64,32,unsigned char> Map_library(false); // 0x01649BD8
static data_hash_table<unsigned int,ri_effects_tracker*,&hash_uint,255,unsigned char> Renderable_instance_hash(false); // 0x01649DA8
// material_effect_info Effect_infos[32] (0x01DEB6F0) and int Num_effect_infos (0x01DEE4C4) are defined in
// material_effects_shared.cpp.
static unsigned int Next_effect_guid; // 0x01DEE4B8
static bool Material_effects_init; // 0x01DEE4C0
static object_pool<ri_effects_tracker,255,1> Active_renderable_instances; // 0x01DF2CF0
static object_pool<material_map_library_entry,32,1> Map_library_entry_pool; // 0x01E1A010
static object_pool<material_effect,255,1> Effect_pool; // 0x01E1C600
static game_render_task Pre_stream_render_task; // 0x01E21EF0
static game_render_task Update_render_task_material; // 0x01E24BAC

// The map library key: the original map in the low half, the effect id << 16 in the high half.
static unsigned __int64 map_library_key(int effect_id, const rl_material_map* orig_mat_map)
{
    return (unsigned __int64)(unsigned int)(effect_id << 16) << 32 | (unsigned int)orig_mat_map;
}

// Unlinks an entry from the finalize list (inlined list macro in rfg.exe).
static void finalize_list_remove(material_map_library_entry* lib_entry_p)
{
    if (Finalize_map_list == lib_entry_p && (Finalize_map_list = lib_entry_p->next) == lib_entry_p)
    {
        lib_entry_p->next = nullptr;
        lib_entry_p->prev = nullptr;
        Finalize_map_list = nullptr;
        return;
    }
    if (lib_entry_p->prev != nullptr)
    {
        lib_entry_p->prev->next = lib_entry_p->next;
    }
    if (lib_entry_p->next != nullptr)
    {
        lib_entry_p->next->prev = lib_entry_p->prev;
    }
    lib_entry_p->next = nullptr;
    lib_entry_p->prev = nullptr;
}

// 0x006E1340 (.h:38)
material_effect_parameters::material_effect_parameters()
{
    lifetime_ms = -1;
    time_passed_ms = 0;
    flags = 0;
    cb_data = nullptr;
    update_render_done_cb = nullptr;
    world_pos = ZERO_VECTOR;
    use_local_pos = false;
    use_time_in_w = true;
}

// 0x006E4920
material_effect_parameters::material_effect_parameters(const material_effect_parameters& __that)
{
    lifetime_ms = __that.lifetime_ms;
    time_passed_ms = __that.time_passed_ms;
    flags = __that.flags;
    cb_data = __that.cb_data;
    update_render_done_cb = __that.update_render_done_cb;
    world_pos = __that.world_pos;
    use_local_pos = __that.use_local_pos;
    use_time_in_w = __that.use_time_in_w;
}

// 0x006E4540
material_effect_ri_dead_cb::material_effect_ri_dead_cb()
{
}

// 0x006E4560
material_effect_ri_dead_cb::~material_effect_ri_dead_cb()
{
}

// 0x006E5E30
material_map_library_entry::material_map_library_entry()
{
}

// 0x006E98E0 (:91)
material_effect::material_effect()
{
    on_ri_dead_cb.m_mat_effect = this;
    prev = nullptr;
    next = nullptr;
}

// 0x006F61A0 (:161)
// Console command (debug, unregistered in retail): applies effect 0 to the local player, its head and its items for
// the given milliseconds (3000 by default; negative fades out).
void unused_dcf_test_matfx_on_player()
{
    if (!Console_command)
    {
        return;
    }
    material_effect_parameters parameters;
    // Magic number: console argument types 1 (none) | 8 (integer).
    console_get_arg(9, 0);
    if ((Console_arg_type & 8) == 0)
    {
        parameters.lifetime_ms = 3000; // Magic number: 3 s
    }
    else if (Console_arg_int < 0)
    {
        parameters.lifetime_ms = -Console_arg_int;
        parameters.flags = MATFX_FADE_OUT;
    }
    else
    {
        parameters.lifetime_ms = Console_arg_int;
    }
    player* pp = player_get_local_player();
    // Original: no check that the character instance is loaded.
    material_effects_apply((rl_renderable_instance*)pp->char_inst->rdd.get_data()->renderable_instance_p, 0, &parameters);
    rl_renderable_instance* head_p = pp->rdd.is_loaded() ? (rl_renderable_instance*)pp->rdd.get_data()->head_renderable_instance_p : nullptr;
    if (head_p != nullptr)
    {
        material_effects_apply(head_p, 0, &parameters);
    }
    object* child = pp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            rl_renderable_instance* ri_p = ip->rdd.is_loaded() ? (rl_renderable_instance*)ip->rdd.get_data()->smesh_instance_p : nullptr;
            if (ri_p != nullptr)
            {
                material_effects_apply(ri_p, 0, &parameters);
            }
            if (ip->char_inst != nullptr)
            {
                ri_p = ip->char_inst->rdd.is_loaded() ? (rl_renderable_instance*)ip->char_inst->rdd.get_data()->renderable_instance_p : nullptr;
                if (ri_p != nullptr)
                {
                    material_effects_apply(ri_p, 0, &parameters);
                }
            }
        }
        child = child->child_next;
        if (child == pp->child_ptr)
        {
            break;
        }
    }
}

// 0x006F1360 (:250)
// The instance's tracker (made on its first effect, keeping its original map) with the new effect appended (shown).
ri_effects_tracker* get_ri_tracker(rl_renderable_instance* r_inst, material_effect* new_effect)
{
    ri_effects_tracker* tracker = nullptr;
    if (!Renderable_instance_hash.get((unsigned int)r_inst, tracker))
    {
        tracker = Active_renderable_instances.alloc(0);
        // Original: the hash entry is added before the null check (a full pool leaves a null tracker in the hash).
        Renderable_instance_hash.insert_key_data((unsigned int)r_inst, tracker);
        if (tracker == nullptr)
        {
            return nullptr;
        }
        tracker->orig_mat_map_p = r_inst->get_material_map();
        tracker->flags = 0;
        tracker->applied_effects_list = nullptr;
    }
    if (tracker->applied_effects_list != nullptr)
    {
        new_effect->prev = tracker->applied_effects_list->prev;
        new_effect->next = tracker->applied_effects_list;
        tracker->applied_effects_list->prev->next = new_effect;
        tracker->applied_effects_list->prev = new_effect;
    }
    else
    {
        new_effect->next = new_effect;
        new_effect->prev = new_effect;
        tracker->applied_effects_list = new_effect;
    }
    tracker->flags |= RI_TRACKER_MAP_DIRTY;
    return tracker;
}

// 0x006F1420 (:272)
// Removes the effect from the instance's tracker; the original map comes back when it was the shown one, and the
// tracker goes when it was the last.
void release_ri_tracker(rl_renderable_instance* r_inst, material_effect* effect)
{
    ri_effects_tracker* tracker = nullptr;
    if (!Renderable_instance_hash.get((unsigned int)r_inst, tracker))
    {
        return;
    }
    const material_effect* shown = tracker->applied_effects_list != nullptr ? tracker->applied_effects_list->prev : nullptr;
    if (shown == effect)
    {
        r_inst->set_material_map(tracker->orig_mat_map_p);
        tracker->flags |= RI_TRACKER_MAP_DIRTY;
    }
    if (tracker->applied_effects_list == effect && (tracker->applied_effects_list = effect->next) == effect)
    {
        effect->next = nullptr;
        effect->prev = nullptr;
        tracker->applied_effects_list = nullptr;
    }
    else
    {
        if (effect->prev != nullptr)
        {
            effect->prev->next = effect->next;
        }
        if (effect->next != nullptr)
        {
            effect->next->prev = effect->prev;
        }
        effect->next = nullptr;
        effect->prev = nullptr;
    }
    if (tracker->applied_effects_list == nullptr)
    {
        r_inst->set_material_map(tracker->orig_mat_map_p);
        tracker->orig_mat_map_p = nullptr;
        Renderable_instance_hash.remove((unsigned int)r_inst);
        Active_renderable_instances.free(tracker);
    }
}

// 0x006F14E0 (:297)
// The instance died: forget it and end the effect at the next update render.
void material_effect_ri_dead_cb::on_callback()
{
    m_mat_effect->ri_p->remove_on_death_cb(this);
    release_ri_tracker(m_mat_effect->ri_p, m_mat_effect);
    m_mat_effect->ri_p = nullptr;
    m_mat_effect->parameters.flags |= MATFX_REMOVE;
}

// 0x006F1520 (:305)
// Drops a reference; the last one unregisters and releases the entry's materials and map.
void maybe_destroy_map_library_entry(material_map_library_entry* lib_entry_p)
{
    if (lib_entry_p == nullptr)
    {
        return;
    }
    if (--lib_entry_p->ref_count != 0)
    {
        return;
    }
    Map_library.remove(map_library_key(lib_entry_p->effect_id, lib_entry_p->orig_mat_map_p));
    if (lib_entry_p->materials_registered)
    {
        for (int i = 0; i < lib_entry_p->num_materials; i++)
        {
            rl_material_library_unregister(lib_entry_p->materials[i]);
        }
        lib_entry_p->materials_registered = false;
    }
    if (lib_entry_p->next != nullptr && lib_entry_p->prev != nullptr)
    {
        finalize_list_remove(lib_entry_p);
    }
    if (lib_entry_p->mat_map_p != nullptr)
    {
        lib_entry_p->mat_map_p->release();
        lib_entry_p->mat_map_p = nullptr;
    }
    if (lib_entry_p->materials != nullptr)
    {
        for (int i = 0; i < lib_entry_p->num_materials; i++)
        {
            if (lib_entry_p->materials[i] != nullptr)
            {
                lib_entry_p->materials[i]->release();
                lib_entry_p->materials[i] = nullptr;
            }
        }
    }
    lib_entry_p->num_materials = 0;
    if (lib_entry_p->materials != nullptr)
    {
        Material_effects_mempool.release_bytes(lib_entry_p->materials);
        lib_entry_p->materials = nullptr;
    }
    if (lib_entry_p->material_layers != nullptr)
    {
        Material_effects_mempool.release_bytes(lib_entry_p->material_layers);
        lib_entry_p->material_layers = nullptr;
    }
    if (lib_entry_p->mat_map_handle_array != nullptr)
    {
        Material_effects_mempool.release_bytes(lib_entry_p->mat_map_handle_array);
        lib_entry_p->mat_map_handle_array = nullptr;
    }
    Map_library_entry_pool.free(lib_entry_p);
}

// 0x006F1660 (:389)
// LTCG: effect_p in ESI.
static void destroy_material_effect(material_effect* effect_p)
{
    if (effect_p->ri_p != nullptr)
    {
        effect_p->ri_p->remove_on_death_cb(&effect_p->on_ri_dead_cb);
        release_ri_tracker(effect_p->ri_p, effect_p);
        effect_p->ri_p = nullptr;
    }
    if (effect_p->mat_info != nullptr)
    {
        maybe_destroy_map_library_entry(effect_p->mat_info);
        effect_p->mat_info = nullptr;
    }
    effect_p->handle = 0xFFFFFFFF;
    Effect_pool.free(effect_p);
}

// 0x006E45C0 (:407)
// Registers the entry's materials and gives its map their handles.
void finalize_materials(material_map_library_entry* lib_entry_p)
{
    for (int i = 0; i < lib_entry_p->num_materials; i++)
    {
        lib_entry_p->mat_map_handle_array[i] = rl_material_library_register(lib_entry_p->materials[i]);
    }
    lib_entry_p->mat_map_data.materials = lib_entry_p->mat_map_handle_array;
    lib_entry_p->mat_map_data.num_materials = lib_entry_p->num_materials;
    lib_entry_p->mat_map_p->set_map_data(&lib_entry_p->mat_map_data);
    lib_entry_p->materials_registered = true;
}

// 0x006E4610 (:424)
// The pre-stream render task: finalizes the entries made while render_lib was locked.
static void material_effects_pre_stream_process_all(void*, rl_renderer*)
{
    material_map_library_entry* lib_entry_p = Finalize_map_list;
    while (lib_entry_p != nullptr)
    {
        material_map_library_entry* next = lib_entry_p->next != Finalize_map_list ? lib_entry_p->next : nullptr;
        finalize_materials(lib_entry_p);
        finalize_list_remove(lib_entry_p);
        lib_entry_p = next;
    }
}

// 0x006F16C0 (:436)
// The update-render task: ends the expired and removed effects, then shows each instance's last effect (its map once
// registered) with the effect's time in the instance parameters' w (and the effect's position in xyz when given).
static void material_effects_update_render_all(void*, rl_renderer*)
{
    {
        csspinlock_scope lock(Effect_pool.thread_access);
        material_effect* effect = Effect_pool.get_first(0);
        while (effect != nullptr)
        {
            material_effect* next = Effect_pool.get_next(effect);
            const bool expired = effect->parameters.lifetime_ms != -1 && effect->birthtime_ms + effect->parameters.lifetime_ms < (int)Frametime_total_milliseconds;
            if ((effect->parameters.flags & MATFX_REMOVE) != 0 || expired)
            {
                effect->parameters.flags &= ~MATFX_REMOVE;
                if (effect->parameters.update_render_done_cb != nullptr)
                {
                    effect->parameters.update_render_done_cb(effect->parameters.cb_data, effect->ri_p);
                }
                destroy_material_effect(effect);
            }
            effect = next;
        }
    }

    csspinlock_scope lock(Active_renderable_instances.thread_access);
    ri_effects_tracker* tracker = Active_renderable_instances.get_first(0);
    while (tracker != nullptr)
    {
        material_effect* shown = tracker->applied_effects_list != nullptr ? tracker->applied_effects_list->prev : nullptr;
        if (shown != nullptr && shown->ri_p != nullptr)
        {
            if ((tracker->flags & RI_TRACKER_MAP_DIRTY) != 0 && shown->mat_info->materials_registered)
            {
                shown->ri_p->set_material_map(shown->mat_info->mat_map_p);
                tracker->flags &= ~RI_TRACKER_MAP_DIRTY;
            }
            vector4 params = shown->ri_p->get_instance_params();
            if (shown->parameters.use_time_in_w)
            {
                if (shown->parameters.lifetime_ms == -1)
                {
                    params.w = shown->manual_time_pct;
                }
                else
                {
                    // x87 in rfg.exe: the elapsed milliseconds as unsigned, divided in extended precision.
                    float t = (float)((double)(unsigned int)(Frametime_total_milliseconds - shown->birthtime_ms) / (double)shown->parameters.lifetime_ms);
                    if ((shown->parameters.flags & MATFX_PARTIAL_FADE) != 0)
                    {
                        if (t >= 0.5f)
                        {
                            t = (float)(1.0 - (double)t);
                        }
                        t = (float)((double)Partial_fade_percentage * (double)t);
                    }
                    if ((shown->parameters.flags & MATFX_FADE_OUT) != 0)
                    {
                        t = (float)(1.0 - (double)t);
                    }
                    params.w = t;
                }
            }
            if (shown->parameters.use_local_pos)
            {
                params.x = shown->parameters.world_pos.x;
                params.y = shown->parameters.world_pos.y;
                params.z = shown->parameters.world_pos.z;
                shown->parameters.use_local_pos = false;
            }
            shown->ri_p->set_instance_params(params);
        }
        tracker = Active_renderable_instances.get_next(tracker);
    }
}

// 0x006F19F0 (:511)
// The shared entry of an effect over an original map (a new reference), made on first use: per material of the map a
// layered material of the effect's layers (a missing layer material drops the layer; the base surface layer takes the
// map's own material), in a new map queued for finalizing.
material_map_library_entry* find_or_create_map_library_entry(int effect_id, const rl_material_map* orig_mat_map)
{
    const unsigned __int64 key = map_library_key(effect_id, orig_mat_map);
    material_map_library_entry* lib_entry_p = nullptr;
    if (Map_library.get(key, lib_entry_p))
    {
        lib_entry_p->ref_count++;
        return lib_entry_p;
    }
    if (effect_id < 0 || effect_id >= Num_effect_infos)
    {
        return nullptr;
    }
    const material_effect_info* info = &Effect_infos[effect_id];
    lib_entry_p = Map_library_entry_pool.alloc(0);
    if (lib_entry_p == nullptr)
    {
        return nullptr;
    }
    lib_entry_p->orig_mat_map_p = orig_mat_map;
    lib_entry_p->materials = nullptr;
    lib_entry_p->material_layers = nullptr;
    lib_entry_p->mat_map_handle_array = nullptr;
    lib_entry_p->mat_map_p = nullptr;
    lib_entry_p->materials_registered = false;
    lib_entry_p->ref_count = 1;
    lib_entry_p->effect_id = effect_id;
    lib_entry_p->num_materials = 0;
    lib_entry_p->next = nullptr;
    lib_entry_p->prev = nullptr;
    const int num_maps = orig_mat_map->get_num_materials();
    const int num_layers = info->num_layers;
    lib_entry_p->materials = (rl_layered_material**)Material_effects_mempool.alloc(num_maps * sizeof(rl_layered_material*), 0);
    lib_entry_p->material_layers = (rl_material_layer*)Material_effects_mempool.alloc(num_layers * num_maps * sizeof(rl_material_layer), 0);
    lib_entry_p->mat_map_handle_array = (unsigned int*)Material_effects_mempool.alloc(num_maps * sizeof(unsigned int), 0);
    if (lib_entry_p->materials != nullptr && lib_entry_p->material_layers != nullptr && lib_entry_p->mat_map_handle_array != nullptr)
    {
        memset(lib_entry_p->materials, 0, num_maps * sizeof(rl_layered_material*));
        memset(lib_entry_p->material_layers, 0, num_layers * num_maps * sizeof(rl_material_layer));
        memset(lib_entry_p->mat_map_handle_array, 0, num_maps * sizeof(unsigned int));

        // The effect's layers once (the base surface's handle -1 is filled per material below).
        rl_material_layer* layer_template = (rl_material_layer*)_alloca(num_layers * sizeof(rl_material_layer));
        int num_used_layers = 0;
        for (int i = 0; i < num_layers; i++)
        {
            const material_effect_layer_info* layer_info = &info->layers[i];
            rl_material_layer* layer = &layer_template[num_used_layers];
            layer->m_flags = 0;
            if (layer_info->mat_name[0] == '\0' || layer_info->map_index == -1)
            {
                layer->m_mat_handle = 0xFFFFFFFF;
                layer->m_flags = LAYER_BASE_SURFACE;
            }
            else
            {
                material* mat_p = material_find(layer_info->mat_name, info->srid);
                if (mat_p == nullptr)
                {
                    continue;
                }
                layer->m_mat_handle = ((rl_material_map*)mat_p->m_material_map)->get_material(layer_info->map_index);
            }
            // Layer flags: Stencil_write (1) draws in the depth pass, Additive_blending (8) blends additively.
            if ((layer_info->flags & 1) != 0)
            {
                layer->m_flags |= LAYER_DEPTH_PASS;
            }
            if ((layer_info->flags & 8) != 0)
            {
                layer->m_flags |= LAYER_ADDITIVE;
            }
            num_used_layers++;
        }

        const unsigned int name_checksum = crc_stri(info->name, 0);
        bool failed = false;
        for (int i = 0; i < num_maps; i++)
        {
            rl_layered_material* layered_p = rl_layered_material::create();
            if (layered_p == nullptr)
            {
                failed = true;
                break;
            }
            rl_material_layer* layers = &lib_entry_p->material_layers[i * num_layers];
            for (int j = 0; j < num_used_layers; j++)
            {
                layers[j] = layer_template[j];
                if (layers[j].m_mat_handle == 0xFFFFFFFF)
                {
                    layers[j].m_mat_handle = orig_mat_map->get_material(i);
                }
            }
            layered_p->init(name_checksum, layers, num_used_layers);
            lib_entry_p->materials[i] = layered_p;
            lib_entry_p->num_materials++;
        }
        if (!failed)
        {
            lib_entry_p->mat_map_p = rl_material_map::create();
            if (lib_entry_p->mat_map_p != nullptr)
            {
                if (Finalize_map_list == nullptr)
                {
                    lib_entry_p->next = lib_entry_p;
                    lib_entry_p->prev = lib_entry_p;
                    Finalize_map_list = lib_entry_p;
                }
                else
                {
                    lib_entry_p->prev = Finalize_map_list->prev;
                    lib_entry_p->next = Finalize_map_list;
                    Finalize_map_list->prev->next = lib_entry_p;
                    Finalize_map_list->prev = lib_entry_p;
                }
                Map_library.insert_key_data(key, lib_entry_p);
                return lib_entry_p;
            }
        }
    }
    maybe_destroy_map_library_entry(lib_entry_p);
    return nullptr;
}

// 0x006F6300 (:674)
void material_effects_system_init()
{
    if (Material_effects_init)
    {
        return;
    }
    Material_effects_init = true;
    Update_render_task_material.m_callback = material_effects_update_render_all;
    game_render_update_render_task_register(&Update_render_task_material);
    Pre_stream_render_task.m_callback = material_effects_pre_stream_process_all;
    game_render_pre_stream_render_task_register(&Pre_stream_render_task);
    parse_material_effects_table();
    material_effects_generate_stream_data(&Common_mempool, &Material_effects_mempool, PLATFORM_PC);
    Map_library.clear();
    Renderable_instance_hash.clear();
}

// 0x006ED9D0 (:719)
void material_effects_system_shutdown()
{
    if (Material_effects_init)
    {
        Material_effects_init = false;
        material_effects_shared_system_shutdown();
    }
}

// 0x006F63E0 (:729)
unsigned int material_effects_apply(rl_renderable_instance* r_inst, const char* effect_name, const material_effect_parameters* parameters)
{
    const int effect_id = material_effects_find_by_name(effect_name);
    if (effect_id == -1)
    {
        return 0xFFFFFFFF;
    }
    return material_effects_apply(r_inst, effect_id, parameters);
}

// 0x006F1DA0 (:738)
unsigned int material_effects_apply(rl_renderable_instance* r_inst, int effect_id, const material_effect_parameters* parameters)
{
    if (r_inst == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if ((parameters->flags & MATFX_FADE_OUT) != 0 && (parameters->flags & MATFX_PARTIAL_FADE) == 0)
    {
        // A fading-out instance loses its decals.
        r_inst->remove_all_decals();
    }
    material_effect* effect = Effect_pool.alloc(0);
    if (effect == nullptr)
    {
        return 0xFFFFFFFF;
    }
    new (effect) material_effect();
    ri_effects_tracker* tracker = get_ri_tracker(r_inst, effect);
    if (tracker != nullptr)
    {
        effect->parameters = *parameters;
        // The handle: the pool index in the low 16 bits, a running guid in the high 16.
        effect->handle = ((unsigned int)Effect_pool.get_index(effect) & 0xFFFF) | Next_effect_guid++ << 16;
        effect->info = effect_id >= 0 && effect_id < Num_effect_infos ? &Effect_infos[effect_id] : nullptr;
        effect->birthtime_ms = (int)Frametime_total_milliseconds - parameters->time_passed_ms;
        effect->manual_time_pct = 1.0f;
        effect->ri_p = r_inst;
        r_inst->add_on_death_cb(&effect->on_ri_dead_cb);
        effect->mat_info = find_or_create_map_library_entry(effect_id, tracker->orig_mat_map_p);
        if (effect->mat_info != nullptr)
        {
            if (!rl_is_locked() && !effect->mat_info->materials_registered)
            {
                finalize_materials(effect->mat_info);
                finalize_list_remove(effect->mat_info);
            }
            return effect->handle;
        }
    }
    destroy_material_effect(effect);
    return 0xFFFFFFFF;
}

// The effect of a handle, or null when it ended (inlined).
static material_effect* material_effect_from_handle(unsigned int effect_handle)
{
    if (effect_handle == 0xFFFFFFFF || (effect_handle & 0xFFFF) >= Effect_pool.get_capacity())
    {
        return nullptr;
    }
    material_effect* effect = Effect_pool.get_entry(effect_handle & 0xFFFF);
    return effect->handle == effect_handle ? effect : nullptr;
}

// 0x006F1F50 (:786)
// While render_lib is locked the effect ends at the next update render.
void material_effects_remove(unsigned int effect_handle)
{
    material_effect* effect = material_effect_from_handle(effect_handle);
    if (effect == nullptr)
    {
        return;
    }
    if (rl_is_locked())
    {
        effect->parameters.flags |= MATFX_REMOVE;
        return;
    }
    if (effect->ri_p != nullptr)
    {
        effect->ri_p->set_material_map(effect->mat_info->orig_mat_map_p);
    }
    destroy_material_effect(effect);
}

// 0x006F1FA0 (:815)
void material_effects_destroy_all()
{
    const bool locked = rl_is_locked();
    csspinlock_scope lock(Effect_pool.thread_access);
    material_effect* effect = Effect_pool.get_first(0);
    while (effect != nullptr)
    {
        material_effect* next = Effect_pool.get_next(effect);
        if (locked)
        {
            effect->parameters.flags |= MATFX_REMOVE;
        }
        else
        {
            if (effect->ri_p != nullptr && effect->mat_info != nullptr)
            {
                effect->ri_p->set_material_map(effect->mat_info->orig_mat_map_p);
            }
            destroy_material_effect(effect);
        }
        effect = next;
    }
}

// 0x006ED9F0 (:834)
// The instance's map from before its effects.
const rl_material_map* material_effects_get_original_material_map(rl_renderable_instance* r_inst)
{
    if (r_inst == nullptr)
    {
        return nullptr;
    }
    ri_effects_tracker* tracker = nullptr;
    if (!Renderable_instance_hash.get((unsigned int)r_inst, tracker))
    {
        return r_inst->get_material_map();
    }
    return tracker->orig_mat_map_p;
}

// 0x006E9980 (:850)
void material_effects_set_manual_time_pct(unsigned int effect_handle, float time_pct)
{
    material_effect* effect = material_effect_from_handle(effect_handle);
    if (effect != nullptr)
    {
        effect->manual_time_pct = time_pct;
    }
}

// 0x006E46D0 (:870)
void material_effects_force_finalize()
{
    if (!rl_is_locked())
    {
        material_effects_pre_stream_process_all(nullptr, nullptr);
    }
}
