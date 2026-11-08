// Original: library/modules/runtime/base/include/keen/base/trace.hpp
// Implementation: library/modules/runtime/base/sources/win32/trace_win32.cpp
//
// Debug output (native print and the log file).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace debug
    {
        void initializeLogFile(const char* pFileName); // 0x00C0B080 (trace_win32.cpp:20)
        void shutdownLogFile(); // 0x00BEDEE0 (trace_win32.cpp:52)
        void nativePrint(const char* pMessage); // 0x00BF4920 (trace_win32.cpp:71)
        void nativePrintArgs(const char*, ...); // 0x00C07580 (trace_win32.cpp:139)
    }
}
