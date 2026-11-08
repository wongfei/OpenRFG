// Original: volition/rfg/code/effects/effects_shared.cpp
//
// Effect tables shared with the tools: vfx.xtbl (visual effects, each a streamed .cefct_pc container built through
// the asset assembler) and effects.xtbl (effects: a visual effect and/or a sound cue). An effect without either is
// dropped, so effect handles depend on which visual effects and cues exist.
//
// Notes (decompiled behaviour, kept as is):
// - visual_effects_parse_effect_shared leaves radial_blur_info_p untouched without a <Radial_blur> element and
//   los_blocker_life_span untouched without a <Blocker>.
// - effects_parse_table_shared does not reset the flags or the fade time of an entry it reuses after dropping the
//   previous one.

#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/always_loaded_shared.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/peg_assemble.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>

effect_info Effect_info[1500]; // 0x02182E60
unsigned int Num_effect_types; // 0x02182DF0
visual_effect_info Visual_effect_info[650]; // 0x021BEFA0
int Num_visual_effect_types; // 0x02182DF8

// 0x007420C0 (.h:108)
visual_effect_info::visual_effect_info()
{
    flags = 0;
    template_p = nullptr;
    stream_category = -1;
}

// 0x007420E0 (.h:134)
effect_info::effect_info()
{
    visual_handle = 0xFFFFFFFF;
    sound_handle = -1;
    sound_alr_handle = -1;
    name[0] = '\0';
    vfx_kill_particles_fade_time_s = 0.0f;
    flags.sound_kill_oneshot_when_done = false;
    flags.stop_when_host_destroyed = false;
    flags.vfx_kill_particles_on_stop = false;
    flags.sound_follows_effect_pos = true;
}

// 0x00742110 (.h:174)
int get_visual_effect_streaming_category(const char* stream_cat_text)
{
    // 0 preload, 1 multiplayer, 2 anything else (the "Environmental" comparison is unused).
    const int preload = v_stricmp(stream_cat_text, "Preload");
    const int multiplayer = v_stricmp(stream_cat_text, "Multiplayer");
    v_stricmp(stream_cat_text, "Environmental");
    if (preload == 0)
    {
        return 0;
    }
    return multiplayer != 0 ? 2 : 1;
}

