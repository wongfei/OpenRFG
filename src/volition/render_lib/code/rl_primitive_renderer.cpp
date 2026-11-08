// Original: volition/render_lib/code/rl_primitive_renderer.cpp
//
// rl_primitive_renderer: immediate 2D/3D primitives (lines, strips, textured and bink quads). Each thread has an
// rl_primitive_renderer_state (RL_primitive_renderer_state). In immediate mode the vertices go to m_imm_db and are
// drawn by end_primitive; in deferred mode (the simulation thread) they go to the ring buffer m_buff_db with draw and
// state commands in m_cb/m_sb, and the render thread replays them with playback_deferred.

#include "volition/render_lib/code/rl_primitive_renderer.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "keen/base/trace.hpp"

#include <string.h>
#include <windows.h>

// The primitive renderer state of the current thread (TLS 0x14).
thread_local rl_primitive_renderer_state* RL_primitive_renderer_state = nullptr; // 0x03BAE014

rl_primitive_renderer_state* rl_primitive_renderer::ms_main_state = nullptr; // 0x01B7BCF4

// The id of the next buffered command.
static unsigned int Cur_ID; // 0x01CD271C

// Per rl_primitive_render_type: the draw type, vertex stride and vertex format, and the material handle (set by
// initialize; 0xFFFFFFFF for the material strips, which use the renderer's primitive material).
static const rl_primitive_render_type Prim_type_lookup[NUM_RL_PRIMITIVE_RENDER_TYPES] = {
    RLPRT_LINELIST_2D,
    RLPRT_TEXTURED_LINELIST_2D,
    RLPRT_LINELIST_3D,
    RLPRT_TEXTURED_LINELIST_3D,
    RLPRT_TRISTRIP_2D,
    RLPRT_TEXTURED_TRISTRIP_2D,
    RLPRT_TRISTRIP_3D,
    RLPRT_TEXTURED_TRISTRIP_3D,
    RLPRT_BINKED_TRISTRIP_2D,
    RLPRT_MATERIAL_TRISTRIP_2D,
    RLPRT_MATERIAL_TRISTRIP_3D,
}; // 0x014FA320
// Magic numbers: rl_primitive_2d_vert is 0x1C bytes, rl_primitive_3d_vert 0x18.
static const unsigned int Vert_stride_lookup[NUM_RL_PRIMITIVE_RENDER_TYPES] = {0x1C, 0x1C, 0x18, 0x18, 0x1C, 0x1C, 0x18, 0x18, 0x1C, 0x1C, 0x18}; // 0x014FA34C
static const rl_dev_vertex_format Vert_decl_lookup[NUM_RL_PRIMITIVE_RENDER_TYPES] = {
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_3D,
    RLVF_PRIMITIVE_3D,
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_3D,
    RLVF_PRIMITIVE_3D,
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_2D,
    RLVF_PRIMITIVE_3D,
}; // 0x014FA378
static unsigned int Mat_lookup[NUM_RL_PRIMITIVE_RENDER_TYPES]; // 0x01B6EBCC

// The size of a serialized rl_state_manager primitive state (rl_state_manager::serialize_primitive_render_state).
static const int Serialized_state_size = 0xE0;

// :346
// <rl_primitive_renderer::release> 0x004C4540 (:346)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_primitive_renderer)
// :347
// <rl_primitive_renderer_state::release> 0x004C4570 (:347)
// <rl_primitive_renderer_state::create> 0x004F0E50 (:347)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_primitive_renderer_state)

// 0x00542EA0 (:124)
// Creates and registers a primitive material on a shader with no constants and textures.
// Notes (decompiled behaviour, kept as is): pConst becomes the constant block although num_constants is 0.
void create_material(unsigned int& mat_handle, rl_material** mat_ptr, rl_material_data& mat_data, const char* shader_name, rl_material_const* pConst)
{
    mat_data.shader_handle = rl_shader_library::get_instance()->lookup(shader_name);
    if (mat_data.shader_handle == 0xFFFFFFFF)
    {
        return;
    }
    mat_data.num_textures = 0;
    mat_data.textures.set_offset(-1);
    mat_data.num_constants = 0;
    mat_data.constant_block.from_pointer(pConst);
    // Magic number: material flag 0x10 marks primitive materials (rl_renderer::set_material keeps the alpha test).
    mat_data.mat_flags |= 0x10;
    mat_data.max_constants = 0;
    *mat_ptr = rl_material::create();
    (*mat_ptr)->set_material_data(&mat_data);
    mat_handle = rl_material_library_register(*mat_ptr);
}

