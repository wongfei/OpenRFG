// Original: volition/anim_lib/code/anim_lib.cpp
//
// The public anim_lib API: instances by handle (aim handle; handle % 356 is the instance), rigs, animations and
// triggers, and the per-frame transform commands (animlib_request_batch_start / _end queue one per instance;
// animlib_batch_process_all_start runs them as scheduler jobs, or at once with cpu_processing). Part of
// anim_lib_unity.cpp (player_minimal.md P2).
// Every function of this file in rfg.pdb is ported (stub_misc.md group 1).
// The external interface is declared in anim_lib.h.

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/anim_lib/code/anim_lib_spinebend.h"

#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/util/string_comp.h"

#include "volition/rfg/code/gamewide/game_trace.h"

#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// ---- globals
animlib_transform_command Animlib_transform_command_buffer[176]; // 0x038D1DF0
int Num_animlib_transform_commands; // 0x039ACB18
event_trigger* Anim_scheduler_trigger; // 0x038C288C
bool Animlib_multi_thread; // 0x016F0888, false: every command runs on the calling thread (cpu_processing)
bool Bash_spinebend_bones; // 0x039ACB1C (debug: spine bend pitches jump to their targets)

// ---- game trace (new code, not in rfg.exe)
// The "player invisible after the turret Demolitions Master" hunt (2026-11-06): the watched instance (OpenRFG's trace
// sets the player's) is checked after each step of its command; the first step that leaves a non-finite bone matrix
// dumps what the instance plays. Rearms once the instance is finite again; at most 8 dumps a run.

static bool animlib_trace_finite(const matrix43& m)
{
    const float* f = (const float*)&m;
    for (int i = 0; i < 12; i++) {
        if (!isfinite(f[i])) {
            return false;
        }
    }
    return true;
}

extern "C" char __ImageBase; // the linker's symbol for the module base (MSVC)

// A non-finite value given to the watched instance, with the caller's offset in the module (resolve with the pdb). At
// most 32 lines a run.
static void animlib_trace_value(int aim_handle, const char* what, float value, void* caller)
{
    static volatile long lines = 0;
    if (Game_trace_file == nullptr || aim_handle != Game_trace_anim_watch_handle || isfinite(value) || lines >= 32) {
        return;
    }
    lines++;
    GAME_TRACE("anim", "%s(%g) from OpenRFG.exe+0x%X", what, value, (unsigned int)((uintptr_t)caller - (uintptr_t)&__ImageBase));
}

static void animlib_trace_check(int aim_handle, const matrix43* tmp_matrices, const char* step, float frametime)
{
    static volatile long armed = 1;
    static volatile long dumps = 0;
    if (Game_trace_file == nullptr || aim_handle != Game_trace_anim_watch_handle) {
        return;
    }
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr || aim->bone_info == nullptr || aim->entity_bone_info == nullptr) {
        return;
    }
    const int num_bones = aim->entity_bone_info->num_bones;
    int bad[16];
    int num_bad = 0;
    for (int b = 0; b < num_bones && num_bad < 16; b++) {
        bool ok;
        if (tmp_matrices != nullptr) {
            ok = !aim->bone_info[b].flags.tmp_matrices_valid || animlib_trace_finite(tmp_matrices[b]);
        } else {
            ok = aim->cache_instanced_model == nullptr || animlib_trace_finite(aim->cache_instanced_model[b]);
        }
        if (!ok) {
            bad[num_bad++] = b;
        }
    }
    if (num_bad == 0) {
        if (tmp_matrices == nullptr) {
            armed = 1;
        }
        return;
    }
    if (!armed || dumps >= 8) {
        return;
    }
    armed = 0;
    dumps++;
    char list[256] = "";
    for (int i = 0; i < num_bad; i++) {
        const int rig_bone = aim->entity_bone_info->entity_to_rig_bone_mapping[bad[i]];
        char one[64];
        snprintf(one, sizeof(one), "%s%d:%s", i == 0 ? "" : " ", bad[i], rig_bone >= 0 && aim->rig != nullptr ? aim->rig->bones[rig_bone].name.to_pointer() : "?");
        strncat_s(list, one, _TRUNCATE);
    }
    GAME_TRACE("anim", "non-finite bones after %s (frametime %g): %s", step, frametime, list);
    for (int i = 0; i < aim->num_active_anims; i++) {
        const anim_instance_info& a = aim->active_anims[i];
        const char* name = a.anim_handle >= 0 && a.anim_handle < Num_animations ? animlib_get_animation_name(a.anim_handle) : "?";
        GAME_TRACE("anim", "  active %d: %s mix=%g base_mix=%g time=%d prev=%d speed=%g cyclic=%g rem=%g flags=0x%X prio=%d", i, name != nullptr ? name : "?", a.mix_percent, a.base_mix_percent, a.cur_time, a.prev_time,
                   a.speed_multiplier, a.init_cyclic_percent, a.remainder_time, a.flags, (int)a.priority);
    }
    GAME_TRACE("anim", "  cyclic_percent=%g old=%g speed=%g partial=%g frozen=%d", aim->cyclic_percent, aim->old_cyclic_percent, aim->cyclic_speed, aim->partial_frame_amount, (int)aim->frozen);
    for (int b = 0; b < num_bones; b++) {
        const anim_bone_info& info = aim->bone_info[b];
        if (!info.flags.use_special_func || aim->bone_modifiers == nullptr || info.data_idx < 0 || info.data_idx >= 15) {
            continue;
        }
        const bone_modifier_data& d = aim->bone_modifiers->bones[info.data_idx];
        GAME_TRACE("anim", "  modifier bone %d: type=%d slot=%d desired fvec=(%g %g %g) uvec=(%g %g %g) old fvec=(%g %g %g) blend=%g", b, (int)info.modifier, info.data_idx, d.desired_bone_orient.fvec.x,
                   d.desired_bone_orient.fvec.y, d.desired_bone_orient.fvec.z, d.desired_bone_orient.uvec.x, d.desired_bone_orient.uvec.y, d.desired_bone_orient.uvec.z, d.old_bone_orient.fvec.x,
                   d.old_bone_orient.fvec.y, d.old_bone_orient.fvec.z, d.forced_bone_blend);
    }
}

