// Original: volition/render_lib/code/rl_morph_weights.cpp
//
// Partial: the morph system (a page mempool of morph weights and three rotating dynamic vertex buffers of weights),
// rl_morph_system_init/shutdown, the buffer flip and the per-frame buffer fill (VFX.md FX7, for the deformable mesh
// decals). The weight allocation functions are not implemented yet.

#include "volition/render_lib/code/rl_morph_weights.h"

#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/vertexformat.hpp"
#include "keen/graphics/vertexattribute.hpp"

#include <intrin.h>

// size 0x40100
struct MorphSystem
{
    MorphSystem(); // 0x004C38A0
    ~MorphSystem(); // 0x004C38C0

    unsigned char morphWeightsBuffer[262144]; // 0x00
    page_mempool morphWeightsAllocator; // 0x40000
    const keen::VertexFormat* pMorphWeightsVertexFormat; // 0x40058
    keen::RenderGeometry morphGeometryBuffer[3]; // 0x4005C
    unsigned int currentMorphGeometryBuffer; // 0x400E0
    unsigned int currentMorphGeometryVertexBufferOffset; // 0x400E4
    keen::MappedRenderGeometryInfo mappedMorphGeometryVertexBufferInfo; // 0x400E8
};

static MorphSystem g_morphSystem; // 0x01B7BD80

// 0x004C38A0
MorphSystem::MorphSystem()
{
}

// 0x004C38C0
MorphSystem::~MorphSystem()
{
}

// 0x004E9E60 (:54)
// The weights live in morphWeightsBuffer; each of the three geometry buffers holds 1 MB of weights (one Color0
// x8y8z8w8_unorm attribute per vertex).
void rl_morph_system_init()
{
    // Magic numbers: 256 KB in pages of 0x100 bytes, alignment 1.
    g_morphSystem.morphWeightsAllocator.init(g_morphSystem.morphWeightsBuffer, 0x40000, 0x100, "Deformation weights", 1, 0);
    keen::VertexAttributeDescription attribute;
    attribute.id = keen::VertexAttributeId_Color0;
    attribute.format = keen::VertexAttributeFormat_x8y8z8w8_unorm;
    attribute.inputStreamIndex = 0;
    attribute.instanceStepRate = 0;
    g_morphSystem.pMorphWeightsVertexFormat = keen::graphics::createVertexFormat(Init_params.m_pKeenGraphicsSystem, &attribute, 1);
    keen::RenderGeometryDescription description;
    // Magic number: 1 MB per buffer.
    description.vertexCount = 0x100000 / g_morphSystem.pMorphWeightsVertexFormat->streamStride[0];
    description.indexCount = 0;
    description.primitiveType = keen::PrimitiveType_TriangleList;
    description.indexFormat = keen::IndexFormat_Invalid;
    // Magic number: CPU write access (dynamic).
    description.cpuAccessMode = 2;
    description.streamIndex = 0;
    description.pVertexFormat = g_morphSystem.pMorphWeightsVertexFormat;
    for (int i = 0; i < 3; i++)
    {
        keen::graphics::allocateRenderGeometry(&g_morphSystem.morphGeometryBuffer[i], Init_params.m_pKeenGraphicsSystem, Init_params.m_pBufferAllocator, description, "MorphWeights Buffer");
    }
    g_morphSystem.currentMorphGeometryBuffer = 0;
}

