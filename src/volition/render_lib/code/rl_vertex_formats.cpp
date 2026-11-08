// Original: volition/render_lib/code/rl_vertex_formats.cpp
//
// The keen vertex formats of the rl_dev_vertex_format values. The pixel-lit mesh formats are combined from a vertex
// stream layout (with 0-4 texture coordinates) and the instance streams (stream 1: morph/bitangent data, stream 2:
// per-vertex color of static meshes, stream 4: the per-instance transform and color, step rate 1).
//
// The attribute tables are the stack arrays of rl_create_vertex_formats (decoded from the packed initializers).
//
// Partial: rl_mesh_pixlit_nmap_ca_vertex is not implemented yet.

#include "volition/render_lib/code/rl_vertex_formats.hpp"

#include "volition/render_lib/code/rl_device.h"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/vertexattribute.hpp"

#include <string.h>

using namespace keen;

// Short names for the attribute descriptions below.
#define RL_ATTR(ID, FORMAT, STREAM, STEP) {VertexAttributeId_##ID, VertexAttributeFormat_##FORMAT, STREAM, STEP}

// 0x004D4700 (:9)
// keen::graphics::createVertexFormat inlined.
const VertexFormat* rl_create_keen_vertex_format(GraphicsSystem* pGraphicsSystem, VertexAttributeDescription* pAttributesDesc, unsigned int numAttributes)
{
    return graphics::createVertexFormat(pGraphicsSystem, pAttributesDesc, numAttributes);
}

// 0x004D4710 (:14)
// Concatenates two attribute lists (at most 65 attributes).
const VertexFormat* rl_create_combined_keen_vertex_format(GraphicsSystem* pGraphicsSystem, VertexAttributeDescription* pAttributesDescA, unsigned int numAttributesA, VertexAttributeDescription* pAttributesDescB, unsigned int numAttributesB)
{
    VertexAttributeDescription attributes[65];
    for (unsigned int i = 0; i < numAttributesA; i++)
    {
        attributes[i] = pAttributesDescA[i];
    }
    for (unsigned int i = 0; i < numAttributesB; i++)
    {
        attributes[numAttributesA + i] = pAttributesDescB[i];
    }
    return graphics::createVertexFormat(pGraphicsSystem, attributes, numAttributesA + numAttributesB);
}