// ---- internal functions
// 0x00C9B0B0; LTCG: command ESI. One instance's frame: process the animations, the local bone matrices, spine
// bending, the model space and skinning matrices (each step by its command bit).
static void animlib_process_command(animlib_transform_command* command)
{
    alignas(16) matrix43 tmp_matrices[64];
    if (command == nullptr) {
        return;
    }
    if (command->process_instance) {
        animlib_process_instance(command->aim_handle, command->frametime);
    }
    if (command->transform_all_bones_pre) {
        animlib_transform_all_bones_pre(command->aim_handle, tmp_matrices, command->bone_lods, command->bone_lod_level);
        animlib_trace_check(command->aim_handle, tmp_matrices, "pre", command->frametime);
    }
    if (command->process_spinebending) {
        animlib_spinebend_process(command->spinebend_data, tmp_matrices);
        animlib_trace_check(command->aim_handle, tmp_matrices, "spinebend", command->frametime);
    }
    if (command->transform_all_bones_post) {
        animlib_transform_all_bones_post(command->aim_handle, tmp_matrices, command->bone_lods, command->bone_lod_level);
        animlib_trace_check(command->aim_handle, nullptr, "post", command->frametime);
    }
}

// 0x00C892E0. The handle of a file name (linear probing over Animation_lookup_table); -1 when not in the table.
int animlib_find_anim(const char* filename)
{
    const unsigned int hash = crc_stri(filename, 0);
    for (int i = 0; i < Animation_lookup_table_collisions; i++) {
        const int handle = Animation_lookup_table[(i + hash % 7750) % 7750];
        if (handle != -1 && Animations[handle].anim_filename_hash == hash) {
            return handle;
        }
    }
    return -1;
}

// 0x00C89210. Forgets the paged in data of the animations (the permanent ones only with including_perm).
void animlib_reset_all_anims(bool including_perm)
{
    for (anim_animation& anim : Animations) {
        if (anim.anim_filename != nullptr && anim.animation_data != nullptr && ((anim.flags & 1) == 0 || including_perm)) {
            anim.animation_data = nullptr;
            anim.flags &= ~1u;
        }
    }
}

// 0x00C89350
void animlib_batch_process_all_end()
{
    scheduler_relinquish_cpu(Anim_scheduler_trigger, ccjt_any);
    Num_animlib_transform_commands = 0;
}

// 0x00C89370. A cleared command; the last one is reused when the buffer is full (175 commands).
animlib_transform_command* animlib_request_batch_start()
{
    int index = Num_animlib_transform_commands;
    if (index == 175) {
        index = 174;
    }
    memset(&Animlib_transform_command_buffer[index], 0, sizeof(animlib_transform_command));
    Num_animlib_transform_commands = index + 1;
    return &Animlib_transform_command_buffer[index];
}

// 0x00C893C0
void animlib_request_batch_end()
{
    if (!Animlib_multi_thread) {
        Animlib_transform_command_buffer[Num_animlib_transform_commands - 1].cpu_processing = 1;
    }
}

// 0x00C90BF0
void animlib_system_init()
{
    anim_rig_system_init();
    anim_instance_system_init();
    anim_animation_system_init();
}

// 0x00C90C10
void animlib_system_close()
{
    anim_rig_system_shutdown();
    animlib_reset_all_anims(true);
}

// 0x00C90C40
void animlib_reset_anim_system()
{
    anim_animation_system_init();
}

// 0x00C90C50
void animlib_do_frame_pre_transform(bool clear_morphs)
{
    anim_instance_do_frame(clear_morphs);
}

// 0x00C90C60
bool animlib_is_rig_loaded(const char* filename)
{
    return anim_rig_find_index(filename) > -1;
}

// 0x00C90C80
anim_rig* animlib_get_loaded_rig(const char* filename)
{
    const int index = anim_rig_find_index(filename);
    return index > -1 ? &Anim_rig[index] : nullptr;
}

// 0x00C90CA0
int animlib_get_rig_index(const char* filename)
{
    return anim_rig_find_index(filename);
}

// 0x00C98800
anim_rig* animlib_new_anim_rig(const char* filename, unsigned char* data_buffer, unsigned int buffer_size, bool permanent)
{
    return anim_rig_new(filename, data_buffer, buffer_size, permanent);
}

// 0x00C90CB0
void animlib_reset_all_anim_rig(bool including_perm)
{
    for (anim_rig& rig : Anim_rig) {
        if ((rig.flags & 1) != 0 && (including_perm || (rig.flags & 2) == 0)) {
            rig.clear();
        }
    }
}

// 0x00C90CF0
void animlib_delete_anim_rig(anim_rig* rig)
{
    if (rig != nullptr) {
        rig->clear();
    }
}

// 0x00C98810
int animlib_find_bone_by_substring(const anim_rig* rig, const char* substr)
{
    return rig == nullptr ? -1 : rig->find_bone_by_substring(substr);
}

// 0x00C98830
int animlib_find_bone_by_name(const anim_rig* rig, const char* bone_name)
{
    return rig == nullptr ? -1 : rig->find_bone_by_name(bone_name);
}

// 0x00C98850
int animlib_find_tag_by_name(const anim_rig* rig, const char* tag_name)
{
    return rig == nullptr ? -1 : rig->find_tag_by_name(tag_name);
}