// 0x00542F70 (:145)
void rl_primitive_renderer::create_materials()
{
    rl_material_const* pConst = &m_null_const;
    create_material(m_primitive_2d_material_handle, &m_primitive_2d_material, m_primitive_2d_material_data, "rl_prim_2d", pConst);
    create_material(m_primitive_2d_tex_material_handle, &m_primitive_2d_tex_material, m_primitive_2d_tex_material_data, "rl_prim_2d_tex", pConst);
    create_material(m_primitive_3d_material_handle, &m_primitive_3d_material, m_primitive_3d_material_data, "rl_prim_3d", pConst);
    create_material(m_primitive_3d_tex_material_handle, &m_primitive_3d_tex_material, m_primitive_3d_tex_material_data, "rl_prim_3d_tex", pConst);
    create_material(m_primitive_2d_binked_material_handle, &m_primitive_2d_binked_material, m_primitive_2d_binked_material_data, "rl_prim_2d_bink", pConst);
}

// 0x00514180 (:156)
// Serializes the render state into the state ring buffer m_sb and records a RLBC_SET_STATE command. A full buffer
// drops all buffered state.
void rl_primitive_renderer::record_state_command(rl_state_manager& state)
{
    if (RL_primitive_renderer_state == nullptr)
    {
        return;
    }
    int size = 0;
    unsigned char* read_ptr;
    // Wait while the buffer is full (read == write with segments pending).
    do
    {
        read_ptr = (unsigned char*)m_sb_read_ptr;
    } while (read_ptr == m_sb_write_ptr && m_sb_num_segments != 0);
    unsigned char* write_ptr = m_sb_write_ptr;
    if (write_ptr < read_ptr)
    {
        const int available = (int)(read_ptr - 1 - write_ptr);
        if (available < Serialized_state_size || !state.serialize_primitive_render_state(write_ptr, size, available))
        {
            keen::debug::nativePrintArgs("[primitive renderer] WARNING! Filled all of our command buffer within a single frame; this probabnly isn't good.\n");
            InterlockedExchange((volatile LONG*)&m_sb_num_segments, 0);
            m_sb_read_ptr = m_sb_write_ptr;
            return;
        }
    }
    else
    {
        const int available = (int)(m_sb + m_max_buffered_state_size - write_ptr);
        if (available < Serialized_state_size || !state.serialize_primitive_render_state(write_ptr, size, available))
        {
            // Wrap to the start of the buffer.
            const int wrapped_available = (int)(read_ptr - 1 - m_sb);
            if (wrapped_available < Serialized_state_size || !state.serialize_primitive_render_state(m_sb, size, wrapped_available))
            {
                keen::debug::nativePrintArgs("[primitive renderer] WARNING! Filled all of our command buffer within a single frame; this probabnly isn't good.\n");
                InterlockedExchange((volatile LONG*)&m_sb_num_segments, 0);
                m_sb_write_ptr = m_sb;
                return;
            }
            write_ptr = m_sb;
        }
    }
    InterlockedIncrement((volatile LONG*)&m_sb_num_segments);
    rl_buffered_command command;
    command.data_offset = (int)(write_ptr - m_sb);
    command.prim_count = size;
    m_sb_write_ptr = write_ptr + size;
    command.type = RLBC_SET_STATE;
    command.prim_type = RLPRT_INVALID;
    record_command(command);
}

// 0x0054C980 (:239)
void rl_primitive_renderer::restore_state(rl_renderer& renderer, int data_offset, int data_size)
{
    if (m_sb_num_segments == 0)
    {
        keen::debug::nativePrintArgs("[primitive renderer] Calling restore_state(), but m_sb_num_segments == 0 - this is bad!\n");
        return;
    }
    int offset = 0;
    rl_state_manager::set_primitive_render_state_to_renderer(renderer, m_sb + data_offset, offset, data_size);
    InterlockedDecrement((volatile LONG*)&m_sb_num_segments);
    m_sb_read_ptr = m_sb + data_size + data_offset;
}

