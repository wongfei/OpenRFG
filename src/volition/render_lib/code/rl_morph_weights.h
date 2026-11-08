// Original: volition/render_lib/code/rl_morph_weights.h
// Implementation: volition/render_lib/code/rl_morph_weights.cpp
//
// Morph weight helpers.
#pragma once

#include "keen/graphics/graphicssystem.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    struct VertexBufferData;
}

float rl_morph_weight_convert(const unsigned char& weight); // 0x004BDAE0 (.h:30)
unsigned char rl_morph_weight_convert(float weight); // 0x004BDB50 (.h:39)
void rl_morph_system_init(); // 0x004E9E60 (:54)
void rl_morph_system_shutdown(); // 0x004C38D0 (:85)
unsigned char* rl_morph_weights_alloc(int num_weights); // 0x004E9F40 (:96)
void rl_morph_weights_release(unsigned char* weights); // 0x004C38E0 (:113)
void rl_morph_weights_get_stats(int& amount_used, int& amount_total); // 0x004C38F0 (:118)
keen::GraphicsMapType rl_morph_weights_determine_mapType(unsigned int bufferOffset); // 0x004C3920 (:124)
bool rl_morph_weights_fillDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter, keen::VertexBufferData* pVertexBufferData, const unsigned char* pWeightData, unsigned int numWeights); // 0x004C3930 (:134)
void* rl_morph_system_beginDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter, keen::VertexBufferData* pVertexBufferData, unsigned int size); // 0x004C3A10 (:165)
void rl_morph_system_endDynamicVertexBufferData(keen::GraphicsCommandWriter* pCommandWriter); // 0x004C3A80 (:186)

class page_mempool;
// Not in the binary: g_morphSystem's weight allocator, which rl_morph_instance::alloc_morph_weights reads directly in
// rfg.exe (render_lib is one unity TU there).
page_mempool& rl_morph_weights_allocator();
void rl_morph_system_flipBuffers(); // 0x004C3AB0 (:193)
