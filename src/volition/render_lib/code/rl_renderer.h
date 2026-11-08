// Original: volition/render_lib/code/rl_renderer.h
// Implementation: volition/render_lib/code/rl_renderer.cpp
//
// rl_renderer, a render context recording into a keen::GraphicsCommandWriter: shader and material state, constant
// buffers (common, instance, decal, material), render targets, viewport and the primitive macro state. The game uses a
// main and a deferred renderer (do_game_render_frame: begin, game_render_do_frame, end, flip).
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/primitivetype.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/render_lib/code/rl_state_defines.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/shader/commonshaderdata.hpp"
#include "volition/render_lib/code/shader/decalshaderdata.hpp"
#include "volition/render_lib/code/shader/instanceshaderdata.hpp"

class matrix;
class matrix43;
class matrix44;
class rl_camera;
class rl_color;
class rl_color_float;
struct rl_constant_binding;
class rl_init_params;
class rl_light;
struct rl_material_data;
class rl_mesh;
class rl_primitive_state;
class rl_render_lib;
class rl_render_texture;
class rl_shader;
struct rl_texture_binding;
struct rl_vertex_format;
class vector;
class vector4;

namespace keen
{
    struct DynamicConstantBuffer;
    struct GeometryInstancingBatch;
    struct GeometryInstancingBuffer;
    class GraphicsCommandWriter;
    struct RenderTarget;
    struct ShaderPipeline;
    struct VertexInputBinding;
}

// size 0x500
class rl_renderer
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred from rl_primitive_renderer::end_primitive and render_primitive.
    friend class rl_primitive_renderer;
    // Access inferred: rl_gpu_vis_render sets the state manager's shader type, flags and world transform directly.
    friend void rl_gpu_vis_render(rl_renderer& renderer, rl_camera& camera);
    friend void rl_gpu_vis_render_debug(rl_renderer& renderer, rl_camera& camera);
    // Access inferred: rl_compiled_render_group::render_immediate writes the instance transform constants and draws
    // through the command writer.
    friend class rl_compiled_render_group;
    // Access inferred: rl_compositor::downsample and the blurs bind textures through the command writer.
    friend class rl_compositor;
    // Access inferred: rl_star_field::render draws its geometry through the command writer.
    friend class rl_star_field;
    // Access inferred: rl_skin_instance::render unbinds the morph vertex stream (m_vertexBufferData[1]).
    friend class rl_skin_instance;
    // Access inferred: rl_morph_instance binds its morph vertex streams and fills its weights through the command writer.
    friend class rl_morph_instance;
    // Access inferred (stub_impl.md S3): the morph container and the dynamic morph instance bind vertex stream 1; the
    // dynamic instance maps its buffer through the command writer.
    friend class rl_morph_container;
    friend class rl_dynamic_morph_instance;
    // Access inferred: rl_layered_material sets the override shader and hands its layers the renderer's state store.
    // Deviation from rfg.exe: a friend added for the port (the PDB records none).
    friend class rl_layered_material;