// 0x00C90D10
void animlib_get_bone_tree_list(int aim_handle, int root_bone_index, int max_bones, int& num_bones, int* bone_list)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->rig->get_bone_tree_list(root_bone_index, aim->entity_bone_info->rig_to_entity_bone_mapping, max_bones, num_bones, bone_list);
    }
}

// 0x00C94A10
int animlib_new_anim_instance_manager(anim_rig* rig, int entity_class_id)
{
    return anim_instance_manager_create(rig, entity_class_id);
}

// 0x00C90D70
void animlib_register_entity_base_bone_set(const anim_rig* rig, const animlib_bones_used_info& bones_used_info)
{
    register_entity_base_bone_set(rig, bones_used_info);
}

// 0x00C90D80
void animlib_unregister_entity_base_bone_set(int entity_class_id)
{
    unregister_entity_base_bone_set(entity_class_id);
}

// 0x00C9AEA0
void animlib_delete_anim_instance(int aim_handle)
{
    anim_instance_manager_delete(aim_from_handle(aim_handle));
}

// 0x00C90DB0
anim_rig* animlib_get_rig(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->rig : nullptr;
}

// 0x00C90DF0
void animlib_set_rig(int aim_handle, anim_rig* _rig)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->rig = _rig;
        aim->rig_data_size = _rig->data_size;
    }
}

// 0x00C94A20. Bones with the same name in the parent instance's rig follow that instance (parent_rig_bone_indecies,
// the parent's entity bone).
void animlib_set_parent_rig(int base_aim_handle, int parent_aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(base_aim_handle);
    if (aim == nullptr) {
        return;
    }
    aim->parent_rig_aim_handle = parent_aim_handle;
    anim_instance_manager* parent = aim_from_handle(parent_aim_handle);
    for (int b = 0; b < aim->entity_bone_info->num_bones; b++) {
        const int rig_bone = aim->entity_bone_info->entity_to_rig_bone_mapping[b];
        aim->parent_rig_bone_indecies[b] = -1;
        const anim_rig* parent_rig = parent->rig;
        for (int p = 0; p < parent_rig->num_bones; p++) {
            if (v_stricmp(aim->rig->bones[rig_bone].name.to_pointer(), parent_rig->bones[p].name.to_pointer()) == 0) {
                aim->parent_rig_bone_indecies[b] = parent->entity_bone_info->rig_to_entity_bone_mapping[p];
                break;
            }
        }
    }
}

// 0x00C9AEF0
void animlib_get_bone_velocities(int aim_handle, int num_bones, int* entity_bone_indices, vector* rotational_vel, vector* translational_vel)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_bone_velocities(num_bones, entity_bone_indices, rotational_vel, translational_vel);
    }
}

// 0x00C98870
void animlib_reset_looping_animations(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->clear_looping_animations(true);
        aim->cyclic_percent = 0.0f;
        aim->old_cyclic_percent = 0.0f;
    }
}

// 0x00C94B70
void animlib_clear_looping_animations(int aim_handle, bool immediate)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->clear_looping_animations(immediate);
    }
}

// 0x00C94BB0
void animlib_clear_non_looping_animations(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->clear_non_looping_animations();
    }
}

// 0x00C90E40
void animlib_set_cyclic_percent(int aim_handle, float cyclic_percent)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->cyclic_percent = cyclic_percent;
    }
}

// 0x00C94BF0. Nothing plays on a locked instance. (A negative handle reads through a null instance, as the original.)
unsigned int animlib_play_animation(int aim_handle, int anim_handle, unsigned int flags, float start_time_sec, float base_mix_percent, float speed_multiplier, float ramp_in_sec)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim->locked != 0) {
        return 0xFFFFFFFF;
    }
    return aim->play_animation(anim_handle, flags, start_time_sec, base_mix_percent, ramp_in_sec, speed_multiplier);
}

// 0x00C90EC0
unsigned int animlib_play_animation_looping(int aim_handle, int anim_handle, float base_mix_percent, unsigned int flags, float speed_multiplier, float ramp_in_sec)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim->locked != 0) {
        return 0xFFFFFFFF;
    }
    return aim->play_animation_looping(anim_handle, flags | 1, base_mix_percent, ramp_in_sec, speed_multiplier);
}

// 0x00C988C0
void animlib_stop_animation(int aim_handle, unsigned int handle, float blend_out_time_sec)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr && aim->locked == 0) {
        aim->stop_animation(handle, blend_out_time_sec);
    }
}

// 0x00C94C80
unsigned int animlib_swap_animation(int aim_handle, unsigned int handle, int new_anim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim->locked != 0) {
        return 0xFFFFFFFF;
    }
    return aim->swap_animation(handle, new_anim_handle);
}

// 0x00C90F40. A missing destination is not checked, as the original.
void animlib_sync_anims(int aim_handle_src, int aim_handle_dst, unsigned int src_handle, unsigned int dst_handle)
{
    anim_instance_manager* src = aim_from_handle(aim_handle_src);
    anim_instance_manager* dst = aim_from_handle(aim_handle_dst);
    if (src != nullptr) {
        src->sync_anims(dst, src_handle, dst_handle);
    }
}

// 0x00C9AF40
void animlib_process_instance(int aim_handle, float frametime)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->process_animations(frametime, true);
    }
}

// 0x00C98920
void animlib_process_instance_min(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->remove_inactive_anims();
    }
}

// 0x00C90FC0
int animlib_get_num_active_anims(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->num_active_anims : 0;
}

// 0x00C91000
void animlib_get_bone_transform(int aim_handle, int bone_index, matrix43& transform)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        transform = aim->calculate_final_transformation_matrix(bone_index);
    }
}

// 0x00C91090
void animlib_set_bone_transform(int aim_handle, unsigned int entity_bone_index, const vector& pos, const matrix& orient)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->set_bone_transformation((int)entity_bone_index, pos, orient);
    }
}

// 0x00C9AF90
void animlib_transform_all_bones_pre(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_local_bone_matrices(tmp_matrices, bone_transform_list, bone_lod_level);
    }
}

