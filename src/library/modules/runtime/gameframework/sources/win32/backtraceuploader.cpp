// Original: library/modules/runtime/gameframework/sources/win32/backtraceuploader.cpp
//
// The crash report upload: rfg.exe posts the minidump and the log to the publisher's Backtrace endpoint (libcurl, a
// multipart form with the two files), which rfg::Application sets with setBacktraceURL.
//
// Deviation (orig_main.md 3.4): a reimplementation never uploads anything; uploadToBacktrace does nothing (the minidump
// stays next to the exe, the log next to it).

#include "keen/gameframework/gameframework.hpp"
#include "keen/base/string.hpp"
#include "library/modules/runtime/gameframework/sources/win32/backtraceuploader.hpp"

namespace keen
{
    static char s_backtraceUrl[1024]; // 0x038B67C0

    namespace GameFramework
    {
        // 0x00C0E850
        void setBacktraceURL(const char* pUrl)
        {
            copyString(s_backtraceUrl, sizeof(s_backtraceUrl), pUrl);
        }
    }

    // 0x00C0E870
    // Deviation (orig_main.md 3.4): the original sends both files to s_backtraceUrl (curl_formadd of the dump and the
    // log, CURLOPT_URL, CURLOPT_HTTPPOST, curl_easy_perform); nothing is sent here.
    void uploadToBacktrace(const char*, const char*)
    {
    }
}
