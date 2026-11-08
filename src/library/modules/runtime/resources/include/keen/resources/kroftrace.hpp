// Original: library/modules/runtime/resources/include/keen/resources/kroftrace.hpp
// Implementation: library/modules/runtime/resources/sources/kroftrace.cpp
//
// Tracing of krof (keen resource object file) loading.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct FileSystem;
    struct SectionHeader;

    namespace kroftrace
    {
        void startTrace(FileSystem*, const char*); // 0x00CDC520 (.h:26)
        void endTrace(); // 0x00CDC530 (.h:27)
        void traceSectionHeader(uint32, const SectionHeader&); // 0x00CDC540 (.h:28)
        void traceDataSection(void*, uint32); // 0x00CDC550 (.h:29)
        void traceRelocation(uint32, uint32, uint32, uint32, const void*); // 0x00CDC560 (.h:30)
    }
}