// 0x005142C0 (:261)
// A dirty state (flag 0x40) is recorded first.
void rl_primitive_renderer::record_draw_command(rl_primitive_render_type prim_type, int data_offset, int num_prims, int num_data_segments, rl_state_manager& state)
{
    // Magic number: rl_state_manager flag 0x40, the primitive state changed.
    if ((state.m_flags & 0x40) != 0)
    {
        record_state_command(state);
    }
    rl_buffered_command command;
    command.prim_count = num_prims;
    command.data_offset = data_offset;
    command.type = RLBC_DRAW_PRIM;
    command.prim_type = prim_type;
    command.num_data_segments = num_data_segments;
    record_command(command);
}

// 0x00514310 (:281)
void rl_primitive_renderer::record_flush_command(int deferred_block_index, int num_deferred_blocks)
{
    rl_buffered_command command;
    command.data_offset = deferred_block_index;
    command.type = RLBC_FLUSH;
    command.prim_count = 0;
    command.prim_type = RLPRT_INVALID;
    command.num_data_segments = num_deferred_blocks;
    record_command(command);
}

// 0x004F0D00 (:298)
// Appends a command to the command ring (waiting while it is full) and signals the reader.
void rl_primitive_renderer::record_command(const rl_buffered_command& command)
{
    if (RL_primitive_renderer_state == nullptr)
    {
        return;
    }
    unsigned int read_slot = m_cb_read_index % (unsigned int)m_max_buffered_commands;
    unsigned int write_slot = m_cb_write_index % (unsigned int)m_max_buffered_commands;
    while (m_cb_read_index != m_cb_write_index && read_slot == write_slot)
    {
        read_slot = m_cb_read_index % (unsigned int)m_max_buffered_commands;
        write_slot = m_cb_write_index % (unsigned int)m_max_buffered_commands;
    }
    m_cb[(int)write_slot] = command;
    m_cb[(int)write_slot].id = Cur_ID;
    Cur_ID++;
    InterlockedIncrement((volatile LONG*)&m_cb_write_index);
    m_command_event.signal();
}

// 0x004F0DC0 (:325)
// Takes the next command, waiting for the writer when the ring is empty.
void rl_primitive_renderer::get_next_buffered_command(rl_buffered_command& command)
{
    if (RL_primitive_renderer_state == nullptr)
    {
        return;
    }
    while (m_cb_read_index == m_cb_write_index)
    {
        m_command_event.wait();
    }
    command = m_cb[(int)(m_cb_read_index % (unsigned int)m_max_buffered_commands)];
    InterlockedIncrement((volatile LONG*)&m_cb_read_index);
}

// 0x004C4590 (:350)
rl_primitive_renderer_state::rl_primitive_renderer_state()
{
    m_begin_deferred_called = false;
    m_num_queued_verts = -1;
    m_num_data_segments = 0;
    m_prim_type = RLPRT_INVALID;
    m_prim_state.invalidate();
    m_pending_stitch_read = nullptr;
    m_pending_stitch_write = nullptr;
    m_cur_macro_state.invalidate();
    m_mode = RLPRM_IMMEDIATE;
}

// 0x004C45C0 (:356)
rl_primitive_renderer_state::~rl_primitive_renderer_state()
{
}

// 0x00514350 (:363)
rl_primitive_renderer::rl_primitive_renderer()
    : m_command_event(false, false)
{
    m_is_initialized = false;
    m_cb_write_index = 0;
    m_cb_read_index = 0;
    m_sb_write_ptr = nullptr;
    m_sb_read_ptr = nullptr;
    m_sb = nullptr;
    m_num_deferred_frames = 0;
    m_imm_db = nullptr;
    m_buff_db = nullptr;
    m_primitive_2d_material_data.textures.set_offset(-1);
    m_primitive_2d_material_data.constant_name_checksum.set_offset(-1);
    m_primitive_2d_material_data.constant_block.set_offset(-1);
    m_primitive_2d_tex_material_data.textures.set_offset(-1);
    m_primitive_2d_tex_material_data.constant_name_checksum.set_offset(-1);
    m_primitive_2d_tex_material_data.constant_block.set_offset(-1);
    m_primitive_3d_material_data.textures.set_offset(-1);
    m_primitive_3d_material_data.constant_name_checksum.set_offset(-1);
    m_primitive_3d_material_data.constant_block.set_offset(-1);
    m_primitive_3d_tex_material_data.textures.set_offset(-1);
    m_primitive_3d_tex_material_data.constant_name_checksum.set_offset(-1);
    m_primitive_3d_tex_material_data.constant_block.set_offset(-1);
    m_primitive_2d_binked_material_data.textures.set_offset(-1);
    m_primitive_2d_binked_material_data.constant_name_checksum.set_offset(-1);
    m_primitive_2d_binked_material_data.constant_block.set_offset(-1);
    // Magic numbers: 40960 commands, 400 KB of state, 10 MB of deferred and 480 KB of immediate vertex data.
    m_max_buffered_commands = 0xA000;
    m_max_buffered_state_size = 0x64000;
    m_max_buffered_data_size = 0xA00000;
    m_max_immediate_data_size = 0x78000;
}