// 0x00742A30 (:86)
static void visual_effects_parse_effect_shared(xml_element* effect, bool multiplayer_pass, vlib_platform plat, unsigned int& preload_cpu_size, unsigned int& preload_gpu_size, int& num_preloaded, int dlc_id)
{
    // preload_cpu_size and preload_gpu_size are unused (LTCG drops them). The multiplayer pass parses into a scratch
    // record and only builds containers for multiplayer-only effects.
    static const char* const effect_ext[NUM_VLIB_PLATFORMS] = { ".cefct_pc", ".cefct_pc", ".cefct_pc", ".cefct_pc", ".cefct_pc", ".cefct_pc", ".cefct_pc", ".cefct_pc" };
    const bool is_dlc = dlc_id != -1;
    const bool singleplayer = v_stricmp(xml_table_get_req_string_ref(effect, "Singleplayer"), "True") == 0;
    const bool multiplayer = v_stricmp(xml_table_get_req_string_ref(effect, "Multiplayer"), "True") == 0;

    visual_effect_info scratch;
    visual_effect_info* buf = multiplayer_pass ? &scratch : &Visual_effect_info[Num_visual_effect_types];
    buf->flags = multiplayer ? 8 : 0;
    if (singleplayer)
    {
        buf->flags |= 0x10;
    }
    xml_table_get_req_string(buf->name, 0x40, effect, "Name");
    buf->vfx_filename[0] = '\0';
    char filename[64];
    xml_table_get_req_filename(filename, sizeof(filename), effect, "VFX");
    cf_add_ext(buf->vfx_filename, 0x40, filename, effect_ext[plat], false);
    char category_text[128];
    xml_table_get_req_string(category_text, sizeof(category_text), effect, "Streaming_Category");
    const int category = get_visual_effect_streaming_category(category_text);
    buf->stream_category = category;

    // Container type: 3 preloaded, 2 passive (streamed with the world), 4 DLC, 5 multiplayer only, 0 otherwise.
    unsigned char container_type = 0;
    bool passive = false;
    bool preload = false;
    bool build_container = true;
    if (!multiplayer_pass || !multiplayer || singleplayer)
    {
        if (is_dlc)
        {
            container_type = 4;
        }
        else if (category == 0)
        {
            container_type = 3;
        }
        else if (category == 2)
        {
            container_type = 2;
            passive = true;
        }
        if (singleplayer && !multiplayer_pass)
        {
            preload = container_type == 3 || container_type == 4;
        }
        else if (!multiplayer || singleplayer || !multiplayer_pass)
        {
            build_container = false;
        }
        else
        {
            preload = true;
        }
    }
    else
    {
        container_type = 5;
        preload = true;
    }
    if (build_container)
    {
        buf->srid = stream2_container_find(buf->vfx_filename);
        if (buf->srid == 0)
        {
            assasm_container_start(buf->vfx_filename, container_type, preload, passive);
            stream2_prim* prim = assasm_container_add(buf->vfx_filename, 0x0C, "effects", false, true, false, nullptr, 0);
            if (prim != nullptr && (container_type == 3 || container_type == 4))
            {
                ++num_preloaded;
            }
            buf->srid = assasm_container_finish(true, is_dlc, false);
        }
    }

    buf->radius = 1.0f;
    xml_table_get_opt_float(buf->radius, effect, "Radius");
    buf->m_start_time = 0.0f;
    xml_table_get_opt_float(buf->m_start_time, effect, "Start_time");
    xml_element* radial_blur = xml_table_find_opt(effect, "Radial_blur");
    if (radial_blur != nullptr)
    {
        buf->radial_blur_info_p = effects_get_radial_blur_info(xml_table_get_opt_string_ref(radial_blur, "Radial_blur_entry"));
    }

    xml_element* lod = xml_table_find_opt(effect, "LOD");
    if (lod == nullptr)
    {
        buf->m_lod.enabled = false;
    }
    else
    {
        buf->m_lod.enabled = true;
        xml_element* spawning = xml_table_find_opt(lod, "Spawning");
        if (spawning == nullptr)
        {
            buf->m_lod.spawn_distance = 1e+08f;
            buf->m_lod.spawn_view_dependent = false;
        }
        else
        {
            xml_table_get_req_float(buf->m_lod.spawn_distance, spawning, "Distance");
            xml_table_get_req_bool(buf->m_lod.spawn_view_dependent, spawning, "View");
        }
        xml_element* distance = xml_table_find_opt(lod, "Distance");
        if (distance == nullptr)
        {
            buf->m_lod.update_render_fade_start = 1e+10f;
            buf->m_lod.update_render_fade_end = 1e+10f;
        }
        else
        {
            xml_table_get_req_float(buf->m_lod.update_render_fade_start, distance, "Fading_start");
            xml_table_get_req_float(buf->m_lod.update_render_fade_end, distance, "Fading_end");
            xml_table_get_req_bool(buf->m_lod.restore_faded_object, distance, "Restore");
        }
        xml_element* update = xml_table_find_opt(lod, "Update");
        if (update == nullptr)
        {
            buf->m_lod.update_view_dependent = false;
        }
        else
        {
            buf->m_lod.update_view_dependent = true;
            xml_table_get_req_float(buf->m_lod.view_dependent_minimum_time, update, "Minimum_time");
        }
    }

    xml_element* blocker = xml_table_find_opt(effect, "Blocker");
    if (blocker == nullptr)
    {
        buf->los_blocker_opacity = -1.0f;
    }
    else
    {
        buf->flags |= 4;
        xml_table_get_req_float(buf->los_blocker_opacity, blocker, "Opacity");
        // The original formats an assert message here into a scratch string (the check itself is compiled out).
        char message[128];
        sprintf(message, "%s: Opacity should be greater than 0", buf->name);
        xml_table_get_req_int(buf->los_blocker_life_span, blocker, "LifeSpan");
        if (buf->los_blocker_life_span < 1)
        {
            buf->los_blocker_life_span = -1;
        }
    }
    buf->template_p = nullptr;
    if (!multiplayer_pass)
    {
        ++Num_visual_effect_types;
    }
}