public:
    enum ConstantData
    {
        ConstantData_CommonVertexData = 0,
        ConstantData_CommonFragmentData = 1,
        ConstantData_InstanceVertexData = 2,
        ConstantData_InstanceFragmentData = 3,
        ConstantData_DecalVertexData = 4,
        ConstantData_MaterialData = 5,
    };

    enum ConstantDataMask
    {
        ConstantDataMask_CommonVertexData = 1,
        ConstantDataMask_CommonFragmentData = 2,
        ConstantDataMask_InstanceVertexData = 4,
        ConstantDataMask_InstanceFragmentData = 8,
        ConstantDataMask_DecalVertexData = 16,
        ConstantDataMask_MaterialData = 32,
    };

    rl_renderer(); // 0x004F1990 (:147)
    ~rl_renderer(); // 0x004C4E20 (:155)
    void initialize(const rl_init_params& params); // 0x0052D030 (:91)
    void begin(keen::GraphicsCommandWriter* pCommandWriter); // 0x0052D3B0 (:159)
    void end(); // 0x00515D50 (:204)
    void updateResourceSizes(keen::GraphicsCommandWriter* pCommandWriter, bool force, bool highQualityOnConsoles); // 0x0055D060 (:212)
    keen::GraphicsCommandWriter* getCommandWriter(); // 0x004B7A30 (.h:128)
    void clear(const rl_color_float& clear_color, unsigned int clear_flags, float z_value, unsigned int stencil_value); // 0x004C4FD0 (:268)
    void clear(const rl_color& clear_color, unsigned int clear_flags, float z_value, unsigned int stencil_value); // 0x004C4E40 (:245)
    void set_texture(const unsigned int tmu, const int handle); // 0x004F2360 (:1133)
    void set_texture(unsigned int tmu, const rl_dev_texture& texture); // 0x004C5AB0 (:1603)
    void handshake_begin(); // 0x0054CFC0 (:1010)
    void set_active_camera(rl_camera* camera_p); // 0x004F20C0 (:1038)
    rl_camera* get_active_camera() const; // 0x004F20D0 (:1043)
    void set_macro_state(const rl_primitive_state& state, const bool set_sampler_states); // 0x00515EC0 (:382)
    const rl_primitive_state& get_macro_state() const; // 0x004F1AA0 (:412)
    void copy(const rl_renderer& other); // 0x004F3050 (:1707)
    void flip(); // 0x004C4E30 (:229)
    rl_material_state_store* get_state_store(); // 0x004B7A40 (.h:153)
    void set_current_shader(rl_shader* const shader, const int technique); // 0x00515FC0 (:417)
    void get_current_shader(rl_shader*& shader, int& technique) const; // 0x004F1AB0 (:439)
    void clear_current_shader(); // 0x005160A0 (:444)
    void set_shader_type(const rl_shader_type type); // 0x004F1AD0 (:449)
    const rl_shader_type get_shader_type() const; // 0x004F1AE0 (:454)
    void clear_shader_type(); // 0x004F1AF0 (:459)
    void set_shader_configuration(const int config); // 0x004F1B00 (:464)
    const int get_shader_configuration() const; // 0x004F1B10 (:469)
    void clear_shader_configuration(); // 0x004F1B20 (:474)
    bool set_active_shader(unsigned int handle, rl_shader_technique_type technique_type, int user_technique_idx); // 0x00516B90 (:1653)
    void set_override_shader(unsigned int handle); // 0x004C5B20 (:1692)
    void clear_override_shader(); // 0x004C5B30 (:1697)
    void set_vertex_shader_constant(unsigned int, const unsigned int*, unsigned int);
    void set_vertex_shader_constant(unsigned int start_register, const float* data_p, unsigned int num_consts); // 0x004C57D0 (:1138)
    void set_pixel_shader_constant(unsigned int start_register, const unsigned int* data_p, unsigned int num_consts); // 0x004DCA40 (.h:181)
    void set_pixel_shader_constant(unsigned int start_register, const float* data_p, unsigned int num_consts); // 0x004C5810 (:1149)
    void set_material(const rl_shader_technique_type override_technique_type, const int override_technique_index, rl_material_data* mat_data_p); // 0x0052D960 (:728)
    void cleanup_material(); // 0x004C5AA0 (:1598)
    void update_dynamic_texture(const rl_dev_texture tex, const unsigned char* data_p, unsigned int, unsigned int, unsigned int read_pitch); // 0x004F2030 (:1016)
    void resolve_msaa_target(const rl_dev_render_target&, const rl_dev_texture&); // 0x004C5A90 (:1570)
    const rl_dev_render_target& get_render_target(); // 0x004C57B0 (:1123)
    const rl_dev_depth_stencil_target& get_depth_stencil_target(); // 0x004C57C0 (:1128)
    void begin_pass(const keen::RenderTarget* pRenderTarget); // 0x004C5680 (:1096)
    void begin_pass(const rl_dev_render_target& rt, const rl_dev_depth_stencil_target& ds); // 0x004F2150 (:1075)
    void end_pass(); // 0x004F21C0 (:1107)
    void set_viewport(const rl_rect& vp); // 0x004C51C0 (:291)
    void set_viewport_z_range(const float z_near, const float z_far); // 0x004C52A0 (:305)
    void get_viewport(rl_rect& vp) const; // 0x004C52D0 (:312)
    void get_viewport_z_range(float& z_near, float& z_far) const; // 0x004C52F0 (:317)
    void set_scissor_region(const rl_rect& scissor_rect); // 0x00515D90 (:323)
    void get_scissor_region(rl_rect& rect) const; // 0x004F1A00 (:337)
    void set_tint_color(const rl_color_float& tint_color); // 0x00516170 (:845)
    const rl_color_float& get_tint_color() const; // 0x004F1F80 (:876)
    void set_fade_color(const rl_color_float& fade_color); // 0x005160C0 (:835)
    const rl_color_float& get_fade_color() const; // 0x004F1F70 (:871)
    void set_ambient_color(const rl_color_float& ambient_color); // 0x00516460 (:960)
    const rl_color_float& get_ambient_color() const; // 0x004F2010 (:979)
    void set_back_ambient_color(const rl_color_float& back_ambient_color); // 0x00516590 (:984)
    const rl_color_float& get_back_ambient_color() const; // 0x004F2020 (:1000)
    void set_saturation(float saturation0, float saturation1); // 0x00516260 (:863)
    void set_saturation(float saturation); // 0x00516220 (:855)
    const float get_saturation() const; // 0x004F1F90 (:881)
    void set_fog_parameters(float height_density, float atmosphere_scale, float density, float density_offset); // 0x005162A0 (:886)
    void get_fog_parameters(float& height_density, float& atmosphere_scale, float& density, float& density_offset) const; // 0x004F1FA0 (:895)
    void set_fog_color(const rl_color_float& fog_color); // 0x00516310 (:900)
    const rl_color_float& get_fog_color() const; // 0x004F1FE0 (:908)
    void set_eye_position(const vector& eye_pos); // 0x00516370 (:913)
    const vector& get_eye_position() const; // 0x004F1FF0 (:941)
    void set_eye_orientation(const matrix& orientation); // 0x004C55B0 (:921)
    void set_time_value(const vector4& time_val); // 0x005163D0 (:946)
    const vector4& get_time_value() const; // 0x004F2000 (:955)
    void set_primitive_color(const rl_color_float& primitive_color); // 0x00515E40 (:342)
    void get_primitive_color(rl_color_float& color) const; // 0x004F1A30 (:347)
    void set_primitive_texture(int tex); // 0x00543B20 (:352)
    int get_primitive_texture() const; // 0x004F1A50 (:357)
    void set_primitive_bink_textures(int y_texture, int cr_texture, int cb_texture); // 0x00515E70 (:372)
    void get_primitive_bink_textures(int& y_texture, int& cr_texture, int& cb_texture) const; // 0x004F1A70 (:377)
    void set_primitive_material(unsigned int mat); // 0x0052D560 (:362)
    unsigned int get_primitive_material() const; // 0x004F1A60 (:367)
    void set_vertex_format(rl_dev_vertex_format id); // 0x004F1B30 (:479)
