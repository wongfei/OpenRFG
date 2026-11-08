// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/renderinstance.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/renderinstance.cpp
//
// RenderInstance, the per-object instance data of a render object (world transform, skinning, instancing).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/matrix43.hpp"

namespace keen
{
    struct GeometryInstancingBatch;
    struct SkinnedGeometryInstance;

    // size 0x38
    struct RenderInstance
    {
        Matrix43 worldTransform; // 0x00
        const SkinnedGeometryInstance* pSkinnedInstance; // 0x30
        const GeometryInstancingBatch* pInstancingData; // 0x34
    };
}
