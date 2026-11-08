// Original: volition/render_lib/code/rl_material_library.h
// Implementation: volition/render_lib/code/rl_material_library.cpp
//
// The material library: materials looked up by key (matlib_bucket, matlib_entry, mat_key_wrapper).
#pragma once

#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"

class mempool_base;
struct rl_deferred_material_handle_override;
class rl_material_base;
class rl_renderer;

// size 0x4
class mat_key_wrapper
{
public:
    bool operator==(const mat_key_wrapper& other_mat) const; // 0x004D5D10 (:92)

    rl_material_base* mat; // 0x00
};

// size 0x4
struct matlib_entry
{
    rl_material_base* mat; // 0x00
};

// size 0x30
class matlib_bucket : public object_pool_client<matlib_entry,1>
{
public:
    matlib_bucket(); // 0x00511340 (:158)
    void add_material(rl_material_base* mat_p); // 0x00511360 (:182)
    void remove_material(rl_material_base* mat_p); // 0x00527AC0 (:204)
    void set_override_material(rl_material_base* mat_p); // 0x004C2CB0 (:230)
    void destroy(); // 0x004C2C90 (:165)

    matlib_entry* m_cur_material_instance; // 0x20
    rl_material_base* m_override_material; // 0x24
    unsigned int m_handle; // 0x28
};

unsigned int hash_mat_key_wrapper(mat_key_wrapper key, unsigned int size); // 0x004C2C60 (:151)

// Magic number: 5120 materials at most.
extern data_hash_table<mat_key_wrapper, matlib_bucket*, &hash_mat_key_wrapper, 5120, unsigned short> Matlib_entry_hash; // 0x01C371C8

matlib_bucket* alloc_bucket(); // 0x00511390 (:247)
void free_bucket(matlib_bucket* bucket_p); // 0x00527B80 (:263)
matlib_bucket* get_bucket_by_handle(unsigned int mat_handle); // 0x004E8F20 (:275)
rl_material_base* resolve_material_handle(unsigned int mat_h, bool respect_override); // 0x004E8F50 (:286)
void rl_material_library_system_init(); // 0x004E8FA0 (:309)
void rl_material_library_system_shutdown(); // 0x004C2CD0 (:316)
unsigned int rl_material_library_register(rl_material_base* mat_p); // 0x00527BB0 (:330)
void rl_material_library_unregister(rl_material_base* mat_p); // 0x00527CA0 (:385)
bool rl_material_library_override_handle(rl_deferred_material_handle_override* def_op_p); // 0x0054BD30 (:431)
bool rl_material_library_override_handle(unsigned int mat_h, rl_material_base* mat_p); // 0x0053F120 (:447)
void rl_material_library_set_render_material(rl_renderer& renderer, unsigned int mat_h, int pass_index); // 0x005113F0 (:478)
void rl_material_library_clear_render_material(rl_renderer& renderer); // 0x004E8FB0 (:500)
bool rl_material_library_material_is_transparent(unsigned int mat_h); // 0x004E9040 (:523)
bool rl_material_library_material_has_flags(unsigned int mat_h, int pass_index, unsigned int flags, bool depthPass); // 0x004E9090 (:536)
rl_material_base* rl_material_library_clone(unsigned int mat_h, mempool_base* pool); // 0x004E90F0 (:555)
void rl_material_library_set_override(unsigned int mat_h); // 0x004C2CE0 (:567)
unsigned int rl_material_library_get_override(); // 0x004C2D00 (:572)
int rl_material_library_get_num_passes(unsigned int mat_h, bool depthPass); // 0x004E9140 (:577)
bool rl_material_library_can_combine_drawcalls(unsigned int mat_h); // 0x004E91B0 (:594)
rl_material_base* rl_material_library_get_material(unsigned int mat_h); // 0x004E9220 (:612)
