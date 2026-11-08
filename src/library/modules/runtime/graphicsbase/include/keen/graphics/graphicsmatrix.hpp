// Original: library/modules/runtime/graphicsbase/include/keen/graphics/graphicsmatrix.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/graphicsmatrix.cpp
//
// GraphicsMatrix43/GraphicsMatrix44, matrices in the transposed layout shader constants use, and the writers that
// convert Matrix43/Matrix44 into them (sources/global/graphicsmatrix.inl). Placement of the types inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Matrix43;
    struct Matrix44;

    // size 0x30
    struct GraphicsMatrix43
    {
        float row0[4]; // 0x00
        float row1[4]; // 0x10
        float row2[4]; // 0x20
    };

    // size 0x40
    struct GraphicsMatrix44
    {
        float row0[4]; // 0x00
        float row1[4]; // 0x10
        float row2[4]; // 0x20
        float row3[4]; // 0x30
    };

    void writeGraphicsMatrix(GraphicsMatrix43* pResult, const Matrix43& source); // 0x004B3420 (graphicsmatrix.inl:12)
    void writeGraphicsMatrix(GraphicsMatrix44* pResult, const Matrix44& source); // 0x004B3470 (graphicsmatrix.inl:102)
}
