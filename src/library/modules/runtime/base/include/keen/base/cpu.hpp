// Original: library/modules/runtime/base/include/keen/base/cpu.hpp
// Implementation: library/modules/runtime/base/sources/global/cpu.cpp
//
// CPU queries (current frequency).
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    uint64_t getCurrentCpuFrequency(); // 0x00BED3A0 (cpu_win32.cpp:8)
}
