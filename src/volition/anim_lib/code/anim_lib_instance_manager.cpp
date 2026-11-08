// Original: volition/anim_lib/code/anim_lib_instance_manager.cpp
//
// anim_instance_manager: up to 10 animations playing on one rig instance. Per frame process_animations advances their
// times (looping animations share one cyclic percent, scaled by the dominant looping animation's length), ramps them
// in and out and sets the trigger bits; get_local_bone_matrices blends the animations' bone rotations and
// translations by priority (higher priorities first, each priority level taking what the levels above left), additive
// animations (flags & 0x2000) on top; instance_bone_matrices builds the model space bone matrices down the hierarchy
// and update_vertex_to_model_matrices the skinning matrices. The bone matrices live in pages of two page mempools
// over Bone_matrix_buffer, aged and released when an instance stops being transformed. Part of anim_lib_unity.cpp
// (player_minimal.md P2).
//
// Times are ticks (4800 per second). Instance handles are signature << 4 (signature 28 bits). Trigger bits per
// trigger n of an animation: 3n just elapsed, 3n + 1 elapsed, 3n + 2 suppressed.
// Precision: the vector and matrix operators round every step to float; scalar expressions are evaluated in double
// and rounded once, as rfg.exe does (the chains are written in double where they have more than one operation).
// Not ported (not reached): reset_looping_animations, transform_all_bones_pre/_post, get_base_pose_tag_pos,
// the set_*/get_* accessors not listed below.
// The external interface is declared in anim_lib.h.

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_internal.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <string.h>

static const int ANIM_MAX_ACTIVE = 10;
static const int ANIM_INSTANCE_MANAGERS = 356;
static const unsigned int ANIM_FLAG_LOOPING = 0x1;
static const unsigned int ANIM_FLAG_REVERSE = 0x2;
static const unsigned int ANIM_FLAG_FREEZE_AT_END = 0x4;
static const unsigned int ANIM_FLAG_FINISHED = 0x100;
static const unsigned int ANIM_FLAG_NEW_INSTANCE = 0x200;
static const unsigned int ANIM_FLAG_ADDITIVE = 0x2000;
static const unsigned int ANIM_FLAG_SNAP_TO_FRAMES = 0x8000;
static const unsigned int ANIM_FLAG_MANUAL_TIME = 0x10000;
static const unsigned int ANIM_FLAG_NO_CYCLE_DRIVE = 0x20000;

// ---- globals
static int Next_animation_sig = 100; // 0x016F0890
static animlib_page_pool_manager* Morph_page_pool_manager_list; // 0x038BD6F4
static animlib_page_pool_manager* Unused_page_pool_manager_list; // 0x038CA4D0
static anim_instance_manager* Anim_inst_manager_used_list; // 0x038CA4D4
animlib_page_pool_manager Animlib_page_pool_managers[680]; // 0x038D8BF0
static unsigned char Bone_matrix_buffer[682560]; // 0x038DC110
static anim_instance_manager* Anim_inst_manager_free_list; // 0x03982B50
static int Num_animlib_entity_class_bone_infos; // 0x03982B54
static animlib_page_pool_manager* Large_page_pool_manager_list; // 0x03982B5C
static int Default_bone_list[64]; // 0x03982B60, every bone at LOD 1
// The large pool: 0x87000 bytes in 0x1B00 byte pages (instances of 41 bones or more); the small pool: the rest
// (0x1FA40 bytes) in 0xD8 byte pages (smaller instances and the morph infos).
static page_mempool Bone_pool_large(Bone_matrix_buffer, 0x87000, 0x1B00, "Animlib large bone pool", 1, 0); // 0x039ACB20
static page_mempool Bone_pool_small(Bone_matrix_buffer + 0x87000, 0x1FA40, 0xD8, "Animlib small bone pool", 1, 0); // 0x039ACB78
static anim_instance_manager Anim_instance_managers[ANIM_INSTANCE_MANAGERS]; // 0x039ACBD0
static object_pool<s_bone_modifier_data, 101, 1> bone_modifier_data_list; // 0x03A07210

// 0x00C94990
s_bone_modifier_data::s_bone_modifier_data()
{
}

// 0x00C90A70
bone_modifier_data::bone_modifier_data()
{
}

// 0x00C8B3E0
anim_instance_manager::anim_instance_manager()
{
}

// 0x00C89170 (anim_lib_types.h:570). A member-wise copy (by value, as declared).
void anim_instance_info::operator=(anim_instance_info source)
{
    speed_multiplier = source.speed_multiplier;
    init_cyclic_percent = source.init_cyclic_percent;
    signature = source.signature;
    base_mix_percent = source.base_mix_percent;
    mix_percent = source.mix_percent;
    anim_handle = source.anim_handle;
    prev_time = source.prev_time;
    remainder_time = source.remainder_time;
    cur_time = source.cur_time;
    flags = source.flags;
    ramp = source.ramp;
    priority = source.priority;
    trigger_data = source.trigger_data;
}

// The instance of a handle; null for a negative handle.
static anim_instance_manager* aim_from_handle(int aim_handle)
{
    return aim_handle < 0 ? nullptr : &Anim_instance_managers[aim_handle % ANIM_INSTANCE_MANAGERS];
}

static bool anim_handle_valid(int anim_handle)
{
    return anim_handle > -1 && anim_handle < Num_animations;
}

static int anim_end_ticks(int anim_handle)
{
    const anim_file* amf = Animations[anim_handle].animation_data;
    return amf != nullptr ? amf->end_tock * 80 : 0;
}

static unsigned int signature_of(unsigned int handle)
{
    return ((int)handle >> 4) & 0xFFFFFFF;
}

// floor((double)x) as the binary's inline SSE2 floor, which is exact.
static double floor_d(double x)
{
    return floor(x);
}

// 0x00C8B3F0
anim_instance_manager* get_anim_instance_manager_from_handle(int aim_handle)
{
    return aim_from_handle(aim_handle);
}

// 0x00C8B430
static void entity_class_bone_info_init()
{
    for (anim_entity_class_bone_info& info : Animlib_entity_class_bone_info) {
        info.unique_id = -1;
    }
    Num_animlib_entity_class_bone_infos = 0;
}

// 0x00C8B480; LTCG: amf on the stack, bone_data EAX. Reads the next bone's key counts and sets the key and time
// pointers (see anim_lib_internal.h); bone_idx counts the bones read.
static void setup_bone_data(const anim_file* const amf, animlib_compressed_data_reader& bone_data)
{
    unsigned char* p = (unsigned char*)bone_data.next_bone_start;
    bone_data.bone_idx++;
    bone_data.time_as_shorts = ((amf->flags >> 1) & 1) != 0;
    if (amf->flags & 0x80) {
        p = (unsigned char*)(((unsigned int)p + 1) & ~1u);
        bone_data.num_rotation_keys = ((unsigned short*)p)[0];
        bone_data.num_translation_keys = ((unsigned short*)p)[1];
        p += 4;
        if (amf->flags & 0x20) {
            bone_data.num_scale_keys = *(unsigned short*)p;
            p += 2;
        } else {
            bone_data.num_scale_keys = 0;
        }
    } else {
        bone_data.num_rotation_keys = p[0];
        bone_data.num_translation_keys = p[1];
        p += 2;
        if (amf->flags & 0x20) {
            bone_data.num_scale_keys = *p;
            p += 1;
        } else {
            bone_data.num_scale_keys = 0;
        }
    }
    const int num_rotation_keys = bone_data.num_rotation_keys;
    bone_data.rotation_offsets = (afc_rotation_offset*)p;
    bone_data.num_rotation_offsets = 0;
    for (int keys = 0; keys < num_rotation_keys; bone_data.num_rotation_offsets++) {
        keys += bone_data.rotation_offsets[bone_data.num_rotation_offsets].count + 1;
    }
    bone_data.rotation_keys = (afc_rotation_key*)(p + bone_data.num_rotation_offsets * 5);
    p = (unsigned char*)bone_data.rotation_keys + num_rotation_keys * 4;
    if (amf->flags & 2) {
        p = (unsigned char*)(((unsigned int)p + 1) & ~1u);
        bone_data.rotation_times = p;
        p += num_rotation_keys * 2;
    } else {
        bone_data.rotation_times = p;
        p += num_rotation_keys;
    }
    const int num_translation_keys = bone_data.num_translation_keys;
    bone_data.num_translation_offsets = 0;
    afc_translation_offset* offsets = (afc_translation_offset*)(((unsigned int)p + 1) & ~1u);
    bone_data.translation_offsets = offsets;
    for (int keys = 0; keys < num_translation_keys;) {
        keys += offsets[bone_data.num_translation_offsets].num_keys;
        bone_data.num_translation_offsets++;
    }
    bone_data.translation_keys = (afc_translation_key*)(offsets + bone_data.num_translation_offsets);
    p = (unsigned char*)bone_data.translation_keys + num_translation_keys * 3;
    if (amf->flags & 2) {
        p = (unsigned char*)(((unsigned int)p + 1) & ~1u);
        bone_data.translation_times = p;
        p += num_translation_keys * 2;
    } else {
        bone_data.translation_times = p;
        p += num_translation_keys;
    }
    const int num_scale_keys = bone_data.num_scale_keys;
    if (num_scale_keys > 0) {
        bone_data.scale_keys = (afc_scale_key*)(((unsigned int)p + 3) & ~3u);
        p = (unsigned char*)(bone_data.scale_keys + num_scale_keys);
        bone_data.scale_times = p;
        p += (amf->flags & 2) ? num_scale_keys * 2 : num_scale_keys;
    }
    bone_data.next_bone_start = (char*)p;
}

// 0x00C8B620; LTCG: amf and data_pointer on the stack, bone_data ECX, num_bones EDX. The weight key counts per bone
// (8- or 16-bit, flags & 0x80), their running sums (weight_indicies), the keys and times.
static void setup_bone_weights(const anim_file* const amf, char* data_pointer, animlib_compressed_data_reader& bone_data, int num_bones)
{
    bone_data.num_weight_keys = data_pointer;
    if ((amf->flags & 0x40) == 0) {
        bone_data.next_bone_start = data_pointer;
        return;
    }
    bone_data.weight_as_shorts = (amf->flags >> 7) != 0;
    bone_data.weight_indicies[0] = 0;
    char* end;
    unsigned int last_count;
    if ((amf->flags & 0x80) == 0) {
        const unsigned char* counts = (const unsigned char*)data_pointer;
        end = data_pointer + num_bones;
        for (int i = 1; i < num_bones; i++) {
            bone_data.weight_indicies[i] = counts[i - 1] + bone_data.weight_indicies[i - 1];
        }
        last_count = (unsigned char)end[-1];
    } else {
        const unsigned short* counts = (const unsigned short*)data_pointer;
        end = data_pointer + num_bones * 2;
        for (int i = 1; i < num_bones; i++) {
            bone_data.weight_indicies[i] = counts[i - 1] + bone_data.weight_indicies[i - 1];
        }
        last_count = *(const unsigned short*)(end - 2);
    }
    const int total = (int)last_count + bone_data.weight_indicies[num_bones - 1];
    bone_data.weight_keys = (afc_weight_key*)(((unsigned int)end + 3) & ~3u);
    bone_data.weight_times = bone_data.weight_keys + total;
    if (amf->flags & 2) {
        bone_data.next_bone_start = (char*)bone_data.weight_times + total * 2;
    } else {
        bone_data.next_bone_start = (char*)bone_data.weight_times + total;
    }
}

// 0x00C95FF0. A reader at the animation's first bone: the rig-to-anim bone map (when the animation has one; its
// offset is relative to the anim_file) and the weights.
void anim_get_bone_data(anim_file* amf, animlib_compressed_data_reader& bone_data)
{
    bone_data.init_level = 1;
    char* data_pointer = (char*)(amf + 1);
    if (amf->flags & 0x10) {
        data_pointer += 8; // the anim_morph table
    }
    const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
    if (map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0) {
        memset(bone_data.rig_to_anim_bone_mapping, 0xFF, amf->num_rig_bones);
        const unsigned char* map = (const unsigned char*)amf + map_offset;
        for (unsigned char b = 0; b < amf->num_bones; b++) {
            if (map[b] != 0xFF) {
                bone_data.rig_to_anim_bone_mapping[map[b]] = (signed char)b;
            }
        }
        data_pointer += amf->num_bones;
    }
    setup_bone_weights(amf, data_pointer, bone_data, amf->num_bones);
}

// anim_get_rc_bone_data 0x00C92F90: not reached.

// 0x00C8B6F0. The common bones map to themselves; the others through the entity class's bone list.
void register_entity_base_bone_set(const anim_rig* rig, const animlib_bones_used_info& bones_used_info)
{
    if (Num_animlib_entity_class_bone_infos > 147) {
        keen::debug::nativePrintArgs("Out of animlib_entity_bone_infos");
    }
    anim_entity_class_bone_info* info = nullptr;
    for (anim_entity_class_bone_info& candidate : Animlib_entity_class_bone_info) {
        if (candidate.unique_id == -1) {
            info = &candidate;
            break;
        }
    }
    if (info == nullptr) {
        keen::debug::nativePrintArgs("could not find animlib_entity_bone_infos");
    }
    // The original continues with a null info here (crash); the table holds 148 entries.
    Num_animlib_entity_class_bone_infos++;
    memset(info, 0xFF, sizeof(anim_entity_class_bone_info));
    info->unique_id = bones_used_info.unique_id;
    info->num_bones = bones_used_info.num_bones;
    info->num_common_bones = rig->num_common_bones;
    for (int i = 0; i < info->num_common_bones; i++) {
        info->entity_to_rig_bone_mapping[i] = (signed char)i;
        info->rig_to_entity_bone_mapping[i] = (signed char)i;
        info->use_special_blend_mode[i] = 0;
    }
    for (int i = info->num_common_bones; i < info->num_bones; i++) {
        info->entity_to_rig_bone_mapping[i] = (signed char)bones_used_info.bone_indices[i];
        info->rig_to_entity_bone_mapping[bones_used_info.bone_indices[i]] = (signed char)i;
        info->use_special_blend_mode[i] = 0;
    }
}

// 0x00C8B7E0
void unregister_entity_base_bone_set(int entity_class_id)
{
    for (anim_entity_class_bone_info& info : Animlib_entity_class_bone_info) {
        if (info.unique_id == entity_class_id) {
            Num_animlib_entity_class_bone_infos--;
            info.unique_id = -1;
            return;
        }
    }
}

// 0x00C8B810
static int get_next_animation_signature()
{
    const int signature = Next_animation_sig;
    Next_animation_sig++;
    if (Next_animation_sig > 0x7FFFFFF) {
        Next_animation_sig = 1;
    }
    return signature;
}

