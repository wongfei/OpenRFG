// Original: library/modules/runtime/network/sources/win32/networkplatform_win32.cpp
//
// winsock start-up and shutdown (reference counted) and the host address lookup. rfg.exe inlines initializePlatform
// and shutdownPlatform into createNetworkSystem/destroyNetworkSystem (networksystem_posix.cpp), so the network module
// is one unity TU there; s_libraryRefCount is shared with that file (not compiled yet, multiplayer.md MP2).
// Nothing on the LAN path creates a network system: rfg.exe's winsock start-up comes from curl's global init
// (win32_init 0x0042BF10); the multiplayer programs call initializePlatform in its place.

#include "library/modules/runtime/network/sources/global/networkplatform.hpp"
#include "keen/network/networkaddress.hpp"
#include "keen/base/trace.hpp"

#define _WINSOCK_DEPRECATED_NO_WARNINGS // gethostbyname, as the original uses it
#include <winsock2.h>

namespace keen
{
    namespace network
    {
        static uint32 s_libraryRefCount; // 0x038BD650, WSAStartup/WSACleanup reference count

        // 0x00C816D0 (:12)
        void initializePlatform()
        {
            if (s_libraryRefCount == 0)
            {
                WSADATA data;
                const int result = WSAStartup(MAKEWORD(2, 2), &data);
                if (result != 0)
                {
                    debug::nativePrintArgs("[network] Could not start WinSock library! result=%d\n", result);
                }
            }
            s_libraryRefCount++;
        }

        // 0x00C81730 (:30)
        void shutdownPlatform()
        {
            if (s_libraryRefCount != 0)
            {
                s_libraryRefCount--;
                if (s_libraryRefCount == 0)
                {
                    WSACleanup();
                }
            }
        }

        // 0x00C81750 (:42): the first address of the host (pHostName "" is the local host); the port is left as it is
        bool getHostNetworkAddress(NetworkAddress* pTarget, const char* pHostName)
        {
            const hostent* pHost = gethostbyname(pHostName);
            if (pHost == nullptr)
            {
                return false;
            }
            pTarget->ip4Address = *(const uint32*)pHost->h_addr_list[0];
            return true;
        }
    }
}
