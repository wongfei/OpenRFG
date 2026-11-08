// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/rendercommand.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/rendercommand.cpp
//
// RenderCommand, one sortable draw command (sort key, object and execute function), and RenderCommands, a run of
// commands of one effect executed together. Placement of RenderCommands inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    struct RenderCommands;
    struct RenderContext;
    struct RenderObject;

    // size 0xC
    struct RenderCommand
    {
        uint32 sortKey; // 0x00
        const RenderObject* pObject; // 0x04
        void (*pExecuteCommandsFunc)(GraphicsCommandWriter*, const RenderCommands&, const RenderContext&, void*); // 0x08
    };

    // size 0xC
    struct RenderCommands
    {
        const RenderCommand* pStart; // 0x00
        const RenderCommand* pEnd; // 0x04
        uint32 renderPass; // 0x08
    };
}
