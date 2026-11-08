// Original: library/modules/runtime/network/include/keen/network/networkaddress.hpp
// Implementation: library/modules/runtime/network/sources/global/networkaddress.cpp
//
// Network addresses (IPv4 address and port): parsing, formatting and the local address.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    struct NetworkAddress
    {
        bool operator==(const NetworkAddress& other) const; // 0x00CD01E0 (.h:11)
        bool operator!=(const NetworkAddress& other) const; // 0x00CD0210 (.h:12)
        bool isValid() const; // 0x00CD0240 (.h:14)

        uint32 ip4Address; // 0x00
        uint16 port; // 0x04
        uint16 pad0; // 0x06
    };

    namespace network
    {
        NetworkAddress getInvalidNetworkAddress(); // 0x00C831E0 (:17)
        void clearNetworkAddress(NetworkAddress* pAddress); // 0x00C80F50 (:24)
        bool parseNetworkAddress(NetworkAddress* pAddress, const char* pTcpAddressString); // 0x00C80F70 (:31)
        uint32 formatNetworkAddress(char* pBuffer, uint32 capacity, const NetworkAddress& address, bool includePort); // 0x00C80F90 (:60)
        bool queryLocalNetworkAddress(char* pBuffer, uint32 capacity); // 0x00C83200 (:95)
    }
}
