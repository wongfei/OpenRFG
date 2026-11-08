// Original: volition/render_lib/code/rl_cached_dyn_texture.cpp
//
// rl_cached_dyn_texture: a double-buffered dynamic texture that keeps a copy of the last update, so the texture can
// be recreated after a device reset (renew_resource).

#include "volition/render_lib/code/rl_cached_dyn_texture.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/graphics/texturedata.hpp"

#include <string.h>

const unsigned int rl_cached_dyn_texture::kNumBuffers = 2; // 0x012B806C

// :48
// <rl_cached_dyn_texture::factory_init> 0x00509C70 (:48)
// <rl_cached_dyn_texture::release> 0x00509D60 (:48)
// <rl_cached_dyn_texture::create> 0x00537400 (:48)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_cached_dyn_texture, 0xABBA6473, 9)

// 0x00537350 (:9)
rl_cached_dyn_texture::rl_cached_dyn_texture()
{
    m_buffers.m_current_buffer = 0;
    m_width = -1;
    m_height = -1;
    m_format = RLCDT_FORMAT_INVALID;
    m_memory_mode = RLCDT_MEM_INVALID;
    m_buffer[0].pData = nullptr;
    m_buffer[1].pData = nullptr;
    m_buffers.m_buffers_initialized = 0;
    m_last_pitch = -1;
    m_last_size = -1;
    m_last_update_data_p = nullptr;
}

// 0x00537390 (:24)
rl_cached_dyn_texture::~rl_cached_dyn_texture()
{
    delete[] m_last_update_data_p;
    destroy_buffers();
}

// 0x00520C70 (:32)
void rl_cached_dyn_texture::init(int texture_width, int texture_height, rl_cached_dyn_texture_format format, rl_cached_dyn_texture_memory_mode mem_mode)
{
    m_width = texture_width;
    m_height = texture_height;
    m_format = format;
    m_memory_mode = mem_mode;
    create_buffers();
}

// 0x00520CA0 (:52)
// Copies the update (pitch -1: tightly packed) and uploads it. A null update re-uploads nothing (the copy is freed).
void rl_cached_dyn_texture::update(rl_renderer& renderer, unsigned char* update_data, int pitch)
{
    const int read_pitch = get_read_pitch(pitch);
    const int size = m_height * read_pitch;
    if (size != m_last_size || update_data == nullptr)
    {
        delete[] m_last_update_data_p;
        // Notes (decompiled behaviour, kept as is): the pointer is not cleared when update_data is null.
        if (update_data != nullptr)
        {
            m_last_update_data_p = new unsigned char[size];
            m_last_size = size;
        }
    }
    if (m_last_update_data_p != nullptr && update_data != nullptr)
    {
        memcpy(m_last_update_data_p, update_data, m_height * read_pitch);
        m_last_pitch = pitch;
    }
    internal_update(m_last_update_data_p, m_last_pitch, renderer);
}

// 0x004BA880 (:91)
const rl_dev_texture& rl_cached_dyn_texture::get_texture_data() const
{
    return m_buffer[m_buffers.m_current_buffer];
}

// 0x004DFAA0 (:96)
void rl_cached_dyn_texture::get_dimensions(int& width, int& height, int& mip_levels)
{
    width = m_buffer[0].pData->description.width;
    height = m_buffer[0].pData->description.height;
    mip_levels = m_buffer[0].pData->description.levelCount;
}

// 0x004DFAD0 (:101)
void rl_cached_dyn_texture::release_resource()
{
    destroy_buffers();
}

// 0x00520D50 (:106)
void rl_cached_dyn_texture::renew_resource(rl_renderer& renderer)
{
    create_buffers();
    internal_update(m_last_update_data_p, m_last_pitch, renderer);
}

// 0x00509E70 (:112)
// Uploads into the other buffer and makes it current.
void rl_cached_dyn_texture::internal_update(unsigned char* update_data, int pitch, rl_renderer& renderer)
{
    if (update_data == nullptr)
    {
        return;
    }
    const unsigned int buffer = (m_buffers.m_current_buffer - 1) & 1;
    const int format_bytes = get_format_bytes();
    const int read_pitch = get_read_pitch(pitch);
    if (m_buffer[buffer].pData != nullptr)
    {
        renderer.update_dynamic_texture(m_buffer[buffer], update_data, m_height, m_width * format_bytes, read_pitch);
        m_buffers.m_current_buffer = buffer;
    }
}

// 0x004DFAE0 (:130)
int rl_cached_dyn_texture::get_read_pitch(int pitch)
{
    if (pitch == -1)
    {
        return m_width * get_format_bytes();
    }
    return pitch;
}

// 0x004BA890 (:136)
int rl_cached_dyn_texture::get_format_bytes()
{
    if (m_format == RLCDT_FORMAT_A8R8G8B8)
    {
        return 4;
    }
    if (m_format == RLCDT_FORMAT_L8 || m_format == RLCDT_FORMAT_A8)
    {
        return 1;
    }
    return -1;
}

// 0x00509DE0 (:74)
// rl_render_lib::create_dynamic_texture inlined for each buffer.
void rl_cached_dyn_texture::create_buffers()
{
    for (unsigned int i = 0; i < kNumBuffers; i++)
    {
        m_buffer[i] = rl_render_lib::get_instance()->create_dynamic_texture(m_width, m_height, m_format);
    }
}

// 0x004DFA60 (:82)
void rl_cached_dyn_texture::destroy_buffers()
{
    for (int i = kNumBuffers - 1; i >= 0; i--)
    {
        keen::graphics::freeTextureData(m_buffer[i].pData, Init_params.m_pKeenGraphicsSystem, g_pVramAllocator);
        delete m_buffer[i].pData;
        m_buffer[i].pData = nullptr;
    }
}
