// Original: volition/render_lib/code/rl_morph_target.h
// Implementation: volition/render_lib/code/rl_morph_target.cpp
//
// rl_morph_container, the morph targets of a mesh (compressed and uncompressed vertex deltas).
#pragma once

#include <stdint.h>
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_vertex_format_defines.h"
#include "volition/vlib/code/util/endian_type.h"

class mempool;
class rl_class_factory_info;
struct rl_morph_data;
class rl_renderer;
class vector;

enum rl_morph_target_format
{
    RL_MTF_VBUFFER = 0,
    RL_MTF_COMPRESSED = 1,
    NUM_RL_MORPH_TARGET_FORMATS = 2,
};

// size 0xC
struct rl_morph_vert_compressed
{
    short delta_pos[3]; // 0x00
    unsigned short vb_mapping; // 0x06
    rl_compressed_vector delta_normal; // 0x08
};

// size 0x10
struct rl_morph_target_vbuffer
{
    unsigned int name_chksum; // 0x00
    unsigned int num_morph_verts; // 0x04
    uint64_t verts; // 0x08
};

// size 0x50
class rl_morph_container : public rl_base_object
{
    // Access inferred: rl_morph_instance::render_setup reads the morph data and binds the vertex stream.
    friend class rl_morph_instance;
protected:
    rl_morph_container(); // 0x004C3870 (:65)
    virtual ~rl_morph_container(); // vtbl+0x00 0x004E9D30 (:76)
private:
    static const pool_list<rl_morph_container*>& get_object_pool(); // 0x004BDBE0 (.h:58)
public:
    static void set_objects_max(int objects_max); // 0x007B99A0 (.h:58)
    static int get_objects_max(); // 0x004BDBF0 (.h:58)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00513390 (:7)
    static rl_morph_container* create_placement(rl_morph_container* _data_p); // 0x004E9CB0 (:7)
    static rl_morph_container* create(); // 0x00529480 (:7)
    static bool can_create(); // 0x004E9CF0 (:7)
    virtual void release(); // vtbl+0x04 0x00513490 (:7)
private:
    void create_render_data(); // 0x00513510 (:11)
    void release_render_data(); // 0x004E9D00 (:49)
public:
    void set_morph_data(rl_morph_data* morph_data); // 0x00513640 (:89)
    int get_num_morph_verts(int target); // 0x004E9E30 (:156)
    int find_morph_target(unsigned int name_checksum); // 0x004E9D80 (:112)
    unsigned char* get_morph_target(int index); // 0x004E9DF0 (:133)
    bool render_setup(rl_renderer& renderer); // 0x005136E0 (:173)
    bool render_teardown(rl_renderer& renderer); // 0x00513730 (:205)
    int get_max_vertex_index(); // 0x004C3890 (:216)

    static const unsigned int m___class_id; // 0x012BEDBC
    static rl_class_factory_info ms_class_factory_info; // 0x01514F1C
protected:
    static pool_list<rl_morph_container*> ms_object_pool; // 0x01514F4C
private:
    rl_morph_data* m_morph_data; // 0x08
    bool m_renderGeometryValid; // 0x0C
    keen::RenderGeometry m_renderGeometry; // 0x10
    keen::VertexBufferData m_vertexBufferData; // 0x3C
protected:
    unsigned int m_debug_signature; // 0x48
    int m_max_vertex_index; // 0x4C
};

// size 0x24
struct rl_morph_target_compressed
{
    unsigned int name_chksum; // 0x00
    unsigned int num_morph_verts; // 0x04
    float x_max; // 0x08
    float y_max; // 0x0C
    float z_max; // 0x10
    float nx_max; // 0x14
    float ny_max; // 0x18
    float nz_max; // 0x1C
    et_ptr_offset<rl_morph_vert_compressed,0> verts; // 0x20
};

// size 0xC
struct rl_morph_vert_uncompressed
{
    void set_pos(float, float, float);
    void set_pos(vector);
    vector get_pos();
    void add(const rl_morph_vert_uncompressed&);

    short delta_pos[4]; // 0x00
    rl_compressed_vector normal; // 0x08
};