// Unregisters and releases a primitive material.
static void release_material(rl_material*& mat_p)
{
    if (mat_p != nullptr)
    {
        rl_material_library_unregister(mat_p);
        mat_p->release();
        mat_p = nullptr;
    }
}

// 0x0052C790 (:371)
// Notes (decompiled behaviour, kept as is): the buffers are freed with scalar delete although allocated with new[].
rl_primitive_renderer::~rl_primitive_renderer()
{
    if (m_imm_db != nullptr)
    {
        operator delete(m_imm_db);
    }
    if (m_buff_db != nullptr)
    {
        operator delete(m_buff_db);
    }
    release_material(m_primitive_2d_material);
    release_material(m_primitive_2d_tex_material);
    release_material(m_primitive_3d_material);
    release_material(m_primitive_3d_tex_material);
    release_material(m_primitive_2d_binked_material);
    if (RL_primitive_renderer_state != nullptr && RL_primitive_renderer_state != ms_main_state)
    {
        RL_primitive_renderer_state->release();
    }
    if (ms_main_state != nullptr)
    {
        ms_main_state->release();
    }
    if (m_sb != nullptr)
    {
        operator delete(m_sb);
    }
}

// 0x00543040 (:426)
// Allocates the buffers and the primitive materials (on the thread set up by initialize_for_thread).
void rl_primitive_renderer::initialize()
{
    if (RL_primitive_renderer_state == nullptr || m_is_initialized)
    {
        return;
    }
    m_imm_db = new unsigned char[m_max_immediate_data_size];
    m_imm_db_write_offset = 0;
    m_buff_db = new unsigned char[m_max_buffered_data_size];
    m_buff_db_write_offset = 0;
    m_buff_db_read_offset = 0;
    m_buff_db_num_segments = 0;
    create_materials();
    for (int i = 0; i < NUM_RL_PRIMITIVE_RENDER_TYPES; i++)
    {
        switch (i)
        {
        case RLPRT_LINELIST_2D:
        case RLPRT_TRISTRIP_2D:
            Mat_lookup[i] = m_primitive_2d_material_handle;
            break;
        case RLPRT_TEXTURED_LINELIST_2D:
        case RLPRT_TEXTURED_TRISTRIP_2D:
            Mat_lookup[i] = m_primitive_2d_tex_material_handle;
            break;
        case RLPRT_LINELIST_3D:
        case RLPRT_TRISTRIP_3D:
            Mat_lookup[i] = m_primitive_3d_material_handle;
            break;
        case RLPRT_TEXTURED_LINELIST_3D:
        case RLPRT_TEXTURED_TRISTRIP_3D:
            Mat_lookup[i] = m_primitive_3d_tex_material_handle;
            break;
        case RLPRT_BINKED_TRISTRIP_2D:
            Mat_lookup[i] = m_primitive_2d_binked_material_handle;
            break;
        case RLPRT_MATERIAL_TRISTRIP_2D:
        case RLPRT_MATERIAL_TRISTRIP_3D:
            Mat_lookup[i] = 0xFFFFFFFF;
            break;
        }
    }
    m_cb.init_dynamic(m_max_buffered_commands);
    m_cb_write_index = 0;
    m_cb_read_index = 0;
    m_sb = new unsigned char[m_max_buffered_state_size];
    m_sb_write_ptr = m_sb;
    m_sb_read_ptr = m_sb;
    m_sb_num_segments = 0;
    RL_primitive_renderer_state->m_mode = RLPRM_IMMEDIATE;
    m_is_initialized = true;
}

