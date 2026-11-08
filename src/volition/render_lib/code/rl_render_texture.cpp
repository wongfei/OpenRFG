// Original: volition/render_lib/code/rl_render_texture.cpp
//
// rl_render_texture: a texture with a render target (or a depth target for RL_FORMAT_DEPTH_TEXTURE), exposed to
// shaders through a dynamic texture handle. With msaa_level the target renders into a separate multisampled texture.

#include "volition/render_lib/code/rl_render_texture.h"

#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/texturedata.hpp"

const unsigned int rl_render_texture::kRequiresResolve = 1; // 0x012BECD4

// 0x004C7410 (:5)
rl_render_texture::rl_render_texture()
{
    m_msaa_texture.pData = nullptr;
    m_render_target.pData = nullptr;
    m_depth_stencil_target.pData = nullptr;
    m_dyn_texture_p = nullptr;
}

// 0x00517340 (:10)
rl_render_texture::~rl_render_texture()
{
    destroy();
    if (m_dyn_texture_p != nullptr)
    {
        m_dyn_texture_p->release();
    }
}

// 0x005445D0 (:18)
// Notes (decompiled behaviour, kept as is): with msaa_level the render target is created on the multisampled texture.
void rl_render_texture::init(const int width, const int height, const rl_dev_format format, unsigned int msaa_level, const char* pDebugName, unsigned int initFlags)
{
    m_flags = 0;
    if (m_dyn_texture_p == nullptr)
    {
        m_dyn_texture_p = rl_dyn_texture::create();
    }
    rl_render_lib* render_lib_p = rl_render_lib::get_instance();
    m_dyn_texture_p->set_texture_data(render_lib_p->create_render_texture(width, height, 1, format, 0, pDebugName, initFlags));
    const rl_dev_texture* target_texture_p = &m_dyn_texture_p->get_texture_data();
    if (msaa_level != 0)
    {
        m_msaa_texture = render_lib_p->create_render_texture(width, height, 1, format, msaa_level, "RenderTarget", 0);
        target_texture_p = &m_msaa_texture;
    }
    // rl_render_lib::create_render_target / create_depth_stencil_target inlined.
    if (format != RL_FORMAT_DEPTH_TEXTURE)
    {
        m_render_target = render_lib_p->create_render_target(*target_texture_p);
        return;
    }
    m_depth_stencil_target = render_lib_p->create_depth_stencil_target(*target_texture_p);
}

// 0x004F5AE0 (:44)
void rl_render_texture::destroy()
{
    if (m_depth_stencil_target.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_depth_stencil_target.pData);
        m_depth_stencil_target.pData = nullptr;
    }
    if (m_render_target.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_render_target.pData);
        m_render_target.pData = nullptr;
    }
    if (m_msaa_texture.pData != nullptr)
    {
        keen::graphics::freeTextureData(m_msaa_texture.pData, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
        delete m_msaa_texture.pData;
        m_msaa_texture.pData = nullptr;
    }
    if (m_dyn_texture_p != nullptr)
    {
        keen::TextureData* texture_p = m_dyn_texture_p->get_texture_data().pData;
        keen::graphics::freeTextureData(texture_p, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
        delete texture_p;
        m_dyn_texture_p->release();
        m_dyn_texture_p = nullptr;
    }
    m_flags = 0;
}

// 0x004C7420 (:71)
// Reading the texture clears the pending resolve (kRequiresResolve).
const int rl_render_texture::get_texture_handle() const
{
    if (m_msaa_texture.pData != nullptr && (m_flags & kRequiresResolve) != 0)
    {
        const_cast<rl_render_texture*>(this)->m_flags &= ~kRequiresResolve;
    }
    if (m_dyn_texture_p != nullptr)
    {
        return m_dyn_texture_p->get_texture_handle();
    }
    return -1;
}

// 0x004C7450 (:84)
// Rendering into a multisampled target needs a resolve before the texture is read.
const rl_dev_render_target& rl_render_texture::get_dev_render_target() const
{
    if (m_msaa_texture.pData != nullptr)
    {
        const_cast<rl_render_texture*>(this)->m_flags |= kRequiresResolve;
    }
    return m_render_target;
}

// 0x004C7460 (:95)
const rl_dev_depth_stencil_target& rl_render_texture::get_dev_depth_stencil_target() const
{
    return m_depth_stencil_target;
}

// 0x004BCA60 (.h:19)
const rl_dev_texture& rl_render_texture::get_texture_data() const
{
    return m_dyn_texture_p->get_texture_data();
}
