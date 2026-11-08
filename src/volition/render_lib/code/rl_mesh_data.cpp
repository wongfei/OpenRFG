// Original: volition/render_lib/code/rl_mesh_data.cpp
//
// Partial: loading a mesh split into CPU and GPU data (rl_mesh_data_load_validated and its helpers) and a mesh in one
// buffer (rl_mesh_data_load_unsplit_validated, lod.md L2: the foliage meshes). Not implemented yet:
// rl_mesh_data_calc_crc, rl_submesh_data::get_num_faces and the runtime copies (rl_mesh_data.h).
//
// CPU part (4-aligned): u16 version (4..6), pad to 4, u32 verification crc, u32 CPU size, u32 GPU size, the rl_mesh_data
// (0x20 bytes), pad to 16, the submeshes (0x2C each) followed by their render blocks (0x14 each), pad to 4, and the
// verification crc again. GPU part (16-aligned buffer, 4-aligned offset): u32 crc, pad to 16, the index buffer, pad
// to 16, the vertex buffer (both strides), pad to 4, u32 crc. Both crcs must match and the sizes must agree with the
// header. LoadedMeshInfo receives the GPU addresses of the two buffers.
//
// Unsplit (one 16-aligned buffer, 16-aligned offset): u16 version (5 only), pad to 4, u32 size, the rl_mesh_data, pad
// to 16, the submeshes with their render blocks, pad to 16, the index buffer, pad to 16, the vertex buffer (stream 0
// only). No crcs; the size must agree. LoadedMeshInfo receives the addresses of the two buffers in it.
//
// LTCG: rl_submesh_data_load takes num_submeshes in EAX and offset in EDX; rl_mesh_data_load_cpu_data_validated takes
// cpu_offset in ESI (the stack arguments are cpu_buf, the crc and the GPU size out); rl_mesh_data_load_gpu_data_validated
// takes mesh_p in EAX, gpu_buf in ECX and gpu_offset in ESI (stack: the CPU crc, the GPU size and pMeshInfo). The
// parameters below are the PDB's.

#include "volition/render_lib/code/rl_mesh_data.h"

#include "volition/vlib/code/util/string_util.h"

#include "keen/base/stringencoding.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <windows.h>

// 0x004B6C80 (.h:82)
LoadedMeshInfo::LoadedMeshInfo(const char* pName, const char* pPostfix)
{
    // The release build keeps no name here; callers copy one into name themselves.
    name[0] = '\0';
}

// Rounds offset up to a multiple of align (a power of two).
static int align_offset(int offset, int align)
{
    const int rest = offset % align;
    return rest == 0 ? offset : offset + align - rest;
}

// 0x0053F1D0 (:49)
static rl_submesh_data* rl_submesh_data_load(unsigned char* buf, int& offset, int num_submeshes)
{
    if (num_submeshes == 0)
    {
        return nullptr;
    }
    rl_submesh_data* submeshes = (rl_submesh_data*)(buf + offset);
    if (((unsigned int)submeshes & 0xF) != 0)
    {
        return nullptr;
    }
    offset += num_submeshes * (int)sizeof(rl_submesh_data);
    for (int i = 0; i < num_submeshes; i++)
    {
        submeshes[i].render_blocks.from_pointer((rl_render_block*)(buf + offset));
        offset += submeshes[i].num_render_blocks * 0x14;
    }
    return submeshes;
}

