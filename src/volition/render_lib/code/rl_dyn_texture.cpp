// Original: volition/render_lib/code/rl_dyn_texture.cpp
//
// Dynamic textures: each rl_dyn_texture_base takes a tracker slot, and its texture handle encodes the slot index (bits
// 0-15), a guard (bits 16-30) and the dynamic texture marker (bit 24, which overlaps the guard). rl_renderer's
// get_dev_texture resolves such handles through find_by_handle.

#include "volition/render_lib/code/rl_dyn_texture.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/base/trace.hpp"

// size 0x4
struct dyn_tex_tracker
{
    rl_dyn_texture_base* ptr; // 0x00
};

static object_pool<dyn_tex_tracker, 256, 1> Tracker_pool; // 0x01C226D0
static int Next_dyn_texture_guard; // 0x01B7BC80

// :106
// <rl_dyn_texture::release> 0x004BEBC0 (:106)
// <rl_dyn_texture::create> 0x0053AA20 (:106)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_dyn_texture)

// 0x0050DEC0 (:48)
// Notes (decompiled behaviour, kept as is): the guard is masked to 15 bits and bit 24 is set over it.
rl_dyn_texture_base::rl_dyn_texture_base()
{
    m_tex_handle = -1;
    dyn_tex_tracker* tracker_p = Tracker_pool.alloc(0);
    if (tracker_p == nullptr)
    {
        keen::debug::nativePrintArgs("Out of trackers for dynamic textures");
        return;
    }
    tracker_p->ptr = this;
    // Magic numbers: the guard in bits 16-30, the dynamic texture marker 0x1000000.
    m_tex_handle = Tracker_pool.get_index(tracker_p) | (Next_dyn_texture_guard & 0x7FFF) << 16;
    m_tex_handle |= 0x1000000;
    Next_dyn_texture_guard++;
}

// Frees the tracker of a texture handle if it still belongs to the texture.
static void free_tracker(int tex_handle, const rl_dyn_texture_base* texture_p)
{
    if (tex_handle == -1)
    {
        return;
    }
    const unsigned int index = (unsigned int)tex_handle & 0xFFFF;
    if (index >= Tracker_pool.get_capacity())
    {
        return;
    }
    dyn_tex_tracker* tracker_p = Tracker_pool.get_entry(index);
    if (tracker_p != nullptr && tracker_p->ptr == texture_p)
    {
        Tracker_pool.free(tracker_p);
        tracker_p->ptr = nullptr;
    }
}

// 0x005240C0 (:66)
rl_dyn_texture_base::~rl_dyn_texture_base()
{
    free_tracker(m_tex_handle, this);
}

// 0x004E4D90 (:77)
rl_dyn_texture_base* rl_dyn_texture_base::find_by_handle(int tex_handle)
{
    if (tex_handle == -1)
    {
        return nullptr;
    }
    const unsigned int index = (unsigned int)tex_handle & 0xFFFF;
    if (index >= Tracker_pool.get_capacity())
    {
        return nullptr;
    }
    rl_dyn_texture_base* texture_p = Tracker_pool.get_entry(index)->ptr;
    if (texture_p == nullptr || texture_p->m_tex_handle != tex_handle)
    {
        return nullptr;
    }
    return texture_p;
}

// 0x0050DF50 (:89)
// Releases the device resources of every dynamic texture (before a device reset or resize).
void rl_dyn_texture_base::release_resources()
{
    for (dyn_tex_tracker* tracker_p = Tracker_pool.get_first(0); tracker_p != nullptr; tracker_p = Tracker_pool.get_next(tracker_p))
    {
        if (tracker_p->ptr != nullptr)
        {
            tracker_p->ptr->release_resource();
        }
    }
}

// 0x0050DFC0 (:98)
void rl_dyn_texture_base::renew_resources(rl_renderer& renderer)
{
    for (dyn_tex_tracker* tracker_p = Tracker_pool.get_first(0); tracker_p != nullptr; tracker_p = Tracker_pool.get_next(tracker_p))
    {
        if (tracker_p->ptr != nullptr)
        {
            tracker_p->ptr->renew_resource(renderer);
        }
    }
}

// 0x004B7500 (.h:26)
const int rl_dyn_texture_base::get_texture_handle() const
{
    return m_tex_handle;
}

// 0x0050DF30 (.h:39)
void rl_dyn_texture_base::release_resource()
{
}

// 0x0050DF40 (.h:40)
void rl_dyn_texture_base::renew_resource(rl_renderer&)
{
}

// 0x00524120 (:110)
rl_dyn_texture::rl_dyn_texture()
{
    m_dev_texture.pData = nullptr;
}

// 0x00524150 (:117)
// The texture data belongs to the owner (rl_render_texture frees it).
rl_dyn_texture::~rl_dyn_texture()
{
}

// 0x004E4DD0 (:122)
void rl_dyn_texture::get_dimensions(int& width, int& height, int& mip_levels)
{
    width = m_dev_texture.pData->description.width;
    height = m_dev_texture.pData->description.height;
    mip_levels = m_dev_texture.pData->description.levelCount;
}

// 0x004B7510 (.h:53)
void rl_dyn_texture::set_texture_data(const rl_dev_texture& texture)
{
    m_dev_texture.pData = texture.pData;
}

// 0x00524140 (.h:54)
const rl_dev_texture& rl_dyn_texture::get_texture_data() const
{
    return m_dev_texture;
}
