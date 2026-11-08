// Original: volition/render_lib/code/rl_cached_dyn_texture.h
// Implementation: volition/render_lib/code/rl_cached_dyn_texture.cpp
//
// rl_cached_dyn_texture, a dynamic texture cached in a shared memory pool (format and memory mode enums).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_dyn_texture.h"

class mempool;
class rl_class_factory_info;
class rl_renderer;

enum rl_cached_dyn_texture_format
{
    RLCDT_FORMAT_INVALID = -1,
    RLCDT_FORMAT_A8R8G8B8 = 0,
    RLCDT_FORMAT_L8 = 1,
    RLCDT_FORMAT_A8 = 2,
    NUM_RL_CACHED_DYN_TEXTURE_FORMATS = 3,
};

enum rl_cached_dyn_texture_memory_mode
{
    RLCDT_MEM_INVALID = -1,
    RLCDT_MEM_BUFFER = 0,
    RLCDT_MEM_PILLAGE = 1,
    RLCDT_MEM_DYNAMIC = 2,
    NUM_RL_CACHED_DYN_TEXTURE_MEMORY_MODES = 3,
};

// size 0x34
class rl_cached_dyn_texture : public rl_dyn_texture_base
{
public:
    // size 0x4
    struct BufferStruct
    {
        unsigned int m_buffers_initialized : 1; // 0x00:0
        unsigned int m_current_buffer : 2; // 0x00:1
    };

protected:
    rl_cached_dyn_texture(); // 0x00537350 (:9)
    virtual ~rl_cached_dyn_texture(); // vtbl+0x00 0x00537390 (:24)
private:
    static const pool_list<rl_cached_dyn_texture*>& get_object_pool(); // 0x004B7520 (.h:49)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004B7530 (.h:49)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00509C70 (:48)
    static rl_cached_dyn_texture* create_placement(rl_cached_dyn_texture* _data_p); // 0x00537460 (:48)
    static rl_cached_dyn_texture* create(); // 0x00537400 (:48)
    static bool can_create(); // 0x004DFA50 (:48)
    virtual void release(); // vtbl+0x04 0x00509D60 (:48)
    void init(int texture_width, int texture_height, rl_cached_dyn_texture_format format, rl_cached_dyn_texture_memory_mode mem_mode); // 0x00520C70 (:32)
    void update(rl_renderer& renderer, unsigned char* update_data, int pitch); // 0x00520CA0 (:52)
    virtual const rl_dev_texture& get_texture_data() const; // vtbl+0x08 0x004BA880 (:91)
    virtual void get_dimensions(int& width, int& height, int& mip_levels); // vtbl+0x0C 0x004DFAA0 (:96)
protected:
    void internal_update(unsigned char* update_data, int pitch, rl_renderer& renderer); // 0x00509E70 (:112)
    int get_read_pitch(int pitch); // 0x004DFAE0 (:130)
    int get_format_bytes(); // 0x004BA890 (:136)
    void create_buffers(); // 0x00509DE0 (:74)
    void destroy_buffers(); // 0x004DFA60 (:82)
    virtual void release_resource(); // vtbl+0x10 0x004DFAD0 (:101)
    virtual void renew_resource(rl_renderer& renderer); // vtbl+0x14 0x00520D50 (:106)

public:
    static const unsigned int m___class_id; // 0x012B8068
    static rl_class_factory_info ms_class_factory_info; // 0x01514778
protected:
    static pool_list<rl_cached_dyn_texture*> ms_object_pool; // 0x015147A8
    static const unsigned int kNumBuffers; // 0x012B806C
    int m_width; // 0x0C
    int m_height; // 0x10
    rl_cached_dyn_texture_format m_format; // 0x14
    rl_cached_dyn_texture_memory_mode m_memory_mode; // 0x18
    BufferStruct m_buffers; // 0x1C
    rl_dev_texture m_buffer[2]; // 0x20
    unsigned char* m_last_update_data_p; // 0x28
    int m_last_pitch; // 0x2C
    int m_last_size; // 0x30
};
