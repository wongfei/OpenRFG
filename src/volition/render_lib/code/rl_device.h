// Original: volition/render_lib/code/rl_device.h
// Implementation: volition/render_lib/code/rl_device.cpp
//
// The render_lib device layer on top of keen graphics: handles for textures, render and depth targets, state objects,
// index buffers and vertex formats (rl_dev_*), present results and screen resolutions.
#pragma once


namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct FragmentShader;
    struct RasterizerState;
    struct RenderTarget;
    struct SamplerState;
    struct TextureData;
    struct VertexInputBinding;
    struct VertexShader;
}

enum rl_clear_flags
{
    RL_CLEAR_FLAGS_COLOR = 1,
    RL_CLEAR_FLAGS_DEPTH = 2,
    RL_CLEAR_FLAGS_STENCIL = 4,
    RL_CLEAR_FLAGS_COLOR_SLI = 8,
    RL_CLEAR_FLAGS_ALL = -1,
};

enum rl_dev_access
{
    RL_ACCESS_NONE = 0,
    RL_ACCESS_WRITE_ONLY = 1,
    RL_ACCESS_READ_WRITE = 2,
};

enum rl_dev_format
{
    RL_FORMAT_UNKNOWN = 0,
    RL_FORMAT_BC1_UNORM_SRGB = 1,
    RL_FORMAT_BC1_UNORM = 2,
    RL_FORMAT_BC2_UNORM_SRGB = 3,
    RL_FORMAT_BC2_UNORM = 4,
    RL_FORMAT_BC3_UNORM_SRGB = 5,
    RL_FORMAT_BC3_UNORM = 6,
    RL_FORMAT_R8G8B8A8_UNORM_SRGB = 7,
    RL_FORMAT_R8G8B8A8_UNORM = 8,
    RL_FORMAT_R8G8_SNORM = 9,
    RL_FORMAT_B8G8R8A8_UNORM = 10,
    RL_FORMAT_B8G8R8A8_UNORM_SRGB = 11,
    RL_FORMAT_B5G6R5_UNORM = 12,
    RL_FORMAT_B5G6R5_UNORM_SRGB = 13,
    RL_FORMAT_A8_UNORM = 14,
    RL_FORMAT_R8_UNORM = 15,
    RL_FORMAT_R32G32B32A32_FLOAT = 16,
    RL_FORMAT_R16G16B16A16_FLOAT = 17,
    RL_FORMAT_R32G32_FLOAT = 18,
    RL_FORMAT_R16G16_FLOAT = 19,
    RL_FORMAT_R32_FLOAT = 20,
    RL_FORMAT_R16_FLOAT = 21,
    RL_FORMAT_HDR = 22,
    RL_FORMAT_LDR = 23,
    RL_FORMAT_BLEND_LDR = 24,
    RL_FORMAT_SHADOW_MAP = 25,
    RL_FORMAT_DEPTH_TEXTURE = 26,
    RL_FORMAT_DEPTH_BLEND_FLOAT = 27,
    RL_FORMAT_R16G16B16A16_SNORM = 28,
    RL_FORMAT_R8G8_UNORM = 29,
};

enum rl_dev_present_result
{
    RL_ACQUIRED = 0,
    RL_LOST = 1,
    RL_READY = 2,
};

enum rl_dev_primitive_topology
{
    RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 0,
    RL_PRIMITIVE_TOPOLOGY_LINELIST = 1,
    RL_PRIMITIVE_TOPOLOGY_TRIANGLELIST = 2,
    RL_PRIMITIVE_TOPOLOGY_POINTLIST = 3,
};

enum rl_dev_usage
{
    RL_USAGE_DEFAULT = 0,
    RL_USAGE_IMMUTABLE = 1,
    RL_USAGE_DYNAMIC = 2,
    RL_USAGE_STAGING = 3,
};