// 0x00742F10 (:291)
bool effects_is_valid(effect_info* info)
{
    return info != nullptr && (info->visual_handle != 0xFFFFFFFF || info->sound_handle != -1);
}

// 0x00742F30 (:317)
void visual_effects_parse_table_shared(pasm_data* peg_data, int& cpu_mark, int& gpu_mark, mempool_base* tmp_pool, mempool_base* pool, bool multiplayer_pass, vlib_platform plat, bool verify_only, int dlc_id)
{
    // Builds the vfx containers (and their peg) and records the pool marks after them. When the data is not loaded
    // from a preload packfile, bitmaps already in the peg are disabled in the containers.
    mempool_mark_scope mark_scope(tmp_pool);
    const bool is_dlc = dlc_id != -1;
    const bool load_from_preload = Game_preload || Use_packfiles;
    const char* system_name = is_dlc ? "dlc_vfx_containers" : (multiplayer_pass ? "vfx_mp_containers" : "vfx_containers");
    assasm_start(system_name, tmp_pool, pool, load_from_preload, pool, true, is_dlc, true, false);
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vfx.xtbl");
        root = xml_table_parse(filename);
    }
    else
    {
        root = xml_table_parse("vfx.xtbl");
    }
    int num_preloaded = 0;
    if (is_dlc)
    {
        if (!load_from_preload)
        {
            dlc_allocate_temp_container_list(10, &DLC_effect_preload_mempool);
        }
    }
    else if (!multiplayer_pass)
    {
        Num_visual_effect_types = 0;
    }
    unsigned int preload_cpu_size = 0;
    unsigned int preload_gpu_size = 0;
    for (xml_element* effect = xml_find(root, "Effect"); effect != nullptr; effect = xml_find_next(root, effect, "Effect"))
    {
        visual_effects_parse_effect_shared(effect, multiplayer_pass, plat, preload_cpu_size, preload_gpu_size, num_preloaded, dlc_id);
    }
    xml_table_close(root);

    if ((pool->get_flags() & 0x40) == 0)
    {
        cpu_mark = (int)pool->mark();
    }
    else
    {
        split_mempool* split_pool = static_cast<split_mempool*>(pool);
        cpu_mark = (int)split_pool->mark_cpu();
        gpu_mark = (int)split_pool->mark_gpu();
    }

    if (!load_from_preload)
    {
        for (int i = 0; i < Num_visual_effect_types; ++i)
        {
            const unsigned int srid = Visual_effect_info[i].srid;
            if (srid == 0)
            {
                continue;
            }
            stream2_container* container = stream2_get_container(srid);
            for (int j = 0; j < container->num_entries; ++j)
            {
                stream2_prim* prim = container->entries[j].prim;
                if (prim->pdef->prim_type == 1 && peg_asm_find_bitmap(prim->name))
                {
                    stream2_container_enable_prim(srid, prim->name, false);
                }
            }
        }
    }
    assasm_finish(peg_data, verify_only, multiplayer_pass, false);
    if (peg_data->cpu_data != nullptr)
    {
        always_loaded_add_peg((peg_header*)peg_data->cpu_data, plat);
    }
}

