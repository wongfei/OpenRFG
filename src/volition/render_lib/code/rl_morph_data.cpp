// Original: volition/render_lib/code/rl_morph_data.cpp
//
// Partial: rl_morph_data_load (stub_impl.md S3, for mesh_morph_load). rl_morph_data_write (the tools' writer) is not
// ported.
//
// Layout (version 3, signatures 0xBADBEEF): the CPU file holds the header, the targets (0x10 bytes each for format 0,
// 0x24 for format 1), each compressed target's vertices (16-byte aligned, 0xC bytes each) and an end signature. A
// format 0 container's vertices are in the GPU file, one block per target between two signatures.

#include "volition/render_lib/code/rl_morph_data.h"

#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdint.h>

static const int Morph_data_signature = 0xBADBEEF; // magic number: header, block and end signature
static const int Morph_data_version = 3;

// offset rounded up to a multiple of n (inlined in the original, signed modulo).
static int rl_morph_align(int offset, int n)
{
    const int rem = offset % n;
    return rem == 0 ? offset : offset + (n - rem);
}

// 0x0053FA80 (:41)
// Fixes up the morph data at cpu_buf + cpu_offset in place (the self-relative target and vertex offsets, the GPU vertex
// pointers) and advances both offsets past it. Null when the buffer is misaligned, the version or a signature is wrong,
// or a format 0 container has no separate 16-byte aligned GPU buffer.
rl_morph_data* rl_morph_data_load(unsigned char* cpu_buf, unsigned char* gpu_buf, int& cpu_offset, int& gpu_offset)
{
    if (((uintptr_t)cpu_buf & 3) != 0)
    {
        return nullptr;
    }
    cpu_offset = rl_morph_align(cpu_offset, 4);
    rl_morph_data* data = (rl_morph_data*)(cpu_buf + cpu_offset);
    cpu_offset += sizeof(rl_morph_data);
    if (data->version != Morph_data_version)
    {
        // original: the message is formatted and dropped
        char message[256];
        _snprintf_safe(message, sizeof(message), "Morph version is (%d).  Expecting current version is (%d).", data->version, Morph_data_version);
        return nullptr;
    }
    if (data->signature != Morph_data_signature)
    {
        return nullptr;
    }
    if (data->target_format == RL_MTF_VBUFFER && (gpu_buf == nullptr || gpu_buf == cpu_buf || ((uintptr_t)gpu_buf & 0xF) != 0))
    {
        return nullptr;
    }
    cpu_offset = rl_morph_align(cpu_offset, 4);
    data->targets.from_pointer(cpu_buf + cpu_offset);
    const int target_size = data->target_format == RL_MTF_COMPRESSED ? (int)sizeof(rl_morph_target_compressed) : (int)sizeof(rl_morph_target_vbuffer);
    cpu_offset += data->num_targets * target_size;
    unsigned char* targets = data->targets.to_pointer();
    for (int i = 0; i < data->num_targets; i++)
    {
        if (data->target_format == RL_MTF_COMPRESSED)
        {
            rl_morph_target_compressed& target = ((rl_morph_target_compressed*)targets)[i];
            cpu_offset = rl_morph_align(cpu_offset, 16);
            target.verts.from_pointer((rl_morph_vert_compressed*)(cpu_buf + cpu_offset));
            cpu_offset += (int)target.num_morph_verts * (int)sizeof(rl_morph_vert_compressed);
            cpu_offset = rl_morph_align(cpu_offset, 16);
        }
        else
        {
            rl_morph_target_vbuffer& target = ((rl_morph_target_vbuffer*)targets)[i];
            gpu_offset = rl_morph_align(gpu_offset, 4);
            const int start_signature = *(int*)(gpu_buf + gpu_offset);
            gpu_offset += 4;
            gpu_offset = rl_morph_align(gpu_offset, 16);
            target.verts = (uint64_t)(uintptr_t)(gpu_buf + gpu_offset); // a pointer in the low half
            // magic number: 0xC bytes per vertex (position delta x16y16z16w16_snorm, normal delta x8y8z8w8_unorm)
            gpu_offset += (int)target.num_morph_verts * 0xC;
            gpu_offset = rl_morph_align(gpu_offset, 4);
            const int end_signature = *(int*)(gpu_buf + gpu_offset);
            gpu_offset += 4;
            if (start_signature != end_signature || start_signature != Morph_data_signature)
            {
                return nullptr;
            }
        }
    }
    cpu_offset = rl_morph_align(cpu_offset, 4);
    const int end_signature = *(int*)(cpu_buf + cpu_offset);
    cpu_offset += 4;
    return end_signature == Morph_data_signature ? data : nullptr;
}