// 0x00C8BA80
void anim_instance_system_init()
{
    Anim_inst_manager_free_list = nullptr;
    Anim_inst_manager_used_list = nullptr;
    for (int i = 0; i < ANIM_INSTANCE_MANAGERS; i++) {
        anim_instance_manager* aim = &Anim_instance_managers[i];
        if (Anim_inst_manager_free_list == nullptr) {
            aim->anim_instance_next = aim;
            aim->anim_instance_prev = aim;
            Anim_inst_manager_free_list = aim;
        } else {
            aim->anim_instance_prev = Anim_inst_manager_free_list->anim_instance_prev;
            aim->anim_instance_next = Anim_inst_manager_free_list;
            Anim_inst_manager_free_list->anim_instance_prev->anim_instance_next = aim;
            Anim_inst_manager_free_list->anim_instance_prev = aim;
        }
        aim->handle = i;
    }
    Unused_page_pool_manager_list = nullptr;
    Large_page_pool_manager_list = nullptr;
    Small_page_pool_manager_list = nullptr;
    Morph_page_pool_manager_list = nullptr;
    for (animlib_page_pool_manager& page : Animlib_page_pool_managers) {
        animlib_list_append(Unused_page_pool_manager_list, &page);
    }
    for (int& lod : Default_bone_list) {
        lod = 1;
    }
    entity_class_bone_info_init();
}

// 0x00C8BCF0
void anim_instance_system_shutdown()
{
}

// One aging pass over a page list: a page not renewed for two frames goes back to the unused list. The next page is
// taken before the page may be unlinked, against the list head as it is at that point (decompiled).
static void age_page_list(animlib_page_pool_manager*& list, page_mempool& pool, bool morph)
{
    animlib_page_pool_manager* head = list;
    animlib_page_pool_manager* page = list;
    while (page != nullptr) {
        animlib_page_pool_manager* next = page->next != head ? page->next : nullptr;
        page->age--;
        if (page->age == 0) {
            pool.release_bytes(page->buf);
            if (morph) {
                page->aim->morph_info = nullptr;
                page->aim->num_active_morphs = 0;
            } else {
                page->aim->cache_vertex_to_model = nullptr;
                page->aim->cache_instanced_model = nullptr;
                page->aim->bone_info = nullptr;
                page->aim->cache_instanced_model_valid = false;
            }
            animlib_list_remove(list, page);
            head = list;
            animlib_list_append(Unused_page_pool_manager_list, page);
        }
        page = next;
    }
}

// 0x00C8BD00
void anim_instance_do_frame(bool clear_morphs)
{
    age_page_list(Large_page_pool_manager_list, Bone_pool_large, false);
    age_page_list(Small_page_pool_manager_list, Bone_pool_small, false);
    age_page_list(Morph_page_pool_manager_list, Bone_pool_small, true);
    if (clear_morphs) {
        anim_instance_manager* aim = Anim_inst_manager_used_list;
        while (aim != nullptr) {
            aim->num_active_morphs = 0;
            aim = aim->anim_instance_next;
            if (aim == Anim_inst_manager_used_list) {
                break;
            }
        }
    }
}

// 0x00C92FC0. Takes the head of the free list; the handle advances by 356 per use so stale handles miss.
int anim_instance_manager_create(anim_rig* rig, int entity_class_id)
{
    anim_instance_manager* aim = Anim_inst_manager_free_list;
    if (aim == nullptr) {
        keen::debug::nativePrintArgs("Unable to allocate anim instance manager");
        return -1;
    }
    aim->handle += ANIM_INSTANCE_MANAGERS;
    aim->cyclic_percent = 0.0f;
    aim->old_cyclic_percent = 0.0f;
    aim->partial_frame_amount = 0.0f;
    aim->num_active_anims = 0;
    aim->animation_to_freeze_sig = -1;
    aim->frozen = 0;
    aim->cyclic_speed = 1.0f;
    memset(aim->active_anims, 0, sizeof(aim->active_anims));
    aim->rig = rig;
    aim->morph_data = nullptr;
    aim->morph_func = nullptr;
    aim->bone_bbox_min.x = -0.5f;
    aim->bone_bbox_min.y = 0.0f;
    aim->bone_bbox_min.z = -0.5f;
    aim->rig_data_size = rig->data_size;
    aim->parent_rig_aim_handle = -1;
    aim->do_bone_scaling = false;
    aim->locked = 0;
    aim->bone_bbox_max.x = 0.5f;
    aim->bone_bbox_max.y = 1.8f;
    aim->bone_bbox_max.z = 0.5f;
    aim->cache_vertex_to_model = nullptr;
    aim->cache_instanced_model = nullptr;
    aim->cache_instanced_model_valid = false;
    aim->entity_bone_info = nullptr;
    for (anim_entity_class_bone_info& info : Animlib_entity_class_bone_info) {
        if (info.unique_id == entity_class_id) {
            aim->entity_bone_info = &info;
            break;
        }
    }
    aim->num_active_morphs = 0;
    for (anim_instance_info& info : aim->active_anims) {
        info.anim_handle = -1;
    }
    for (int& index : aim->parent_rig_bone_indecies) {
        index = -1;
    }
    if (aim->bone_info != nullptr) {
        memset(aim->bone_info, 0, aim->entity_bone_info->num_bones * sizeof(anim_bone_info));
    }
    Anim_inst_manager_free_list = aim->anim_instance_next;
    if (Anim_inst_manager_free_list == aim) {
        Anim_inst_manager_free_list = nullptr;
    } else {
        aim->anim_instance_prev->anim_instance_next = aim->anim_instance_next;
        aim->anim_instance_next->anim_instance_prev = aim->anim_instance_prev;
    }
    aim->anim_instance_prev = nullptr;
    aim->anim_instance_next = nullptr;
    if (Anim_inst_manager_used_list == nullptr) {
        aim->anim_instance_prev = aim;
        aim->anim_instance_next = aim;
        Anim_inst_manager_used_list = aim;
    } else {
        aim->anim_instance_prev = Anim_inst_manager_used_list->anim_instance_prev;
        aim->anim_instance_next = Anim_inst_manager_used_list;
        Anim_inst_manager_used_list->anim_instance_prev->anim_instance_next = aim;
        Anim_inst_manager_used_list->anim_instance_prev = aim;
    }
    return aim->handle;
}

// 0x00C98E30
void anim_instance_manager_delete(anim_instance_manager* aim)
{
    if (aim == nullptr) {
        return;
    }
    while (aim->num_active_anims > 0) {
        const int signature = aim->active_anims[0].signature;
        aim->remove_active_anim(signature == -1 ? 0xFFFFFFFF : (unsigned int)signature << 4);
    }
    if (aim->cache_vertex_to_model != nullptr) {
        // The page of the instance (the original crashes if it is not found). Note the small list is chosen below
        // 40 bones although allocate_bone_matrices uses it below 41 (original inconsistency, kept).
        const bool small = aim->entity_bone_info->num_bones < 40;
        animlib_page_pool_manager*& list = small ? Small_page_pool_manager_list : Large_page_pool_manager_list;
        animlib_page_pool_manager* page = list;
        animlib_page_pool_manager* found = nullptr;
        while (page != nullptr) {
            if (page->aim == aim) {
                found = page;
                break;
            }
            page = page->next;
            if (page == list) {
                break;
            }
        }
        (small ? Bone_pool_small : Bone_pool_large).release_bytes(found->buf);
        animlib_list_remove(list, found);
        animlib_list_append(Unused_page_pool_manager_list, found);
    }
    aim->cache_vertex_to_model = nullptr;
    aim->cache_instanced_model = nullptr;
    aim->cache_instanced_model_valid = false;
    aim->bone_info = nullptr;
    if (aim->bone_modifiers != nullptr) {
        bone_modifier_data_list.free(aim->bone_modifiers);
        aim->bone_modifiers = nullptr;
    }
    if (Anim_inst_manager_used_list == aim && (Anim_inst_manager_used_list = aim->anim_instance_next) == aim) {
        Anim_inst_manager_used_list = nullptr;
    } else {
        aim->anim_instance_prev->anim_instance_next = aim->anim_instance_next;
        aim->anim_instance_next->anim_instance_prev = aim->anim_instance_prev;
    }
    aim->anim_instance_prev = nullptr;
    aim->anim_instance_next = nullptr;
    if (Anim_inst_manager_free_list == nullptr) {
        aim->anim_instance_next = aim;
        aim->anim_instance_prev = aim;
        Anim_inst_manager_free_list = aim;
    } else {
        aim->anim_instance_prev = Anim_inst_manager_free_list->anim_instance_prev;
        aim->anim_instance_next = Anim_inst_manager_free_list;
        Anim_inst_manager_free_list->anim_instance_prev->anim_instance_next = aim;
        Anim_inst_manager_free_list->anim_instance_prev = aim;
    }
}

// 0x00C8BF70 (not reached).
void anim_instance_manager::init()
{
}

// 0x00C8BFE0
void anim_instance_manager::invalidate_cache_matrix(int bone_index, bool instanced_model, bool vertex_to_model)
{
    if (bone_index > -1 && bone_index < entity_bone_info->num_bones) {
        if (instanced_model) {
            bone_info[bone_index].flags.instanced_model_matrix_valid = 0;
            cache_instanced_model[bone_index].make_identity();
        }
        if (vertex_to_model) {
            bone_info[bone_index].flags.vertex_to_model_matrix_valid = 0;
            cache_vertex_to_model[bone_index].make_identity();
        }
    }
}

// 0x00C8C080
void anim_instance_manager::invalidate_cache_matrix_list(int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model)
{
    for (int i = 0; i < num_bones; i++) {
        invalidate_cache_matrix(bone_list[i], instanced_model, vertex_to_model);
    }
}

// 0x00C8C100
// The listed bones' cached matrices marked valid (their contents as the caller left them; cloth_simulation).
void anim_instance_manager::validate_cache_matrix_list(int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model)
{
    for (int i = 0; i < num_bones; i++) {
        const int bone_index = bone_list[i];
        if (bone_index > -1 && bone_index < entity_bone_info->num_bones) {
            if (instanced_model) {
                bone_info[bone_index].flags.instanced_model_matrix_valid = 1;
            }
            if (vertex_to_model) {
                bone_info[bone_index].flags.vertex_to_model_matrix_valid = 1;
            }
        }
    }
}

// 0x00C8C4E0. The local bone matrices from the model space ones: each bone through the inverse of its parent's
// rotation, rows renormalized by the length of the first row.
void anim_instance_manager::get_bone_matrices_from_instanced(matrix43* bone_matrix)
{
    const int num_bones = entity_bone_info->num_bones;
    for (int i = 1; i < num_bones; i++) {
        const matrix43& parent = cache_instanced_model[entity_bone_info->rig_to_entity_bone_mapping[rig->bones[entity_bone_info->entity_to_rig_bone_mapping[i]].parent_index]];
        const matrix43& bone = cache_instanced_model[i];
        const matrix inv = parent.m_rotation.copy_transpose();
        bone_matrix[i].m_rotation = bone.m_rotation * inv;
        const float dx = bone.m_translation.x - parent.m_translation.x;
        const float dy = bone.m_translation.y - parent.m_translation.y;
        const float dz = bone.m_translation.z - parent.m_translation.z;
        bone_matrix[i].m_translation.x = inv.rvec.x * dx + inv.uvec.x * dy + inv.fvec.x * dz;
        bone_matrix[i].m_translation.y = inv.rvec.y * dx + inv.uvec.y * dy + inv.fvec.y * dz;
        bone_matrix[i].m_translation.z = inv.rvec.z * dx + inv.uvec.z * dy + inv.fvec.z * dz;
        matrix& m = bone_matrix[i].m_rotation;
        const float scale = 1.0f / sqrtf(m.rvec.x * m.rvec.x + m.rvec.y * m.rvec.y + m.rvec.z * m.rvec.z);
        m.rvec.x *= scale;
        m.rvec.y *= scale;
        m.rvec.z *= scale;
        m.uvec.x *= scale;
        m.uvec.y *= scale;
        m.uvec.z *= scale;
        m.fvec.x *= scale;
        m.fvec.y *= scale;
        m.fvec.z *= scale;
        bone_matrix[i].m_translation.x *= scale;
        bone_matrix[i].m_translation.y *= scale;
        bone_matrix[i].m_translation.z *= scale;
    }
    bone_matrix[0] = cache_instanced_model[0];
}

// 0x00C8C940. The skinning matrices: each bone's model space matrix with the bone's inverse bind translation applied.
// A bone below the LOD level whose matrix is not valid takes its parent's rotation; a bone driven by the parent rig
// copies the parent rig's skinning matrix.
void anim_instance_manager::update_vertex_to_model_matrices(int* bone_transform_list, int bone_lod_level)
{
    if (bone_transform_list == nullptr) {
        bone_transform_list = Default_bone_list;
    }
    const int num_bones = entity_bone_info->num_bones;
    const anim_bone* bones = rig->bones;
    matrix43* parent_vertex_to_model = nullptr;
    if (parent_rig_aim_handle != -1) {
        parent_vertex_to_model = aim_from_handle(parent_rig_aim_handle)->cache_vertex_to_model;
    }
    for (int i = 0; i < num_bones; i++) {
        const int parent_bone = parent_rig_bone_indecies[i];
        if (parent_bone != -1 && parent_vertex_to_model != nullptr) {
            cache_vertex_to_model[i] = parent_vertex_to_model[parent_bone];
            continue;
        }
        const int rig_bone = entity_bone_info->entity_to_rig_bone_mapping[i];
        if (bone_info == nullptr) {
            return;
        }
        const unsigned int flags = *(const unsigned int*)&bone_info[i].flags;
        if (bone_transform_list[rig_bone] < bone_lod_level) {
            if ((flags & 2) == 0) {
                const int parent = entity_bone_info->rig_to_entity_bone_mapping[rig->bones[rig_bone].parent_index];
                cache_vertex_to_model[i].m_translation = bones[rig_bone].inv_translation * cache_instanced_model[i];
                cache_vertex_to_model[i].m_rotation = cache_vertex_to_model[parent].m_rotation;
            }
        } else if ((flags & 2) == 0 && (flags & 1) != 0) {
            cache_vertex_to_model[i].m_translation = bones[rig_bone].inv_translation * cache_instanced_model[i];
            cache_vertex_to_model[i].m_rotation = cache_instanced_model[i].m_rotation;
        }
    }
}

// 0x00C8CE70. The time of a looping animation at a cyclic percent.
int anim_instance_manager::get_looping_animation_time(float cyclic_percentp, anim_instance_info* anim_info, anim_animation* anim) const
{
    if (anim_info->init_cyclic_percent != 0.0f) {
        cyclic_percentp = cyclic_percentp - anim_info->init_cyclic_percent;
        if (cyclic_percentp < 0.0f) {
            cyclic_percentp = cyclic_percentp + 1.0f;
        }
    }
    if (anim_info->flags & ANIM_FLAG_REVERSE) {
        cyclic_percentp = 1.0f - cyclic_percentp;
    }
    const int end = anim->animation_data != nullptr ? anim->animation_data->end_tock * 80 : 0;
    return (int)(float)floor_d((double)((float)end * cyclic_percentp));
}

