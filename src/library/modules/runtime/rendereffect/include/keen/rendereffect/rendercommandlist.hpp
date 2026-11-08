// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/rendercommandlist.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/rendercommandlist.cpp
//
// RenderCommandList, collects RenderCommands per render pass, sorts them by key and executes them in runs.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/slice.hpp"
#include "keen/rendereffect/rendercommand.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    class MemoryAllocator;
    struct RenderContext;
    struct RenderObject;

    // size 0x1
    struct RenderCommandKey
    {
        static uint32 getKey(const RenderCommand& command); // 0x00CC88A0 (:8)

    };

    // size 0xC
    class RenderCommandList
    {
    public:
        RenderCommandList(); // 0x00CC95E0 (:13)
        ~RenderCommandList(); // 0x00CC95F0 (:22)
        bool create(MemoryAllocator* pAllocator, uint32 renderPassCount, const uint32* pRenderPassCommandCounts); // 0x00CCC110 (:27)
        void destroy(MemoryAllocator* pAllocator); // 0x00CCBBA0 (:69)
        uint32 getTotalCommandCount() const; // 0x00CC9630 (:109)
        void reset(); // 0x00CC9600 (:91)
        void sort(uint32 renderPass); // 0x00CCBC00 (:127)
        void sort(); // 0x00CCC240 (:119)
        bool isFull(uint32) const;
        void addCommand(uint32 renderPass, uint32 sortKey, const RenderObject* pObject, void (*pFunction)(GraphicsCommandWriter*, const RenderCommands&, const RenderContext&, void*)); // 0x00CC8FC0 (.h:47)
        RenderCommand* addCommand(uint32);
        uint32 getRenderPassCount() const;
        const RenderCommand* getStart(uint32) const;
        const RenderCommand* getEnd(uint32) const;
        uint32 getSize(uint32) const;
        uint32 getCapacity(uint32) const;
        void getRenderCommands(RenderCommands*, uint32) const;
        RenderCommands getRenderCommands(uint32) const;

    private:
        Array<Slice<RenderCommand> > m_buffers; // 0x00
        RenderCommand* m_pData; // 0x08
    };
}
