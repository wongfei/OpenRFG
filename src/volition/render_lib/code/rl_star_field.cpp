// Original: volition/render_lib/code/rl_star_field.cpp
//
// The star field of the night sky: billboard stars from the star catalog, drawn around the camera at the far clip
// distance with the time-of-day strength. Not done: the point stars (fill_point_star_vb, generate_point_stars), which
// render never draws.
//
// The star catalog (RL_star_catalog_star) is game data: the build extracts it from rfg.exe (tools/rfgdata/extract.py).

#include "volition/render_lib/code/rl_star_field.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/math/matrix43.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "keen/graphics/rendergeometry.hpp"

#include <stdlib.h>

// size 0xC
struct rl_star_catalog_star
{
    float m_ra; // 0x00, right ascension (radians)
    float m_dc; // 0x04, declination (radians)
    float m_mag; // 0x08, magnitude
};

// Magic number: 5456 catalog stars.
static const int Num_star_catalog_stars = 5456;
extern const unsigned int RL_star_catalog_star_words[Num_star_catalog_stars * 3];
static const rl_star_catalog_star* const RL_star_catalog_star = (const rl_star_catalog_star*)RL_star_catalog_star_words; // 0x014FA628

static keen::RenderGeometry g_starFieldBillboardGeometry; // 0x01B69AD0
bool RL_star_field_points = true; // 0x014FA611 (not read by render)
bool RL_star_field_billboards = true; // 0x014FA612

// :195
// <rl_star_field::release> 0x004D1D90 (:195)
// <rl_star_field::create> 0x00519120 (:195)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_star_field)

// 0x004D1C80 (:102)
// Two triangles of a star billboard; the position is pitch/heading, the corners are the uvs. Values are 4.12 fixed
// point.
// LTCG: vertex arrives in EAX, p in XMM0.
static int fill_billboard_star_vb(rl_star_field_billboard_vertex* vertex, float p, float h, float brightness, float random)
{
    // Magic number: 4096, 1.0 in 4.12 fixed point.
    const short pitch = (short)(int)((double)p * 4096.0);
    const short heading = (short)(int)((double)h * 4096.0);
    const short scaled_brightness = (short)(int)((double)brightness * 4096.0);
    const short scaled_random = (short)(int)((double)random * 4096.0);
    const short corners[6][2] = {{0, 0}, {0x1000, 0}, {0, 0x1000}, {0, 0x1000}, {0x1000, 0}, {0x1000, 0x1000}};
    for (int i = 0; i < 6; i++)
    {
        vertex[i].m_pitch = pitch;
        vertex[i].m_heading = heading;
        vertex[i].m_brightness = scaled_brightness;
        vertex[i].m_random = scaled_random;
        vertex[i].m_u = corners[i][0];
        vertex[i].m_v = corners[i][1];
    }
    return 6;
}

// 0x004FEED0 (:200)
rl_star_field::rl_star_field()
{
    m_star_field_shader = 0xFFFFFFFF;
    m_star_texture_handle = -1;
    m_star_strength = 1.0f;
}

// 0x004FEF30 (:207)
rl_star_field::~rl_star_field()
{
}

// 0x005191D0 (:213)
// Looks up the star shader and builds the billboard geometry of 1000 stars.
// Notes (decompiled behaviour, kept as is): the geometry has 0xD05 vertices although 6000 are generated.
void rl_star_field::initialize()
{
    rl_shader_library* library_p = rl_shader_library::get_instance();
    m_star_field_shader = library_p->lookup("rfg-skybox-stars");
    if (m_star_field_shader == 0xFFFFFFFF)
    {
        keen::debug::nativePrintArgs("Could not find star field shader!");
        return;
    }
    m_point_tech.set_ref(library_p->get_shader_user_technique_index(m_star_field_shader, crc_stri("point", 0)));
    m_billboard_tech.set_ref(library_p->get_shader_user_technique_index(m_star_field_shader, crc_stri("billboard", 0)));
    m_star_diffuse_sampler.set_ref(library_p->get_shader_tmu_index(m_star_field_shader, crc_stri("Star_diffuse", 0)));
    m_star_strength_param.set_ref(library_p->get_shader_const_index(m_star_field_shader, crc_stri("Star_strength", 0)));
    m_draw_distance_param.set_ref(library_p->get_shader_const_index(m_star_field_shader, crc_stri("Draw_distance", 0)));
    keen::MemoryAllocator* allocator_p = Init_params.m_pDefaultAllocator;
    keen::RenderGeometryDescription description;
    description.cpuAccessMode = 0;
    description.streamIndex = 0;
    description.indexCount = 0;
    description.indexFormat = keen::IndexFormat_Invalid;
    description.primitiveType = keen::PrimitiveType_TriangleList;
    description.vertexCount = 0xD05;
    description.pVertexFormat = rl_render_lib::get_instance()->m_vertex_formats_p[RLVF_STAR_FIELD_BILLBOARD];
    keen::RenderGeometryInitializationData init_data;
    init_data.pVertexData = nullptr;
    init_data.pIndexData = nullptr;
    init_data.pSkinningData = nullptr;
    init_data.pPlatformData = nullptr;
    init_data.pMorphingData = nullptr;
    // Magic numbers: 1000 stars of 6 vertices (72000 bytes).
    rl_star_field_billboard_vertex* vertices_p = (rl_star_field_billboard_vertex*)allocator_p->allocate(72000, 16, 0, nullptr);
    generate_billboard_stars(1000, vertices_p);
    init_data.pVertexData = vertices_p;
    keen::graphics::initializeRenderGeometry(&g_starFieldBillboardGeometry, Init_params.m_pKeenGraphicsSystem, description, init_data, "Starfield billboards");
    allocator_p->free(vertices_p);
}