// 0x004C45D0 (:485)
void rl_primitive_renderer::set_max_buffered_commands(int max_buffered_commands)
{
    if (!m_is_initialized)
    {
        m_max_buffered_commands = max_buffered_commands;
    }
}

// 0x004C45E0 (:493)
void rl_primitive_renderer::set_max_buffered_state_size(int max_buffered_state_size)
{
    if (!m_is_initialized)
    {
        m_max_buffered_state_size = max_buffered_state_size;
    }
}

// 0x004C45F0 (:501)
void rl_primitive_renderer::set_max_buffered_data_size(int max_buffered_data_size)
{
    if (!m_is_initialized)
    {
        m_max_buffered_data_size = max_buffered_data_size;
    }
}

// 0x004C4600 (:509)
void rl_primitive_renderer::set_max_immediate_data_size(int max_immediate_data_size)
{
    if (!m_is_initialized)
    {
        m_max_immediate_data_size = max_immediate_data_size;
    }
}

// 0x004C4610 (:524)
void rl_primitive_renderer::begin_primitive(rl_primitive_render_type prim_type, const rl_primitive_state& state)
{
    rl_primitive_renderer_state* state_p = RL_primitive_renderer_state;
    if (state_p == nullptr)
    {
        return;
    }
    state_p->m_num_queued_verts = 0;
    state_p->m_data_offset = -1;
    state_p->m_prim_type = prim_type;
    state_p->m_prim_state = state;
    switch (prim_type)
    {
    case RLPRT_LINELIST_2D:
    case RLPRT_TEXTURED_LINELIST_2D:
    case RLPRT_TRISTRIP_2D:
    case RLPRT_TEXTURED_TRISTRIP_2D:
    case RLPRT_BINKED_TRISTRIP_2D:
    case RLPRT_MATERIAL_TRISTRIP_2D:
        state_p->m_prim_vert_size = 0x1C;
        return;
    case RLPRT_LINELIST_3D:
    case RLPRT_TEXTURED_LINELIST_3D:
    case RLPRT_TRISTRIP_3D:
    case RLPRT_TEXTURED_TRISTRIP_3D:
    case RLPRT_MATERIAL_TRISTRIP_3D:
        state_p->m_prim_vert_size = 0x18;
        return;
    default:
        return;
    }
}

// The number of primitives of num_verts vertices.
static int get_num_prims(rl_primitive_render_type prim_type, int num_verts)
{
    switch (prim_type)
    {
    case RLPRT_LINELIST_2D:
    case RLPRT_TEXTURED_LINELIST_2D:
    case RLPRT_LINELIST_3D:
    case RLPRT_TEXTURED_LINELIST_3D:
        return num_verts >> 1;
    case RLPRT_TRISTRIP_2D:
    case RLPRT_TEXTURED_TRISTRIP_2D:
    case RLPRT_TRISTRIP_3D:
    case RLPRT_TEXTURED_TRISTRIP_3D:
    case RLPRT_BINKED_TRISTRIP_2D:
    case RLPRT_MATERIAL_TRISTRIP_2D:
    case RLPRT_MATERIAL_TRISTRIP_3D:
        return num_verts - 2;
    default:
        return 0;
    }
}

// Completes a pending strip stitch (the first vertex of the next strip copied over the degenerate vertex).
static void flush_pending_stitch(rl_primitive_renderer_state* state_p)
{
    if (state_p->m_pending_stitch_write != nullptr && state_p->m_pending_stitch_read != nullptr)
    {
        memcpy(state_p->m_pending_stitch_write, state_p->m_pending_stitch_read, state_p->m_prim_vert_size);
        state_p->m_pending_stitch_read = nullptr;
        state_p->m_pending_stitch_write = nullptr;
    }
}

