// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/callbackmaterialdata.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/callbackrendereffect.cpp
//
// CallBackMaterialData, the material of the callback render effect (a user function called with the command writer).
#pragma once

#include "keen/base/types.hpp"
#include "keen/rendereffect/rendereffect.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    struct RenderContext;

    // size 0x10
    struct CallBackMaterialData : public BaseMaterialData
    {
        void setup(void (*)(GraphicsCommandWriter*, const RenderContext&, const CallBackMaterialData&), int);

        int renderPass; // 0x08
        void (*pFunction)(GraphicsCommandWriter*, const RenderContext&, const CallBackMaterialData&); // 0x0C
    };
}