// 0x00C98960
void animlib_transform_all_bones_post(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->instance_bone_matrices(tmp_matrices, bone_transform_list, bone_lod_level);
        aim->update_vertex_to_model_matrices(bone_transform_list, bone_lod_level);
    }
}

// 0x00C989C0
void animlib_blend_all_morphs(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->blend_all_morphs();
    }
}

// 0x00C94CD0
void animlib_instance_bone_matrices(int aim_handle, matrix43* tmp_matrices, int* bone_transform_list, int bone_lod_level)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->instance_bone_matrices(tmp_matrices, bone_transform_list, bone_lod_level);
    }
}

// 0x00C910E0
void animlib_get_bone_matrices_from_instanced(int aim_handle, matrix43* bone_matrices)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_bone_matrices_from_instanced(bone_matrices);
    }
}

// 0x00C91120
void animlib_set_special_bone_blend_type(int aim_handle, int bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->entity_bone_info->use_special_blend_mode[bone_index] = 1;
    }
}

// 0x00C91170
void animlib_set_bone_transform(int aim_handle, int entity_bone_index, const vector& pos, const matrix& orient)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->set_bone_transformation(entity_bone_index, pos, orient);
    }
}

// 0x00C911C0
void animlib_update_vertex_to_model_matrices(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->update_vertex_to_model_matrices(nullptr, 0);
    }
}

// 0x00C91200
void animlib_invalidate_cache_bone(int aim_handle, int bone_index, bool instanced_model, bool vertex_to_model)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->invalidate_cache_matrix(bone_index, instanced_model, vertex_to_model);
    }
}

// 0x00C91250
void animlib_invalidate_cache_bone_list(int aim_handle, int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->invalidate_cache_matrix_list(num_bones, bone_list, instanced_model, vertex_to_model);
    }
}

// 0x00C912A0
void animlib_validate_cache_bone_list(int aim_handle, int num_bones, int* bone_list, bool instanced_model, bool vertex_to_model)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->validate_cache_matrix_list(num_bones, bone_list, instanced_model, vertex_to_model);
    }
}

// 0x00C912F0. (A negative handle reads through a null instance, as the original.)
bool animlib_allocate_bones(int aim_handle)
{
    return aim_from_handle(aim_handle)->allocate_bone_matrices();
}

// 0x00C91330
void animlib_clear_bone_info(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->clear_bone_info();
    }
}

// 0x00C91390. True when this call allocated them.
bool animlib_allocate_morphs(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr && aim->morph_info == nullptr) {
        aim->allocate_morph_info();
        return true;
    }
    return false;
}

// 0x00C913E0. Magic number: 0xF0 bytes, the 20 morph infos of allocate_morph_info. morph_info is not checked, as the
// original.
void animlib_clear_morph_info(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->num_active_morphs = 0;
        memset(aim->morph_info, 0, 0xF0);
    }
}

// 0x00C91430
int animlib_get_morph_info(int aim_handle, anim_morph_info** morph_info_ptr)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0;
    }
    return aim->get_morph_info(morph_info_ptr);
}

// 0x00C91490
void animlib_add_morph_special_func(int aim_handle, void (*func)(unsigned char&, anim_morph_info*, void*), void* data)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->add_morph_special_func(func, data);
    }
}

// 0x00C914E0
void animlib_disable_instance_bone_special_func(int aim_handle, int bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->disable_instance_bone_special_func(bone_index);
    }
}

// 0x00C98A00
bool animlib_is_bone_animating(int aim_handle, int bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->is_bone_animating(bone_index);
}

// 0x00C91530
vector animlib_get_base_pose_bone_pos(int aim_handle, int entity_bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return ZERO_VECTOR;
    }
    return aim->get_base_pose_bone_pos(entity_bone_index);
}

// 0x00C98A40
bool animlib_is_bone_animating(int aim_handle, unsigned int handle, int bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->is_bone_animating(handle, bone_index);
}

// 0x00C98A90
float animlib_get_bone_animating_fraction(int aim_handle, int bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->get_bone_animating_fraction(bone_index) : 0.0f;
}

// 0x00C915A0. The tag's parent bone in the bind pose plus the tag's offset (tags are numbered after the bones).
vector animlib_get_base_pose_tag_pos(int aim_handle, int entity_tag_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return ZERO_VECTOR;
    }
    const anim_tag& tag = aim->rig->tags[entity_tag_index - aim->rig->num_bones];
    vector pos = aim->get_base_pose_bone_pos(tag.parent_index);
    pos.x = tag.transformation.m_translation.x + pos.x;
    pos.y = tag.transformation.m_translation.y + pos.y;
    pos.z = tag.transformation.m_translation.z + pos.z;
    return pos;
}

// 0x00C9B460
anim_rig* animlib_create_duplicate_rig(int aim_handle, mempool* pool)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->rig->duplicate_rig(pool) : nullptr;
}

// 0x00C91690
void animlib_get_tag_world_coords(int aim_handle, int tag_index, const vector& base_pos, const matrix& base_orient, vector& pos, matrix* orient)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_tag_world_coords(tag_index, base_orient, base_pos, pos, orient);
    }
}

// 0x00C916E0
void animlib_get_tag_local_coords(int aim_handle, int tag_index, vector& pos, matrix* orient)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_tag_local_coords(tag_index, pos, orient);
    }
}

// 0x00C94D20
float animlib_get_animation_length(int aim_handle, int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->anim_length(handle) : 0.0f;
}

// 0x00C91730. Seconds.
float animlib_get_animation_length(int anim_handle)
{
    if (anim_handle > -1 && anim_handle < Num_animations) {
        return (float)anim_end_ticks(anim_handle) / 4800.0f;
    }
    return 0.0f;
}

