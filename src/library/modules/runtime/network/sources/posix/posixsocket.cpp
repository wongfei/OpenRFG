// Original: library/modules/runtime/network/sources/posix/posixsocket.cpp
//
// Berkeley socket wrappers, built on winsock for win32. Socket handles are winsock SOCKETs (0xFFFFFFFF invalid);
// createSocket answers 0 on failure. The send and receive functions answer 0 or the winsock error: "would block"
// (WSAEWOULDBLOCK) is 0 with nothing transferred, a closed connection (a zero-byte transfer) is WSAENOTCONN.
// The inline helpers of posixsocket.hpp (and the keen::setsockopt/keen::close shims over the BSD names) are defined
// here too. Debug messages: the format arguments are not in the decompile; the obvious ones are passed.

#include "keen/network/posixsocket.hpp"
#include "keen/network/networkaddress.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

namespace keen
{
    namespace network
    {
        static bool setNoBlock(uint32 socket); // 0x00C81F10 (:535)
        static bool setAllowBroadcast(uint32 socket, bool allow); // 0x00C81F40 (:567)
        static bool setAllowReuse(uint32 socket); // 0x00C81F50 (:575)
        static bool setNoDelay(uint32 socket); // 0x00C81F90 (:592)
    }

    // 0x00C80F20 (posixsocket.hpp): the BSD setsockopt. LTCG: optval in ECX, optlen in EAX.
    int setsockopt(uint32 fd, int level, int optname, const void* optval, int optlen)
    {
        return ::setsockopt(fd, level, optname, (const char*)optval, optlen);
    }

    // 0x00C80F40 (posixsocket.hpp): the BSD close
    void close(uint32 socket)
    {
        closesocket(socket);
    }

    namespace network
    {
        // 0x00C80F00 (.h:36)
        bool isValidSocketHandle(uint32 socket)
        {
            return socket != INVALID_SOCKET;
        }

        // 0x00C80F10 (.h:41)
        int getLastSocketError()
        {
            return WSAGetLastError();
        }

        // 0x00C84C10 (:26): creationFlags 1 non-blocking, 2 no delay (TCP), 4 address reuse, 8 broadcast
        uint32 createSocket(NetworkSocketType socketType, uint32 creationFlags)
        {
            const SOCKET s = socket(AF_INET, socketType != NetworkSocketType_TCP ? SOCK_DGRAM : SOCK_STREAM, 0);
            if (s == INVALID_SOCKET)
            {
                return 0;
            }
            const char* pError = nullptr;
            if ((creationFlags & 1) != 0)
            {
                u_long nonBlocking = 1;
                if (ioctlsocket(s, FIONBIO, &nonBlocking) == SOCKET_ERROR)
                {
                    pError = "[network] Could not activate non blocking socket mode!\n";
                }
            }
            if (pError == nullptr && (creationFlags & 2) != 0 && !setNoDelay(s))
            {
                pError = "[network] Could not activate no delay socket mode!\n";
            }
            if (pError == nullptr && (creationFlags & 4) != 0 && !setAllowReuse(s))
            {
                pError = "[network] Could not activate reuse addr socket mode!\n";
            }
            if (pError != nullptr)
            {
                debug::nativePrintArgs(pError);
                ::shutdown(s, SD_BOTH);
                closesocket(s);
                return 0;
            }
            if ((creationFlags & 8) != 0)
            {
                // the broadcast option is set here directly (setAllowBroadcast does nothing on win32)
                const int allow = 1;
                if (::setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&allow, sizeof(allow)) != 0)
                {
                    debug::nativePrintArgs("[network] Could not set broadcast socket option!\n");
                    destroySocket(s);
                    return 0;
                }
            }
            return s;
        }

        // 0x00C81DD0 (:95)
        void destroySocket(uint32 socket)
        {
            ::shutdown(socket, SD_BOTH);
            closesocket(socket);
        }