// 0x0053F220 (:399)
static rl_mesh_data* rl_mesh_data_load_cpu_data_validated(unsigned char* cpu_buf, int& cpu_offset, unsigned int& mesh_simple_crc, unsigned int& gpu_data_size_out)
{
    if (((unsigned int)cpu_buf & 3) != 0)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: Mesh GPU file must must be 4 byte aligned.\n");
        return nullptr;
    }
    const int start = cpu_offset;
    if ((start & 3) != 0)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: Mesh CPU offset must be four byte aligned.\n");
        return nullptr;
    }
    const unsigned short version = *(unsigned short*)(cpu_buf + start);
    cpu_offset = align_offset(start + 2, 4);
    if ((unsigned short)(version - 4) >= 3)
    {
        char message[256];
        _snprintf_safe(message, sizeof(message), "Mesh version is (%d) which is not between the current version of %d and the least compatible version of %d", version, 6, 4);
        keen::debug::nativePrintArgs(message);
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: %s\n", message);
        return nullptr;
    }
    const unsigned int crc = *(unsigned int*)(cpu_buf + cpu_offset);
    mesh_simple_crc = crc;
    cpu_offset += 4;
    const int cpu_data_size = *(int*)(cpu_buf + cpu_offset);
    const unsigned int gpu_data_size = *(unsigned int*)(cpu_buf + cpu_offset + 4);
    rl_mesh_data* mesh = (rl_mesh_data*)(cpu_buf + cpu_offset + 8);
    cpu_offset += 8 + sizeof(rl_mesh_data);
    if (version < 6)
    {
        // Older meshes have no second vertex stream.
        mesh->vertex_buffer.vert_stride_1 = 0;
    }
    cpu_offset = align_offset(cpu_offset, 16);
    rl_submesh_data* submeshes = rl_submesh_data_load(cpu_buf, cpu_offset, mesh->num_sub_meshes);
    mesh->sub_meshes.from_pointer(submeshes);
    if (mesh->num_sub_meshes != 0 && mesh->sub_meshes.to_pointer() == nullptr)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: load submesh data failed.\n");
        return nullptr;
    }
    cpu_offset = align_offset(cpu_offset, 4);
    const unsigned int end_crc = *(unsigned int*)(cpu_buf + cpu_offset);
    cpu_offset += 4;
    if (end_crc != crc)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: Mesh verification crc's do not match for CPU data.  File may be invalid.\n");
        return nullptr;
    }
    if (cpu_offset - start != cpu_data_size)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_cpu_data_validated: (cpu_offset - original_cpu_offset) != (int)cpu_data_size\n");
        return nullptr;
    }
    gpu_data_size_out = gpu_data_size;
    return mesh;
}

// 0x004C2EB0 (:504)
static void rl_mesh_data_show_error_message(LoadedMeshInfo* pMeshInfo)
{
    char message[512];
    keen::formatString(message, sizeof(message), "Unable to load corrupted file '%s'. Please verify your installation.", pMeshInfo->name);
    // OpenRFG deviation: the message box is replaced by a log line.
    // WCHAR wide_message[512];
    // keen::convertUTF8ToUTF16((uint16*)wide_message, sizeof(wide_message), message);
    // MessageBoxW(nullptr, wide_message, L"Corrupted Data", 0x12010);
    keen::debug::nativePrintArgs("[Corrupted Data] %s\n", message);
}

// 0x004C2F30 (:514)
static bool rl_mesh_data_load_gpu_data_validated(unsigned char* gpu_buf, int& gpu_offset, unsigned int mesh_simple_crc, int gpu_data_size, rl_mesh_data* mesh_p, LoadedMeshInfo* pMeshInfo)
{
    const int start = gpu_offset;
    if (((unsigned int)gpu_buf & 0xF) != 0)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_gpu_data_validated: Mesh GPU file must must be 16 byte aligned.\n");
        return false;
    }
    if ((start & 3) != 0)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_gpu_data_validated: Mesh GPU data offset must be four byte aligned.\n");
        return false;
    }
    const unsigned int crc = *(unsigned int*)(gpu_buf + start);
    gpu_offset = align_offset(start + 4, 16);
    pMeshInfo->pIndexBuffer = gpu_buf + gpu_offset;
    gpu_offset += mesh_p->index_buffer.index_size * mesh_p->index_buffer.num_indices;
    gpu_offset = align_offset(gpu_offset, 16);
    pMeshInfo->pVertexBuffer = gpu_buf + gpu_offset;
    gpu_offset += (mesh_p->vertex_buffer.vert_stride_1 + mesh_p->vertex_buffer.vert_stride_0) * mesh_p->vertex_buffer.num_verts;
    gpu_offset = align_offset(gpu_offset, 4);
    const unsigned int end_crc = *(unsigned int*)(gpu_buf + gpu_offset);
    gpu_offset += 4;
    const char* message;
    if (end_crc != crc)
    {
        message = "rl_mesh_data_load_gpu_data_validated: Mesh verification crc's do not match for GPU data.  File may be invalid.\n";
    }
    else if (mesh_simple_crc != crc)
    {
        message = "rl_mesh_data_load_gpu_data_validated: Mesh CRC for GPU does not match mesh CRC for CPU.  File may have been crunched at different times.\n";
    }
    else
    {
        if (gpu_offset - start != gpu_data_size)
        {
            keen::debug::nativePrintArgs("rl_mesh_data_load_gpu_data_validated: (gpu_offset - original_gpu_offset) != (int)gpu_data_size\n");
        }
        return gpu_offset - start == gpu_data_size;
    }
    keen::debug::nativePrintArgs(message);
    rl_mesh_data_show_error_message(pMeshInfo);
    return false;
}

