// Original: volition/render_lib/code/rl_render_texture.h
// Implementation: volition/render_lib/code/rl_render_texture.cpp
//
// rl_render_texture, a texture that can be rendered to (color and/or depth).
#pragma once

#include "volition/render_lib/code/rl_device.h"

class rl_dyn_texture;

// size 0x14
class rl_render_texture
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred from rl_renderer::copy_render_texture.
    friend class rl_renderer;
    // Access inferred from rl_hdr_compositor::create_render_textures.
    friend class rl_hdr_compositor;
    // Access inferred from rl_outline_compositor::initialize.
    friend class rl_outline_compositor;
public:
    rl_render_texture(); // 0x004C7410 (:5)
    ~rl_render_texture(); // 0x00517340 (:10)
    void init(const int width, const int height, const rl_dev_format format, unsigned int msaa_level, const char* pDebugName, unsigned int initFlags); // 0x005445D0 (:18)
    void destroy(); // 0x004F5AE0 (:44)
    const int get_texture_handle() const; // 0x004C7420 (:71)
    const rl_dev_texture& get_texture_data() const; // 0x004BCA60 (.h:19)
    const rl_dev_render_target& get_dev_render_target() const; // 0x004C7450 (:84)
    const rl_dev_depth_stencil_target& get_dev_depth_stencil_target() const; // 0x004C7460 (:95)

private:
    static const unsigned int kRequiresResolve; // 0x012BECD4
    rl_dyn_texture* m_dyn_texture_p; // 0x00
    rl_dev_texture m_msaa_texture; // 0x04
    rl_dev_render_target m_render_target; // 0x08
    rl_dev_depth_stencil_target m_depth_stencil_target; // 0x0C
    unsigned int m_flags; // 0x10
};
