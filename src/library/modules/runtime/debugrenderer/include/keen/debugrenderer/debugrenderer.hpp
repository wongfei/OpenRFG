// Original: library/modules/runtime/debugrenderer/include/keen/debugrenderer/debugrenderer.hpp
//
// Debug drawing, compiled out of the retail build: every function is an empty inline (drawText returns 0).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class Frustum;
    struct OrientedBox;

    namespace DebugRenderer
    {
        inline void drawWireframeFrustum(const Frustum&, uint32) {} // 0x00C5D0E0 (:177)
        inline void drawWireframeBox(const OrientedBox&, uint32) {} // 0x00C5D0F0 (:183)
        inline void drawSolidRect(int, int, int, int, uint32) {} // 0x004BC290 (:209)
        inline void drawWireframeRect(int, int, int, int, uint32) {} // 0x004BC2A0 (:210)
        inline void drawWireframeCircle(int, int, int, uint32) {} // 0x004BC2B0 (:211)
        inline void drawLine(int, int, int, int, uint32) {} // 0x004BC2C0 (:213)
        inline int drawText(int, int, uint32, const char*, ...) { return 0; } // 0x008119C0 (:220)
        inline void clear() {} // 0x00C0E110 (:222)
    }
}