// 0x004C3930 (:134)
// The weights as vertices of this frame's weight buffer (one vertex per weight, the weight in its first byte), at a
// range reserved atomically; the first range of a frame discards the buffer. False when the buffer is full or the
// map fails.
bool rl_morph_weights_fillDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter, keen::VertexBufferData* pVertexBufferData, const unsigned char* pWeightData, unsigned int numWeights)
{
    const unsigned int buffer = g_morphSystem.currentMorphGeometryBuffer;
    // Magic number: 4 bytes per weight vertex (x8y8z8w8).
    const unsigned int num_bytes = numWeights * 4;
    const unsigned int end_offset = (unsigned int)_InterlockedExchangeAdd((volatile long*)&g_morphSystem.currentMorphGeometryVertexBufferOffset, (long)num_bytes) + num_bytes;
    const unsigned int start_offset = end_offset - num_bytes;
    // Magic number: 1 MB per buffer (rl_morph_system_init).
    if (end_offset > 0x100000)
    {
        return false;
    }
    keen::RenderGeometry* geometry = &g_morphSystem.morphGeometryBuffer[g_morphSystem.currentMorphGeometryBuffer];
    keen::MappedRenderGeometryInfo mapped;
    const keen::GraphicsMapType map_type = start_offset != 0 ? keen::GraphicsMapType_WriteNoOverwrite : keen::GraphicsMapType_WriteDiscard;
    unsigned int* dest_p = (unsigned int*)pCommandWriter->mapRenderGeometryVertexData(geometry, map_type, 0, start_offset, num_bytes, &mapped);
    if (dest_p == nullptr)
    {
        return false;
    }
    for (unsigned int i = 0; i < numWeights; i++)
    {
        dest_p[i] = pWeightData[i];
    }
    pVertexBufferData->pBuffer = g_morphSystem.morphGeometryBuffer[buffer].pVertexBuffer;
    pVertexBufferData->offset = start_offset;
    pVertexBufferData->vertexCount = g_morphSystem.morphGeometryBuffer[buffer].vertexCount;
    pCommandWriter->unmapRenderGeometryVertexData(geometry, &mapped);
    return true;
}

// Not in the binary (see the header).
page_mempool& rl_morph_weights_allocator()
{
    return g_morphSystem.morphWeightsAllocator;
}

// 0x004C3A10 (:165)
// Maps size bytes of this frame's morph buffer (reserved atomically, the first range of a frame discards) for the
// caller to fill; rl_morph_system_endDynamicVertexBufferData unmaps it. Null when the buffer is full or the map fails.
void* rl_morph_system_beginDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter, keen::VertexBufferData* pVertexBufferData, unsigned int size)
{
    const unsigned int buffer = g_morphSystem.currentMorphGeometryBuffer;
    const unsigned int end_offset = (unsigned int)_InterlockedExchangeAdd((volatile long*)&g_morphSystem.currentMorphGeometryVertexBufferOffset, (long)size) + size;
    const unsigned int start_offset = end_offset - size;
    // Magic number: 1 MB per buffer (rl_morph_system_init).
    if (end_offset > 0x100000)
    {
        return nullptr;
    }
    keen::RenderGeometry* geometry = &g_morphSystem.morphGeometryBuffer[g_morphSystem.currentMorphGeometryBuffer];
    const keen::GraphicsMapType map_type = start_offset != 0 ? keen::GraphicsMapType_WriteNoOverwrite : keen::GraphicsMapType_WriteDiscard;
    void* dest_p = pCommandWriter->mapRenderGeometryVertexData(geometry, map_type, 0, start_offset, size, &g_morphSystem.mappedMorphGeometryVertexBufferInfo);
    if (dest_p != nullptr)
    {
        pVertexBufferData->pBuffer = g_morphSystem.morphGeometryBuffer[buffer].pVertexBuffer;
        pVertexBufferData->offset = start_offset;
        pVertexBufferData->vertexCount = g_morphSystem.morphGeometryBuffer[buffer].vertexCount;
    }
    return dest_p;
}

// 0x004C3A80 (:186)
void rl_morph_system_endDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter)
{
    pCommandWriter->unmapRenderGeometryVertexData(&g_morphSystem.morphGeometryBuffer[g_morphSystem.currentMorphGeometryBuffer], &g_morphSystem.mappedMorphGeometryVertexBufferInfo);
}

// 0x004C38D0 (:85)
// Notes (decompiled behaviour, kept as is): nothing is freed.
void rl_morph_system_shutdown()
{
}

// 0x004C3AB0 (:193)
// Next frame: the next of the three weight buffers, written from the start.
void rl_morph_system_flipBuffers()
{
    g_morphSystem.currentMorphGeometryBuffer++;
    g_morphSystem.currentMorphGeometryVertexBufferOffset = 0;
    if (g_morphSystem.currentMorphGeometryBuffer > 2)
    {
        g_morphSystem.currentMorphGeometryBuffer = 0;
    }
}