protected:
    const rl_vertex_format get_vertex_format(const rl_dev_vertex_format);
public:
    const rl_dev_vertex_format get_vertex_format() const; // 0x004F1B50 (:485)
    const bool get_depth_only() const; // 0x004F1ED0 (:800)
    void set_depth_only(const bool depth_only); // 0x004F1EE0 (:805)
    unsigned int get_depth_only_shader() const; // 0x004F1F00 (:810)
    const bool get_depth_requires_pixel_shader() const; // 0x004F1F10 (:815)
    void set_depth_requires_pixel_shader(const bool depth_required); // 0x004F1F20 (:820)
    const bool get_is_base_pass() const; // 0x004F1F40 (:825)
    void set_is_base_pass(const bool is_base_pass); // 0x004F1F50 (:830)
    void set_object_instance_parameters(const vector4& inst_params); // 0x004B7A50 (.h:279)
    void set_object_transform(const matrix43& transform); // 0x004B7A90 (.h:285)
    void set_bone_transforms(const matrix43* bone_matrices, int num_bone_matrices); // 0x004F2790 (:1299)
    void set_terrain_subzone_offset(const vector& offset); // 0x004C5910 (:1410)
    void set_target_dimensions(const unsigned int width, const unsigned int height); // 0x004F2A30 (:1430)
    void get_target_dimensions(unsigned int& width, unsigned int& height); // 0x004F2B20 (:1442)
    void set_light(const rl_light& light, bool isMainLight); // 0x004F2B50 (:1447)
    void set_dummy_light_pos(const vector& pos); // 0x004F2CA0 (:1461)
    void set_decal_transform(const vector& pos, const matrix& rotation); // 0x004C5940 (:1470)
    void set_decal_params(const vector4& tint, const vector4& params1, const vector4& params2); // 0x004C59E0 (:1480)
    void copy_render_texture(rl_render_texture& src, rl_dev_texture& dst); // 0x004F2ED0 (:1608)
    void release_all_resources(); // 0x0054D050 (:1575)
    void renew_all_resources(); // 0x00553DF0 (:1582)
    const matrix43& get_world_transform() const; // 0x004F29D0 (:1385)
    const matrix44& get_projection_transform() const; // 0x004F29F0 (:1390)
    const matrix44& get_clip_screen_transform() const; // 0x004F2A00 (:1395)
    const matrix44& get_local_clip_transform() const; // 0x004F2A10 (:1400)
    const matrix44& get_world_clip_transform() const; // 0x004F2A20 (:1405)
    void set_world_transform(const matrix43& world_transform); // 0x005166B0 (:1213)
    void push_world_transform(const matrix43& world_transform); // 0x00516B20 (:1416)
    void pop_world_transform(); // 0x00516B70 (:1423)
    void set_3d_transforms(const matrix44& world_view_transform, const matrix44& projection_transform); // 0x00516670 (:1048)
    void get_3d_transforms(matrix44& world_view_transform, matrix44& projection_transform); // 0x004F20E0 (:1054)
    void update_transforms(); // 0x004F2500 (:1188)
    void update_local_transform(); // 0x004F23C0 (:1160)
    void set_shadow_texture_transform(const matrix44& shadow_texture_matrix); // 0x004F26C0 (:1220)
    void update_shadow_transforms(); // 0x004C5850 (:1226)
    void set_shadow_fade_params(const vector4& params); // 0x004C5640 (:1064)
    void set_render_offset(const vector& offset); // 0x004F2110 (:1059)
    const vector& get_render_offset() const; // 0x004F2140 (:1070)
    const float get_texel_offset() const; // 0x004C5630 (:1005)
    void flush_states(); // 0x00516720 (:1258)
    void setVertexBufferData(unsigned int streamIndex, keen::VertexBufferData* pData); // 0x004F2990 (:1319)
    void render(const rl_mesh* mesh_p, int sub_mesh_idx, int block_idx); // 0x00516980 (:1333)
    void draw_primitive(rl_dev_primitive_topology prim_type, unsigned int num_prims, const void* vertex_data_p, unsigned int vertex_stride); // 0x0052D690 (:713)
    void draw_primitive(rl_primitive_render_type prim_type, int num_prims, rl_dev_vertex_format vertex_format, unsigned int vertex_stride, const void* vertex_data_p, unsigned int material, int num_textures, rl_texture_binding* textures, int num_constants, rl_constant_binding* constants); // 0x00543B60 (:610)
    void* begin_dynamic(rl_dev_primitive_topology prim_type, unsigned int num_prims, unsigned int vertex_stride); // 0x0052D5A0 (:681)
    void end_dynamic(); // 0x004C5310 (:691)
    void* begin_instancing_batch(keen::GeometryInstancingBatch* batch_p, unsigned int num_instances, unsigned int instance_size); // 0x004C54F0 (:696)
    void end_instancing_batch(); // 0x004C5590 (:708)
    void draw_geometry(const keen::RenderGeometry* pGeometry, const unsigned int stride, const unsigned int num_verts, const unsigned int start_idx); // 0x00516860 (:1279)
    void set_full_state(const rl_state_manager& other); // 0x004DCA80 (.h:384)
    const rl_state_manager& get_full_state(); // 0x007B99C0 (.h:385)
    void dump() const; // 0x004C5B50 (:1720)