// 0x00C8D280
void anim_instance_manager::process_bone_modifier(bone_modifier modifier, matrix43& mat, const matrix43& parent_mat, matrix43& local_matrix, bone_modifier_data* data)
{
    switch (modifier) {
    case BONE_MODIFIER_PRE_ROTATE:
        mat.multiply_two(local_matrix, parent_mat);
        mat.m_rotation = mat.m_rotation * data->desired_bone_orient;
        return;
    case BONE_MODIFIER_POST_ROTATE:
        mat.multiply_two(local_matrix, parent_mat);
        mat.m_rotation = data->desired_bone_orient * mat.m_rotation;
        return;
    case BONE_MODIFIER_ROTATE: {
        const quaternion desired(data->desired_bone_orient);
        const quaternion old(data->old_bone_orient);
        quaternion delta = desired * old.copy_inverse();
        if (data->forced_bone_blend < 0.995f) {
            quaternion blended;
            pltind_slerp(blended, IDENTITY_QUATERNION, delta, data->forced_bone_blend);
            delta = blended;
        }
        matrix m;
        delta.extract_matrix(m);
        local_matrix.m_rotation = m * local_matrix.m_rotation;
        mat = local_matrix * parent_mat;
        return;
    }
    case BONE_MODIFIER_COPY:
        mat = local_matrix;
        return;
    case BONE_MODIFIER_FORCE: {
        matrix forced;
        if (data->forced_bone_blend < 0.995f) {
            quaternion desired;
            quaternion old;
            desired.set(data->desired_bone_orient);
            old.set(data->old_bone_orient);
            quaternion blended;
            pltind_slerp(blended, old, desired, data->forced_bone_blend);
            blended.extract_matrix(forced);
        } else {
            forced = data->desired_bone_orient;
        }
        matrix local = forced * parent_mat.m_rotation.copy_inverse();
        if (0.001f < fabsf(local.rvec.mag_and_normalize() - 1.0f)) {
            local.uvec.normalize();
            local.fvec.normalize();
        }
        local_matrix.m_rotation = local;
        mat = local_matrix * parent_mat;
        return;
    }
    case BONE_MODIFIER_POSITION_OFFSET:
        mat = local_matrix;
        mat.m_translation.x = data->desired_bone_orient.fvec.x + mat.m_translation.x;
        mat.m_translation.y = data->desired_bone_orient.fvec.y + mat.m_translation.y;
        mat.m_translation.z = data->desired_bone_orient.fvec.z + mat.m_translation.z;
        return;
    case BONE_MODIFIER_ROTATE_RC:
        mat.m_rotation = local_matrix.m_rotation * data->desired_bone_orient;
        mat.m_translation = local_matrix.m_translation.copy_unrotate(data->desired_bone_orient);
        return;
    default:
        return;
    }
}

// 0x00C8D760. The animation's ramp in and out lengths in ticks (swapped when it plays in reverse).
void anim_instance_manager::get_ramp_in_out_times(anim_instance_info* anim_info, int& ramp_in, int& ramp_out) const
{
    ramp_in = 0;
    ramp_out = 0;
    const int anim_handle = anim_info->anim_handle;
    if (!anim_handle_valid(anim_handle)) {
        return;
    }
    const anim_file* amf = Animations[anim_handle].animation_data;
    if (amf == nullptr) {
        return;
    }
    // frames at 30 fps to ticks, in double
    if ((anim_info->flags & ANIM_FLAG_REVERSE) == 0) {
        ramp_in = (int)(((double)amf->ramp_in / 30.0) * 4800.0);
        ramp_out = (int)(((double)amf->ramp_out / 30.0) * 4800.0);
    } else {
        ramp_in = (int)(((double)amf->ramp_out / 30.0) * 4800.0);
        ramp_out = (int)(((double)amf->ramp_in / 30.0) * 4800.0);
    }
}

// 0x00C8D810
unsigned int compose_anim_instance_handle(int signature)
{
    return signature == -1 ? 0xFFFFFFFF : (unsigned int)signature << 4;
}

// 0x00C8D820
void decompose_anim_instance_handle(unsigned int handle, int& signature)
{
    signature = signature_of(handle);
}

// 0x00C8D840
int anim_instance_manager::find_active_anim(unsigned int anim_handle) const
{
    for (int i = 0; i < num_active_anims; i++) {
        if ((unsigned int)active_anims[i].signature == signature_of(anim_handle)) {
            return i;
        }
    }
    return -1;
}

// 0x00C8D880
const anim_instance_info* anim_instance_manager::get_active_anim(unsigned int handlep) const
{
    if (handlep == 0xFFFFFFFF) {
        return nullptr;
    }
    const int index = find_active_anim(handlep);
    return index < 0 ? nullptr : &active_anims[index];
}

// 0x00C8D8E0
anim_instance_info* anim_instance_manager::get_active_anim(unsigned int handlep)
{
    if (handlep == 0xFFFFFFFF) {
        return nullptr;
    }
    const int index = find_active_anim(handlep);
    return index < 0 ? nullptr : &active_anims[index];
}

// 0x00C8D930. A slot for a new animation: the active animations stay sorted by priority (the new one goes after the
// last one of its priority or lower). With all 10 slots taken, the slot with the lowest base mix below 100 is reused
// (the last slot unless an earlier one is lower).
anim_instance_info* anim_instance_manager::add_active_anim(int anim_handle, int priority)
{
    if (locked != 0 || !anim_handle_valid(anim_handle)) {
        return nullptr;
    }
    if (Animations[anim_handle].animation_data == nullptr) {
        // "\nAnim '%s' not paged in!" is formatted into a thread-local buffer and not shown.
        // The game trace (game_trace.h, not in rfg.exe): every 2 s per animation at most, a state retries every frame.
        if (Game_trace_file != nullptr) {
            static long last_trace[3875];
            const long now = (long)clock();
            if (last_trace[anim_handle] == 0 || now - last_trace[anim_handle] >= 2000) {
                last_trace[anim_handle] = now;
                game_trace_write("anim", "refused, not paged in: %s", Animations[anim_handle].anim_filename);
            }
        }
        return nullptr;
    }
    int slot;
    if (num_active_anims < ANIM_MAX_ACTIVE) {
        slot = 0;
        for (int i = num_active_anims - 1; i > -1; i--) {
            if ((int)active_anims[i].priority <= priority) {
                slot = i + 1;
                break;
            }
            active_anims[i + 1] = active_anims[i];
        }
        num_active_anims++;
    } else {
        slot = -1;
        float lowest = 100.0f;
        for (int i = 0; i < 9; i++) {
            if (active_anims[i].base_mix_percent < lowest) {
                slot = i;
                lowest = active_anims[i].base_mix_percent;
            }
        }
        if (lowest > active_anims[9].base_mix_percent) {
            slot = 9;
        } else if (slot == -1) {
            return nullptr;
        }
    }
    anim_instance_info* info = &active_anims[slot];
    info->flags = 0;
    info->base_mix_percent = 0.0f;
    info->mix_percent = 0.0f;
    info->cur_time = 0;
    info->prev_time = 0;
    info->remainder_time = 0.0f;
    return info;
}

// 0x00C8DB60
int anim_instance_manager::get_parent_bone_index(int entity_bone_index) const
{
    if (entity_bone_index > -1 && entity_bone_index < entity_bone_info->num_bones) {
        if (entity_bone_index < rig->num_common_bones) {
            return rig->bones[entity_bone_index].parent_index;
        }
        return entity_bone_info->rig_to_entity_bone_mapping[rig->bones[entity_bone_info->entity_to_rig_bone_mapping[entity_bone_index]].parent_index];
    }
    keen::debug::nativePrintArgs("invalid entity bone index");
    return -1;
}

// 0x00C8DBC0
void anim_instance_manager::get_animation_times(unsigned int handlep, int& prev_time, int& cur_time)
{
    const int index = find_active_anim(handlep);
    if (index > -1) {
        cur_time = active_anims[index].cur_time;
        prev_time = active_anims[index].prev_time;
    }
}

// 0x00C8DC70 (not reached).
unsigned int anim_instance_manager::get_anim_flags(unsigned int handlep) const
{
    const anim_instance_info* info = get_active_anim(handlep);
    return info != nullptr ? info->flags : 0;
}

// 0x00C8DC90
void anim_instance_manager::set_cyclic_percent(float _cyclic_percent)
{
    cyclic_percent = _cyclic_percent;
}

// 0x00C8DCD0. The destination takes the source's times, mix and cyclic start: it runs at its own cyclic percent as far
// behind it as the source runs behind the source's cyclic percent.
void anim_instance_manager::sync_anims(anim_instance_manager* aim_dst, unsigned int src_handle, unsigned int dst_handle)
{
    const int src = find_active_anim(src_handle);
    const int dst = aim_dst->find_active_anim(dst_handle);
    if (src == -1 || dst == -1) {
        return;
    }
    anim_instance_info& to = aim_dst->active_anims[dst];
    const anim_instance_info& from = active_anims[src];
    to.prev_time = from.prev_time;
    to.cur_time = from.cur_time;
    to.mix_percent = from.mix_percent;
    float behind = cyclic_percent - from.init_cyclic_percent;
    if (behind < 0.0f) {
        behind = behind + 1.0f;
    }
    const float start = cyclic_percent - behind;
    to.init_cyclic_percent = start;
    if (start < 0.0f) {
        to.init_cyclic_percent = start + 1.0f;
    }
}

// 0x00C8DE00. A looping animation already on the instance is reused (with a new signature); the time follows the
// shared cyclic percent unless flags & 0x10000 (time advanced by the animation's own speed). flags & 0x80 starts the
// animation at the current cyclic percent.
unsigned int anim_instance_manager::play_animation_looping(int handlep, unsigned int flags, float base_mix_percent, float ramp_in_sec, float speed_multiplier)
{
    if (!anim_handle_valid(handlep)) {
        return 0xFFFFFFFF;
    }
    anim_animation* anim = &Animations[handlep];
    anim_instance_info* info = nullptr;
    for (int i = 0; i < num_active_anims; i++) {
        if (active_anims[i].anim_handle == handlep) {
            info = &active_anims[i];
        }
    }
    anim_priority priority = AP_PRIORITY_NONE;
    if (flags & 0x70) {
        if (flags & 0x10) {
            priority = AP_PRIORITY_LOW;
        } else if (flags & 0x20) {
            priority = AP_PRIORITY_MEDIUM;
        } else if (flags & 0x40) {
            priority = AP_PRIORITY_HIGH;
        }
    }
    if (info == nullptr) {
        info = add_active_anim(handlep, priority);
        if (info == nullptr) {
            return 0xFFFFFFFF;
        }
    }
    const int signature = get_next_animation_signature();
    info->signature = signature;
    if (info->anim_handle != handlep) {
        info->anim_handle = handlep;
        info->speed_multiplier = speed_multiplier;
        info->init_cyclic_percent = 0.0f;
        if ((signed char)flags < 0) {
            info->init_cyclic_percent = cyclic_percent;
        }
    }
    info->priority = priority;
    info->base_mix_percent = base_mix_percent;
    info->flags = flags | ANIM_FLAG_LOOPING;
    info->ramp.start_time = 0;
    info->ramp.cur_time = 0;
    info->ramp.state = ARS_RAMP_IN;
    if (0.0f <= ramp_in_sec) {
        info->ramp.duration = (int)((double)ramp_in_sec * 4800.0);
    } else {
        int ramp_in;
        int ramp_out;
        get_ramp_in_out_times(info, ramp_in, ramp_out);
        info->ramp.duration = ramp_in;
    }
    if ((flags & ANIM_FLAG_MANUAL_TIME) == 0) {
        info->cur_time = get_looping_animation_time(cyclic_percent, info, anim);
    }
    info->prev_time = info->cur_time;
    if (info->ramp.duration == 0) {
        info->ramp.state = ARS_RAMP_NONE;
    }
    if (signature != -1) {
        return (unsigned int)signature << 4;
    }
    return 0xFFFFFFFF;
}

