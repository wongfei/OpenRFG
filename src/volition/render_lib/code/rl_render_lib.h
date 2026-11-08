// Original: volition/render_lib/code/rl_render_lib.h
// Implementation: volition/render_lib/code/rl_render_lib.cpp
//
// rl_render_lib, the render_lib singleton: initialization from rl_init_params, the keen graphics system, the
// precreated device states for the primitive macro states (blend, depth/stencil, rasterizer per mode), vertex formats
// and the frame-level services.
#pragma once

#include "keen/base/win32/mutex_win32.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/occlusionquery.hpp"
#include "keen/graphics/pixelformat.hpp"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_cached_dyn_texture.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/vlib/code/memory/vmemory.h"

enum DXGI_FORMAT;
struct bitmap_entry;
class matrix44;
class rl_class_dynamic_info;
class rl_init_params;
class rl_rect;
class rl_renderer;
class rl_renderer_metrics;
struct rl_shader_const_fixup;
struct rl_shader_header_runtime;
struct rl_shader_tex_fixup;
struct rl_shader_tex_fixup_runtime;
class rl_stamped_handle;
class vector;

namespace keen
{
    struct GraphicsSystem;
    class MemoryAllocator;
    struct SamplerState;
    struct TextureData;
    struct TextureDescription;
    struct VertexFormat;
}

enum rl_warning_flags
{
    RLWF_DYNAMIC_INDICES_AVAILABLE = 1,
    RLWF_INSTANCE_BUFFER_SPACE = 2,
    RLWF_MAX_INSTANCE_BLOCKS = 4,
    RLWF_MAX_RENDER_GROUPS = 8,
    RLWF_MAX_DECALS = 16,
    RLWF_MAX_DECAL_BLOCKS = 32,
    RLWF_MAX_CLONE_RENDER_GROUPS = 64,
    RLWF_HARDWARE_BUFFER_SPACE = 128,
    RLWF_MORPH_WEIGHT_SPACE = 256,
    RLWF_UNBATCHED_RENDERABLES = 512,
    RLWF_BATCHED_MATERIALS = 1024,
    RLWF_MAX_RENDER_CALLS = 2048,
    RLWF_VIS_LIST_ENTRIES = 4096,
};

// size 0x954
class rl_render_lib : public rl_base_object, public rl_singleton<rl_render_lib>
{
public:
    enum RenderTextureFlags
    {
        RenderTextureFlags_ESRAM = 1,
    };

    enum DepthStencilTargetFlags
    {
        DepthStencilTargetFlags_HTile = 1,
        DepthStencilTargetFlags_ESRAM = 2,
    };