protected:
    bool apply_macro_state(const rl_primitive_state& state, const bool set_sampler_states); // 0x004F1B60 (:490)
    void set_cull_mode(const rl_cull_mode mode); // 0x004F1E10 (:563)
    const rl_cull_mode get_cull_mode() const; // 0x004F1E30 (:573)
    void set_shader_cull_mode(const rl_cull_mode mode); // 0x004F1E20 (:568)
    const rl_cull_mode get_shader_cull_mode() const; // 0x004F1E40 (:578)
    void set_fill_mode(const rl_fill_mode mode); // 0x004F2D50 (:1489)
    const rl_fill_mode get_fill_mode() const; // 0x004F2D60 (:1494)
    void set_scissor_test_enabled(const bool enabled); // 0x004F2D70 (:1499)
    const bool get_scissor_test_enabled() const; // 0x004F2D90 (:1504)
    void set_msaa_enabled(const bool enabled); // 0x004F2DA0 (:1509)
    const bool get_msaa_enabled() const; // 0x004F2DC0 (:1514)
    void set_zbias_mode(const rl_prim_zbias_mode enabled); // 0x004F2DD0 (:1519)
    const rl_prim_zbias_mode get_zbias_mode() const; // 0x004F2DF0 (:1524)
    void set_alpha_test(bool doAlphaTest, float alphaRef); // 0x004B7AC0 (.h:413)
    void commit_device_state(); // 0x004F2EA0 (:1592)
    void commit_rasterizer_state(); // 0x004F2E00 (:1529)
