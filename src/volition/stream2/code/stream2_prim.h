// Original: volition/stream2/code/stream2_prim.h
// Implementation: volition/stream2/code/stream2_prim.cpp
//
// A streamed primitive (one asset file such as a mesh or a texture peg) and its type definition. Contents inferred: the
// PDB records no inline code for this header.
// - stream2_prim_def is filled from a stream2_primitive_type_reg (stream2.h). load_cb(name, srid, cpu_data, cpu_size,
//   gpu_data, gpu_size) is called once the data is read: on the stream2 loading thread when flags.load_threaded is set
//   (stream2_container_load), otherwise on the sim thread (stream2_container::stream_wait). alloc_split means the GPU
//   part gets its own allocation; otherwise it follows the CPU data, aligned to gpu_align.
// - stream2_prim::flags: 0x1 loaded, 0x4 split into a CPU and a GPU file (cpu_ext/gpu_ext[split_ext_index]).
//   Primitives are shared between containers (container_ref_count) and found by name and allocator.
#pragma once

struct cfile;
class mempool_base;

// size 0x30
class stream2_prim_def
{
public:
    // size 0x1
    struct s2pf_flags
    {
        unsigned char debug_skippable : 1; // 0x00:0
        unsigned char alloc_split : 1; // 0x00:1
        unsigned char load_threaded : 1; // 0x00:2
    };

    stream2_prim_def(); // 0x004AB460 (:20)
    bool valid(); // 0x004AB480 (:33)

    bool (*load_cb)(const char*, unsigned int, unsigned char*, int, unsigned char*, int); // 0x00
    void (*unload_cb)(const char*, unsigned int); // 0x04
    int cpu_align; // 0x08
    int gpu_align; // 0x0C
    s2pf_flags flags; // 0x10
    char* name; // 0x14
    const char* cpu_ext[2]; // 0x18
    const char* gpu_ext[2]; // 0x20
    int total_allocated; // 0x28
    unsigned char prim_type; // 0x2C
};

// size 0x28
class stream2_prim
{
public:
    void init(); // 0x004AB490 (:45)
    void inc_ref_count(); // 0x004AB4B0 (:64)
    void dec_ref_count(); // 0x004AB4C0 (:69)
    int get_ref_count(); // 0x004AB4D0 (:74)
    void inc_container_ref_count(); // 0x004AB4E0 (:79)
    void dec_container_ref_count(); // 0x004AB4F0 (:84)
    int get_container_ref_count(); // 0x004AB500 (:89)
    bool load(unsigned int srid, bool fromLoadThread); // 0x004AB510 (:94)
    void unload(unsigned int srid); // 0x004AB590 (:121)
    bool loaded(); // 0x004AB5B0 (:135)
    int get_total_size();
    static bool load_prim(cfile* asm_file, mempool_base* pool, bool report_errors); // 0x004B07D0 (:232)

    char* name; // 0x00
    stream2_prim_def* pdef; // 0x04
    int cpu_size; // 0x08
    int gpu_size; // 0x0C
    unsigned char* cpu_data; // 0x10
    unsigned char* gpu_data; // 0x14
    stream2_prim* next; // 0x18
    stream2_prim* prev; // 0x1C
    unsigned char split_ext_index; // 0x20
    unsigned char flags; // 0x21
    short allocator_idx; // 0x22
private:
    short ref_count; // 0x24
    short container_ref_count; // 0x26
};