    // size 0x8
    struct multisample_desc
    {
        unsigned int m_samples; // 0x00
        unsigned int m_quality : 31; // 0x04:0
        unsigned int m_enabled : 1; // 0x04:31
    };

protected:
    rl_render_lib(); // 0x00543D90 (:703)
    virtual ~rl_render_lib(); // vtbl+0x00 0x00516CF0 (:720)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_render_lib* create(); // 0x0054D3B0 (:36)
    static bool can_create(); // 0x004C5D20 (:36)
    virtual void release(); // vtbl+0x04 0x004C5D30 (:36)
    void initialize(const rl_init_params& params); // 0x00516DD0 (:766)
    rl_dev_texture create_texture_2d(const int width, const int height, const int mip_levels, const rl_dev_format format, const rl_dev_usage usage, const rl_dev_access access, const void* const data_p, unsigned int flags); // 0x004F4100 (:2400)
    void destroy_texture_2d(const rl_dev_texture& texture); // 0x004C72B0 (:2415)
    bool load_texture(bitmap_entry& be); // 0x004F3730 (:827)
    bool unload_texture(bitmap_entry& be); // 0x004C6390 (:959)
    rl_renderer_metrics* get_metrics() const; // 0x004B7B60 (rl_renderer.h:549)
    unsigned short get_clamped_sampler_handle(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags, keen::SamplerParameters* params_p); // 0x004C70E0 (:2294)
    unsigned short get_clamped_sampler_handle(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags); // 0x004C7290 (:2393)
    rl_shader_header_runtime* load_shader(const char* const shader_fname); // 0x00543FA0 (:1084)
    void unload_shader(rl_shader_header_runtime* shader_p); // 0x004C6400 (:1327)
    rl_shader_header_runtime* createKgShader(const keen::ShaderPipeline** pPipelines, const rl_shader_tex_fixup* pTexFixups, unsigned int numTex, const rl_shader_const_fixup* pConstFixUps, unsigned int numConsts); // 0x005171B0 (:1348)
    void deleteKgShader(rl_shader_header_runtime* shader_p); // 0x004C6410 (:1395)
    const rl_dev_texture create_dynamic_texture(unsigned int width, unsigned int height, rl_cached_dyn_texture_format format); // 0x004F3ED0 (:2009)
    void get_dynamic_texture_dimensions(const rl_dev_texture tex, int& width, int& height, int& mip_levels); // 0x004C6D60 (:2042)
    void destroy_dynamic_texture(rl_dev_texture tex); // 0x004C6D30 (:2037)
    const rl_dev_texture create_render_texture(const int width, const int height, const int mip_levels, const rl_dev_format format, unsigned int msaa_level, const char* pDebugName, unsigned int initFlags); // 0x004C6D90 (:2049)
    void destroy_render_texture(const rl_dev_texture& texture); // 0x004C6E70 (:2079)
    const rl_dev_render_target create_render_target(const rl_dev_texture& texture); // 0x004C6EA0 (:2084)
    void destroy_render_target(const rl_dev_render_target& rt); // 0x004C6F20 (:2105)
    const rl_dev_depth_stencil_target create_depth_stencil_target(const rl_dev_texture& texture); // 0x004C7010 (:2150)
    const rl_dev_depth_stencil_target create_depth_stencil_target(const unsigned int width, const unsigned int height, const unsigned int msaa_level, const char* pDebugName, unsigned int flags); // 0x004C6F40 (:2110)
    void destroy_depth_stencil_target(const rl_dev_depth_stencil_target& ds); // 0x004C7080 (:2201)
    void update_screen_info(int refresh_num, int refresh_denom); // 0x004F3F60 (:2170)
    void set_screen_info(const rl_screen_info& screen_info, const rl_level_of_detail* level_of_detail_p); // 0x00517300 (:2176)
    const rl_screen_info* get_screen_info() const; // 0x004C7060 (:2165)
    void set_level_of_detail(const rl_level_of_detail& lod); // 0x004F4000 (:2185)
    const rl_level_of_detail& get_level_of_detail() const; // 0x004C7070 (:2196)
    void validate_msaa_mode(); // 0x004C72F0 (:2426)
    void set_vsync_enabled(bool enabled); // 0x004C63C0 (:969)
    bool get_vsync_enabled() const; // 0x004C63F0 (:979)
    unsigned int get_current_frame_count() const; // 0x004B7B40 (rl_renderer.h:538)
    unsigned int get_num_gpus() const; // 0x004B7B50 (rl_renderer.h:543)
    bool is_d3d10() const; // 0x004B7930 (.h:105)
    bool supports_native_pcf() const;
    bool supports_depth_texture_shadow_maps() const; // 0x004B7940 (.h:107)
    bool supports_fetch4() const;
    const keen::VertexFormat* get_keen_vertex_format(const rl_dev_vertex_format format); // 0x004C72E0 (:2420)
    void recreate_engine_rasterizer_states(); // 0x004F3EC0 (:1987)
    void initialize_engine_rasterizer_states(); // 0x004F3D70 (:1847)
    void initialize_primitive_macro_states(); // 0x004F3B00 (:1401)
    void destroy_engine_rasterizer_states(); // 0x004C6C40 (:1963)
    void destroy_primitive_macro_states(); // 0x004C6420 (:1514)
    void initialize_stencil_states(keen::DepthStencilParameters& desc, rl_prim_zbuf_mode zmode); // 0x004C6820 (:1700)
    void initialize_alpha_states(keen::BlendParametersEx& desc, rl_prim_color_write_mode cwm); // 0x004C64C0 (:1556)
    rl_dev_blend_state get_blend_state_from_prim_alpha_mode(rl_prim_alpha_mode mode, rl_prim_color_write_mode color_write, bool undefined_override); // 0x004C6CC0 (:1993)
    rl_dev_depth_stencil_state get_depth_stencil_state_from_prim_stencil_zbuf_mode(rl_prim_stencil_mode stencil_mode, rl_prim_zbuf_mode zbuf_mode); // 0x004C6CF0 (:1999)
    rl_dev_sampler_state get_sampler_state_from_prim_clamp_mode(rl_prim_clamp_mode mode); // 0x004C6D10 (:2004)
    rl_dev_sampler_state get_sampler_state_from_tex_flags(bool, bool, bool);
    const keen::SamplerState* create_and_cache_clamped_sampler_state(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags); // 0x004F4030 (:2382)
    void processTexFixups(rl_shader_header_runtime* shader_runtime, const rl_shader_tex_fixup* pFixups, unsigned int numTex); // 0x00516FB0 (:984)
    void set_frametime_total_ms(unsigned int frametime_total_ms); // 0x007B99B0 (.h:134)
    unsigned int get_frametime_total_ms() const; // 0x004B7950 (.h:135)
    void next_frame(); // 0x004C6360 (:821)
    void check_device_status(); // 0x0054D3F0 (:2210)
    bool hasTargetSizeChanged() const; // 0x004B7960 (.h:140)
    const unsigned int get_num_msaa_modes(); // 0x004C6290 (:741)
    void get_msaa_modes(rl_antialias_lod* modes_p); // 0x004C62E0 (:752)
    rl_stamped_handle occlusion_query_issue_begin(rl_renderer& renderer); // 0x004C73D0 (:2456)
    void occlusion_query_issue_end(rl_stamped_handle handle, rl_renderer& renderer); // 0x004F41A0 (:2476)
    int occlusion_query_get_pixels_drawn(rl_stamped_handle handle, rl_renderer& renderer, bool wait); // 0x004F41E0 (:2487)
    void occlusion_query_release_handle(rl_stamped_handle handle); // 0x004F4240 (:2507)
    unsigned int get_occlusion_query_actual_meshes_drawn() const;
    unsigned int get_occlusion_query_meshes_drawn() const;
    void lockInputBindingMutex(); // 0x004B7970 (.h:161)
    void unlockInputBindingMutex(); // 0x004B7980 (.h:162)

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x015151FC
    rl_dev_blend_state m_prim_blend_states[22][8]; // 0x0C
    rl_dev_sampler_state m_prim_sampler_states[2]; // 0x2CC
    rl_dev_depth_stencil_state m_prim_depth_stencil_states[13][5]; // 0x2D4
    const keen::VertexFormat* m_vertex_formats_p[50]; // 0x3D8
    static const int kNumEngineFillModes; // 0x012B8084
    static const int kNumEngineCullModes; // 0x012B8088
    static const int kNumEngineScissorModes; // 0x012B808C
    static const int kNumEngineMsaaModes; // 0x012B8090
    static const unsigned int kDepthBiasLevels; // 0x012B8094
    rl_dev_rasterizer_state m_engine_rasterizer_states[5][2][3][2]; // 0x4A0
    static const unsigned int kNumUAddressModes; // 0x012B8098
    static const unsigned int kNumVAddressModes; // 0x012B809C
    static const unsigned int kNumWAddressModes; // 0x012B80A0
    rl_dev_sampler_state m_material_sampler_states[2][2][2]; // 0x590
    rl_renderer_metrics* m_metrics_p; // 0x5B0
    rl_level_of_detail m_level_of_detail; // 0x5B4
    rl_level_of_detail m_new_level_of_detail; // 0x648
    unsigned int m_current_frame_count; // 0x6DC
    float m_texel_offset; // 0x6E0
    unsigned int m_frametime_total_ms; // 0x6E4
    unsigned int m_num_gpus; // 0x6E8
    mempool m_shader_base_mempool; // 0x6EC
    rl_screen_info m_screen_info; // 0x73C
    int m_last_width; // 0x7DC
    int m_last_height; // 0x7E0
    keen::graphics::WindowMode m_lastWindowMode; // 0x7E4
protected:
    multisample_desc m_msaa_level[7]; // 0x7E8
    bool m_lodChanged; // 0x820
    bool m_sizesChanged; // 0x821
    keen::OcclusionQueryManager m_occlusionQueries; // 0x824
    keen::Mutex m_inputBindingMutex; // 0x91C
};