// 0x00500500 (:29)
// Fills vertex_formats_p[RLVF_NUM_FORMATS]. RLVF_PIXLIT_CA, RLVF_PIXLIT_NMAP, RLVF_PIXLIT_NMAP_CA, RLVF_SG_MESH,
// RLVF_CLONE_UVS and RLVF_CONDITIONAL_BBOX have no format.
bool rl_create_vertex_formats(GraphicsSystem* graphics_system_p, const VertexFormat** vertex_formats_p, unsigned int buffer_size)
{
    // The instance streams of the pixel-lit formats (static meshes with the stream 2 color, skinned meshes without).
    VertexAttributeDescription instance_static[7] = {
        RL_ATTR(TexCoord4, x16y16z16w16_snorm, 1, 0),
        RL_ATTR(BiTangent, x8y8z8w8_unorm, 1, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 2, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x8y8z8w8_unorm, 4, 1),
    };
    VertexAttributeDescription instance_static_nmap[7] = {
        RL_ATTR(TexCoord4, x16y16z16w16_snorm, 1, 0),
        RL_ATTR(BiTangent, x8y8z8w8_unorm, 1, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 2, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x8y8z8w8_unorm, 4, 1),
    };
    VertexAttributeDescription instance_ca[6] = {
        RL_ATTR(TexCoord4, x16y16z16w16_snorm, 1, 0),
        RL_ATTR(BiTangent, x8y8z8w8_unorm, 1, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x8y8z8w8_unorm, 4, 1),
    };
    VertexAttributeDescription instance_nmap_ca[6] = {
        RL_ATTR(TexCoord4, x16y16z16w16_snorm, 1, 0),
        RL_ATTR(BiTangent, x8y8z8w8_unorm, 1, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x8y8z8w8_unorm, 4, 1),
    };
    // The vertex streams of the pixel-lit formats; the 0-4 UV variants use a prefix.
    VertexAttributeDescription pixlit[6] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord3, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription pixlit_ca[8] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(BoneWeight, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(BoneIndex, x8y8z8w8_uint, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord3, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription pixlit_nmap[7] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Tangent, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord3, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription pixlit_nmap_ca[9] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Tangent, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(BoneWeight, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(BoneIndex, x8y8z8w8_uint, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord3, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription clone_uvs[4] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription unlit[5] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord1, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord3, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription particle_pointsprite[4] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord7, x32_float, 0, 0),
    };
    VertexAttributeDescription particle_billboard[9] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32_float, 0, 0),
        RL_ATTR(TexCoord2, x32_float, 0, 0),
        RL_ATTR(TexCoord3, x32_float, 0, 0),
        RL_ATTR(Color2, x32_float, 0, 0),
        RL_ATTR(TexCoord4, x32_float, 0, 0),
    };
    VertexAttributeDescription particle_radial[8] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord4, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32_float, 0, 0),
        RL_ATTR(Color2, x32_float, 0, 0),
        RL_ATTR(TexCoord2, x32_float, 0, 0),
    };
    VertexAttributeDescription particle_drop[8] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord4, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32y32_float, 0, 0),
        RL_ATTR(Color2, x32_float, 0, 0),
        RL_ATTR(TexCoord2, x32_float, 0, 0),
    };
    VertexAttributeDescription particle_ribbon[9] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32y32_float, 0, 0),
        RL_ATTR(TexCoord2, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord3, x32y32z32_float, 0, 0),
        RL_ATTR(Color2, x32_float, 0, 0),
        RL_ATTR(TexCoord4, x32_float, 0, 0),
    };
    VertexAttributeDescription particle_oriented[9] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Color1, x32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32_float, 0, 0),
        RL_ATTR(TexCoord2, x32_float, 0, 0),
        RL_ATTR(TexCoord3, x32y32z32_float, 0, 0),
        RL_ATTR(Color2, x32_float, 0, 0),
        RL_ATTR(TexCoord4, x32_float, 0, 0),
    };
    VertexAttributeDescription primitive_3d[3] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
    };
    VertexAttributeDescription primitive_2d[3] = {
        RL_ATTR(Position, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(Color0, x8y8z8w8_unorm, 0, 0),
    };
    VertexAttributeDescription height_mesh[2] = {
        RL_ATTR(Position, x16y16_sint, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
    };
    VertexAttributeDescription height_mesh_low_lod[1] = {
        RL_ATTR(Position, x16y16z16w16_sint, 0, 0),
    };
    VertexAttributeDescription height_mesh_landmark_lod[3] = {
        RL_ATTR(Position, x10y10z10w2_snorm, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription particle_parametric[7] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32z32_float, 0, 0),
        RL_ATTR(TexCoord1, x32_float, 0, 0),
        RL_ATTR(TexCoord3, x32_float, 0, 0),
        RL_ATTR(Color0, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord4, x32_float, 0, 0),
        RL_ATTR(TexCoord2, x16y16_sint, 1, 0),
    };
    VertexAttributeDescription compositor[2] = {
        RL_ATTR(Position, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
    };
    VertexAttributeDescription clone_nmap[4] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Tangent, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription clone_clr[8] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color0, x8y8z8w8_unorm, 4, 1),
    };
    VertexAttributeDescription spline_2d[1] = {
        RL_ATTR(Position, x16y16z16w16_sint, 0, 0),
    };
    VertexAttributeDescription particle_corona[11] = {
        RL_ATTR(Position, x32y32z32w32_float, 0, 0),
        RL_ATTR(Color0, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord1, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord2, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord3, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord4, x32y32z32w32_float, 0, 0),
        RL_ATTR(Color1, x32y32z32w32_float, 0, 0),
        RL_ATTR(Color2, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 0, 0),
    };
    VertexAttributeDescription particle_ribbon_parametric[6] = {
        RL_ATTR(Position, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord4, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32_float, 0, 0),
        RL_ATTR(TexCoord1, x32y32_float, 0, 0),
        RL_ATTR(TexCoord2, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord3, x32y32z32w32_float, 0, 0),
    };
    VertexAttributeDescription terrain_road[8] = {
        RL_ATTR(Position, x32y32z32_float, 0, 0),
        RL_ATTR(Normal, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(Tangent, x8y8z8w8_unorm, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
        RL_ATTR(TexCoord5, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord6, x32y32z32w32_float, 4, 1),
        RL_ATTR(TexCoord7, x32y32z32w32_float, 4, 1),
        RL_ATTR(Color1, x8y8z8w8_unorm, 4, 1),
    };
    VertexAttributeDescription star_field_point[1] = {
        RL_ATTR(Position, x16y16z16w16_sint, 0, 0),
    };
    VertexAttributeDescription star_field_billboard[2] = {
        RL_ATTR(Position, x16y16z16w16_sint, 0, 0),
        RL_ATTR(TexCoord0, x16y16_sint, 0, 0),
    };
    VertexAttributeDescription meteor_shower_line[3] = {
        RL_ATTR(Position, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord0, x32y32z32w32_float, 0, 0),
        RL_ATTR(TexCoord1, x32_float, 0, 0),
    };
    VertexAttributeDescription uncompressed_morph[2] = {
        RL_ATTR(TexCoord4, x16y16z16w16_snorm, 1, 0),
        RL_ATTR(BiTangent, x8y8z8w8_unorm, 1, 0),
    };

    memset(vertex_formats_p, 0, buffer_size * sizeof(const VertexFormat*));
    vertex_formats_p[RLVF_PIXLIT] = nullptr;
    vertex_formats_p[RLVF_PIXLIT_CA] = nullptr;
    vertex_formats_p[RLVF_PIXLIT_NMAP] = nullptr;
    vertex_formats_p[RLVF_PIXLIT_NMAP_CA] = nullptr;
    vertex_formats_p[RLVF_UNLIT] = graphics::createVertexFormat(graphics_system_p, unlit, 5);
    vertex_formats_p[RLVF_PARTICLE_POINTSPRITE] = graphics::createVertexFormat(graphics_system_p, particle_pointsprite, 4);
    vertex_formats_p[RLVF_PARTICLE_BILLBOARD] = graphics::createVertexFormat(graphics_system_p, particle_billboard, 9);
    vertex_formats_p[RLVF_PARTICLE_RADIAL] = graphics::createVertexFormat(graphics_system_p, particle_radial, 8);
    vertex_formats_p[RLVF_PARTICLE_DROP] = graphics::createVertexFormat(graphics_system_p, particle_drop, 8);
    vertex_formats_p[RLVF_PARTICLE_RIBBON] = graphics::createVertexFormat(graphics_system_p, particle_ribbon, 9);
    vertex_formats_p[RLVF_PARTICLE_ORIENTED] = graphics::createVertexFormat(graphics_system_p, particle_oriented, 9);
    vertex_formats_p[RLVF_PRIMITIVE_3D] = graphics::createVertexFormat(graphics_system_p, primitive_3d, 3);
    vertex_formats_p[RLVF_PRIMITIVE_2D] = graphics::createVertexFormat(graphics_system_p, primitive_2d, 3);
    vertex_formats_p[RLVF_HEIGHT_MESH] = graphics::createVertexFormat(graphics_system_p, height_mesh, 2);
    vertex_formats_p[RLVF_HEIGHT_MESH_LOW_LOD] = graphics::createVertexFormat(graphics_system_p, height_mesh_low_lod, 1);
    vertex_formats_p[RLVF_HEIGHT_MESH_LANDMARK_LOD] = graphics::createVertexFormat(graphics_system_p, height_mesh_landmark_lod, 3);
    vertex_formats_p[RLVF_PARTICLE_PARAMETRIC] = graphics::createVertexFormat(graphics_system_p, particle_parametric, 7);
    vertex_formats_p[RLVF_COMPOSITOR] = graphics::createVertexFormat(graphics_system_p, compositor, 2);
    vertex_formats_p[RLVF_CLONE_UVS] = nullptr;
    vertex_formats_p[RLVF_CLONE_NMAP] = graphics::createVertexFormat(graphics_system_p, clone_nmap, 4);
    vertex_formats_p[RLVF_CLONE_CLR] = graphics::createVertexFormat(graphics_system_p, clone_clr, 8);
    vertex_formats_p[RLVF_2D_SPLINE] = graphics::createVertexFormat(graphics_system_p, spline_2d, 1);
    vertex_formats_p[RLVF_PARTICLE_CORONA] = graphics::createVertexFormat(graphics_system_p, particle_corona, 11);
    vertex_formats_p[RLVF_PARTICLE_RIBBON_PARAMETRIC] = graphics::createVertexFormat(graphics_system_p, particle_ribbon_parametric, 6);
    vertex_formats_p[RLVF_TERRAIN_ROAD] = graphics::createVertexFormat(graphics_system_p, terrain_road, 8);
    vertex_formats_p[RLVF_STAR_FIELD_POINT] = graphics::createVertexFormat(graphics_system_p, star_field_point, 1);
    vertex_formats_p[RLVF_STAR_FIELD_BILLBOARD] = graphics::createVertexFormat(graphics_system_p, star_field_billboard, 2);
    vertex_formats_p[RLVF_METEOR_SHOWER_LINE] = graphics::createVertexFormat(graphics_system_p, meteor_shower_line, 3);
    // The pixel-lit mesh formats with 0-4 texture coordinates (rl_create_combined_keen_vertex_format inlined).
    vertex_formats_p[RLVF_PIXLIT_0UV] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 2, nullptr, 0);
    vertex_formats_p[RLVF_PIXLIT_1UV] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 3, instance_static, 7);
    vertex_formats_p[RLVF_PIXLIT_1UV_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_ca, 5, instance_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_1UV_NMAP] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap, 4, instance_static_nmap, 7);
    vertex_formats_p[RLVF_PIXLIT_1UV_NMAP_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap_ca, 6, instance_nmap_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_2UV] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 4, instance_static, 7);
    vertex_formats_p[RLVF_PIXLIT_2UV_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_ca, 6, instance_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_2UV_NMAP] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap, 5, instance_static_nmap, 7);
    vertex_formats_p[RLVF_PIXLIT_2UV_NMAP_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap_ca, 7, instance_nmap_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_3UV] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 5, instance_static, 7);
    vertex_formats_p[RLVF_PIXLIT_3UV_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_ca, 7, instance_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_3UV_NMAP] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap, 6, instance_static_nmap, 7);
    vertex_formats_p[RLVF_PIXLIT_3UV_NMAP_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap_ca, 8, instance_nmap_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_4UV] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 6, instance_static, 7);
    vertex_formats_p[RLVF_PIXLIT_4UV_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_ca, 8, instance_ca, 6);
    vertex_formats_p[RLVF_PIXLIT_4UV_NMAP] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap, 7, instance_static_nmap, 7);
    vertex_formats_p[RLVF_PIXLIT_4UV_NMAP_CA] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit_nmap_ca, 9, instance_nmap_ca, 6);
    vertex_formats_p[RLVF_CLONE_1UV_UVS] = graphics::createVertexFormat(graphics_system_p, clone_uvs, 3);
    vertex_formats_p[RLVF_CLONE_2UV_UVS] = graphics::createVertexFormat(graphics_system_p, clone_uvs, 4);
    vertex_formats_p[RLVF_PIXLIT] = rl_create_combined_keen_vertex_format(graphics_system_p, pixlit, 2, nullptr, 0);
    vertex_formats_p[RLVF_UNCOMPRESSED_MORPH] = graphics::createVertexFormat(graphics_system_p, uncompressed_morph, 2);
    return true;
}
