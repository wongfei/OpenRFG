// Original: library/modules/runtime/network/include/keen/network/posixsocket.hpp
// Implementation: library/modules/runtime/network/sources/posix/posixsocket.cpp
//
// Berkeley socket wrappers (winsock on win32): creation, bind, listen, send and receive, socket pairs.
#pragma once

#include "keen/base/types.hpp"
#include "keen/network/networksystem.hpp"

namespace keen
{
    struct NetworkAddress;

    // The BSD names keen's posix code calls, as shims (winsock spells them differently or not at all).
    int setsockopt(uint32 fd, int level, int optname, const void* optval, int optlen); // 0x00C80F20 (.h)
    void close(uint32 socket); // 0x00C80F40 (.h)

    namespace network
    {
        bool isValidSocketHandle(uint32 socket); // 0x00C80F00 (.h:36)
        int getLastSocketError(); // 0x00C80F10 (.h:41)
        uint32 createSocket(NetworkSocketType socketType, uint32 creationFlags); // 0x00C84C10 (:26)
        void destroySocket(uint32 socket); // 0x00C81DD0 (:95)
        bool bind(uint32 socket, const NetworkAddress& address); // 0x00C84CF0 (:109)
        bool listen(uint32 socket, int backlogSize); // 0x00C81DF0 (:140)
        int sendData(uint32 socket, uint32* pSent, const void* pData, uint32 size); // 0x00C81E20 (:159)
        int receiveData(uint32 socket, uint32* pReceived, void* pData, uint32 capacity); // 0x00C81EA0 (:217)
        bool createSocketPair(uint32* pServerSocket, uint32* pClientSocket); // 0x00C84DE0 (:269)
        int receiveDataFrom(uint32 socket, uint32* pReceived, void* pData, uint32 capacity, NetworkAddress* pSenderAddress); // 0x00C85010 (:403)
        int sendDataTo(uint32 socket, uint32* pSent, const void* pData, uint32 size, const NetworkAddress& targetAddress); // 0x00C85100 (:469)
    }
}