        // 0x00C84CF0 (:109)
        bool bind(uint32 socket, const NetworkAddress& address)
        {
            sockaddr_in socketAddress = {};
            socketAddress.sin_family = AF_INET;
            socketAddress.sin_port = htons(address.port);
            socketAddress.sin_addr.s_addr = address.ip4Address;
            if (::bind(socket, (const sockaddr*)&socketAddress, sizeof(socketAddress)) != 0)
            {
                char addressText[128];
                const uint32 ip = address.ip4Address;
                formatString(addressText, sizeof(addressText), "%i.%i.%i.%i:%i", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24, (uint32)address.port);
                debug::nativePrintArgs("[network] Could not bind socket to address %s error=0x%08x!\n", addressText, WSAGetLastError());
                return false;
            }
            return true;
        }

        // 0x00C81DF0 (:140)
        bool listen(uint32 socket, int backlogSize)
        {
            if (::listen(socket, backlogSize) == 0)
            {
                return true;
            }
            debug::nativePrintArgs("[network] Could not start to listen on socket error=0x%08x!\n", WSAGetLastError());
            return false;
        }

        // 0x00C81E20 (:159)
        int sendData(uint32 socket, uint32* pSent, const void* pData, uint32 size)
        {
            if (size == 0)
            {
                return 0;
            }
            const int sent = send(socket, (const char*)pData, size, 0);
            if (sent > 0)
            {
                *pSent = sent;
                return 0;
            }
            if (sent == 0)
            {
                return WSAENOTCONN;
            }
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                *pSent = 0;
                return 0;
            }
            if (error != WSAECONNRESET && error != WSAENOTCONN)
            {
                debug::nativePrintArgs("[network] Could not send data to TCP Socket. (socket=%d) error=0x%08x!\n", socket, error);
            }
            return error;
        }

        // 0x00C81EA0 (:217)
        int receiveData(uint32 socket, uint32* pReceived, void* pData, uint32 capacity)
        {
            if (capacity == 0)
            {
                return 0;
            }
            const int received = recv(socket, (char*)pData, capacity, 0);
            if (received > 0)
            {
                *pReceived = received;
                return 0;
            }
            if (received == 0)
            {
                return WSAENOTCONN;
            }
            const int error = WSAGetLastError();
            *pReceived = 0;
            if (error == WSAEWOULDBLOCK)
            {
                return 0;
            }
            if (error != WSAECONNRESET)
            {
                debug::nativePrintArgs("[network] Could not receive data from TCP Socket. (socket=%d) error=0x%08x!\n", socket, error);
            }
            return error;
        }

        // 0x00C84DE0 (:269): a connected pair of non-blocking TCP sockets over the loopback address (winsock has no
        // socketpair): a listener on 127.0.0.1 with a system port, a client connects, the accepted end is the client
        // socket. Leaks the listener when a later step fails (original behaviour).
        bool createSocketPair(uint32* pServerSocket, uint32* pClientSocket)
        {
            const SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET)
            {
                debug::nativePrintArgs("[network] Could not create socket pair! error=0x%08x!\n", WSAGetLastError());
                return false;
            }
            const int reuse = 1;
            if (::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) != 0)
            {
                debug::nativePrintArgs("[network] Could not create socket pair! error=0x%08x!\n", WSAGetLastError());
                closesocket(listener);
                return false;
            }
            sockaddr_in address = {};
            address.sin_family = AF_INET;
            address.sin_port = 0;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            int addressSize = sizeof(address);
            if (::bind(listener, (const sockaddr*)&address, sizeof(address)) == 0 && ::listen(listener, 1) == 0)
            {
                address = {};
                addressSize = sizeof(address);
                if (getsockname(listener, (sockaddr*)&address, &addressSize) == 0)
                {
                    const SOCKET client = ::socket(AF_INET, SOCK_STREAM, 0);
                    if (client != INVALID_SOCKET)
                    {
                        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                        if (connect(client, (const sockaddr*)&address, sizeof(address)) == 0)
                        {
                            const SOCKET accepted = accept(listener, nullptr, nullptr);
                            if (accepted != INVALID_SOCKET)
                            {
                                closesocket(listener);
                                if (setNoBlock(client) && setNoBlock(accepted))
                                {
                                    *pServerSocket = client;
                                    *pClientSocket = accepted;
                                    return true;
                                }
                                debug::nativePrintArgs("[network] Could not create socket pair! error=0x%08x!\n", WSAGetLastError());
                                return false;
                            }
                        }
                        debug::nativePrintArgs("[network] Could not create socket pair! error=0x%08x!\n", WSAGetLastError());
                        closesocket(client);
                        closesocket(listener);
                        return false;
                    }
                }
            }
            debug::nativePrintArgs("[network] Could not create socket pair! error=0x%08x!\n", WSAGetLastError());
            closesocket(listener);
            return false;
        }

        // 0x00C85010 (:403)
        int receiveDataFrom(uint32 socket, uint32* pReceived, void* pData, uint32 capacity, NetworkAddress* pSenderAddress)
        {
            if (capacity == 0)
            {
                return 0;
            }
            sockaddr_in sender;
            int senderSize = sizeof(sender);
            const int received = recvfrom(socket, (char*)pData, capacity, 0, (sockaddr*)&sender, &senderSize);
            if (received > 0)
            {
                *pReceived = received;
                pSenderAddress->ip4Address = 0;
                pSenderAddress->port = 0;
                pSenderAddress->pad0 = 0;
                pSenderAddress->ip4Address = sender.sin_addr.s_addr;
                pSenderAddress->port = ntohs(sender.sin_port);
                return 0;
            }
            if (received == 0)
            {
                return WSAENOTCONN;
            }
            const int error = WSAGetLastError();
            *pReceived = 0;
            if (error == WSAEWOULDBLOCK)
            {
                return 0;
            }
            if (error != WSAECONNRESET)
            {
                debug::nativePrintArgs("[network] Could not receive data from Socket. (socket=%d) error=0x%08x!\n", socket, error);
            }
            return error;
        }

        // 0x00C85100 (:469)
        int sendDataTo(uint32 socket, uint32* pSent, const void* pData, uint32 size, const NetworkAddress& targetAddress)
        {
            if (size == 0)
            {
                return 0;
            }
            sockaddr_in target = {};
            target.sin_family = AF_INET;
            target.sin_port = htons(targetAddress.port);
            target.sin_addr.s_addr = targetAddress.ip4Address;
            const int sent = sendto(socket, (const char*)pData, size, 0, (const sockaddr*)&target, sizeof(target));
            if (sent > 0)
            {
                *pSent = sent;
                return 0;
            }
            if (sent == 0)
            {
                return WSAENOTCONN;
            }
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                *pSent = 0;
                return 0;
            }
            if (error == WSAECONNRESET || error == WSAENOTCONN)
            {
                return error;
            }
            debug::nativePrintArgs("[network] Could not send data to TCP Socket. (socket=%d) error=0x%08x!\n", socket, error);
            return error;
        }

        // 0x00C81F10 (:535)
        static bool setNoBlock(uint32 socket)
        {
            u_long nonBlocking = 1;
            return ioctlsocket(socket, FIONBIO, &nonBlocking) != SOCKET_ERROR;
        }

        // 0x00C81F40 (:567): nothing to do on win32 (createSocket sets SO_BROADCAST itself)
        static bool setAllowBroadcast(uint32, bool)
        {
            return true;
        }

        // 0x00C81F50 (:575)
        static bool setAllowReuse(uint32 socket)
        {
            const int reuse = 1;
            if (::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) != 0)
            {
                debug::nativePrintArgs("[network] error 0x%80X while setting option SO_REUSEADDR.\n", WSAGetLastError());
                return false;
            }
            return true;
        }

        // 0x00C81F90 (:592)
        static bool setNoDelay(uint32 socket)
        {
            const int noDelay = 1;
            if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelay, sizeof(noDelay)) != 0)
            {
                debug::nativePrintArgs("[network] error 0x%80X while setting option TCP_NODELAY.\n", WSAGetLastError());
                return false;
            }
            return true;
        }
    }
}
