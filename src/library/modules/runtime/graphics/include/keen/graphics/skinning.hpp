// Original: library/modules/runtime/graphics/include/keen/graphics/skinning.hpp
// Implementation: library/modules/runtime/graphics/sources/global/skinning.cpp
//
// GPU skinning entry points. SkinningBatch and SkinnedGeometryInstance are defined in sources/d3d11/skinning_d3d11.hpp.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Matrix43;

    namespace graphics
    {
        void prepareSkinningMatrices(Matrix43* pMatrices, const Matrix43* pSkinToBoneMatrices, uint32 skinToBoneMatrixCount); // 0x00C5FD70 (:11)
    }
}