extern keen::MemoryAllocator* g_pVramAllocator; // 0x01B6AAF4
extern keen::MemoryAllocator* g_pFramebufferAllocator; // 0x01B7BC0C
extern rl_dev_sampler_state rl_Clamped_sampler_states[5184]; // 0x01C27400
extern unsigned short rl_Clamped_sampler_states_to_create[1]; // 0x014FA450

void unused_dcf_refresh_rasterizer_states(); // 0x00516C30 (:40)
void build_screen_info(keen::GraphicsSystem* pGraphicsSystem, rl_screen_info& info); // 0x004F3250 (:157)
keen::PixelFormat keen_format_from_dev_format(rl_dev_format format); // 0x004C5D50 (:172)
unsigned char keen_access_from_dev_access_and_usage(rl_dev_access access, rl_dev_usage usage); // 0x004C5E50 (:211)
void fixup_level_count(keen::TextureDescription& desc); // 0x004C5E70 (:226)
DXGI_FORMAT d3d_format_from_dev_format(rl_dev_format dev_format); // 0x004C5FC0 (:311)
void get_pitch(DXGI_FORMAT d3d_fmt, int w, int h, int& pitch, int& pitch_rows, int& min_pitch); // 0x004C60E0 (:350)
unsigned char* convert_565_to_8888(const int width, const int height, const int mip_levels, const int sides, const void* const data_p, int pitch, bool rgba); // 0x004F32D0 (:424)
keen::TextureData* create_keen_texture_data(keen::GraphicsSystem* graphics_system_p, keen::TextureDescription& desc, const rl_dev_format format, const void* data_p, bool cpu_data, const char* debug_name_p); // 0x004F3490 (:548)
void destroy_keen_texture_data(keen::GraphicsSystem* graphics_system_p, const rl_dev_texture& texture, keen::MemoryAllocator* pAllocator); // 0x004C6260 (:697)
unsigned int getFilterIndex(keen::SamplerFilterMode filter); // 0x004C70A0 (:2259)
unsigned int getAddressIndex(keen::SamplerAddressMode address); // 0x004C70C0 (:2276)
bool is_clamped_sampler_state_needed(unsigned short idx); // 0x004C7270 (:2372)
bool rl_project_get_bbox_2d_rect_ex(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h, const rl_rect& screen_rect, const matrix44& clip_screen, const rl_renderer& renderer); // 0x004F53A0 (:2625)
bool rl_project_get_bbox_2d_rect_clipped_ex(rl_rect& scissor_rect_r, const vector& bmin, const vector& bmax, const rl_rect& screen_rect, const matrix44& clip_screen, const rl_renderer& renderer); // 0x004F5990 (:2680)
