// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/renderobject.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/renderobject.cpp
//
// RenderObject, what a render effect draws: material data, geometry and instance.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct BaseMaterialData;
    struct RenderGeometry;
    struct RenderInstance;

    // size 0xC
    struct RenderObject
    {
        const BaseMaterialData* pMaterial; // 0x00
        const RenderGeometry* pGeometry; // 0x04
        const RenderInstance* pInstance; // 0x08
    };
}