// 0x00550480 (:566)
// Draws (immediate: with a copy of the caller's state) or records (deferred) the queued vertices.
void rl_primitive_renderer::end_primitive(rl_state_manager& state, rl_renderer* pRenderer)
{
    rl_primitive_renderer_state* state_p = RL_primitive_renderer_state;
    if (state_p->m_num_queued_verts > 0)
    {
        flush_pending_stitch(state_p);
        const int num_prims = get_num_prims(state_p->m_prim_type, state_p->m_num_queued_verts);
        if (state_p->m_mode == RLPRM_IMMEDIATE)
        {
            memcpy(pRenderer->m_state_p, &state, sizeof(rl_state_manager));
            render_primitive(*pRenderer, state_p->m_prim_type, state_p->m_data_offset, num_prims, state_p->m_num_data_segments, false);
        }
        else if (state_p->m_mode == RLPRM_DEFERRED)
        {
            record_draw_command(state_p->m_prim_type, state_p->m_data_offset, num_prims, state_p->m_num_data_segments, state);
        }
    }
    state_p->m_num_queued_verts = 0;
    state_p->m_num_data_segments = 0;
    state_p->m_data_offset = -1;
    state_p->m_prim_type = RLPRT_INVALID;
    state_p->m_prim_vert_size = 0;
    state_p->m_prim_state.invalidate();
}

