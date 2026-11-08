// Original: library/modules/runtime/base/sources/win32/float32_win32.hpp
//
// Win32 float helpers (SSE) of the pf:: namespace.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace pf
    {
        float sqrt(float a); // 0x00BF4140 (.h:49)
        float mod(float a, float b); // 0x00CBEA10 (.h:50)
        float floor(float a); // 0x00BF4160 (.h:51)
        float rsqrt(float x); // 0x00BED360 (.h:55)
        void enableFlushToZero(); // 0x00BED380 (.h:70)
        float select(float x, float a, float b); // 0x004587B0 (.h:96)
        float min(float a, float b); // 0x004587F0 (.h:97)
        float max(float a, float b); // 0x00458830 (.h:98)
        float clamp(float x, float min, float max); // 0x00458870 (.h:99)
        float clamp01(float x); // 0x004588D0 (.h:100)
    }
}
