// Original: library/modules/runtime/gameframework/sources/win32/backtraceuploader.hpp (private)
// Implementation: library/modules/runtime/gameframework/sources/win32/backtraceuploader.cpp
//
// Uploads a minidump and log to the URL set with GameFramework::setBacktraceURL (via libcurl).
// OpenRFG must never call this with the publisher's endpoint.
#pragma once

namespace keen
{
    void uploadToBacktrace(const char* pDmpFilename, const char* pLogFilename); // 0x00C0E870
}