// 0x005505C0 (:633)
// Allocates num_verts vertices of the current primitive. Strips allocated in several pieces are stitched with
// degenerate vertices (2 or 3, keeping the winding); when the data space is full or not contiguous the primitive is
// ended and restarted.
// Notes (decompiled behaviour, kept as is): the restart passes a null renderer to end_primitive (only valid in
// deferred mode).
unsigned char* rl_primitive_renderer::allocate_verts(int num_verts, rl_state_manager& state)
{
    rl_primitive_renderer_state* state_p = RL_primitive_renderer_state;
    flush_pending_stitch(state_p);
    const unsigned int queued = (unsigned int)state_p->m_num_queued_verts;
    bool stitch = false;
    bool even = false;
    int stitch_verts = 0;
    if (queued != 0)
    {
        switch (state_p->m_prim_type)
        {
        case RLPRT_TRISTRIP_2D:
        case RLPRT_TEXTURED_TRISTRIP_2D:
        case RLPRT_TRISTRIP_3D:
        case RLPRT_TEXTURED_TRISTRIP_3D:
        case RLPRT_BINKED_TRISTRIP_2D:
        case RLPRT_MATERIAL_TRISTRIP_2D:
        case RLPRT_MATERIAL_TRISTRIP_3D:
            stitch = true;
            even = ((int)queued % 2) == 0;
            stitch_verts = even ? 2 : 3;
            break;
        default:
            break;
        }
    }
    int alloc_verts = stitch_verts + num_verts;
    int offset = 0;
    unsigned char* data_p = allocate_data_space(state_p->m_prim_vert_size * alloc_verts, offset, false, queued == 0);
    if (data_p == nullptr || (state_p->m_num_queued_verts != 0 && offset != state_p->m_end_of_last_alloc))
    {
        const rl_primitive_render_type prim_type = state_p->m_prim_type;
        const rl_primitive_state prim_state = state_p->m_prim_state;
        end_primitive(state, nullptr);
        begin_primitive(prim_type, prim_state);
        state_p = RL_primitive_renderer_state;
        if (data_p == nullptr)
        {
            data_p = allocate_data_space(state_p->m_prim_vert_size * num_verts, offset, true, state_p->m_num_queued_verts == 0);
            if (data_p == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            // The stitch vertices are skipped.
            const int skip = state_p->m_prim_vert_size * stitch_verts;
            offset += skip;
            data_p += skip;
        }
        stitch_verts = 0;
        stitch = false;
        alloc_verts = num_verts;
    }
    memset(data_p, 0, state_p->m_prim_vert_size * alloc_verts);
    if (stitch)
    {
        // Repeat the previous last vertex; the first new vertex is copied in later (flush_pending_stitch).
        const unsigned char* last_p = data_p - state_p->m_prim_vert_size;
        memcpy(data_p, last_p, state_p->m_prim_vert_size);
        if (even)
        {
            state_p->m_pending_stitch_write = data_p + state_p->m_prim_vert_size;
        }
        else
        {
            memcpy(data_p + state_p->m_prim_vert_size, last_p, state_p->m_prim_vert_size);
            state_p->m_pending_stitch_write = data_p + state_p->m_prim_vert_size * 2;
        }
        state_p->m_pending_stitch_read = data_p + state_p->m_prim_vert_size * stitch_verts;
    }
    if (state_p->m_data_offset == -1)
    {
        state_p->m_data_offset = offset;
    }
    state_p->m_num_queued_verts += alloc_verts;
    state_p->m_num_data_segments++;
    state_p->m_end_of_last_alloc = state_p->m_prim_vert_size * alloc_verts + offset;
    return data_p + state_p->m_prim_vert_size * stitch_verts;
}

// 0x004F0ED0 (:735)
void rl_primitive_renderer::begin_deferred()
{
    rl_primitive_renderer_state* state_p = RL_primitive_renderer_state;
    if (state_p == nullptr || state_p->m_mode != RLPRM_DEFERRED)
    {
        return;
    }
    rl_buffered_command command;
    command.type = RLBC_BEGIN;
    record_command(command);
    RL_primitive_renderer_state->m_begin_deferred_called = true;
    InterlockedIncrement((volatile LONG*)&m_num_deferred_frames);
}

// 0x004F0F30 (:753)
void rl_primitive_renderer::end_deferred()
{
    rl_primitive_renderer_state* state_p = RL_primitive_renderer_state;
    if (state_p == nullptr || state_p->m_mode != RLPRM_DEFERRED)
    {
        return;
    }
    rl_buffered_command command;
    command.type = RLBC_END;
    record_command(command);
    RL_primitive_renderer_state->m_begin_deferred_called = false;
}

// 0x00550830 (:769)
// Replays one deferred frame (RLBC_BEGIN .. RLBC_END) if one was recorded.
void rl_primitive_renderer::playback_deferred(rl_renderer& renderer)
{
    if (RL_primitive_renderer_state == nullptr || m_num_deferred_frames == 0)
    {
        return;
    }
    rl_buffered_command command;
    get_next_buffered_command(command);
    if (command.type != RLBC_BEGIN)
    {
        return;
    }
    for (;;)
    {
        get_next_buffered_command(command);
        if (command.type == RLBC_END)
        {
            break;
        }
        if (command.type == RLBC_DRAW_PRIM)
        {
            render_primitive(renderer, command.prim_type, command.data_offset, command.prim_count, command.num_data_segments, true);
        }
        else if (command.type == RLBC_SET_STATE)
        {
            restore_state(renderer, command.data_offset, command.prim_count);
        }
    }
    m_cb_read_index = 0;
    m_cb_write_index = 0;
    InterlockedDecrement((volatile LONG*)&m_num_deferred_frames);
}

// 0x004C46A0 (:826)
void rl_primitive_renderer::set_mode(rl_primitive_renderer_mode new_mode)
{
    RL_primitive_renderer_state->m_mode = new_mode;
}

// 0x00AE7870 (.h:112)
rl_primitive_renderer_mode rl_primitive_renderer::get_mode() const
{
    if (RL_primitive_renderer_state == nullptr)
    {
        return RLPRM_INVALID;
    }
    return RL_primitive_renderer_state->m_mode;
}

// 0x004F0F80 (:832)
// A virtual thread gets its own state; leaving it returns to the main state.
void rl_primitive_renderer::set_thread_is_virtual(bool is_virtual)
{
    if (is_virtual)
    {
        if (RL_primitive_renderer_state == ms_main_state)
        {
            RL_primitive_renderer_state = rl_primitive_renderer_state::create();
        }
        return;
    }
    if (RL_primitive_renderer_state != ms_main_state)
    {
        RL_primitive_renderer_state->release();
        RL_primitive_renderer_state = ms_main_state;
    }
}

// 0x004F1000 (:845)
// The calling thread uses the main state (created on first use).
void rl_primitive_renderer::initialize_for_thread()
{
    if (ms_main_state == nullptr)
    {
        ms_main_state = rl_primitive_renderer_state::create();
        if (ms_main_state == nullptr)
        {
            // OpenRFG deviation: the message box is replaced by a log line.
            // MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0);
            keen::debug::nativePrintArgs("[error] Failed to initialize renderer.\n");
            ExitProcess(0);
        }
    }
    RL_primitive_renderer_state = ms_main_state;
}

// 0x004C46C0 (:861)
void rl_primitive_renderer::deinitialize_for_thread()
{
    if (RL_primitive_renderer_state != ms_main_state)
    {
        RL_primitive_renderer_state->release();
    }
    RL_primitive_renderer_state = nullptr;
}

// 0x004C4700 (:877)
// Deferred mode allocates from the m_buff_db ring (wrapping to the start; with wait_for_data it spins until the
// render thread frees space); immediate mode allocates linearly from m_imm_db.
unsigned char* rl_primitive_renderer::allocate_data_space(int size, int& offset, bool wait_for_data, bool)
{
    if (RL_primitive_renderer_state->m_mode != RLPRM_DEFERRED)
    {
        const int write_offset = m_imm_db_write_offset;
        if (write_offset + size > m_max_immediate_data_size)
        {
            return nullptr;
        }
        offset = write_offset;
        m_imm_db_write_offset += size;
        return m_imm_db + write_offset;
    }
    if (size > m_max_buffered_data_size)
    {
        return nullptr;
    }
    for (;;)
    {
        const int read_offset = m_buff_db_read_offset;
        if (read_offset != m_buff_db_write_offset || m_buff_db_num_segments == 0)
        {
            const int write_offset = m_buff_db_write_offset;
            if (write_offset < read_offset)
            {
                if (size <= read_offset - write_offset - 1)
                {
                    offset = write_offset;
                    InterlockedIncrement((volatile LONG*)&m_buff_db_num_segments);
                    m_buff_db_write_offset += size;
                    return m_buff_db + write_offset;
                }
            }
            else
            {
                if (size <= m_max_buffered_data_size - write_offset)
                {
                    offset = write_offset;
                    InterlockedIncrement((volatile LONG*)&m_buff_db_num_segments);
                    m_buff_db_write_offset += size;
                    return m_buff_db + write_offset;
                }
                if (size <= read_offset - 1)
                {
                    offset = 0;
                    InterlockedIncrement((volatile LONG*)&m_buff_db_num_segments);
                    m_buff_db_write_offset = size;
                    return m_buff_db;
                }
            }
        }
        if (!wait_for_data)
        {
            return nullptr;
        }
    }
}

// 0x0054C9F0 (:944)
// Draws the primitives with their material and textures (the primitive texture, or the three bink planes), then
// releases the data (immediate: the whole buffer; deferred: up to the end of the primitive).
void rl_primitive_renderer::render_primitive(rl_renderer& renderer, rl_primitive_render_type prim_type, int data_offset, int num_prims, int num_data_segments, bool was_deferred)
{
    if ((unsigned int)prim_type > RLPRT_MATERIAL_TRISTRIP_3D)
    {
        return;
    }
    unsigned char* data_p = was_deferred ? m_buff_db : m_imm_db;
    unsigned int material = Mat_lookup[prim_type];
    if (material == 0xFFFFFFFF)
    {
        material = renderer.m_state_p->get_primitive_material();
    }
    rl_texture_binding textures[3];
    rl_constant_binding constants;
    int num_textures = 0;
    if (prim_type == RLPRT_TEXTURED_LINELIST_2D || prim_type == RLPRT_TEXTURED_LINELIST_3D || prim_type == RLPRT_TEXTURED_TRISTRIP_2D || prim_type == RLPRT_TEXTURED_TRISTRIP_3D)
    {
        textures[0].m_handle = renderer.m_state_p->get_primitive_texture();
        textures[0].m_tmu = 0;
        num_textures = 1;
    }
    else if (prim_type == RLPRT_BINKED_TRISTRIP_2D)
    {
        renderer.m_state_p->get_primitive_bink_textures(textures[0].m_handle, textures[1].m_handle, textures[2].m_handle);
        textures[0].m_tmu = 0;
        textures[1].m_tmu = 1;
        textures[2].m_tmu = 2;
        num_textures = 3;
    }
    renderer.draw_primitive(Prim_type_lookup[prim_type], num_prims, Vert_decl_lookup[prim_type], Vert_stride_lookup[prim_type], data_p + data_offset, material, num_textures, textures, 0, &constants);
    if (!was_deferred)
    {
        m_imm_db_write_offset = 0;
        return;
    }
    int num_verts = 0;
    switch (prim_type)
    {
    case RLPRT_LINELIST_2D:
    case RLPRT_TEXTURED_LINELIST_2D:
    case RLPRT_LINELIST_3D:
    case RLPRT_TEXTURED_LINELIST_3D:
        num_verts = num_prims * 2;
        break;
    default:
        num_verts = num_prims + 2;
        break;
    }
    InterlockedExchangeAdd((volatile LONG*)&m_buff_db_num_segments, -num_data_segments);
    m_buff_db_read_offset = data_offset + Vert_stride_lookup[prim_type] * num_verts;
}