// 0x00C94D60
float animlib_get_current_time(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0.0f;
    }
    return (float)aim->get_current_ticks(handle) / 4800.0f;
}

// 0x00C91790
void animlib_get_animation_times(int aim_handle, unsigned int handle, int& prev_time, int& cur_time)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_animation_times(handle, prev_time, cur_time);
    }
}

// 0x00C94DC0. The 30 fps frame.
int animlib_get_current_frame(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0;
    }
    return (int)((double)((float)aim->get_current_ticks(handle) / 4800.0f) * 30.0);
}

// 0x00C917E0
float animlib_get_current_mix_percent(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0.0f;
    }
    const int index = handle == 0xFFFFFFFF ? -1 : aim->find_active_anim(handle);
    const anim_instance_info* info = index < 0 ? nullptr : &aim->active_anims[index];
    return info != nullptr ? info->mix_percent : 0.0f;
}

// 0x00C91840. Looping animations run at the instance's cyclic speed.
float animlib_get_current_speed_multiplier(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0.0f;
    }
    const int index = handle == 0xFFFFFFFF ? -1 : aim->find_active_anim(handle);
    const anim_instance_info* info = index < 0 ? nullptr : &aim->active_anims[index];
    if (info == nullptr) {
        return 0.0f;
    }
    if (info->flags & 1) {
        return aim->cyclic_speed;
    }
    return info->speed_multiplier;
}

// 0x00C918C0. 1 while the instance handle is on the instance, else 0 (a float in the original too).
float animlib_anim_is_playing(int aim_handle, int anim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0.0f;
    }
    return aim->find_active_anim((unsigned int)anim_handle) < 0 ? 0.0f : 1.0f;
}

// 0x00C94E20 (not reached).
float animlib_get_time_till_animation_finished(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->time_till_animation_finished(handle) : 0.0f;
}

// 0x00C94E60. A handle that is not playing counts as finished.
bool animlib_is_anim_finished(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return true;
    }
    return aim->time_till_animation_finished(handle) == 0.0f;
}

// 0x00C91950
anim_priority animlib_get_anim_priority(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return AP_PRIORITY_NONE;
    }
    const int index = aim->find_active_anim(handle);
    return index < 0 ? AP_PRIORITY_NONE : aim->active_anims[index].priority;
}

// 0x00C919D0
bool animlib_trigger_has_elapsed(int aim_handle, int trigger_id, unsigned int handle, int trigger_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->trigger_has_elapsed(trigger_id, handle, trigger_index);
}

// 0x00C91A20
bool animlib_trigger_just_elapsed(int aim_handle, int trigger_id, unsigned int handle, int trigger_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->trigger_just_elapsed(trigger_id, handle, trigger_index);
}

// 0x00C91A70
bool animlib_looping_trigger_has_elapsed(int aim_handle, int trigger_id)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->trigger_has_elapsed(trigger_id, 0xFFFFFFFF, -1);
}

// 0x00C91AC0
float animlib_time_till_trigger(int aim_handle, int trigger_id, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->time_till_trigger(trigger_id, handle) : 0.0f;
}

// 0x00C91B10
bool animlib_anim_has_trigger(int aim_handle, int trigger_id, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->has_trigger(trigger_id, handle);
}

// 0x00C91B60. The tick of the trigger_idx-th trigger of the id; -1 when there is none.
int animlib_get_trigger_time(int trigger_id, int anim_handle, int trigger_idx)
{
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        return -1;
    }
    const anim_animation& anim = Animations[anim_handle];
    for (int t = 0; t < anim.num_triggers; t++) {
        if (anim.triggers[t].id == trigger_id) {
            if (trigger_idx == 0) {
                return anim.triggers[t].ticktime;
            }
            trigger_idx--;
        }
    }
    return -1;
}

// 0x00C91BC0. frame is a 30 fps frame.
bool animlib_has_frame_elapsed(int aim_handle, unsigned int handle, int frame)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return false;
    }
    const int index = handle == 0xFFFFFFFF ? -1 : aim->find_active_anim(handle);
    const anim_instance_info* info = index < 0 ? nullptr : &aim->active_anims[index];
    return info != nullptr && frame * 4800 / 30 <= info->cur_time;
}

// 0x00C91C30
void animlib_set_trigger_flags(int aim_handle, unsigned int instance_handle, int trigger_num, bool just_elapsed, bool elapsed, bool suppress_elapsed)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->set_trigger_flags(instance_handle, trigger_num, just_elapsed, elapsed, suppress_elapsed);
    }
}

// 0x00C94EC0
bool animlib_trigger_should_have_elapsed(int aim_handle, unsigned int instance_handle, int trigger_id)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->trigger_should_have_elapsed(instance_handle, trigger_id);
}

// 0x00C91C80
bool animlib_is_holding_last_frame(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr && aim->frozen != 0;
}

// 0x00C91CC0
bool animlib_is_looping(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return false;
    }
    const int index = handle == 0xFFFFFFFF ? -1 : aim->find_active_anim(handle);
    return index > -1 && (aim->active_anims[index].flags & 1) != 0;
}

// 0x00C91D10
bool animlib_anim_is_ramping_out(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return false;
    }
    const int index = handle == 0xFFFFFFFF ? -1 : aim->find_active_anim(handle);
    const anim_instance_info* info = index < 0 ? nullptr : &aim->active_anims[index];
    return info != nullptr && info->ramp.state == ARS_RAMP_OUT;
}

// 0x00C91D60
int animlib_get_anim_handle_from_instance_handle(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return -1;
    }
    const int index = aim->find_active_anim(handle);
    return index > -1 ? aim->active_anims[index].anim_handle : -1;
}