private:
    void set_flag(const unsigned int flag, const bool val); // 0x004B7AF0 (.h:490)
    const bool check_flag(const unsigned int flag); // 0x004B7B20 (.h:491)
    void set_shader_pipeline(const keen::ShaderPipeline* pipeline_p, const keen::VertexInputBinding** bindings_p); // 0x004C5B00 (:1613)
    void commit_input_layout(); // 0x004F2F30 (:1619)

public:
    static const unsigned int kMaxTmus; // 0x012B80A4
    static const unsigned int DYNAMIC_IB_SIZE; // 0x012B80A8
    static const unsigned int DYNAMIC_VB_SIZE; // 0x012B80AC
    static const unsigned int DYNAMIC_CLONE_SIZE; // 0x012B80B0
    static const unsigned int DYNAMIC_MESH_INSTB_SIZE; // 0x012B80B4
    static const unsigned int DYNAMIC_DECAL_INSTB_SIZE; // 0x012B80B8
    static const unsigned int DYNAMIC_INSTB_SIZE;
    static const unsigned int DYNAMIC_BUFFERS_SIZE; // 0x012B80BC
private:
    keen::VertexBufferData m_vertexBufferData[3]; // 0x00
    keen::GeometryInstancingBuffer* m_instancing_buffer; // 0x24
    keen::CommonVertexShaderConstants m_commonVertexData; // 0x30
    keen::CommonFragmentShaderConstants m_commonFragmentData; // 0x130
    keen::InstanceVertexShaderConstants m_instanceVertexData; // 0x1E0
    keen::InstanceFragmentShaderConstants m_instanceFragmentData; // 0x230
    keen::DecalVertexShaderConstants m_decalVertexData; // 0x250
    keen::float4 m_materialConstantData[24]; // 0x2C0
    keen::GraphicsCommandWriter* m_pCommandWriter; // 0x440
    unsigned int m_constantDataDirtyMask; // 0x444
    keen::DynamicConstantBuffer* m_pCommonVertexConstants; // 0x448
    keen::DynamicConstantBuffer* m_pCommonFragmentConstants; // 0x44C
    keen::DynamicConstantBuffer* m_pSkinningVertexConstants; // 0x450
    keen::DynamicConstantBuffer* m_pDecalVertexConstants; // 0x454
    keen::DynamicConstantBuffer* m_pLightingVertexConstants; // 0x458
    keen::DynamicConstantBuffer* m_pLightingFragmentConstants; // 0x45C
    keen::DynamicConstantBuffer* m_pMaterialVertexConstants; // 0x460
    keen::DynamicConstantBuffer* m_pMaterialFragmentConstants; // 0x464
    keen::DynamicConstantBuffer* m_pInstanceVertexConstants; // 0x468
    keen::DynamicConstantBuffer* m_pInstanceFragmentConstants; // 0x46C
    static const unsigned int kSliClearEnabled; // 0x012B80C0
    static const unsigned int kLodChanged; // 0x012B80C4
    static const unsigned int kAlphaTest; // 0x012B80C8
    unsigned int m_flags; // 0x470
    rl_dev_present_result m_last_dev_status; // 0x474
    rl_state_manager* m_state_p; // 0x478
    rl_render_lib* m_pRenderLib; // 0x47C
    rl_rect m_viewPort; // 0x480
    float m_z_near; // 0x490
    float m_z_far; // 0x494
    const keen::ShaderPipeline* m_pipeline_p; // 0x498
    const keen::VertexInputBinding** m_input_layouts_p; // 0x49C
    rl_dev_vertex_format m_vertex_format; // 0x4A0
    rl_dev_render_target m_render_target; // 0x4A4
    rl_dev_depth_stencil_target m_depth_stencil_target; // 0x4A8
    unsigned int m_merged_target_stack_index; // 0x4AC
    const keen::RenderTarget* m_merged_targets_p[8]; // 0x4B0
    const keen::RenderTarget* m_current_target_p; // 0x4D0
    unsigned int m_override_shader; // 0x4D4
    rl_material_state_store m_state_store; // 0x4D8
};