// 0x00745BB0 (:527)
void effects_parse_table_shared(int dlc_id)
{
    // Appends the effects of effects.xtbl (or "dlcNN_effects.xtbl"), up to 1500.
    xml_element* root;
    if (dlc_id == -1)
    {
        root = xml_table_parse("effects.xtbl");
    }
    else
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "effects.xtbl");
        root = xml_table_parse(filename);
    }
    for (xml_element* effect = xml_table_find_opt(root, "Effect"); effect != nullptr && Num_effect_types < 1500; effect = xml_table_find_next(root, effect, "Effect"))
    {
        effect_info& info = Effect_info[Num_effect_types];
        xml_table_get_req_string(info.name, 0x40, effect, "Name");
        const char* visual = xml_table_get_opt_string_ref(effect, "visual");
        const char* sound = xml_table_get_opt_string_ref(effect, "sound");
        const char* sound_alr = xml_table_get_opt_string_ref(effect, "sound_alr");
        info.visual_handle = 0xFFFFFFFF;
        if (visual != nullptr)
        {
            info.visual_handle = effects_get_visual_effect_info_handle(visual);
            if (info.visual_handle != 0xFFFFFFFF)
            {
                bool value;
                if (xml_table_get_opt_bool(value, effect, "vfx_kill_particles"))
                {
                    info.flags.vfx_kill_particles_on_stop = value;
                }
                xml_table_get_opt_float(info.vfx_kill_particles_fade_time_s, effect, "vfx_kill_particles_fade_time");
                if (xml_table_get_opt_bool(value, effect, "stop_when_host_destroyed"))
                {
                    info.flags.stop_when_host_destroyed = value;
                }
            }
        }
        info.sound_handle = -1;
        if (sound != nullptr)
        {
            audiolib_cue_get_id(sound, &info.sound_handle);
            if (info.sound_handle != -1)
            {
                bool value;
                if (xml_table_get_opt_bool(value, effect, "kill_sound_when_done"))
                {
                    info.flags.sound_kill_oneshot_when_done = value;
                }
                if (xml_table_get_opt_bool(value, effect, "sound_follows_effect"))
                {
                    info.flags.sound_follows_effect_pos = value;
                }
            }
        }
        info.sound_alr_handle = -1;
        if (sound_alr != nullptr)
        {
            audiolib_att_loop_rel_get_cue_id(sound_alr, &info.sound_alr_handle);
        }
        if (info.visual_handle != 0xFFFFFFFF || info.sound_handle != -1)
        {
            ++Num_effect_types;
        }
    }
    xml_table_close(root);
}

// 0x007431D0 (:633)
effect_info* effects_get_effect_info(unsigned int handle)
{
    return handle < Num_effect_types ? &Effect_info[handle] : nullptr;
}

// 0x00745E00 (:642)
effect_info* effects_get_effect_info(const char* name)
{
    return effects_get_effect_info(effects_get_handle(name));
}

// 0x007431F0 (:647)
unsigned int effects_get_handle(const char* name)
{
    if (name != nullptr)
    {
        for (unsigned int i = 0; i < Num_effect_types; ++i)
        {
            if (v_stricmp(name, Effect_info[i].name) == 0)
            {
                return i;
            }
        }
    }
    return 0xFFFFFFFF;
}

// 0x00743240 (:662)
unsigned int effects_get_handle(const effect_info* info)
{
    return (unsigned int)(info - Effect_info);
}

// 0x00743260 (:667)
visual_effect_info* effects_get_visual_effect_info(unsigned int handle)
{
    return handle < (unsigned int)Num_visual_effect_types ? &Visual_effect_info[handle] : nullptr;
}

// 0x00745E30 (:676)
visual_effect_info* effects_get_visual_effect_info(const char* name)
{
    const unsigned int handle = effects_get_visual_effect_info_handle(name);
    if (handle != 0xFFFFFFFF && handle < (unsigned int)Num_visual_effect_types)
    {
        return &Visual_effect_info[handle];
    }
    return nullptr;
}

// 0x00743280 (:685)
unsigned int effects_get_visual_effect_info_handle(const visual_effect_info* info)
{
    return (unsigned int)(info - Visual_effect_info);
}

// 0x007432A0 (:690)
unsigned int effects_get_visual_effect_info_handle(const char* name)
{
    if (name != nullptr)
    {
        for (int i = 0; i < Num_visual_effect_types; ++i)
        {
            if (v_stricmp(name, Visual_effect_info[i].name) == 0)
            {
                return (unsigned int)i;
            }
        }
    }
    return 0xFFFFFFFF;
}
