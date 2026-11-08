// Original: volition/rfg/code/camera/camera_cutscene.cpp
//
// The cutscene camera mode: plays the shots of a .csc_pc camera script. The script file is read into the pool as is, and
// its file offsets are turned into self-relative ones in place.

#include "volition/rfg/code/camera/camera_cutscene.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

// 0x006D1190 (:92)
void camera_cs_process(float step_time, bool last_step_of_frame)
{
    const c_cutscene_camera* const script = Rfg_camera.cscript_params.camera_script;
    if (script == nullptr)
    {
        camera_pop_mode(CAMERA_CUTSCENE_SCRIPT_MODE);
        return;
    }
    const c_cutscene_shot& shot = script->csc_shots[cutscene_current_is_first_shot_processed() ? cutscene_get_cur_shot() : 0];
    // The keys are looked up by the time since the cutscene began, not since the shot began.
    const float time = Cutscene_timer.get_cutscene_elapsed_sec();
    // Magic number: radians to degrees.
    Rfg_camera.ideal_fov = shot.fov_keys.interp_linear(time) * 57.295776f;
    Rfg_camera.ideal_pos = shot.translation_keys.interp_linear(time);
    Rfg_camera.ideal_pos.unrotate(Rfg_camera.cscript_params.base_orient);
    Rfg_camera.ideal_pos.y = Rfg_camera.ideal_pos.y + Rfg_camera.cscript_params.base_pos.y;
    Rfg_camera.ideal_pos.x = Rfg_camera.cscript_params.base_pos.x + Rfg_camera.ideal_pos.x;
    Rfg_camera.ideal_pos.z = Rfg_camera.ideal_pos.z + Rfg_camera.cscript_params.base_pos.z;
    matrix rotation = shot.rotation_keys.interp_slerp(time);
    Rfg_camera.ideal_orient = rotation * Rfg_camera.cscript_params.base_orient;
    Rfg_camera.ideal_orient.fvec.normalize();
    if (cutscene_is_playing())
    {
        cutscene_transform_point_for_camera(Rfg_camera.ideal_pos, Rfg_camera.ideal_pos);
        cutscene_rotate_orient(Rfg_camera.ideal_orient, Rfg_camera.ideal_orient);
    }
}

// A file offset to an offset from the field itself; -1 is no data. rfg.exe writes each one out.
template<typename T, int NULLABLE>
static void camera_script_relocate(et_ptr_offset<T, NULLABLE>& field, const void* const script)
{
    const int file_offset = field.get_offset();
    if (file_offset != -1)
    {
        field.set_offset(file_offset - (int)((const char*)&field - (const char*)script));
    }
}

template<typename KEYLIST>
static void camera_script_relocate_float_keys(KEYLIST& keys, const void* const script)
{
    camera_script_relocate(keys.key_times, script);
    camera_script_relocate(keys.key_vals, script);
}

// 0x006D1360 (:197)
void camera_cutscene_load_scene(const char* filename, mempool_base* pool)
{
    char base_name[68];
    char file_name[68];
    // Deviation: a stack buffer where rfg.exe prints to its thread-local one.
    char message[256];
    cf_drop_ext(base_name, 0x40, filename);
    cf_add_ext(file_name, 0x40, base_name, ".csc_pc", false);
    cfile* const file = cf_open(file_name, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        _snprintf_safe(message, sizeof(message), "Failed opening file %s", filename);
        keen::debug::nativePrintArgs(message);
        return;
    }
    const unsigned int space_free = pool->space_free();
    const unsigned int size = cf_size(file);
    if (space_free < size)
    {
        _snprintf_safe(message, sizeof(message), "%s is larger than the designated mempool, cannot load", file_name);
        keen::debug::nativePrintArgs(message);
        cf_close(file);
        return;
    }
    c_cutscene_camera* const script = (c_cutscene_camera*)pool->alloc(size, 4);
    cf_read(script, size, file, false);
    cf_close(file);

    camera_script_relocate(script->csc_shots, script);
    Rfg_camera.cscript_params.camera_script = script;
    for (int i = 0; i < script->csc_num_shots; i++)
    {
        c_cutscene_shot& shot = script->csc_shots[i];
        camera_script_relocate(shot.translation_keys.key_times, script);
        camera_script_relocate(shot.translation_keys.key_positions, script);
        camera_script_relocate(shot.rotation_keys.key_times, script);
        camera_script_relocate(shot.rotation_keys.key_rotations, script);
        camera_script_relocate_float_keys(shot.fov_keys, script);
        camera_script_relocate_float_keys(shot.blur_keys, script);
        camera_script_relocate_float_keys(shot.near_plane_keys, script);
        camera_script_relocate_float_keys(shot.focus_start_plane_keys, script);
        camera_script_relocate_float_keys(shot.focus_end_plane_keys, script);
        camera_script_relocate_float_keys(shot.far_plane_keys, script);
    }
}

// 0x006C9A70 (:280)
void camera_start_cutscene_mode()
{
    Rfg_camera.cscript_params.base_pos.zero();
    Rfg_camera.cscript_params.base_orient.make_identity();
    Rfg_camera.cscript_params.flags = 0;
    camera_push_mode(CAMERA_CUTSCENE_SCRIPT_MODE);
}

// 0x006C9AA0 (:296)
void camera_stop_cutscene_mode()
{
    Rfg_camera.cscript_params.camera_script = nullptr;
    camera_pop_mode(CAMERA_CUTSCENE_SCRIPT_MODE);
}