enum rl_dev_vertex_format
{
    RLVF_INVALID = -1,
    RLVF_PIXLIT = 0,
    RLVF_PIXLIT_CA = 1,
    RLVF_PIXLIT_NMAP = 2,
    RLVF_PIXLIT_NMAP_CA = 3,
    RLVF_UNLIT = 4,
    RLVF_PARTICLE_POINTSPRITE = 5,
    RLVF_PARTICLE_BILLBOARD = 6,
    RLVF_PARTICLE_RADIAL = 7,
    RLVF_PARTICLE_DROP = 8,
    RLVF_PARTICLE_RIBBON = 9,
    RLVF_PARTICLE_ORIENTED = 10,
    RLVF_PRIMITIVE_3D = 11,
    RLVF_PRIMITIVE_2D = 12,
    RLVF_SG_MESH = 13,
    RLVF_HEIGHT_MESH = 14,
    RLVF_HEIGHT_MESH_LOW_LOD = 15,
    RLVF_PARTICLE_PARAMETRIC = 16,
    RLVF_COMPOSITOR = 17,
    RLVF_CLONE_UVS = 18,
    RLVF_CLONE_NMAP = 19,
    RLVF_CLONE_CLR = 20,
    RLVF_2D_SPLINE = 21,
    RLVF_PARTICLE_CORONA = 22,
    RLVF_PARTICLE_RIBBON_PARAMETRIC = 23,
    RLVF_CONDITIONAL_BBOX = 24,
    RLVF_TERRAIN_ROAD = 25,
    RLVF_HEIGHT_MESH_LANDMARK_LOD = 26,
    RLVF_STAR_FIELD_POINT = 27,
    RLVF_STAR_FIELD_BILLBOARD = 28,
    RLVF_METEOR_SHOWER_LINE = 29,
    RLVF_PIXLIT_0UV = 30,
    RLVF_PIXLIT_1UV = 31,
    RLVF_PIXLIT_1UV_CA = 32,
    RLVF_PIXLIT_1UV_NMAP = 33,
    RLVF_PIXLIT_1UV_NMAP_CA = 34,
    RLVF_PIXLIT_2UV = 35,
    RLVF_PIXLIT_2UV_CA = 36,
    RLVF_PIXLIT_2UV_NMAP = 37,
    RLVF_PIXLIT_2UV_NMAP_CA = 38,
    RLVF_PIXLIT_3UV = 39,
    RLVF_PIXLIT_3UV_CA = 40,
    RLVF_PIXLIT_3UV_NMAP = 41,
    RLVF_PIXLIT_3UV_NMAP_CA = 42,
    RLVF_PIXLIT_4UV = 43,
    RLVF_PIXLIT_4UV_CA = 44,
    RLVF_PIXLIT_4UV_NMAP = 45,
    RLVF_PIXLIT_4UV_NMAP_CA = 46,
    RLVF_CLONE_1UV_UVS = 47,
    RLVF_CLONE_2UV_UVS = 48,
    RLVF_UNCOMPRESSED_MORPH = 49,
    RLVF_NUM_FORMATS = 50,
};

// size 0x4
struct rl_dev_occlusion_query
{
    rl_dev_occlusion_query(void* const);
    rl_dev_occlusion_query();
    const bool operator==(const rl_dev_occlusion_query&) const;
    const bool operator!=(const rl_dev_occlusion_query&) const;

