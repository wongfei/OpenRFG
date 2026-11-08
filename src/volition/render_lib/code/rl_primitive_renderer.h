// Original: volition/render_lib/code/rl_primitive_renderer.h
// Implementation: volition/render_lib/code/rl_primitive_renderer.cpp
//
// rl_primitive_renderer, the singleton that draws immediate 2D/3D primitives (lines, quads, text) with macro states.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/vlib/code/os/sync_event.h"

class rl_class_dynamic_info;
class rl_material;
class rl_primitive_renderer_state;
class rl_renderer;
class rl_state_manager;

// size 0x134
class rl_primitive_renderer : public rl_base_object, public rl_singleton<rl_primitive_renderer>
{
protected:
    rl_primitive_renderer(); // 0x00514350 (:363)
    virtual ~rl_primitive_renderer(); // vtbl+0x00 0x0052C790 (:371)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_primitive_renderer* create(); // 0x0052C750 (:346)
    static bool can_create(); // 0x004C4530 (:346)
    virtual void release(); // vtbl+0x04 0x004C4540 (:346)
    virtual void initialize(); // vtbl+0x08 0x00543040 (:426)
    void set_max_buffered_commands(int max_buffered_commands); // 0x004C45D0 (:485)
    void set_max_buffered_state_size(int max_buffered_state_size); // 0x004C45E0 (:493)
    void set_max_buffered_data_size(int max_buffered_data_size); // 0x004C45F0 (:501)
    void set_max_immediate_data_size(int max_immediate_data_size); // 0x004C4600 (:509)
    void begin_primitive(rl_primitive_render_type prim_type, const rl_primitive_state& state); // 0x004C4610 (:524)
    unsigned char* allocate_verts(int num_verts, rl_state_manager& state); // 0x005505C0 (:633)
    void end_primitive(rl_state_manager& state, rl_renderer* pRenderer); // 0x00550480 (:566)
    void begin_deferred(); // 0x004F0ED0 (:735)
    void end_deferred(); // 0x004F0F30 (:753)
    void playback_deferred(rl_renderer& renderer); // 0x00550830 (:769)
    void set_mode(rl_primitive_renderer_mode new_mode); // 0x004C46A0 (:826)
    rl_primitive_renderer_mode get_mode() const; // 0x00AE7870 (.h:112)
    void set_thread_is_virtual(bool is_virtual); // 0x004F0F80 (:832)
    void initialize_for_thread(); // 0x004F1000 (:845)
    void deinitialize_for_thread(); // 0x004C46C0 (:861)
private:
    unsigned char* allocate_data_space(int size, int& offset, bool wait_for_data, bool); // 0x004C4700 (:877)
    void render_primitive(rl_renderer& renderer, rl_primitive_render_type prim_type, int data_offset, int num_prims, int num_data_segments, bool was_deferred); // 0x0054C9F0 (:944)
    void record_draw_command(rl_primitive_render_type prim_type, int data_offset, int num_prims, int num_data_segments, rl_state_manager& state); // 0x005142C0 (:261)
    void record_command(const rl_buffered_command& command); // 0x004F0D00 (:298)
    void record_state_command(rl_state_manager& state); // 0x00514180 (:156)
    void restore_state(rl_renderer& renderer, int data_offset, int data_size); // 0x0054C980 (:239)
    void record_flush_command(int deferred_block_index, int num_deferred_blocks); // 0x00514310 (:281)
    void get_next_buffered_command(rl_buffered_command& command); // 0x004F0DC0 (:325)
    void create_materials(); // 0x00542F70 (:145)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514FF8
private:
    int m_max_buffered_commands; // 0x0C
    int m_max_buffered_state_size; // 0x10
    int m_max_buffered_data_size; // 0x14
    int m_max_immediate_data_size; // 0x18
    bool m_is_initialized; // 0x1C
    volatile unsigned int m_cb_write_index; // 0x20
    volatile unsigned int m_cb_read_index; // 0x24
    pool_list<rl_buffered_command> m_cb; // 0x28
    unsigned char* m_sb_write_ptr; // 0x40
    volatile unsigned char* m_sb_read_ptr; // 0x44
    volatile int m_sb_num_segments; // 0x48
    unsigned char* m_sb; // 0x4C
    volatile int m_num_deferred_frames; // 0x50
    static rl_primitive_renderer_state* ms_main_state; // 0x01B7BCF4
    sync_event m_command_event; // 0x54
    int m_imm_db_write_offset; // 0x58
    unsigned char* m_imm_db; // 0x5C
    int m_buff_db_write_offset; // 0x60
    volatile int m_buff_db_read_offset; // 0x64
    volatile unsigned int m_buff_db_num_segments; // 0x68
    unsigned char* m_buff_db; // 0x6C
    rl_material_data m_primitive_2d_material_data; // 0x70
    rl_material* m_primitive_2d_material; // 0x8C
    unsigned int m_primitive_2d_material_handle; // 0x90
    rl_material_data m_primitive_2d_tex_material_data; // 0x94
    rl_material* m_primitive_2d_tex_material; // 0xB0
    unsigned int m_primitive_2d_tex_material_handle; // 0xB4
    rl_material_data m_primitive_3d_material_data; // 0xB8
    rl_material* m_primitive_3d_material; // 0xD4
    unsigned int m_primitive_3d_material_handle; // 0xD8
    rl_material_data m_primitive_3d_tex_material_data; // 0xDC
    rl_material* m_primitive_3d_tex_material; // 0xF8
    unsigned int m_primitive_3d_tex_material_handle; // 0xFC
    rl_material_data m_primitive_2d_binked_material_data; // 0x100
    rl_material* m_primitive_2d_binked_material; // 0x11C
    unsigned int m_primitive_2d_binked_material_handle; // 0x120
    rl_material_const m_null_const; // 0x124
};

// size 0x38
class rl_primitive_renderer_state : public rl_base_object
{
protected:
    rl_primitive_renderer_state(); // 0x004C4590 (:350)
    virtual ~rl_primitive_renderer_state(); // vtbl+0x00 0x004C45C0 (:356)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_primitive_renderer_state* create(); // 0x004F0E50 (:347)
    static bool can_create(); // 0x004C4560 (:347)
    virtual void release(); // vtbl+0x04 0x004C4570 (:347)

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01515010
    bool m_begin_deferred_called; // 0x08
    rl_primitive_renderer_mode m_mode; // 0x0C
    int m_num_queued_verts; // 0x10
    int m_num_data_segments; // 0x14
    int m_data_offset; // 0x18
    int m_end_of_last_alloc; // 0x1C
    rl_primitive_render_type m_prim_type; // 0x20
    int m_prim_vert_size; // 0x24
    rl_primitive_state m_prim_state; // 0x28
    const unsigned char* m_pending_stitch_read; // 0x2C
    unsigned char* m_pending_stitch_write; // 0x30
    rl_primitive_state m_cur_macro_state; // 0x34
};

void create_material(unsigned int& mat_handle, rl_material** mat_ptr, rl_material_data& mat_data, const char* shader_name, rl_material_const* pConst); // 0x00542EA0 (:124)