// The elapsed / just elapsed test of trigger_has_elapsed and trigger_just_elapsed over one or all animations.
static bool trigger_bit_test(const anim_instance_manager* aim, int trigger_id, unsigned int handlep, int trigger_index, int bit)
{
    int first = 0;
    int end = aim->num_active_anims;
    if (handlep != 0xFFFFFFFF) {
        const int index = aim->find_active_anim(handlep);
        if (index == -1) {
            return false;
        }
        first = index;
        end = index + 1;
    }
    for (int i = first; i < end; i++) {
        const anim_instance_info& info = aim->active_anims[i];
        if (!anim_handle_valid(info.anim_handle)) {
            continue;
        }
        const anim_animation& anim = Animations[info.anim_handle];
        int found = -1;
        for (int t = 0; t < anim.num_triggers; t++) {
            if (anim.triggers[t].id == trigger_id) {
                found++;
                if ((trigger_index == -1 || trigger_index == found) && (info.trigger_data & (1u << ((t * 3 + bit) & 0x1F))) != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// 0x00C8DFB0. Only sets bits (see the trigger bits above); a false argument does not clear one.
void anim_instance_manager::set_trigger_flags(unsigned int instance_handle, int trigger_num, bool just_triggered, bool triggered, bool supress_trigger)
{
    const int index = find_active_anim(instance_handle);
    if (index == -1) {
        return;
    }
    const int bit = trigger_num * 3;
    active_anims[index].trigger_data |= ((unsigned int)triggered << ((bit + 1) & 0x1F)) | ((unsigned int)supress_trigger << ((bit + 2) & 0x1F)) |
                                        ((unsigned int)just_triggered << (bit & 0x1F));
}

// 0x00C8E030
bool anim_instance_manager::trigger_has_elapsed(int trigger_id, unsigned int handlep, int trigger_index) const
{
    return trigger_bit_test(this, trigger_id, handlep, trigger_index, 1);
}

// 0x00C8E150
bool anim_instance_manager::trigger_just_elapsed(int trigger_id, unsigned int handlep, int trigger_index) const
{
    return trigger_bit_test(this, trigger_id, handlep, trigger_index, 0);
}

// 0x00C8E290
bool anim_instance_manager::has_trigger(int trigger_id, unsigned int handlep) const
{
    const anim_instance_info* info = get_active_anim(handlep);
    if (info == nullptr || !anim_handle_valid(info->anim_handle)) {
        return false;
    }
    const anim_animation& anim = Animations[info->anim_handle];
    for (int t = 0; t < anim.num_triggers; t++) {
        if (anim.triggers[t].id == trigger_id) {
            return true;
        }
    }
    return false;
}

// 0x00C8E2F0. Seconds until the last trigger of the id at the animation's speed (negative once past it); a reversed
// animation counts the other way.
float anim_instance_manager::time_till_trigger(int trigger_id, unsigned int handlep) const
{
    const anim_instance_info* info = get_active_anim(handlep);
    if (info == nullptr) {
        return 0.0f;
    }
    int ticks = 0;
    if (anim_handle_valid(info->anim_handle)) {
        const anim_animation& anim = Animations[info->anim_handle];
        for (int t = 0; t < anim.num_triggers; t++) {
            if (anim.triggers[t].id == trigger_id) {
                ticks = anim.triggers[t].ticktime - info->cur_time;
            }
        }
    }
    if ((info->flags & ANIM_FLAG_REVERSE) != 0) {
        ticks = -ticks;
    }
    return (float)((double)ticks / (double)info->speed_multiplier / 4800.0); // x87 chain, one rounding
}

// 0x00C8E450. start_frame is a 30 fps frame.
void anim_instance_manager::set_start_frame(unsigned int handlep, int start_frame)
{
    const int index = find_active_anim(handlep);
    if (index > -1) {
        active_anims[index].cur_time = (int)(((double)(float)start_frame / 30.0) * 4800.0);
    }
}

// 0x00C8E4C0. The cyclic percent the animation starts at; its time is set from it.
void anim_instance_manager::set_looping_anim_start_percent(unsigned int handlep, float init_cyclic_percent)
{
    const int index = find_active_anim(handlep);
    if (index < 0) {
        return;
    }
    anim_instance_info& info = active_anims[index];
    const float percent = cyclic_percent - init_cyclic_percent;
    info.init_cyclic_percent = percent;
    if (percent < 0.0f) {
        info.init_cyclic_percent = percent + 1.0f;
    }
    anim_animation* anim = (info.anim_handle < 0 || info.anim_handle >= Num_animations) ? nullptr : &Animations[info.anim_handle];
    const int time = get_looping_animation_time(cyclic_percent, &info, anim);
    info.cur_time = time;
    info.prev_time = time;
}

// 0x00C8E5F0
void anim_instance_manager::set_init_cyclic_percent(unsigned int handlep, float init_cyclic_percent)
{
    const int index = find_active_anim(handlep);
    if (index < 0) {
        return;
    }
    anim_instance_info& info = active_anims[index];
    info.init_cyclic_percent = init_cyclic_percent;
    anim_animation* anim = (info.anim_handle < 0 || info.anim_handle >= Num_animations) ? nullptr : &Animations[info.anim_handle];
    const int time = get_looping_animation_time(cyclic_percent, &info, anim);
    info.cur_time = time;
    info.prev_time = time;
}

// 0x00C8E690
void anim_instance_manager::clear_bone_info()
{
    if (bone_info != nullptr) {
        memset(bone_info, 0, entity_bone_info->num_bones * sizeof(anim_bone_info));
    }
}

// 0x00C8E6C0 (not reached).
void anim_instance_manager::clear_morph_info()
{
}

// 0x00C8E6E0
int anim_instance_manager::get_morph_info(anim_morph_info** morph_info_ptr)
{
    if (num_active_morphs == 0) {
        *morph_info_ptr = nullptr;
        return 0;
    }
    *morph_info_ptr = morph_info;
    return num_active_morphs;
}

// 0x00C8E710. A bone page (vertex-to-model matrices, model matrices, bone infos; 16-byte aligned) from the small pool
// for up to 40 bones, else from the large one; renewed (age 2) when the instance already has one.
bool anim_instance_manager::allocate_bone_matrices()
{
    if (cache_vertex_to_model != nullptr) {
        page_pool_manager->age = 2;
        return true;
    }
    const int num_bones = entity_bone_info->num_bones;
    const unsigned int size = num_bones * (2 * sizeof(matrix43) + sizeof(anim_bone_info)) + 0x10;
    page_mempool& pool = num_bones < 41 ? Bone_pool_small : Bone_pool_large;
    animlib_page_pool_manager*& list = num_bones < 41 ? Small_page_pool_manager_list : Large_page_pool_manager_list;
    if (!pool.can_alloc(size, 0)) {
        return false;
    }
    animlib_page_pool_manager* page = Unused_page_pool_manager_list;
    void* buf = pool.alloc(size, 0);
    page->buf = buf;
    if (buf == nullptr) {
        return false;
    }
    const unsigned int misalign = (unsigned int)buf & 0xF;
    matrix43* matrices = (matrix43*)((char*)buf + (misalign == 0 ? 0 : 0x10 - misalign));
    cache_instanced_model = matrices + num_bones;
    cache_vertex_to_model = matrices;
    bone_info = (anim_bone_info*)(matrices + num_bones * 2);
    clear_bone_info();
    cache_instanced_model_valid = false;
    page->aim = this;
    page->age = 2;
    animlib_list_remove(Unused_page_pool_manager_list, page);
    animlib_list_append(list, page);
    page_pool_manager = page;
    clear_bone_info();
    return true;
}

// 0x00C8E940. 20 morph infos from the small pool, cleared every call.
void anim_instance_manager::allocate_morph_info()
{
    if (morph_info == nullptr) {
        if (!Bone_pool_small.can_alloc(0xF0, 0)) {
            return;
        }
        anim_morph_info* infos = (anim_morph_info*)Bone_pool_small.alloc(0xF0, 0);
        animlib_page_pool_manager* page = Unused_page_pool_manager_list;
        page->buf = infos;
        morph_info = infos;
        page->aim = this;
        page->age = 2;
        animlib_list_remove(Unused_page_pool_manager_list, page);
        animlib_list_append(Morph_page_pool_manager_list, page);
        morph_anim_pool_manager = page;
    } else {
        morph_anim_pool_manager->age = 2;
    }
    num_active_morphs = 0;
    memset(morph_info, 0, 0xF0);
}

// 0x00C8EA30
void anim_instance_manager::add_morph_special_func(void (*func)(unsigned char&, anim_morph_info*, void*), void* data)
{
    morph_func = func;
    morph_data = data;
}

// 0x00C8EA50
void anim_instance_manager::disable_instance_bone_special_func(int bone_index)
{
    bone_info[bone_index].flags.use_special_func = 0;
}

// 0x00C8EA70. The bind pose position of the bone in the model: its relative translations summed up the parent chain.
vector anim_instance_manager::get_base_pose_bone_pos(int entity_bone_index)
{
    if (entity_bone_index < 0 || entity_bone_index >= entity_bone_info->num_bones) {
        return ZERO_VECTOR;
    }
    const anim_bone* bones = rig->bones;
    const anim_bone* bone = &bones[entity_bone_info->entity_to_rig_bone_mapping[entity_bone_index]];
    vector pos = bone->rel_bone_translation;
    while (bone->parent_index != -1) {
        bone = &bones[bone->parent_index];
        pos.x = bone->rel_bone_translation.x + pos.x;
        pos.y = bone->rel_bone_translation.y + pos.y;
        pos.z = bone->rel_bone_translation.z + pos.z;
    }
    return pos;
}

// 0x00C8EC00
matrix43* anim_instance_manager::get_render_matrices()
{
    return cache_vertex_to_model;
}

// 0x00C8EC10. The common bones as they are, the others where the entity class uses them.
void anim_instance_manager::copy_render_matrices(matrix43* dest)
{
    memcpy(dest, cache_vertex_to_model, entity_bone_info->num_common_bones * sizeof(matrix43));
    const matrix43* src = cache_vertex_to_model + rig->num_common_bones;
    for (int i = entity_bone_info->num_common_bones; i < entity_bone_info->num_bones; i++, src++) {
        if (entity_bone_info->rig_to_entity_bone_mapping[i] > -1) {
            dest[i] = *src;
        }
    }
}

// 0x00C93230. The model space bone matrices down the hierarchy (parents come before their children in the rig).
// Root: the local matrix (scaled with do_bone_scaling, through its bone modifier) or the parent rig's bone. Other
// bones: local times parent (or the bone modifier), the parent rig's bone, or below the LOD level the parent's
// rotation at the bind pose offset. Also the bounding box of the bone positions.
void anim_instance_manager::instance_bone_matrices(matrix43* bone_matrix, int* bone_transform_list, int bone_lod_level)
{
    if (bone_transform_list == nullptr) {
        bone_transform_list = Default_bone_list;
    }
    matrix43* parent_instanced = nullptr;
    if (parent_rig_aim_handle != -1) {
        anim_instance_manager* parent = aim_from_handle(parent_rig_aim_handle);
        parent_instanced = parent->cache_instanced_model;
        if (!parent->cache_instanced_model_valid) {
            return;
        }
    }
    const int num_bones = entity_bone_info->num_bones;
    if (rig->bones[0].parent_index != -1) {
        return;
    }
    if (parent_rig_bone_indecies[0] == -1) {
        if (bone_transform_list[0] < bone_lod_level || bone_matrix == nullptr || cache_instanced_model == nullptr) {
            return;
        }
        matrix43& root = cache_instanced_model[0];
        root = bone_matrix[0];
        if (do_bone_scaling) {
            root.m_rotation.fvec.x = root.m_rotation.fvec.x * bone_scaling.z;
            root.m_rotation.fvec.y = root.m_rotation.fvec.y * bone_scaling.z;
            root.m_rotation.fvec.z = root.m_rotation.fvec.z * bone_scaling.z;
            root.m_rotation.rvec.x = root.m_rotation.rvec.x * bone_scaling.x;
            root.m_rotation.rvec.y = bone_scaling.x * root.m_rotation.rvec.y;
            root.m_rotation.rvec.z = root.m_rotation.rvec.z * bone_scaling.x;
            root.m_rotation.uvec.x = root.m_rotation.uvec.x * bone_scaling.y;
            root.m_rotation.uvec.y = root.m_rotation.uvec.y * bone_scaling.y;
            root.m_rotation.uvec.z = bone_scaling.y * root.m_rotation.uvec.z;
            root.m_translation.x = bone_scaling.x * root.m_translation.x;
            root.m_translation.y = root.m_translation.y * bone_scaling.y;
            root.m_translation.z = bone_scaling.z * root.m_translation.z;
        }
        if (bone_info[0].flags.use_special_func) {
            process_bone_modifier(bone_info[0].modifier, cache_instanced_model[0], IDENTITY_MATRIX43, bone_matrix[0], &bone_modifiers->bones[bone_info[0].data_idx]);
        }
    } else {
        cache_instanced_model[0] = parent_instanced[parent_rig_bone_indecies[0]];
    }
    bone_info[0].flags.instanced_model_matrix_valid = 1;
    bone_bbox_min = cache_instanced_model[0].m_translation;
    bone_bbox_max = cache_instanced_model[0].m_translation;
    for (int i = 1; i < num_bones; i++) {
        const int rig_bone = entity_bone_info->entity_to_rig_bone_mapping[i];
        const int rig_parent = rig->bones[rig_bone].parent_index;
        if (rig_parent < 0) {
            return;
        }
        if (!bone_info[i].flags.instanced_model_matrix_valid) {
            if (parent_rig_bone_indecies[i] == -1) {
                const int parent = entity_bone_info->rig_to_entity_bone_mapping[rig_parent];
                if (bone_transform_list[rig_bone] < bone_lod_level) {
                    // below the LOD level: the parent's rotation, the bind pose offset (the matrix stays not valid)
                    cache_instanced_model[i].m_rotation = cache_instanced_model[parent].m_rotation;
                    const anim_bone* bones = rig->bones;
                    const vector offset(bones[rig_parent].inv_translation.x - bones[rig_bone].inv_translation.x,
                                        bones[rig_parent].inv_translation.y - bones[rig_bone].inv_translation.y,
                                        bones[rig_parent].inv_translation.z - bones[rig_bone].inv_translation.z);
                    cache_instanced_model[i].m_translation = offset * cache_instanced_model[parent];
                } else {
                    bone_info[i].flags.instanced_model_matrix_valid = 1;
                    if (bone_info[i].flags.use_special_func) {
                        process_bone_modifier(bone_info[i].modifier, cache_instanced_model[i], cache_instanced_model[parent], bone_matrix[i], &bone_modifiers->bones[bone_info[i].data_idx]);
                    } else {
                        cache_instanced_model[i].multiply_two(bone_matrix[i], cache_instanced_model[parent]);
                    }
                }
            } else {
                bone_info[i].flags.instanced_model_matrix_valid = 1;
                cache_instanced_model[i] = parent_instanced[parent_rig_bone_indecies[i]];
            }
        }
        const vector& p = cache_instanced_model[i].m_translation;
        if (bone_bbox_min.x - p.x >= 0.0f) {
            bone_bbox_min.x = p.x;
        }
        if (bone_bbox_min.y - p.y >= 0.0f) {
            bone_bbox_min.y = p.y;
        }
        if (bone_bbox_min.z - p.z >= 0.0f) {
            bone_bbox_min.z = p.z;
        }
        if (!(bone_bbox_max.x - p.x >= 0.0f)) {
            bone_bbox_max.x = p.x;
        }
        if (!(bone_bbox_max.y - p.y >= 0.0f)) {
            bone_bbox_max.y = p.y;
        }
        if (!(bone_bbox_max.z - p.z >= 0.0f)) {
            bone_bbox_max.z = p.z;
        }
    }
    cache_instanced_model_valid = true;
}

// 0x00C93A30. Ticks to the end (to the start when playing in reverse); 0 when past it.
int anim_instance_manager::get_animation_time_to_end(const anim_instance_info* anim_info) const
{
    const int anim_handle = anim_info->anim_handle;
    if (!anim_handle_valid(anim_handle)) {
        return 0;
    }
    int left;
    if ((anim_info->flags & ANIM_FLAG_REVERSE) == 0) {
        left = anim_end_ticks(anim_handle) - anim_info->cur_time;
    } else {
        left = anim_info->cur_time;
    }
    return left > -1 ? left : 0;
}

// Removes slot `index` (the slots after it move down) and clears the freed slots, as the original's inlined
// remove_active_anim does. index -1 (handle not found) reads slot -1, as in the original.
static void remove_active_anim_at(anim_instance_manager* aim, int index)
{
    if (aim->animation_to_freeze_sig == aim->active_anims[index].signature) {
        aim->animation_to_freeze_sig = -1;
    }
    aim->num_active_anims--;
    for (; index < aim->num_active_anims; index++) {
        aim->active_anims[index] = aim->active_anims[index + 1];
    }
    for (; index < ANIM_MAX_ACTIVE; index++) {
        aim->active_anims[index].signature = -1;
        aim->active_anims[index].base_mix_percent = 0.0f;
        aim->active_anims[index].mix_percent = 0.0f;
        aim->active_anims[index].flags = 0;
    }
}

// 0x00C93A80
void anim_instance_manager::remove_active_anim(unsigned int instance_handle)
{
    remove_active_anim_at(this, find_active_anim(instance_handle));
}

// get_animation_time_since_start 0x00C93BE0: not reached.

// 0x00C93C30. Seconds to the end; 0 when the handle is not playing.
float anim_instance_manager::time_till_animation_finished(unsigned int handlep) const
{
    const anim_instance_info* info = get_active_anim(handlep);
    if (info == nullptr || !anim_handle_valid(info->anim_handle) || Animations[info->anim_handle].animation_data == nullptr) {
        return 0.0f;
    }
    int left;
    if ((info->flags & ANIM_FLAG_REVERSE) == 0) {
        left = anim_end_ticks(info->anim_handle) - info->cur_time;
    } else {
        left = info->cur_time;
    }
    if (left < 0) {
        left = 0;
    }
    return (float)left / 4800.0f;
}

// 0x00C93CB0. Ticks since the start (from the end when playing in reverse).
int anim_instance_manager::get_current_ticks(unsigned int handlep) const
{
    const anim_instance_info* info = get_active_anim(handlep);
    if (info == nullptr || !anim_handle_valid(info->anim_handle)) {
        return 0;
    }
    int ticks;
    if ((info->flags & ANIM_FLAG_REVERSE) == 0) {
        ticks = info->cur_time;
    } else {
        ticks = anim_end_ticks(info->anim_handle) - info->cur_time;
    }
    return ticks < 0 ? 0 : ticks;
}

// 0x00C93D10
float anim_instance_manager::get_current_time(unsigned int handlep) const
{
    return (float)get_current_ticks(handlep) / 4800.0f;
}

// 0x00C93D40
int anim_instance_manager::get_current_frame(unsigned int handlep) const
{
    return (int)((double)((float)get_current_ticks(handlep) / 4800.0f) * 30.0);
}

// 0x00C93D70
float anim_instance_manager::anim_length(int handlep) const
{
    if (!anim_handle_valid(handlep)) {
        return 0.0f;
    }
    return (float)anim_end_ticks(handlep) / 4800.0f;
}

// 0x00C93DD0. Looping animations go at once or are faded out by zeroing their mix (they are removed later).
void anim_instance_manager::clear_looping_animations(bool immediate)
{
    for (int i = num_active_anims - 1; i > -1; i--) {
        anim_instance_info& info = active_anims[i];
        if (info.flags & ANIM_FLAG_LOOPING) {
            if (immediate) {
                remove_active_anim_at(this, find_active_anim(compose_anim_instance_handle(info.signature)));
            } else {
                info.base_mix_percent = 0.0f;
                info.mix_percent = 0.0f;
            }
        }
    }
}

// 0x00C93FA0
void anim_instance_manager::clear_non_looping_animations()
{
    animation_to_freeze_sig = -1;
    frozen = 0;
    for (int i = num_active_anims - 1; i > -1; i--) {
        if ((active_anims[i].flags & ANIM_FLAG_LOOPING) == 0) {
            remove_active_anim_at(this, find_active_anim(compose_anim_instance_handle(active_anims[i].signature)));
        }
    }
}

// 0x00C94160. Seconds into the animation (from its end in reverse), clamped to the animation.
void anim_instance_manager::set_current_time(anim_instance_info* anim_info, float time_sec)
{
    const int anim_handle = anim_info->anim_handle;
    if (!anim_handle_valid(anim_handle)) {
        return;
    }
    const int ticks = (int)((double)time_sec * 4800.0);
    if ((anim_info->flags & ANIM_FLAG_REVERSE) == 0) {
        anim_info->cur_time = ticks;
    } else {
        anim_info->cur_time = anim_end_ticks(anim_handle) - ticks;
    }
    const int end = anim_end_ticks(anim_handle);
    if (anim_info->cur_time < 0) {
        anim_info->cur_time = 0;
    } else if (end < anim_info->cur_time) {
        anim_info->cur_time = end;
    }
    anim_info->prev_time = anim_info->cur_time;
}

// 0x00C94210
void anim_instance_manager::set_current_time(unsigned int handlep, float time_sec)
{
    anim_instance_info* info = get_active_anim(handlep);
    if (info != nullptr) {
        set_current_time(info, time_sec);
    }
}

// 0x00C94270. A non-looping animation of the same handle is restarted in its slot unless flags & 0x200. flags & 4
// freezes the instance when the animation ends (no other animation can start meanwhile).
unsigned int anim_instance_manager::play_animation(int handlep, unsigned int flags, float start_time_sec, float base_mix_percent, float ramp_in_sec, float speed_multiplier)
{
    if (!anim_handle_valid(handlep)) {
        return 0xFFFFFFFF;
    }
    if (animation_to_freeze_sig != -1) {
        return 0xFFFFFFFF;
    }
    anim_priority priority = AP_PRIORITY_NONE;
    if (flags & 0x70) {
        if (flags & 0x10) {
            priority = AP_PRIORITY_LOW;
        } else if (flags & 0x20) {
            priority = AP_PRIORITY_MEDIUM;
        } else if (flags & 0x40) {
            priority = AP_PRIORITY_HIGH;
        }
    }
    anim_instance_info* info = nullptr;
    if ((flags & ANIM_FLAG_NEW_INSTANCE) == 0 && num_active_anims > 0) {
        for (int i = 0; i < num_active_anims; i++) {
            if (active_anims[i].anim_handle == handlep && (active_anims[i].flags & ANIM_FLAG_LOOPING) == 0) {
                info = &active_anims[i];
            }
        }
        if (info != nullptr) {
            info->flags = 0;
        }
    }
    if (info == nullptr) {
        info = add_active_anim(handlep, priority);
    }
    if (info == nullptr) {
        return 0xFFFFFFFF;
    }
    const int signature = get_next_animation_signature();
    info->flags |= flags;
    info->base_mix_percent = base_mix_percent;
    info->speed_multiplier = speed_multiplier;
    info->signature = signature;
    info->anim_handle = handlep;
    info->priority = priority;
    info->ramp.start_time = 0;
    info->ramp.cur_time = 0;
    info->ramp.state = ARS_RAMP_IN;
    if (0.0f <= ramp_in_sec) {
        info->ramp.duration = (int)((double)ramp_in_sec * 4800.0);
    } else {
        int ramp_in;
        int ramp_out;
        get_ramp_in_out_times(info, ramp_in, ramp_out);
        info->ramp.duration = ramp_in;
    }
    if (info->ramp.duration == 0) {
        info->ramp.state = ARS_RAMP_NONE;
    }
    set_current_time(info, start_time_sec);
    if (flags & ANIM_FLAG_FREEZE_AT_END) {
        animation_to_freeze_sig = info->signature;
    }
    info->trigger_data = 0;
    if (info->signature != -1) {
        return (unsigned int)info->signature << 4;
    }
    return 0xFFFFFFFF;
}

// 0x00C94430. The first trigger of the id, by the animation's time alone (not the trigger bits).
bool anim_instance_manager::trigger_should_have_elapsed(unsigned int instance_handle, int trigger_id)
{
    if (!has_trigger(trigger_id, instance_handle)) {
        return false;
    }
    const anim_instance_info& info = active_anims[find_active_anim(instance_handle)];
    if (!anim_handle_valid(info.anim_handle)) {
        return false;
    }
    const anim_animation& anim = Animations[info.anim_handle];
    for (int t = 0; t < anim.num_triggers; t++) {
        if (anim.triggers[t].id == trigger_id) {
            const int ticktime = anim.triggers[t].ticktime;
            if (ticktime < 0) {
                return false;
            }
            if ((info.flags & ANIM_FLAG_REVERSE) != 0) {
                return info.cur_time <= ticktime;
            }
            return ticktime <= info.cur_time;
        }
    }
    return false;
}

// 0x00C944F0. Without a blend time the animation's mix drops to 0 at once; else a ramp out (from the current ramp in
// level, or shortened when already ramping out more slowly).
void anim_instance_manager::stop_animation_internal(anim_instance_info* cur_anim, float blend_out_time_sec)
{
    if (blend_out_time_sec <= 0.0f) {
        cur_anim->base_mix_percent = 0.0f;
        cur_anim->mix_percent = 0.0f;
        return;
    }
    const int blend_ticks = (int)((double)blend_out_time_sec * 4800.0);
    const anim_ramp_state state = cur_anim->ramp.state;
    if (state == ARS_RAMP_IN) {
        const float ramped_in = (float)((double)(float)(cur_anim->ramp.cur_time - cur_anim->ramp.start_time) / (double)(float)cur_anim->ramp.duration);
        const float level = (float)(1.0 - (double)ramped_in);
        int duration = get_animation_time_to_end(cur_anim);
        if (blend_ticks <= duration) {
            duration = blend_ticks;
        }
        cur_anim->ramp.cur_time = (int)((double)(float)blend_ticks * (double)level);
        cur_anim->ramp.duration = duration;
        cur_anim->ramp.start_time = 0;
        cur_anim->ramp.state = ARS_RAMP_OUT;
    } else if (state == ARS_RAMP_NONE) {
        int duration = get_animation_time_to_end(cur_anim);
        if (blend_ticks <= duration) {
            duration = blend_ticks;
        }
        cur_anim->ramp.cur_time = 0;
        cur_anim->ramp.start_time = 0;
        cur_anim->ramp.duration = duration;
        cur_anim->ramp.state = ARS_RAMP_OUT;
    } else if (state == ARS_RAMP_OUT && blend_ticks < cur_anim->ramp.duration) {
        const int old_duration = cur_anim->ramp.duration;
        const int old_start = cur_anim->ramp.start_time;
        cur_anim->ramp.start_time = 0;
        cur_anim->ramp.duration = blend_ticks;
        const float ramped_out = (float)((double)(float)(cur_anim->ramp.cur_time - old_start) / (double)(float)old_duration);
        cur_anim->ramp.cur_time = (int)((double)(float)blend_ticks * (double)ramped_out);
        cur_anim->ramp.state = ARS_RAMP_OUT;
    }
}

// 0x00C94630. Plays new_anim_handle in place of a playing animation (same time, flags, mix, triggers matched by id)
// and fades the old one out over 0.15 s while the new one ramps in over 720 ticks.
unsigned int anim_instance_manager::swap_animation(unsigned int handlep, int new_anim_handle)
{
    const int old_index = find_active_anim(handlep);
    if (old_index == -1) {
        return 0xFFFFFFFF;
    }
    const int old_handle = active_anims[old_index].anim_handle;
    if (!anim_handle_valid(old_handle) || active_anims[old_index].ramp.state == ARS_RAMP_OUT) {
        return 0xFFFFFFFF;
    }
    const unsigned int new_instance = play_animation(new_anim_handle, active_anims[old_index].flags, 0.0f, 1.0f, 0.0f, 1.0f);
    const int new_index = find_active_anim(new_instance);
    anim_instance_info& old_info = active_anims[old_index];
    anim_instance_info& new_info = active_anims[new_index];
    const anim_animation* new_anim = (new_info.anim_handle < 0 || new_info.anim_handle >= Num_animations) ? nullptr : &Animations[new_info.anim_handle];
    new_info.cur_time = old_info.cur_time;
    new_info.prev_time = old_info.prev_time;
    new_info.flags = old_info.flags;
    new_info.priority = old_info.priority;
    new_info.speed_multiplier = old_info.speed_multiplier;
    new_info.init_cyclic_percent = old_info.init_cyclic_percent;
    new_info.base_mix_percent = old_info.base_mix_percent;
    new_info.mix_percent = old_info.mix_percent;
    new_info.trigger_data = 0;
    new_info.remainder_time = old_info.remainder_time;
    if (new_anim != nullptr) {
        const anim_animation& old_anim = Animations[old_handle];
        for (int t = 0; t < old_anim.num_triggers; t++) {
            for (int n = 0; n < new_anim->num_triggers; n++) {
                if (old_anim.triggers[t].id == new_anim->triggers[n].id) {
                    new_info.trigger_data |= ((old_info.trigger_data & (7u << ((t * 3) & 0x1F))) >> ((t * 3) & 0x1F)) << ((n * 3) & 0x1F);
                    break;
                }
            }
        }
    }
    new_info.ramp = old_info.ramp;
    new_info.ramp.start_time = new_info.cur_time;
    new_info.ramp.state = ARS_RAMP_IN;
    new_info.ramp.duration = 720;
    stop_animation_internal(&old_info, 0.15f);
    return new_instance;
}

// The weight blending state of one entity bone in get_local_bone_matrices.
struct local_bone_blend
{
    float weight;
    unsigned int heading_only; // use_special_blend_mode: blend the heading only
    quaternion rotation;
};

// A time snapped to a 30 fps frame (flags & 0x8000), in double as the binary does.
static int snap_to_frame(int time)
{
    const int frame = (int)((double)(float)time / 4800.0 * 30.0);
    return (int)((double)(float)frame / 30.0 * 4800.0);
}

// 0x00C964D0. The local (parent relative) bone matrices of the playing animations:
// 1. per animation a reader at its first bone; animations not paged in get mix 0;
// 2. per entity bone (at the LOD level and not driven by the parent rig) the weights, by priority from the highest:
//    each level's bone weights times mixes, normalized when they add up to more than what is left, additive
//    animations taking what is left as is; a level that fills the bone zeroes the lower levels;
// 3. the normal animations blended in order (rotation slerped, or only the heading for heading-only bones);
// 4. the additive animations on top (their change from the start, or from the identity when looping);
// 5. the rotation matrices, plus each bone's translation relative to its parent.
void anim_instance_manager::get_local_bone_matrices(matrix43* bone_matrix, int* rig_bone_transform_list, int bone_lod_level)
{
    animlib_compressed_data_reader readers[ANIM_MAX_ACTIVE];
    local_bone_blend blend[64];
    float weights[ANIM_MAX_ACTIVE][64];
    anim_animation* anims[ANIM_MAX_ACTIVE];
    if (rig_bone_transform_list == nullptr) {
        rig_bone_transform_list = Default_bone_list;
    }
    if (cache_vertex_to_model == nullptr || cache_instanced_model == nullptr || bone_info == nullptr || entity_bone_info == nullptr) {
        return;
    }
    const int num_bones = entity_bone_info->num_bones;
    for (int b = 0; b < num_bones; b++) {
        blend[b].weight = 0.0f;
    }
    int count[AP_NUM_PRIORITIES] = {0, 0, 0, 0};
    for (int i = 0; i < num_active_anims; i++) {
        anim_instance_info& info = active_anims[i];
        count[info.priority]++;
        anims[i] = get_animation_from_handle(info.anim_handle);
        if (anims[i] == nullptr || anims[i]->animation_data == nullptr) {
            info.mix_percent = 0.0f;
        }
        if (0.0f < info.mix_percent) {
            anim_get_bone_data(anims[i]->animation_data, readers[i]);
        }
    }
    int start[AP_NUM_PRIORITIES];
    start[0] = 0;
    start[1] = count[0];
    start[2] = count[0] + count[1];
    start[3] = count[2] + count[0] + count[1];

    // 2. weights
    for (int b = 0; b < num_bones; b++) {
        // the LOD test indexes the transform list with the entity bone here (the rig bone elsewhere), as the original
        if (bone_lod_level > rig_bone_transform_list[b]) {
            continue;
        }
        const int rig_bone = entity_bone_info->entity_to_rig_bone_mapping[b];
        if (parent_rig_bone_indecies[rig_bone] != -1) {
            continue;
        }
        blend[b].heading_only = entity_bone_info->use_special_blend_mode[b];
        float total = 0.0f;
        for (int p = AP_NUM_PRIORITIES - 1; p > -1; p--) {
            if (count[p] <= 0) {
                continue;
            }
            const int first = start[p];
            const int last = first + count[p];
            float sum = 0.0f;
            for (int i = first; i < last; i++) {
                const float mix = active_anims[i].mix_percent;
                if (0.0f < mix) {
                    const float w = anims[i]->find_animation_bone_weight(rig_bone, active_anims[i].cur_time, readers[i]) * mix;
                    weights[i][b] = w;
                    if ((active_anims[i].flags & ANIM_FLAG_ADDITIVE) == 0) {
                        sum = w + sum;
                    }
                } else {
                    weights[i][b] = 0.0f;
                }
            }
            const float left = (float)(1.0 - (double)total);
            float scale = left;
            if (1.0f < sum) {
                scale = left / sum;
            }
            for (int i = first; i < last; i++) {
                if ((active_anims[i].flags & ANIM_FLAG_ADDITIVE) == 0) {
                    weights[i][b] = scale * weights[i][b];
                } else {
                    weights[i][b] = weights[i][b] * left;
                }
            }
            total = sum + total;
            if (1.0f <= total) {
                for (int i = 0; i < start[p]; i++) {
                    weights[i][b] = 0.0f;
                }
                break;
            }
        }
    }

    // 3. normal animations
    for (int i = 0; i < num_active_anims; i++) {
        const anim_instance_info& info = active_anims[i];
        if (info.flags & ANIM_FLAG_ADDITIVE) {
            continue;
        }
        int time = info.cur_time;
        if (info.flags & ANIM_FLAG_SNAP_TO_FRAMES) {
            time = snap_to_frame(time);
        }
        if (!(0.0f < info.mix_percent)) {
            continue;
        }
        anim_animation* anim = get_animation_from_handle(info.anim_handle);
        if (anim == nullptr) {
            return;
        }
        anim_file* amf = anim->animation_data;
        if (amf == nullptr) {
            return;
        }
        const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
        const bool mapped = map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0;
        const unsigned char* map = (const unsigned char*)amf + map_offset;
        for (int j = 0; j < amf->num_bones; j++) {
            setup_bone_data(amf, readers[i]);
            const int rig_bone = mapped ? map[j] : j;
            if (parent_rig_bone_indecies[rig_bone] != -1 || bone_lod_level > rig_bone_transform_list[rig_bone]) {
                continue;
            }
            const int b = entity_bone_info->rig_to_entity_bone_mapping[rig_bone];
            if (b == -1) {
                continue;
            }
            const float w = weights[i][b];
            if (!(0.0f < w)) {
                continue;
            }
            quaternion q;
            vector pos;
            interpolate_compressed_rotation(time, readers[i], q);
            anim->find_animation_translation(pos, time, readers[i]);
            local_bone_blend& bb = blend[b];
            vector& out = bone_matrix[b].m_translation;
            if (bb.weight == 0.0f) {
                out = pos;
                bb.rotation = q;
            } else {
                const float keep = (float)((double)bb.weight / ((double)w + (double)bb.weight));
                const float take = (float)(1.0 - (double)keep);
                out.x = out.x * keep;
                out.y = out.y * keep;
                out.z = out.z * keep;
                out.x = pos.x * take + out.x;
                out.y = out.y + pos.y * take;
                out.z = out.z + pos.z * take;
                if (bb.heading_only == 0) {
                    const quaternion old = bb.rotation;
                    pltind_slerp(bb.rotation, old, q, take);
                    bb.rotation.normalize();
                } else {
                    matrix m;
                    float heading_new;
                    float heading_old;
                    q.extract_matrix(m);
                    m.extract_heading(heading_new);
                    bb.rotation.extract_matrix(m);
                    m.extract_heading(heading_old);
                    m.set(0.0f, 0.0f, (float)((double)heading_new * (double)take + (double)heading_old * (double)keep));
                    bb.rotation.set(m);
                }
            }
            bb.weight = w + bb.weight;
        }
    }

    // 4. additive animations
    for (int i = 0; i < num_active_anims; i++) {
        const anim_instance_info& info = active_anims[i];
        if ((info.flags & ANIM_FLAG_ADDITIVE) == 0 || !(0.0f < info.mix_percent)) {
            continue;
        }
        int time = info.cur_time;
        if (info.flags & ANIM_FLAG_SNAP_TO_FRAMES) {
            time = snap_to_frame(time);
        }
        anim_animation* anim = get_animation_from_handle(info.anim_handle);
        if (anim == nullptr) {
            return;
        }
        anim_file* amf = anim->animation_data;
        if (amf == nullptr) {
            return;
        }
        const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
        const bool mapped = map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0;
        const unsigned char* map = (const unsigned char*)amf + map_offset;
        for (int j = 0; j < amf->num_bones; j++) {
            setup_bone_data(amf, readers[i]);
            const int rig_bone = mapped ? map[j] : j;
            if (parent_rig_bone_indecies[rig_bone] != -1 || bone_lod_level > rig_bone_transform_list[rig_bone]) {
                continue;
            }
            const int b = entity_bone_info->rig_to_entity_bone_mapping[rig_bone];
            if (b == -1) {
                continue;
            }
            const float w = weights[i][b];
            if (!(0.0f < w) || !(0.0f < blend[b].weight)) {
                continue;
            }
            quaternion base_rotation;
            vector base_pos;
            if ((info.flags & ANIM_FLAG_LOOPING) == 0) {
                interpolate_compressed_rotation(0, readers[i], base_rotation);
                anim->find_animation_translation(base_pos, 0, readers[i]);
            } else {
                base_rotation.make_identity();
                base_pos.zero();
            }
            base_rotation.inverse();
            quaternion q;
            vector pos;
            interpolate_compressed_rotation(time, readers[i], q);
            anim->find_animation_translation(pos, time, readers[i]);
            vector& out = bone_matrix[b].m_translation;
            out.x = out.x + (pos.x - base_pos.x) * w;
            out.y = (pos.y - base_pos.y) * w + out.y;
            out.z = out.z + (pos.z - base_pos.z) * w;
            const quaternion target = (blend[b].rotation * base_rotation) * q;
            const quaternion old = blend[b].rotation;
            pltind_slerp(blend[b].rotation, old, target, w);
        }
    }

    // 5. matrices
    for (int b = 0; b < num_bones; b++) {
        if (bone_lod_level > rig_bone_transform_list[entity_bone_info->entity_to_rig_bone_mapping[b]] || parent_rig_bone_indecies[b] != -1) {
            continue;
        }
        bone_info[b].flags.tmp_matrices_valid = 1;
        if (blend[b].weight <= 0.0f) {
            bone_matrix[b].make_identity();
        } else {
            blend[b].rotation.extract_matrix(bone_matrix[b].m_rotation);
            bone_info[b].flags.is_weighted = 1;
        }
        if (bone_info[b].modifier == BONE_MODIFIER_COPY) {
            const float w = blend[b].weight;
            bone_matrix[b].m_translation.x = w * bone_matrix[b].m_translation.x;
            bone_matrix[b].m_translation.y = w * bone_matrix[b].m_translation.y;
            bone_matrix[b].m_translation.z = w * bone_matrix[b].m_translation.z;
        }
        const anim_bone& bone = rig->bones[entity_bone_info->entity_to_rig_bone_mapping[b]];
        bone_matrix[b].m_translation.x = bone_matrix[b].m_translation.x + bone.rel_bone_translation.x;
        bone_matrix[b].m_translation.y = bone.rel_bone_translation.y + bone_matrix[b].m_translation.y;
        bone_matrix[b].m_translation.z = bone.rel_bone_translation.z + bone_matrix[b].m_translation.z;
    }
}

// 0x00C96080. The morph weights of the playing animations with morph tables (like the bone weights: each animation's
// share is its mix times what the higher priority animations above it left), the 20 strongest kept, then morph_func.
void anim_instance_manager::blend_all_morphs()
{
    int i = 0;
    for (; i < num_active_anims; i++) {
        const int anim_handle = active_anims[i].anim_handle;
        if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
            return;
        }
        if (Animations[anim_handle].animation_data->flags & 0x10) {
            break;
        }
    }
    if (i >= num_active_anims) {
        return;
    }
    allocate_morph_info();
    if (morph_info == nullptr) {
        return;
    }
    for (int a = 0; a < num_active_anims; a++) {
        const anim_instance_info& info = active_anims[a];
        anim_animation* anim = get_animation_from_handle(info.anim_handle);
        const float mix = info.mix_percent;
        if (!(0.0f < mix) || anim == nullptr || anim->animation_data == nullptr || (anim->animation_data->flags & 0x10) == 0) {
            continue;
        }
        const anim_file* amf = anim->animation_data;
        float above = 0.0f;
        for (int j = num_active_anims - 1; j > a; j--) {
            if ((active_anims[j].flags & ANIM_FLAG_ADDITIVE) == 0) {
                if ((int)active_anims[j].priority <= (int)info.priority) {
                    break;
                }
                above = active_anims[j].mix_percent + above;
            }
        }
        if (0.0f <= (float)((double)above - 1.0)) {
            above = 1.0f;
        }
        const double left = 1.0 - (double)above;
        const anim_morph* morphs = (const anim_morph*)(amf + 1);
        const int* offsets = morphs->morph_offset.to_pointer();
        for (int m = 0; m < (int)morphs->num_morphs; m++) {
            float w = anim->find_animation_morph_weight(m, info.cur_time);
            if (!(0.0f <= w - 0.0f)) {
                w = 0.0f;
            }
            if (0.0f <= (float)((double)w - 1.0)) {
                w = 1.0f;
            }
            const float amount = (float)((double)(w * mix) * left);
            if (!(0.0f < amount)) {
                continue;
            }
            int slot = 0;
            if (num_active_morphs != 0) {
                while (slot < num_active_morphs && !(morph_info[slot].blend_amount < amount)) {
                    slot++;
                }
                if (slot > 19) {
                    continue;
                }
            }
            if (19 - slot > 0) {
                memmove(&morph_info[slot + 1], &morph_info[slot], (19 - slot) * sizeof(anim_morph_info));
            }
            morph_info[slot].blend_amount = amount;
            morph_info[slot].name_check = ((const afc_morph*)((const char*)amf + offsets[m]))->name_chk;
            morph_info[slot].morph_set = anim->morph_set_name;
            if (num_active_morphs < 20) {
                num_active_morphs++;
            }
        }
    }
    if (morph_func != nullptr) {
        morph_func(num_active_morphs, morph_info, morph_data);
    }
}

// 0x00C97310. Advances the ramp and sets mix_percent: ramping in, fully in (a non-looping animation starts its ramp
// out when its end is within the ramp out time) or ramping out (base mix 0 when done).
void anim_instance_manager::process_ramp(anim_instance_info* anim_info, int tick_delta)
{
    anim_info->ramp.cur_time += tick_delta;
    const anim_ramp_state state = anim_info->ramp.state;
    float level;
    if (state == ARS_RAMP_IN) {
        level = (float)(anim_info->ramp.cur_time - anim_info->ramp.start_time) / (float)anim_info->ramp.duration;
        if (1.0f <= level) {
            anim_info->ramp.state = ARS_RAMP_NONE;
            level = 1.0f;
        }
    } else if (state == ARS_RAMP_NONE) {
        if (anim_info->flags & ANIM_FLAG_LOOPING) {
            anim_info->mix_percent = anim_info->base_mix_percent;
            return;
        }
        int ramp_in;
        int ramp_out;
        get_ramp_in_out_times(anim_info, ramp_in, ramp_out);
        const int left = get_animation_time_to_end(anim_info);
        if (ramp_out < 1 || ramp_out < left) {
            level = (left == 0 && anim_info->prev_time == anim_info->cur_time) ? 0.0f : 1.0f;
        } else {
            anim_info->ramp.start_time = left - ramp_out;
            anim_info->ramp.duration = ramp_out;
            anim_info->ramp.cur_time = 0;
            anim_info->ramp.state = ARS_RAMP_OUT;
            level = (float)(1.0 - (double)(float)(ramp_out - left) / (double)(float)ramp_out);
        }
    } else if (state == ARS_RAMP_OUT) {
        level = (float)(anim_info->ramp.cur_time - anim_info->ramp.start_time) / (float)anim_info->ramp.duration;
        if (1.0f <= level) {
            anim_info->base_mix_percent = 0.0f;
            level = 1.0f;
        }
        anim_info->mix_percent = (1.0f - level) * anim_info->base_mix_percent;
        return;
    } else {
        return;
    }
    anim_info->mix_percent = anim_info->base_mix_percent * level;
}

// frac(x) the way the binary computes it (floor in double, through float and int).
static float frac_through_int(float x)
{
    const int whole = (int)(float)floor_d((double)x);
    return (float)((double)x - (double)(float)whole);
}

// 0x00C974A0. The looping animations share cyclic_percent. The dominant priority's strongest animations (by mix,
// non-morph, not flags & 0x22000) set the cycle length (long animations, over 900 ticks, before short ones); the
// cyclic percent advances by the frame's share of that length and every looping animation's time follows it (or its
// own speed with flags & 0x10000). The triggers of the strongest animation are set.
void anim_instance_manager::process_animation_looping(int* looping_indices, int num_animations, int tick_delta, bool allow_state_change)
{
    old_cyclic_percent = cyclic_percent;
    anim_priority best_priority = active_anims[looping_indices[0]].priority;
    for (int n = 0; n < num_animations; n++) {
        anim_instance_info* info = &active_anims[looping_indices[n]];
        if (allow_state_change) {
            process_ramp(info, tick_delta);
        }
        if ((int)best_priority < (int)info->priority && 0.1f < info->mix_percent) {
            best_priority = info->priority;
        }
    }
    float best_mix = 0.0f;
    int chosen = -1;
    float long_length = 0.0f;
    float long_mix = 0.0f;
    float short_length = 0.0f;
    float short_mix = 0.0f;
    for (int n = 0; n < num_animations; n++) {
        anim_instance_info& info = active_anims[looping_indices[n]];
        const float mix = info.mix_percent;
        if (!(0.0f < mix) || (info.flags & (ANIM_FLAG_ADDITIVE | ANIM_FLAG_NO_CYCLE_DRIVE)) != 0 || !(best_mix < mix) || info.priority != best_priority) {
            continue;
        }
        if (!anim_handle_valid(info.anim_handle)) {
            info.mix_percent = 0.0f;
            continue;
        }
        const anim_file* amf = Animations[info.anim_handle].animation_data;
        if (amf == nullptr || (amf->flags & 0x10) != 0) {
            continue;
        }
        const unsigned int end = amf->end_tock * 80;
        best_mix = mix;
        chosen = n;
        if (end < 901) {
            short_length = (float)((double)(float)end * (double)mix / (double)info.speed_multiplier + (double)short_length);
            short_mix = mix + short_mix;
        } else {
            long_length = (float)((double)(float)end * (double)mix / (double)info.speed_multiplier + (double)long_length);
            long_mix = mix + long_mix;
        }
    }
    if (chosen == -1) {
        cyclic_percent = 0.0f;
        old_cyclic_percent = 0.0f;
        return;
    }
    float length;
    float length_mix;
    // 0.0001 is a float constant held in double (__real@3f1a36e2e0000000)
    if ((double)0.0001f <= (double)(long_mix - 0.0f) || (double)0.0001f <= (double)-(long_mix - 0.0f)) {
        length = long_length;
        length_mix = long_mix;
    } else {
        if ((double)(short_length - 0.0f) < (double)0.0001f && (double)-(short_length - 0.0f) < (double)0.0001f) {
            return;
        }
        length = short_length;
        length_mix = short_mix;
    }
    const float cycle = length / length_mix;
    const float advance = (float)((double)cyclic_speed * (double)(float)tick_delta / (double)cycle);
    const float advance_frac = (float)((double)advance - (double)(float)floor_d((double)advance));
    const float delta = frac_through_int(advance_frac);
    cyclic_percent = delta + cyclic_percent;
    if (num_animations == 1) {
        anim_instance_info& only = active_anims[looping_indices[0]];
        cyclic_percent = cyclic_percent - only.init_cyclic_percent;
        if (0.0f > cyclic_percent) {
            cyclic_percent = cyclic_percent + 1.0f;
        }
        only.init_cyclic_percent = 0.0f;
    } else if (num_animations == 2 && active_anims[looping_indices[0]].init_cyclic_percent == active_anims[looping_indices[1]].init_cyclic_percent) {
        cyclic_percent = cyclic_percent - active_anims[looping_indices[0]].init_cyclic_percent;
        if (0.0f > cyclic_percent) {
            cyclic_percent = cyclic_percent + 1.0f;
        }
        active_anims[looping_indices[0]].init_cyclic_percent = 0.0f;
        active_anims[looping_indices[1]].init_cyclic_percent = 0.0f;
    }
    const float unwrapped = cyclic_percent;
    if (1.0f <= unwrapped) {
        cyclic_percent = frac_through_int(unwrapped);
    }
    // Game trace only (the turret Demolitions Master hunt, 2026-11-06): the update that makes the watched instance's
    // cyclic percent non-finite, with its inputs.
    if (Game_trace_file != nullptr && handle == Game_trace_anim_watch_handle && isfinite(old_cyclic_percent) && !isfinite(cyclic_percent)) {
        GAME_TRACE("anim", "cyclic_percent %g -> %g: length=%g length_mix=%g long=%g/%g short=%g/%g cyclic_speed=%g tick_delta=%d advance=%g delta=%g looping=%d chosen=%d", old_cyclic_percent, cyclic_percent,
                   length, length_mix, long_length, long_mix, short_length, short_mix, cyclic_speed, tick_delta, advance, delta, num_animations, chosen);
        for (int n = 0; n < num_animations; n++) {
            const anim_instance_info& a = active_anims[looping_indices[n]];
            const anim_file* amf = anim_handle_valid(a.anim_handle) ? Animations[a.anim_handle].animation_data : nullptr;
            GAME_TRACE("anim", "  looping %d: %s mix=%g speed=%g init=%g end=%d flags=0x%X prio=%d", n, anim_handle_valid(a.anim_handle) ? Animations[a.anim_handle].anim_filename : "?", a.mix_percent,
                       a.speed_multiplier, a.init_cyclic_percent, amf != nullptr ? (int)(amf->end_tock * 80) : -1, a.flags, (int)a.priority);
        }
    }
    for (int n = 0; n < num_animations; n++) {
        anim_instance_info& info = active_anims[looping_indices[n]];
        if (!(0.0f < info.mix_percent) || !anim_handle_valid(info.anim_handle)) {
            continue;
        }
        const int anim_handle = info.anim_handle;
        if (info.speed_multiplier != 1.0f) {
            const float shifted = (float)((double)info.init_cyclic_percent - ((double)info.speed_multiplier - 1.0) * (double)delta);
            info.init_cyclic_percent = frac_through_int(shifted);
        }
        info.prev_time = info.cur_time;
        const unsigned int flags = info.flags;
        if ((flags & ANIM_FLAG_MANUAL_TIME) == 0) {
            float percent = cyclic_percent;
            if (info.init_cyclic_percent != 0.0f) {
                percent = percent - info.init_cyclic_percent;
                if (0.0f > percent) {
                    percent = percent + 1.0f;
                }
            }
            if (flags & ANIM_FLAG_REVERSE) {
                percent = 1.0f - percent;
            }
            info.cur_time = (int)(float)floor_d((double)((float)anim_end_ticks(anim_handle) * percent));
        } else {
            const float ticks = (float)((double)info.speed_multiplier * (double)(float)tick_delta + (double)info.remainder_time);
            const int whole = (int)ticks;
            info.remainder_time = ticks - (float)whole;
            if ((flags & ANIM_FLAG_REVERSE) == 0) {
                info.cur_time += whole;
                if (anim_end_ticks(anim_handle) < info.cur_time) {
                    info.cur_time = 0;
                }
            } else {
                info.cur_time -= whole;
                if (info.cur_time < 0) {
                    info.cur_time = anim_end_ticks(anim_handle);
                }
            }
        }
        if (allow_state_change && n == chosen) {
            const anim_animation& anim = Animations[anim_handle];
            for (int t = 0; t < anim.num_triggers; t++) {
                int from;
                int to;
                if ((flags & ANIM_FLAG_REVERSE) == 0) {
                    from = info.prev_time;
                    to = info.cur_time;
                } else {
                    from = info.cur_time;
                    to = info.prev_time;
                }
                const int tick = anim.triggers[t].ticktime;
                if (((tick <= to && from < tick) || (1.0f <= unwrapped && (from < tick || tick < to))) && (info.trigger_data & (1u << ((t * 3 + 2) & 0x1F))) == 0) {
                    info.trigger_data |= (1u << ((t * 3) & 0x1F)) | (1u << ((t * 3 + 1) & 0x1F));
                }
            }
        }
    }
}

// 0x00C97D10. Advances the non-looping animations by their speed; at the end an animation is marked finished
// (flags & 0x100, removed with mix 0 next frame) or freezes the instance. Note the original returns from the whole
// function at the first animation when !allow_state_change.
void anim_instance_manager::process_animation_non_looping(int* non_looping_indices, int num_animations, int tick_delta, bool allow_state_change)
{
    for (int n = 0; n < num_animations; n++) {
        anim_instance_info* info = &active_anims[non_looping_indices[n]];
        const int anim_handle = info->anim_handle;
        if (!anim_handle_valid(anim_handle) || Animations[anim_handle].animation_data == nullptr) {
            info->base_mix_percent = 0.0f;
            info->mix_percent = 0.0f;
            continue;
        }
        if (info->flags & ANIM_FLAG_FINISHED) {
            info->base_mix_percent = 0.0f;
            info->mix_percent = 0.0f;
        }
        const float ticks = (float)((double)info->speed_multiplier * (double)(float)tick_delta + (double)info->remainder_time);
        const int whole = (int)ticks;
        bool finished = false;
        info->prev_time = info->cur_time;
        info->remainder_time = ticks - (float)whole;
        if ((info->flags & ANIM_FLAG_REVERSE) == 0) {
            info->cur_time += whole;
            if (anim_end_ticks(anim_handle) < info->cur_time) {
                finished = true;
                info->cur_time = anim_end_ticks(anim_handle);
            }
        } else {
            info->cur_time -= whole;
            if (info->cur_time < 0) {
                finished = true;
                info->cur_time = 0;
            }
        }
        if (!allow_state_change) {
            return;
        }
        process_ramp(info, tick_delta);
        if (finished) {
            if (info->signature == animation_to_freeze_sig) {
                frozen = 1;
            } else {
                info->flags |= ANIM_FLAG_FINISHED;
            }
        }
        if (0.0f < info->mix_percent) {
            const anim_animation& anim = Animations[anim_handle];
            for (int t = 0; t < anim.num_triggers; t++) {
                const int tick = anim.triggers[t].ticktime;
                if (tick <= info->cur_time && info->prev_time < tick && (info->trigger_data & (1u << ((t * 3 + 2) & 0x1F))) == 0) {
                    info->trigger_data |= (1u << ((t * 3) & 0x1F)) | (1u << ((t * 3 + 1) & 0x1F));
                }
            }
        }
    }
}

// 0x00C8D250 (not reached).
void anim_instance_manager::clear_trigger_just_elapsed_flags()
{
    for (int i = 0; i < num_active_anims; i++) {
        active_anims[i].trigger_data &= 0xB6DB6DB6;
    }
}

// 0x00C99070. frametime in seconds; the fractions of a tick carry over in partial_frame_amount.
void anim_instance_manager::process_animations(float frametime, bool allow_state_change)
{
    if (frozen != 0 || num_active_anims < 1) {
        return;
    }
    if (allow_state_change) {
        for (int i = 0; i < num_active_anims; i++) {
            active_anims[i].trigger_data &= 0xB6DB6DB6; // clears the just elapsed bits
        }
    }
    int ticks = (int)((double)frametime * 4800.0);
    float partial = (float)((double)frametime * 4800.0 - (double)(float)ticks + (double)partial_frame_amount);
    partial_frame_amount = partial;
    if (1.0f <= partial) {
        partial_frame_amount = partial - 1.0f;
        ticks++;
    } else if (!(-1.0f < partial)) {
        partial_frame_amount = partial + 1.0f;
        ticks--;
    }
    int looping[ANIM_MAX_ACTIVE];
    int non_looping[ANIM_MAX_ACTIVE];
    int num_looping = 0;
    int num_non_looping = 0;
    for (int i = 0; i < num_active_anims; i++) {
        if ((active_anims[i].flags & ANIM_FLAG_LOOPING) == 0) {
            non_looping[num_non_looping++] = i;
        } else {
            looping[num_looping++] = i;
        }
    }
    if (num_looping > 0) {
        process_animation_looping(looping, num_looping, ticks, allow_state_change);
    }
    process_animation_non_looping(non_looping, num_non_looping, ticks, allow_state_change);
    if (allow_state_change) {
        remove_inactive_anims();
    }
}

// 0x00C97EF0. Removes the animations with base mix 0.
void anim_instance_manager::remove_inactive_anims()
{
    for (int i = num_active_anims - 1; i > -1; i--) {
        if (active_anims[i].base_mix_percent == 0.0f) {
            remove_active_anim_at(this, find_active_anim(compose_anim_instance_handle(active_anims[i].signature)));
        }
    }
}

// 0x00C980D0
void anim_instance_manager::stop_animation(unsigned int handlep, float blend_out_time_sec)
{
    const int index = find_active_anim(handlep);
    if (index == -1 || !anim_handle_valid(active_anims[index].anim_handle)) {
        return;
    }
    if (animation_to_freeze_sig != -1 && animation_to_freeze_sig == active_anims[index].signature) {
        animation_to_freeze_sig = -1;
        frozen = 0;
    }
    stop_animation_internal(&active_anims[index], blend_out_time_sec);
    remove_inactive_anims();
}

// Inlined in get_bone_animating_fraction and both is_bone_animating (helper inferred): whether the file has keys for
// the rig bone, through its bone map when it has one.
static bool anim_file_has_rig_bone(const anim_file* amf, int bone_index)
{
    if (amf == nullptr || bone_index >= amf->num_rig_bones) {
        return false;
    }
    const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
    const bool mapped = map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0;
    if (!mapped) {
        return bone_index < amf->num_bones;
    }
    const unsigned char* map = (const unsigned char*)amf + map_offset;
    if (map[bone_index] == bone_index) {
        return true;
    }
    for (int j = 0; j < amf->num_bones; j++) {
        if (map[j] == bone_index) {
            return true;
        }
    }
    return false;
}

// 0x00C98180. The share of the mix of the playing animations that has the bone. Magic number: the mix counts as at
// least 0.001, so nothing playing answers 0.
float anim_instance_manager::get_bone_animating_fraction(int bone_index)
{
    float total = 0.0f;
    float animating = 0.0f;
    for (int i = 0; i < num_active_anims; i++) {
        const anim_instance_info& info = active_anims[i];
        total = info.mix_percent + total;
        if (anim_handle_valid(info.anim_handle) && anim_file_has_rig_bone(Animations[info.anim_handle].animation_data, bone_index)) {
            animating = info.mix_percent + animating;
        }
    }
    if (total - 0.001f < 0.0f) {
        total = 0.001f;
    }
    return animating / total;
}

// 0x00C982E0. From the bone infos when the instance has them, else whether any playing animation has the bone.
bool anim_instance_manager::is_bone_animating(int bone_index)
{
    if (bone_info != nullptr) {
        return bone_info[bone_index].flags.is_weighted != 0;
    }
    for (int i = 0; i < num_active_anims; i++) {
        const int anim_handle = active_anims[i].anim_handle;
        if (!anim_handle_valid(anim_handle)) {
            continue;
        }
        const anim_file* amf = Animations[anim_handle].animation_data;
        if (amf == nullptr || bone_index >= amf->num_rig_bones) {
            continue;
        }
        const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
        const bool mapped = map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0;
        if (!mapped && bone_index < amf->num_bones) {
            return true;
        }
        if (mapped) {
            const unsigned char* map = (const unsigned char*)amf + map_offset;
            if (map[bone_index] == bone_index) {
                return true;
            }
            for (int j = 0; j < amf->num_bones; j++) {
                if (map[j] == bone_index) {
                    return true;
                }
            }
        }
    }
    return false;
}

// 0x00C983D0. Whether that playing animation has the bone.
bool anim_instance_manager::is_bone_animating(unsigned int handlep, int bone_index)
{
    const int index = find_active_anim(handlep);
    if (index < 0 || !anim_handle_valid(active_anims[index].anim_handle)) {
        return false;
    }
    return anim_file_has_rig_bone(Animations[active_anims[index].anim_handle].animation_data, bone_index);
}

// 0x00C984C0
void anim_instance_manager::allocate_bone_modifiers()
{
    if (bone_modifiers == nullptr) {
        bone_modifiers = bone_modifier_data_list.alloc(0);
    }
}

// 0x00C984E0. storage_idx selects one of the 15 modifier slots; old_bone_orient and blend_pct are kept for the force
// and rotate modifiers.
void anim_instance_manager::register_bone_modifier(bone_modifier modifier, int bone_index, int storage_idx, matrix& delta_orient, matrix* old_bone_orient, float blend_pct)
{
    if ((unsigned int)storage_idx >= 15 || bone_info == nullptr) {
        return;
    }
    if (bone_modifiers == nullptr) {
        bone_modifiers = bone_modifier_data_list.alloc(0);
        if (bone_modifiers == nullptr) {
            return;
        }
    }
    if (!delta_orient.is_valid()) {
        keen::debug::nativePrintArgs("anim_instance_manager::register_bone_modifier: !delta_orient.is_valid()\n");
        // Game trace only: storage_idx tells the caller (spinebend bone states are small, 0xD camera bone copy, 0xE root
        // offset or lean, 9 and up foot IK).
        GAME_TRACE("anim", "register_bone_modifier rejects a non-finite delta: modifier=%d bone=%d storage=%d fvec=(%g %g %g)", (int)modifier, bone_index, storage_idx, delta_orient.fvec.x, delta_orient.fvec.y,
                   delta_orient.fvec.z);
        return;
    }
    if (old_bone_orient != nullptr && !old_bone_orient->is_valid()) {
        keen::debug::nativePrintArgs("anim_instance_manager::register_bone_modifier: !old_bone_orient.is_valid()\n");
        return;
    }
    bone_modifiers->bones[storage_idx].desired_bone_orient = delta_orient;
    if (modifier == BONE_MODIFIER_FORCE || modifier == BONE_MODIFIER_ROTATE) {
        if (old_bone_orient == nullptr) {
            return;
        }
        bone_modifiers->bones[storage_idx].old_bone_orient = *old_bone_orient;
        bone_modifiers->bones[storage_idx].forced_bone_blend = blend_pct;
    }
    bone_info[bone_index].modifier = modifier;
    bone_info[bone_index].flags.use_special_func = 1;
    bone_info[bone_index].data_idx = storage_idx;
}

// 0x00C991D0. The root controller movement of the playing animations this frame, weighted like the bone weights
// (each animation's root weight times its mix times what higher priorities left): the non-looping animations into
// action_delta_pos (normalized by their total weight) and anim_local_delta_pos, the looping ones (with wrap around)
// into state_delta_pos.
void anim_instance_manager::get_position_change(vector& action_delta_pos, vector& state_delta_pos, vector& anim_local_delta_pos) const
{
    float root_weight[ANIM_MAX_ACTIVE];
    float action_weight = 0.0f;
    action_delta_pos.zero();
    state_delta_pos.zero();
    anim_local_delta_pos.zero();
    for (int i = 0; i < num_active_anims; i++) {
        const int anim_handle = active_anims[i].anim_handle;
        if (!anim_handle_valid(anim_handle) || Animations[anim_handle].animation_data == nullptr) {
            root_weight[i] = 0.0f;
            continue;
        }
        const anim_file* amf = Animations[anim_handle].animation_data;
        if ((amf->flags & 0x40) == 0) {
            root_weight[i] = 1.0f;
            continue;
        }
        animlib_compressed_data_reader reader;
        reader.bone_idx = -1;
        reader.init_level = 2;
        setup_bone_weights(amf, (char*)amf + amf->root_controller_bone.get_offset(), reader, 1);
        setup_bone_data(amf, reader);
        root_weight[i] = Animations[anim_handle].find_animation_bone_weight(0, active_anims[i].cur_time, reader);
    }
    for (int i = 0; i < num_active_anims; i++) {
        const anim_instance_info& info = active_anims[i];
        float above = 0.0f;
        for (int j = num_active_anims - 1; j > i; j--) {
            if ((active_anims[j].flags & ANIM_FLAG_ADDITIVE) == 0) {
                if ((int)active_anims[j].priority <= (int)info.priority) {
                    break;
                }
                above = root_weight[j] + above;
            }
        }
        if (0.0f <= (float)((double)above - 1.0)) {
            above = 1.0f;
        }
        const float w = (float)((double)root_weight[i] * (double)info.mix_percent * (double)(float)(1.0 - (double)above));
        if (!(0.0f < w) || !anim_handle_valid(info.anim_handle) || Animations[info.anim_handle].animation_data == nullptr) {
            continue;
        }
        const anim_animation& anim = Animations[info.anim_handle];
        vector delta;
        vector local_delta;
        if ((info.flags & ANIM_FLAG_LOOPING) == 0) {
            anim.get_delta_pos(delta, local_delta, info.prev_time, info.cur_time);
            anim_local_delta_pos.x = anim_local_delta_pos.x + local_delta.x;
            anim_local_delta_pos.y = anim_local_delta_pos.y + local_delta.y;
            anim_local_delta_pos.z = anim_local_delta_pos.z + local_delta.z;
            action_delta_pos.x = action_delta_pos.x + delta.x * w;
            action_delta_pos.y = action_delta_pos.y + delta.y * w;
            action_delta_pos.z = action_delta_pos.z + delta.z * w;
            action_weight = w + action_weight;
            continue;
        }
        const int prev = info.prev_time;
        const int cur = info.cur_time;
        if ((info.flags & ANIM_FLAG_REVERSE) == 0 && cur < prev) {
            // wrapped: to the end, then from the start
            vector wrap;
            anim.get_delta_pos(delta, local_delta, prev, anim_end_ticks(info.anim_handle));
            anim.get_delta_pos(wrap, local_delta, 0, cur);
            delta.x = wrap.x + delta.x;
            delta.y = wrap.y + delta.y;
            delta.z = wrap.z + delta.z;
        } else if ((info.flags & ANIM_FLAG_REVERSE) != 0 && prev < cur) {
            vector wrap;
            anim.get_delta_pos(delta, local_delta, prev, 0);
            anim.get_delta_pos(wrap, local_delta, anim_end_ticks(info.anim_handle), cur);
            delta.x = wrap.x + delta.x;
            delta.y = wrap.y + delta.y;
            delta.z = wrap.z + delta.z;
        } else {
            anim.get_delta_pos(delta, local_delta, prev, cur);
        }
        state_delta_pos.x = state_delta_pos.x + delta.x * w;
        state_delta_pos.y = state_delta_pos.y + delta.y * w;
        state_delta_pos.z = state_delta_pos.z + delta.z * w;
    }
    if (action_weight <= 0.0f) {
        return;
    }
    const float inv = 1.0f / action_weight;
    action_delta_pos.x = action_delta_pos.x * inv;
    action_delta_pos.y = action_delta_pos.y * inv;
    action_delta_pos.z = action_delta_pos.z * inv;
}

// 0x00C99930. The weighted root heading change of the playing animations over the next future_ticks (or since the
// last frame when future_ticks is 0); looping animations wrap around.
float anim_instance_manager::get_heading_change(int future_ticks) const
{
    float root_weight[ANIM_MAX_ACTIVE];
    float total = 0.0f;
    for (int i = 0; i < num_active_anims; i++) {
        const int anim_handle = active_anims[i].anim_handle;
        // The original reads the anim_file through a null animation for an invalid handle.
        const anim_animation* anim = (anim_handle < 0 || anim_handle >= Num_animations) ? nullptr : &Animations[anim_handle];
        const anim_file* amf = anim->animation_data;
        if (amf == nullptr) {
            root_weight[i] = 0.0f;
        } else if ((amf->flags & 0x40) == 0) {
            root_weight[i] = 1.0f;
        } else {
            animlib_compressed_data_reader reader;
            reader.bone_idx = -1;
            reader.init_level = 2;
            setup_bone_weights(amf, (char*)amf + amf->root_controller_bone.get_offset(), reader, 1);
            setup_bone_data(amf, reader);
            root_weight[i] = anim->find_animation_bone_weight(0, active_anims[i].cur_time + future_ticks, reader);
        }
    }
    for (int i = 0; i < num_active_anims; i++) {
        if (root_weight[i] == 0.0f) {
            continue;
        }
        const anim_instance_info& info = active_anims[i];
        float above = 0.0f;
        for (int j = num_active_anims - 1; j > i; j--) {
            if ((int)active_anims[j].priority <= (int)info.priority) {
                break;
            }
            above = (float)((double)root_weight[j] * (double)active_anims[j].mix_percent + (double)above);
        }
        if (0.0f <= (float)((double)above - 1.0)) {
            above = 1.0f;
        }
        const float w = (float)((double)(float)(1.0 - (double)above) * (double)root_weight[i]);
        if (!(0.0f < w)) {
            continue;
        }
        const anim_animation* anim = (info.anim_handle < 0 || info.anim_handle >= Num_animations) ? nullptr : &Animations[info.anim_handle];
        const anim_file* amf = anim->animation_data;
        const int end = amf != nullptr ? amf->end_tock * 80 : 0;
        const int future = future_ticks + info.cur_time;
        const int from = future_ticks > 0 ? info.cur_time : info.prev_time;
        int to = future;
        if (amf != nullptr && end < future) {
            to = (info.flags & ANIM_FLAG_LOOPING) ? future - end : end;
        }
        float heading;
        if ((info.flags & ANIM_FLAG_LOOPING) == 0 || from <= to) {
            heading = anim->get_delta_heading(from, to);
        } else {
            const float to_end = anim->get_delta_heading(from, end);
            heading = anim->get_delta_heading(0, to) + to_end;
        }
        total = (float)((double)heading * (double)w + (double)total);
    }
    return total;
}

// 0x00C99D20. The root headings of the playing animations, weighted by what the higher priorities above each leave of
// it (additive animations take nothing) and wrapped to +-pi; 0 with no weight.
float anim_instance_manager::get_root_heading() const
{
    // Magic numbers: pi and 2 pi as rfg.exe's floats.
    const float PI_F = 3.1415927f;
    const float TWO_PI_F = 6.2831855f;
    float root_weight[ANIM_MAX_ACTIVE];
    for (int i = 0; i < num_active_anims; i++) {
        const int anim_handle = active_anims[i].anim_handle;
        // The original reads the anim_file through a null animation for an invalid handle.
        const anim_animation* anim = (anim_handle < 0 || anim_handle >= Num_animations) ? nullptr : &Animations[anim_handle];
        const anim_file* amf = anim->animation_data;
        if (amf == nullptr) {
            root_weight[i] = 0.0f;
        } else if ((amf->flags & 0x40) == 0) {
            root_weight[i] = 1.0f;
        } else {
            animlib_compressed_data_reader reader;
            reader.bone_idx = -1;
            reader.init_level = 2;
            setup_bone_weights(amf, (char*)amf + amf->root_controller_bone.get_offset(), reader, 1);
            setup_bone_data(amf, reader);
            root_weight[i] = anim->find_animation_bone_weight(0, active_anims[i].cur_time, reader);
        }
    }
    float heading_sum = 0.0f;
    float weight_sum = 0.0f;
    for (int i = 0; i < num_active_anims; i++) {
        const anim_instance_info& info = active_anims[i];
        float above = 0.0f;
        for (int j = num_active_anims - 1; j > i; j--) {
            const anim_instance_info& higher = active_anims[j];
            if ((higher.flags & ANIM_FLAG_ADDITIVE) != 0) {
                continue;
            }
            if ((int)higher.priority <= (int)info.priority) {
                break;
            }
            above = (float)((double)higher.mix_percent * (double)root_weight[j] + (double)above);
        }
        if (0.0f <= above - 1.0f) {
            above = 1.0f;
        }
        const float weight = root_weight[i] * info.mix_percent * (1.0f - above);
        if (0.0f < weight) {
            const anim_animation* anim = (info.anim_handle < 0 || info.anim_handle >= Num_animations) ? nullptr : &Animations[info.anim_handle];
            const float shifted = anim->get_heading(info.cur_time) + PI_F;
            const int turns = (int)((double)shifted / (double)TWO_PI_F);
            const float wrapped = (float)((double)shifted - (double)(float)(turns * 2) * (double)PI_F);
            const float heading = (float)((double)wrapped - (double)PI_F);
            heading_sum = (float)((double)heading * (double)weight + (double)heading_sum);
            weight_sum = weight + weight_sum;
        }
    }
    if (0.0f < weight_sum) {
        return heading_sum / weight_sum;
    }
    return heading_sum;
}

// 0x00C9A140. The bones' model space velocities: runs the instance a 30 fps frame ahead on its own state and restores
// it (the active animations, the bone infos, the cyclic percents). Velocities are zero while no animation has any mix.
// Magic numbers: 0.033333335 is the frame; 0x300 bytes are 64 bone infos of 12 bytes. The bone infos are read before
// any check, as the original.
void anim_instance_manager::get_bone_velocities(int num_bones, int* entity_bone_indices, vector* rotational_vel, vector* translational_vel)
{
    const float FRAME = 0.033333335f;
    const float saved_cyclic_percent = cyclic_percent;
    const float saved_old_cyclic_percent = old_cyclic_percent;
    anim_instance_info saved_anims[ANIM_MAX_ACTIVE];
    memcpy(saved_anims, active_anims, sizeof(saved_anims));
    unsigned char saved_bone_info[0x300];
    memcpy(saved_bone_info, bone_info, sizeof(saved_bone_info));
    if (cache_instanced_model == nullptr) {
        return;
    }
    process_animations(0.0f, false);
    bool any_mix = false;
    for (int i = 0; i < num_active_anims; i++) {
        if (0.0f < active_anims[i].mix_percent) {
            any_mix = true;
            break;
        }
    }
    if (!any_mix) {
        memset(rotational_vel, 0, num_bones * sizeof(vector));
        memset(translational_vel, 0, num_bones * sizeof(vector));
    } else {
        alignas(16) matrix43 local_matrices[64];
        alignas(16) matrix43 previous[64];
        get_local_bone_matrices(local_matrices, nullptr, 0);
        instance_bone_matrices(local_matrices, nullptr, 0);
        memcpy(previous, cache_instanced_model, entity_bone_info->num_bones * sizeof(matrix43));
        process_animations(FRAME, false);
        get_local_bone_matrices(local_matrices, nullptr, 0);
        if (bone_info != nullptr) {
            memset(bone_info, 0, entity_bone_info->num_bones * 0xC);
        }
        instance_bone_matrices(local_matrices, nullptr, 0);
        for (int i = 0; i < num_bones; i++) {
            const int bone = entity_bone_indices[i];
            const matrix43& now = cache_instanced_model[bone];
            translational_vel[i].x = (now.m_translation.x - previous[bone].m_translation.x) / FRAME;
            translational_vel[i].y = (now.m_translation.y - previous[bone].m_translation.y) / FRAME;
            translational_vel[i].z = (now.m_translation.z - previous[bone].m_translation.z) / FRAME;
            quaternion current_rotation(now.m_rotation);
            const quaternion previous_rotation(previous[bone].m_rotation);
            current_rotation.inverse();
            const quaternion delta = previous_rotation * current_rotation;
            vector axis(delta.x, delta.y, delta.z);
            float w = delta.w;
            if (w - -1.0f < 0.0f) {
                w = -1.0f;
            }
            if (0.0f <= w - 1.0f) {
                w = 1.0f;
            }
            const float half_angle = fl_acos(w);
            const float angle = half_angle + half_angle;
            axis.normalize_safe();
            rotational_vel[i].x = (axis.x * angle) / FRAME;
            rotational_vel[i].y = (axis.y * angle) / FRAME;
            rotational_vel[i].z = (axis.z * angle) / FRAME;
        }
        cyclic_percent = saved_cyclic_percent;
        old_cyclic_percent = saved_old_cyclic_percent;
    }
    memcpy(active_anims, saved_anims, sizeof(saved_anims));
    memcpy(bone_info, saved_bone_info, sizeof(saved_bone_info));
}

// force_bone_orient 0x00C8EDF0 and modify_bone_orient_pbh 0x00C8EEA0: not reached.

// 0x00C8EF20
void animlib_set_thread_ownership()
{
    Bone_pool_large.set_thread_ownership();
    Bone_pool_small.set_thread_ownership();
}
