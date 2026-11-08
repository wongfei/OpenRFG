// Original: library/modules/runtime/network/sources/global/networkaddress.cpp
//
// IPv4 network addresses. ip4Address is in network byte order (as inet_addr and sockaddr_in hold it: the first octet
// in the lowest byte), port in host byte order. The inline operators of networkaddress.hpp are defined here too.

#include "keen/network/networkaddress.hpp"
#include "keen/base/string.hpp"

#define _WINSOCK_DEPRECATED_NO_WARNINGS // gethostbyname and inet_addr, as the original uses them
#include <winsock2.h>

namespace keen
{
    // 0x00CD01E0 (.h:11)
    bool NetworkAddress::operator==(const NetworkAddress& other) const
    {
        return ip4Address == other.ip4Address && port == other.port;
    }

    // 0x00CD0210 (.h:12)
    bool NetworkAddress::operator!=(const NetworkAddress& other) const
    {
        return !(ip4Address == other.ip4Address && port == other.port);
    }

    // 0x00CD0240 (.h:14)
    bool NetworkAddress::isValid() const
    {
        return ip4Address != 0 || port != 0;
    }

    namespace network
    {
        // 0x00C831E0 (:17)
        NetworkAddress getInvalidNetworkAddress()
        {
            NetworkAddress address;
            address.ip4Address = 0;
            address.port = 0;
            address.pad0 = 0;
            return address;
        }

        // 0x00C80F50 (:24)
        void clearNetworkAddress(NetworkAddress* pAddress)
        {
            pAddress->ip4Address = 0;
            pAddress->port = 0;
        }

        // 0x00C80F70 (:31): the address only (dotted decimal); the port is left as it is
        bool parseNetworkAddress(NetworkAddress* pAddress, const char* pTcpAddressString)
        {
            pAddress->ip4Address = inet_addr(pTcpAddressString);
            return pAddress->ip4Address != INADDR_NONE;
        }

        // 0x00C80F90 (:60)
        uint32 formatNetworkAddress(char* pBuffer, uint32 capacity, const NetworkAddress& address, bool includePort)
        {
            const uint32 ip = address.ip4Address;
            if (includePort)
            {
                return formatString(pBuffer, capacity, "%i.%i.%i.%i:%i", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24, (uint32)address.port);
            }
            return formatString(pBuffer, capacity, "%i.%i.%i.%i", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
        }

        // 0x00C83200 (:95): the first address of the local host, or the loopback address when there is none
        bool queryLocalNetworkAddress(char* pBuffer, uint32 capacity)
        {
            const hostent* pHost = gethostbyname("");
            if (pHost != nullptr)
            {
                const uint8* pIp = (const uint8*)pHost->h_addr_list[0];
                formatString(pBuffer, capacity, "%i.%i.%i.%i", pIp[0], pIp[1], pIp[2], pIp[3]);
                return true;
            }
            copyString(pBuffer, capacity, "127.0.0.1");
            return false;
        }
    }
}