// 0x00C91DF0. Ticks.
void animlib_get_ramp_in_out_times(int anim_handle, int& ramp_in, int& ramp_out)
{
    ramp_in = 0;
    ramp_out = 0;
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        return;
    }
    const anim_file* amf = Animations[anim_handle].animation_data;
    if (amf != nullptr) {
        ramp_in = (int)(((double)amf->ramp_in / 30.0) * 4800.0);
        ramp_out = (int)(((double)amf->ramp_out / 30.0) * 4800.0);
    }
}

// 0x00C98AD0
int animlib_find_character_tag(int aim_handle, const char* name)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->find_tag(name) : -1;
}

// 0x00C98B10
int animlib_find_nth_character_tag(int aim_handle, const char* name, int num_tag)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->find_nth_tag(name, num_tag) : -1;
}

// 0x00C98B60
int animlib_find_character_tag_with_active_vids(int aim_handle, const char* name, int num_active_vids, const int* active_vids)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->find_tag_with_active_vids(name, num_active_vids, active_vids) : -1;
}

// 0x00C98BB0
int animlib_find_character_bone(int aim_handle, const char* name)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->find_bone(name) : -1;
}

// 0x00C9AFE0
void animlib_get_position_change(vector& action_delta_pos, vector& state_delta_pos, vector& anim_local_delta_pos, int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->get_position_change(action_delta_pos, state_delta_pos, anim_local_delta_pos);
    }
}

// 0x00C9B030
float animlib_get_heading_change(int aim_handle, int future_ticks)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->get_heading_change(future_ticks) : 0.0f;
}

// 0x00C9B070
float animlib_get_root_heading(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->get_root_heading() : 0.0f;
}

// 0x00C98BF0. The root translation between two ticks (-1: the start / the end; both -1: the file's total).
void animlib_get_total_translation(int anim_handle, vector& translation, int start_time, int end_time)
{
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        translation.zero();
        return;
    }
    anim_animation& anim = Animations[anim_handle];
    if (start_time == -1) {
        if (end_time == -1) {
            translation.zero();
            if (anim.animation_data == nullptr) {
                translation.zero();
                return;
            }
            translation = anim.animation_data->total_translation;
            return;
        }
        start_time = 0;
    }
    if (end_time == -1) {
        if (anim.animation_data != nullptr) {
            anim.get_delta_pos_unrotated(translation, start_time, anim.animation_data->end_tock * 80);
            return;
        }
        end_time = 0;
    }
    anim.get_delta_pos_unrotated(translation, start_time, end_time);
}

// 0x00C91E70. The heading of the file's total rotation, negated.
float animlib_get_total_rotation(int anim_handle)
{
    if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
        return 0.0f;
    }
    const quaternion q = Animations[anim_handle].animation_data->total_rotation;
    matrix m;
    q.extract_matrix(m);
    return -m.extract_heading();
}

// 0x00C91EF0
void animlib_set_speed_multiplier(int aim_handle, unsigned int handle, float speed_multiplier)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return;
    }
    animlib_trace_value(aim_handle, "set_speed_multiplier", speed_multiplier, _ReturnAddress());
    const int index = aim->find_active_anim(handle);
    if (index > -1) {
        aim->active_anims[index].speed_multiplier = speed_multiplier;
    }
}

// 0x00C91F70
void animlib_set_looping_speed_multiplier(int aim_handle, float speed_multiplier)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    animlib_trace_value(aim_handle, "set_looping_speed_multiplier", speed_multiplier, _ReturnAddress());
    if (aim != nullptr) {
        aim->cyclic_speed = speed_multiplier;
    }
}

// 0x00C91FB0
void animlib_set_start_frame(int aim_handle, unsigned int handle, int start_frame)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->set_start_frame(handle, start_frame);
    }
}

// 0x00C94F10
void animlib_set_animation_time(int aim_handle, unsigned int handle, float new_time)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->set_current_time(handle, new_time);
    }
}

// 0x00C92000
void animlib_set_looping_anim_start_percent(int aim_handle, unsigned int handle, float init_cyclic_percent)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    animlib_trace_value(aim_handle, "set_looping_anim_start_percent", init_cyclic_percent, _ReturnAddress());
    if (aim != nullptr) {
        aim->set_looping_anim_start_percent(handle, init_cyclic_percent);
    }
}

// 0x00C92050
float animlib_get_init_cyclic_percent(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0.0f;
    }
    const int index = aim->find_active_anim(handle);
    return index < 0 ? 0.0f : aim->active_anims[index].init_cyclic_percent;
}

// 0x00C920E0
void animlib_set_init_cyclic_percent(int aim_handle, unsigned int handle, float init_cyclic_percent)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    animlib_trace_value(aim_handle, "set_init_cyclic_percent", init_cyclic_percent, _ReturnAddress());
    if (aim != nullptr) {
        aim->set_init_cyclic_percent(handle, init_cyclic_percent);
    }
}

// 0x00C92130
int animlib_get_hold_animation_handle(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr || aim->animation_to_freeze_sig == -1) {
        return -1;
    }
    return aim->animation_to_freeze_sig << 4;
}

// 0x00C92180. The bone's inverse bind translation (the rotation is the identity).
matrix43 animlib_get_bone_inv_transformation(int aim_handle, int entity_bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr || entity_bone_index < 0 || entity_bone_index >= aim->entity_bone_info->num_bones) {
        return IDENTITY_MATRIX43;
    }
    matrix m;
    m.make_identity();
    return matrix43(aim->rig->bones[aim->entity_bone_info->entity_to_rig_bone_mapping[entity_bone_index]].inv_translation, m);
}

// 0x00C922C0
int animlib_get_bone_parent_index(int aim_handle, int entity_bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->get_parent_bone_index(entity_bone_index) : -1;
}

// 0x00C92300
int animlib_get_num_bones(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->entity_bone_info->num_bones : 0;
}

// 0x00C92340
int animlib_get_num_virtual_bones(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->rig->num_virtual_bones : 0;
}