// size 0x8
class rl_index_stream_info
{
public:
    rl_index_stream_info(); // 0x004B7A20 (.h:101)

    rl_dev_index_buffer m_buffer; // 0x00
};

int RL_screen_width(); // 0x004DCAA0 (.h:528)
int RL_screen_height(); // 0x004DCAB0 (.h:533)
void get_topology(rl_dev_primitive_topology prim_type, unsigned int num_prims, keen::PrimitiveType& topology, unsigned int& num_verts); // 0x004C4D60 (:65)
rl_dev_texture get_dev_texture(const int texture); // 0x004F1E50 (:583)
void flushDynamicVertexConstantBuffer(keen::GraphicsCommandWriter* pCommandWriter, unsigned int mask, unsigned int testMask, keen::DynamicConstantBuffer* pBuffer, unsigned int slotIndex, const void* pBufferSourceData, unsigned int bufferCapacity); // 0x004F26E0 (:1238)
void flushDynamicFragmentConstantBuffer(keen::GraphicsCommandWriter* pCommandWriter, unsigned int mask, unsigned int testMask, keen::DynamicConstantBuffer* pBuffer, unsigned int slotIndex, const void* pBufferSourceData, unsigned int bufferCapacity); // 0x004C5860 (:1248)
rl_renderer* rl_getDebugRenderer(); // 0x004C5B40 (:1702)