    void* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_pixel_shader
{
    rl_dev_pixel_shader();
    const bool operator==(const rl_dev_pixel_shader&) const;
    const bool operator!=(const rl_dev_pixel_shader&) const;

    const keen::FragmentShader* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_vertex_buffer
{
    rl_dev_vertex_buffer(void* const);
    rl_dev_vertex_buffer();
    const bool operator==(const rl_dev_vertex_buffer&) const;
    const bool operator!=(const rl_dev_vertex_buffer&) const;

    void* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_vertex_declaration
{
    rl_dev_vertex_declaration(void* const);
    rl_dev_vertex_declaration();
    const bool operator==(const rl_dev_vertex_declaration&) const;
    const bool operator!=(const rl_dev_vertex_declaration&) const;

    void* m_internal_state_p; // 0x00
};

// size 0x8
struct rl_dev_vertex_shader
{
    rl_dev_vertex_shader();
    const bool operator==(const rl_dev_vertex_shader&) const;
    const bool operator!=(const rl_dev_vertex_shader&) const;

    const keen::VertexShader* m_internal_state_p; // 0x00
    const keen::VertexInputBinding** m_platform_state_p; // 0x04
};

// size 0x4
struct rl_dev_texture
{
    rl_dev_texture(keen::TextureData* pTextureData); // 0x004B6900 (.h:134)
    rl_dev_texture(); // 0x004B68F0 (.h:133)
    const keen::TextureData* operator->() const; // 0x004B6910 (.h:137)
    keen::TextureData* operator->();
    bool operator==(const void*&) const;
    bool operator!=(const void*&) const;
    operator keen::TextureData*() const;

    keen::TextureData* pData; // 0x00
};

// size 0x4
struct rl_dev_blend_state
{
    rl_dev_blend_state(const keen::BlendState* state_p); // 0x004B6940 (.h:178)
    rl_dev_blend_state(); // 0x004B6930 (.h:177)
    const bool operator==(const rl_dev_blend_state&) const;
    const bool operator!=(const rl_dev_blend_state&) const;
    operator const keen::BlendState*() const;

    const keen::BlendState* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_rasterizer_state
{
    rl_dev_rasterizer_state(const keen::RasterizerState* state_p); // 0x004B6970 (.h:190)
    rl_dev_rasterizer_state(); // 0x004B6960 (.h:189)
    const bool operator==(const rl_dev_rasterizer_state&) const;
    const bool operator!=(const rl_dev_rasterizer_state&) const;
    operator const keen::RasterizerState*() const;

    const keen::RasterizerState* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_sampler_state
{
    rl_dev_sampler_state(const keen::SamplerState* state_p); // 0x004B69A0 (.h:202)
    rl_dev_sampler_state(); // 0x004B6990 (.h:201)
    operator const keen::SamplerState*() const;

    const keen::SamplerState* m_internal_state_p; // 0x00
};

// size 0x4
struct rl_dev_depth_stencil_state
{
    rl_dev_depth_stencil_state(const keen::DepthStencilState* state_p); // 0x004B69D0 (.h:212)
    rl_dev_depth_stencil_state(); // 0x004B69C0 (.h:211)
    const bool operator==(const rl_dev_depth_stencil_state&) const;
    const bool operator!=(const rl_dev_depth_stencil_state&) const;
    operator const keen::DepthStencilState*() const;

    const keen::DepthStencilState* m_internal_state_p; // 0x00
};

// size 0x8
struct rl_dev_index_buffer
{
    rl_dev_index_buffer(void* const, void* const);
    rl_dev_index_buffer(); // 0x004B69F0 (.h:248)
    const bool operator==(const rl_dev_index_buffer&) const;
    const bool operator!=(const rl_dev_index_buffer&) const;

    void* m_internal_state_p; // 0x00
    void* m_platform_state_p; // 0x04
};

// size 0x4
struct rl_dev_render_target
{
    rl_dev_render_target(const keen::RenderTarget* pRenderTarget); // 0x004B6A10 (.h:274)
    rl_dev_render_target(); // 0x004B6A00 (.h:273)
    const keen::RenderTarget* operator->();
    bool operator==(const void*&) const;
    bool operator!=(const void*&) const;
    operator const keen::RenderTarget*() const;

    const keen::RenderTarget* pData; // 0x00
};

// size 0x4
struct rl_dev_depth_stencil_target
{
    rl_dev_depth_stencil_target(const keen::RenderTarget* pRenderTarget); // 0x004B6A40 (.h:302)
    rl_dev_depth_stencil_target(); // 0x004B6A30 (.h:301)
    const keen::RenderTarget* operator->(); // 0x004B6A50 (.h:304)
    bool operator==(const void*&) const;
    bool operator!=(const void*&) const;
    operator const keen::RenderTarget*() const;

    const keen::RenderTarget* pData; // 0x00
};

// size 0x10
struct rl_screen_res
{
    rl_screen_res(unsigned short, unsigned short, unsigned short, unsigned int, bool);
    rl_screen_res(); // 0x00883420 (.h:351)

    unsigned short width; // 0x00
    unsigned short height; // 0x02
    unsigned int refresh_rate_num; // 0x04
    unsigned int refresh_rate_den; // 0x08
    unsigned int is_centered : 1; // 0x0C:0
};

// ---- inline members

// 0x00883420 (.h:351)
inline rl_screen_res::rl_screen_res()
    : width(0)
    , height(0)
    , refresh_rate_num(0)
    , refresh_rate_den(1)
    , is_centered(0)
{
}

// 0x004B68F0 (.h:133)
inline rl_dev_texture::rl_dev_texture()
    : pData(nullptr)
{
}

// 0x004B6900 (.h:134)
inline rl_dev_texture::rl_dev_texture(keen::TextureData* pTextureData)
    : pData(pTextureData)
{
}

// 0x004B6910 (.h:137)
inline const keen::TextureData* rl_dev_texture::operator->() const
{
    return pData;
}

// 0x004B6930 (.h:177)
inline rl_dev_blend_state::rl_dev_blend_state()
    : m_internal_state_p(nullptr)
{
}

// 0x004B6940 (.h:178)
inline rl_dev_blend_state::rl_dev_blend_state(const keen::BlendState* state_p)
    : m_internal_state_p(state_p)
{
}

// 0x004B6960 (.h:189)
inline rl_dev_rasterizer_state::rl_dev_rasterizer_state()
    : m_internal_state_p(nullptr)
{
}

// 0x004B6970 (.h:190)
inline rl_dev_rasterizer_state::rl_dev_rasterizer_state(const keen::RasterizerState* state_p)
    : m_internal_state_p(state_p)
{
}

// 0x004B6990 (.h:201)
inline rl_dev_sampler_state::rl_dev_sampler_state()
    : m_internal_state_p(nullptr)
{
}

// 0x004B69A0 (.h:202)
inline rl_dev_sampler_state::rl_dev_sampler_state(const keen::SamplerState* state_p)
    : m_internal_state_p(state_p)
{
}

// 0x004B69C0 (.h:211)
inline rl_dev_depth_stencil_state::rl_dev_depth_stencil_state()
    : m_internal_state_p(nullptr)
{
}

// 0x004B69D0 (.h:212)
inline rl_dev_depth_stencil_state::rl_dev_depth_stencil_state(const keen::DepthStencilState* state_p)
    : m_internal_state_p(state_p)
{
}

// 0x004B69F0 (.h:248)
inline rl_dev_index_buffer::rl_dev_index_buffer()
    : m_internal_state_p(nullptr)
    , m_platform_state_p(nullptr)
{
}

// 0x004B6A00 (.h:273)
inline rl_dev_render_target::rl_dev_render_target()
    : pData(nullptr)
{
}

// 0x004B6A10 (.h:274)
inline rl_dev_render_target::rl_dev_render_target(const keen::RenderTarget* pRenderTarget)
    : pData(pRenderTarget)
{
}

// 0x004B6A30 (.h:301)
inline rl_dev_depth_stencil_target::rl_dev_depth_stencil_target()
    : pData(nullptr)
{
}

// 0x004B6A40 (.h:302)
inline rl_dev_depth_stencil_target::rl_dev_depth_stencil_target(const keen::RenderTarget* pRenderTarget)
    : pData(pRenderTarget)
{
}

// 0x004B6A50 (.h:304)
inline const keen::RenderTarget* rl_dev_depth_stencil_target::operator->()
{
    return pData;
}
