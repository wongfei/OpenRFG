// Original: library/modules/runtime/network/sources/global/networkplatform.hpp
// Implementation: library/modules/runtime/network/sources/win32/networkplatform_win32.cpp
//
// Platform setup of the network module (winsock startup and shutdown) and host name lookup.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct NetworkAddress;

    namespace network
    {
        void initializePlatform(); // 0x00C816D0 (networkplatform_win32.cpp:12)
        void shutdownPlatform(); // 0x00C81730 (networkplatform_win32.cpp:30)
        bool getHostNetworkAddress(NetworkAddress* pTarget, const char* pHostName); // 0x00C81750 (networkplatform_win32.cpp:42)
    }
}