// 0x0053F450 (:616)
rl_mesh_data* rl_mesh_data_load_validated(unsigned char* cpu_buf, unsigned char* gpu_buf, int& cpu_offset, int& gpu_offset, LoadedMeshInfo* pMeshInfo)
{
    unsigned int crc;
    unsigned int gpu_data_size;
    rl_mesh_data* mesh = rl_mesh_data_load_cpu_data_validated(cpu_buf, cpu_offset, crc, gpu_data_size);
    if (mesh == nullptr)
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_validated: Failed to load CPU data for rl_mesh_data structure. pMeshInfo->name: %s\n", pMeshInfo != nullptr ? pMeshInfo->name : "");
        return nullptr;
    }
    if (!rl_mesh_data_load_gpu_data_validated(gpu_buf, gpu_offset, crc, (int)gpu_data_size, mesh, pMeshInfo))
    {
        keen::debug::nativePrintArgs("rl_mesh_data_load_validated: Failed to load GPU data for rl_mesh_data structure. pMeshInfo->name: %s\n", pMeshInfo != nullptr ? pMeshInfo->name : "");
        return nullptr;
    }
    return mesh;
}

// 0x0053F4F0 (:769)
// LTCG: rl_submesh_data_load gets offset in EDX and num_submeshes in EAX.
rl_mesh_data* rl_mesh_data_load_unsplit_validated(unsigned char* buf, int& offset, LoadedMeshInfo* pMeshInfo)
{
    if (((unsigned int)buf & 0xF) != 0)
    {
        keen::debug::nativePrintArgs("Mesh file must must be 16 byte aligned.");
        return nullptr;
    }
    const int start = offset;
    if ((start & 0xF) != 0)
    {
        keen::debug::nativePrintArgs("Mesh offset must be 16 byte aligned.");
        return nullptr;
    }
    // Magic number: the only unsplit mesh version.
    const unsigned short version = *(unsigned short*)(buf + start);
    offset = align_offset(start + 2, 4);
    if (version != 5)
    {
        char message[256];
        _snprintf_safe(message, sizeof(message), "Mesh version is (%d) which is not between the current version of %d and the least compatible version of %d", version, 5, 5);
        keen::debug::nativePrintArgs(message);
        return nullptr;
    }
    const int data_size = *(int*)(buf + offset);
    rl_mesh_data* mesh = (rl_mesh_data*)(buf + offset + 4);
    offset = align_offset(offset + 4 + (int)sizeof(rl_mesh_data), 16);
    mesh->sub_meshes.from_pointer(rl_submesh_data_load(buf, offset, mesh->num_sub_meshes));
    if (mesh->num_sub_meshes != 0 && mesh->sub_meshes.to_pointer() == nullptr)
    {
        return nullptr;
    }
    offset = align_offset(offset, 16);
    pMeshInfo->pIndexBuffer = buf + offset;
    offset += mesh->index_buffer.index_size * mesh->index_buffer.num_indices;
    offset = align_offset(offset, 16);
    pMeshInfo->pVertexBuffer = buf + offset;
    offset += mesh->vertex_buffer.vert_stride_0 * mesh->vertex_buffer.num_verts;
    return offset - start == data_size ? mesh : nullptr;
}