// 0x00C92380. The most bones any animation with some mix drives on this entity; 0 as soon as one of them has no data.
int animlib_get_num_animating_bones(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0;
    }
    const int entity_bones = aim->entity_bone_info->num_bones;
    int result = 0;
    for (int i = 0; i < aim->num_active_anims; i++) {
        if (0.0f < aim->active_anims[i].mix_percent) {
            const anim_animation* anim = get_animation_from_handle(aim->active_anims[i].anim_handle);
            if (anim == nullptr || anim->animation_data == nullptr) {
                return 0;
            }
            const int bones = anim->animation_data->num_bones < entity_bones ? anim->animation_data->num_bones : entity_bones;
            if (result <= bones) {
                result = bones;
            }
        }
    }
    return result;
}

// 0x00C92430
int animlib_get_num_animating_bones(int aim_handle, unsigned int handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return 0;
    }
    const int index = aim->find_active_anim(handle);
    if (index < 0) {
        return 0;
    }
    const int anim_handle = aim->active_anims[index].anim_handle;
    if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
        return 0;
    }
    const unsigned int entity_bones = (unsigned int)aim->entity_bone_info->num_bones;
    const unsigned int bones = Animations[anim_handle].animation_data->num_bones;
    return (int)(entity_bones < bones ? entity_bones : bones);
}

// 0x00C92500
bool animlib_get_instanced_model_matrices(int aim_handle, matrix43** matrices)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return false;
    }
    *matrices = aim->cache_instanced_model;
    return aim->cache_instanced_model_valid;
}

// 0x00C92550
void animlib_validate_instanced_model_matrices(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->cache_instanced_model_valid = true;
    }
}

// 0x00C92590
matrix43* animlib_get_vertex_to_model_matrices(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->cache_vertex_to_model : nullptr;
}

// 0x00C925D0
matrix43* animlib_get_render_matrices(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? aim->cache_vertex_to_model : nullptr;
}

// 0x00C92610
void animlib_copy_render_matrices(int aim_handle, matrix43* dest)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->copy_render_matrices(dest);
    }
}

// 0x00C92650. The bone's bind translation relative to its parent.
// Original bug kept: the index check is (index >= 0 || index < num_bones), so only the instance is checked.
vector animlib_get_bone_rel_translation(int aim_handle, int entity_bone_index)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr || (entity_bone_index < 0 && entity_bone_index >= aim->entity_bone_info->num_bones)) {
        return ZERO_VECTOR;
    }
    return aim->rig->bones[aim->entity_bone_info->entity_to_rig_bone_mapping[entity_bone_index]].rel_bone_translation;
}

// 0x00C926F0. Attaches loaded data to an animation slot (once; a permanent one stays through
// animlib_reset_all_anims). The morph table offset becomes self-relative.
int animlib_overlay_anim(int anim_handle, void* data_block, unsigned int data_block_size, bool is_permanent)
{
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        return -1;
    }
    anim_animation& anim = Animations[anim_handle];
    const unsigned int flags = anim.flags;
    if ((flags & 1) == 0 && anim.animation_data == nullptr) {
        anim.data_size = data_block_size;
        anim.animation_data = (anim_file*)data_block;
        if (is_permanent) {
            anim.flags = flags | 1;
        }
        unsigned char* data = (unsigned char*)data_block;
        int* morph_offset = (int*)(data + 0x34);
        if ((data[5] & 0x10) != 0 && data + 0x30 != nullptr && *morph_offset != -1) {
            *morph_offset = *morph_offset - (int)(data + 0x30) - 4 + (int)data;
        }
    }
    return anim_handle;
}

// 0x00C92760
int animlib_overlay_anim(const char* filename, void* data_block, unsigned int data_block_size, bool is_permanent)
{
    return animlib_overlay_anim(animlib_find_anim(filename), data_block, data_block_size, is_permanent);
}

// 0x00C92780. A permanent animation (flags bit 0) stays.
void animlib_unload_anim(int anim_handle)
{
    if (anim_handle > -1 && anim_handle < Num_animations && (Animations[anim_handle].flags & 1) == 0) {
        anim_animation& anim = Animations[anim_handle];
        animlib_remove_anim_from_lookup_table(anim.anim_filename);
        anim.anim_filename = nullptr;
        anim.anim_filename_hash = 0;
        anim.animation_data = nullptr;
    }
}

// 0x00C927D0
void animlib_unload_anim(const char* filename)
{
    animlib_unload_anim(animlib_find_anim(filename));
}

// 0x00C92820. Forgets streamed (not permanent, flags bit 0) animation data.
void animlib_unload_anim_data(int anim_handle)
{
    if (anim_handle > -1 && anim_handle < Num_animations && (Animations[anim_handle].flags & 1) == 0) {
        Animations[anim_handle].animation_data = nullptr;
    }
}

// 0x00C92850
void animlib_unload_anim_data(const char* filename)
{
    animlib_unload_anim_data(animlib_find_anim(filename));
}

// 0x00C92890
bool animlib_is_paged_in(int anim_handle)
{
    if (anim_handle > -1 && anim_handle < Num_animations) {
        return Animations[anim_handle].animation_data != nullptr;
    }
    return false;
}

// 0x00C928C0
bool animlib_get_anim_filename(int anim_handle, char* filename, int max_len)
{
    filename[0] = '\0';
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        return false;
    }
    strncpy(filename, Animations[anim_handle].anim_filename, max_len - 1);
    filename[max_len - 1] = '\0';
    return true;
}

// 0x00C92910
void animlib_set_anim_flag(int anim_handle, unsigned int flag)
{
    if (anim_handle > -1 && anim_handle < Num_animations) {
        Animations[anim_handle].flags |= flag;
    }
}

// 0x00C92940
void animlib_clear_anim_flag(int anim_handle, unsigned int flag)
{
    if (anim_handle > -1 && anim_handle < Num_animations) {
        Animations[anim_handle].flags &= ~flag;
    }
}