// 0x004D1DB0 (:264)
void rl_star_field::shutdown()
{
    keen::graphics::freeRenderGeometry(&g_starFieldBillboardGeometry, Init_params.m_pKeenGraphicsSystem, Init_params.m_pDefaultAllocator);
}

// 0x004D1DD0 (:271)
void rl_star_field::set_strength(float star_strength)
{
    m_star_strength = star_strength;
}

// 0x004D1DE0 (:278)
void rl_star_field::prerender()
{
    if (m_star_texture_handle == -1)
    {
        m_star_texture_handle = bm_load("vfx_sun_01.tga", false);
    }
}

// 0x005193A0 (:392): the billboard stars, centered on the camera (the shader places them at Draw_distance), blended
// additive, depth read only.
void rl_star_field::render(rl_renderer& renderer, rl_camera& camera)
{
    if (m_star_strength == 0.0f)
    {
        return;
    }
    if (m_star_texture_handle == -1)
    {
        m_star_texture_handle = bm_load("vfx_sun_01.tga", false);
    }
    const rl_primitive_state old_state = renderer.get_macro_state();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    matrix43 transform;
    transform.set_orientation(IDENTITY_MATRIX);
    transform.set_translation(camera.get_position());
    renderer.set_world_transform(transform);
    renderer.set_eye_orientation(camera.get_rotation());
    // Magic number: the stars sit one unit inside the far clip plane.
    const float draw_distance = camera.get_far_clip() - 1.0f;
    renderer.set_pixel_shader_constant(m_draw_distance_param.get_ref(), &draw_distance, 1);
    renderer.set_pixel_shader_constant(m_star_strength_param.get_ref(), &m_star_strength, 1);
    if (m_star_texture_handle != -1)
    {
        renderer.set_texture(m_star_diffuse_sampler.get_ref(), m_star_texture_handle);
    }
    rl_primitive_state state = old_state;
    state.set_alpha_mode(RLPAM_ALPHA_ADDITIVE);
    state.set_zbuf_mode(RLPZM_READ);
    renderer.set_macro_state(state, false);
    if (RL_star_field_billboards)
    {
        renderer.set_shader_type(shader_type_generic);
        renderer.set_shader_configuration(0);
        renderer.set_active_shader(m_star_field_shader, rl_shader_technique_user, m_billboard_tech.get_ref());
        renderer.set_vertex_format(RLVF_STAR_FIELD_BILLBOARD);
        renderer.flush_states();
        // The whole geometry from stream 0 (the command writer's vertex buffer draw, inlined).
        keen::VertexBufferData vertex_buffer;
        vertex_buffer.pBuffer = g_starFieldBillboardGeometry.pVertexBuffer;
        vertex_buffer.vertexCount = g_starFieldBillboardGeometry.vertexCount;
        vertex_buffer.offset = 0;
        keen::IndexBufferData index_buffer;
        index_buffer.pBuffer = g_starFieldBillboardGeometry.pIndexBuffer;
        index_buffer.indexCount = g_starFieldBillboardGeometry.indexCount;
        index_buffer.offset = 0;
        index_buffer.indexFormat = g_starFieldBillboardGeometry.indexFormat;
        keen::PrimitiveData primitive;
        primitive.primitiveTopology = g_starFieldBillboardGeometry.primitiveTopology;
        primitive.vertexDrawCount = g_starFieldBillboardGeometry.pIndexBuffer == nullptr ? g_starFieldBillboardGeometry.vertexCount : g_starFieldBillboardGeometry.indexCount;
        renderer.m_pCommandWriter->writeDrawVertexBuffersCommand(keen::Matrix43(), &vertex_buffer, 1, &index_buffer, &primitive);
    }
    renderer.set_macro_state(old_state, false);
}

// 0x004D1F90 (:359)
// The first num_stars catalog stars below 11 degrees of declination (15 - 4), shifted down by 15 degrees, with
// brightness magnitude / 4 (at least 0.0015) and a random value in [0, 1]; the rest is filled with null stars.
// The constants are floats widened to double (as in the original).
void rl_star_field::generate_billboard_stars(int num_stars, rl_star_field_billboard_vertex* vertex)
{
    int count = 0;
    if (num_stars <= 0)
    {
        return;
    }
    for (int i = 0; i < Num_star_catalog_stars; i++)
    {
        const rl_star_catalog_star& star = RL_star_catalog_star[i];
        // Magic number: 0.2617994, 15 degrees.
        const float pitch = (float)((double)star.m_dc - (double)0.2617994f);
        const float heading = star.m_ra;
        float brightness = (float)((double)star.m_mag * 0.25);
        // Magic number: 0.0015, the minimum brightness.
        if ((float)((double)brightness - (double)0.0015f) < 0.0f)
        {
            brightness = 0.0015f;
        }
        // Magic numbers: a random value in [0, 1] in steps of 0.001.
        const float random = (float)((double)(float)(rand() % 1001) * (double)0.001f);
        // Magic number: -0.06981317, -4 degrees.
        if ((double)pitch < (double)-0.06981317f)
        {
            vertex += fill_billboard_star_vb(vertex, pitch, heading, brightness, random);
            count++;
        }
        if (count >= num_stars)
        {
            return;
        }
    }
    for (; count < num_stars; count++)
    {
        vertex += fill_billboard_star_vb(vertex, 0.0f, 0.0f, 0.0f, 0.0f);
    }
}