// 0x00C92970. The instance handles of the playing animations, at most num_handles. Magic number: 10, the active
// animations of an instance (ANIM_MAX_ACTIVE).
bool animlib_get_active_animations(unsigned int* anim_handles, int& num_active, int num_handles, int aim_handle)
{
    if (num_handles < 0 || num_handles > 10) {
        return false;
    }
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim == nullptr) {
        return false;
    }
    num_active = 0;
    for (int i = 0; i < aim->num_active_anims; i++) {
        if (num_handles <= num_active) {
            return true;
        }
        anim_handles[num_active] = compose_anim_instance_handle(aim->active_anims[i].signature);
        num_active++;
    }
    return true;
}

// 0x00C92A10
int animlib_get_anim_flags(int aim_handle, int anim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    return aim != nullptr ? (int)aim->get_anim_flags((unsigned int)anim_handle) : 0;
}

// 0x00C92A60. The file's total translation over its length. Magic numbers: a tock is 80 ticks, 4800 ticks a second.
void animlib_get_animation_vel(vector& vel, int anim_handle)
{
    vel.zero();
    if (anim_handle < 0 || anim_handle >= Num_animations) {
        return;
    }
    const anim_file* amf = Animations[anim_handle].animation_data;
    const int ticks = amf == nullptr ? 0 : amf->end_tock * 80;
    const float length = (float)ticks / 4800.0f;
    if (0.0f < length) {
        vel.zero();
        if (amf == nullptr) {
            vel.zero();
        } else {
            vel = amf->total_translation;
        }
        const float scale = 1.0f / length;
        vel.x = vel.x * scale;
        vel.y = vel.y * scale;
        vel.z = vel.z * scale;
    }
}

// 0x00C92B60. More than sqrt(0.2) m of total root movement.
bool animlib_is_moving_animation(int anim_handle)
{
    if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
        return false;
    }
    const vector& t = Animations[anim_handle].animation_data->total_translation;
    return (double)0.2f < (double)t.y * (double)t.y + (double)t.x * (double)t.x + (double)t.z * (double)t.z; // __real@3fc99999a0000000
}

// 0x00C98CA0
void animlib_get_animation_vel(vector& vel, int anim_handle, int start_time, int end_time)
{
    vel.zero();
    if (anim_handle > -1 && anim_handle < Num_animations) {
        Animations[anim_handle].get_velocity(vel, start_time, end_time);
    }
}

// 0x00C92BD0
const char* animlib_get_animation_name(int anim_handle)
{
    if (anim_handle > -1 && anim_handle < Num_animations) {
        return Animations[anim_handle].anim_filename;
    }
    return nullptr;
}

// 0x00C92BF0. Seconds; the file's ramp is in 30 fps frames, truncated to ticks.
float animlib_get_animation_ramp_in_length(int anim_handle)
{
    if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
        return 0.0f;
    }
    const int ticks = (int)(((double)Animations[anim_handle].animation_data->ramp_in / 30.0) * 4800.0);
    return (float)((double)ticks / 4800.0);
}

// 0x00C92C70. Seconds, as animlib_get_animation_ramp_in_length.
float animlib_get_animation_ramp_out_length(int anim_handle)
{
    if (anim_handle < 0 || anim_handle >= Num_animations || Animations[anim_handle].animation_data == nullptr) {
        return 0.0f;
    }
    const int ticks = (int)(((double)Animations[anim_handle].animation_data->ramp_out / 30.0) * 4800.0);
    return (float)((double)ticks / 4800.0);
}

// 0x00C92CF0
void animlib_clear_trigger_just_elapsed_flags(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->clear_trigger_just_elapsed_flags();
    }
}

// 0x00C92D50
void animlib_enable_scaling(int aim_handle, vector& scale)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->do_bone_scaling = true;
        aim->bone_scaling = scale;
    }
}

// 0x00C92DB0
void animlib_get_bone_bbox(int aim_handle, vector& bmin, vector& bmax)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        bmin = aim->bone_bbox_min;
        bmax = aim->bone_bbox_max;
    }
}

// 0x00C98CE0
void animlib_allocate_bone_modifiers(int aim_handle)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->allocate_bone_modifiers();
    }
}

// 0x00C98D40
void animlib_register_bone_modifier(int aim_handle, bone_modifier modifier, int bone_index, int storage_idx, matrix& delta_orient, matrix* old_bone_orient, float blend_pct)
{
    anim_instance_manager* aim = aim_from_handle(aim_handle);
    if (aim != nullptr) {
        aim->register_bone_modifier(modifier, bone_index, storage_idx, delta_orient, old_bone_orient, blend_pct);
    }
}

// 0x00C9B150; LTCG: the command in ESI (arg1 in the job).
void animlib_process_command_xbox_pc(cpu_job* lj, cpu_job_arg arg1, cpu_job_arg arg2)
{
    animlib_process_command((animlib_transform_command*)arg1.pointer_val);
}

// 0x00C9B160. The queued commands: scheduler jobs for the multi-threaded ones, then the cpu_processing ones here.
void animlib_batch_process_all_start()
{
    Anim_scheduler_trigger = scheduler_get_event_trigger();
    cpu_job job;
    for (int i = 0; i < Num_animlib_transform_commands; i++) {
        animlib_transform_command* command = &Animlib_transform_command_buffer[i];
        if (!command->cpu_processing) {
            job.job_func = animlib_process_command_xbox_pc;
            job.type = ccjt_sim;
            job.arg1.pointer_val = command;
            scheduler_add_job(nullptr, &job, Anim_scheduler_trigger);
        }
    }
    for (int i = 0; i < Num_animlib_transform_commands; i++) {
        animlib_transform_command* command = &Animlib_transform_command_buffer[i];
        if (command->cpu_processing) {
            animlib_process_command(command);
        }
    }
}
