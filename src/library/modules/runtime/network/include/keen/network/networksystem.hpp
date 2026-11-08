// Original: library/modules/runtime/network/include/keen/network/networksystem.hpp
// Implementation: library/modules/runtime/network/sources/posix/networksystem_posix.cpp
//
// The network system: asynchronous sockets (accept, connect, read, write and user events) serviced by a select loop;
// waitForAsyncOperationResult returns the next finished operation. Implemented by networksystem_posix.cpp, whose
// NetworkSystem and AsyncNetworkSocket stay opaque. Placement of the operation and result types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/network/networkaddress.hpp"

namespace keen
{
    struct AsyncNetworkSocket;
    class MemoryAllocator;
    struct NetworkSystem;

    enum NetworkSocketType
    {
        NetworkSocketType_TCP = 0,
        NetworkSocketType_UDP = 1,
    };

    enum AsyncNetworkOperationType
    {
        AsyncNetworkOperationType_Accept = 0,
        AsyncNetworkOperationType_Connect = 1,
        AsyncNetworkOperationType_Read = 2,
        AsyncNetworkOperationType_Write = 3,
        AsyncNetworkOperationType_User = 4,
        AsyncNetworkOperationType_Count = 5,
    };

    enum AsyncNetworkIoError
    {
        AsyncNetworkIoError_Ok = 0,
        AsyncNetworkIoError_Closed = 1,
        AsyncNetworkIoError_Error = 2,
        AsyncNetworkIoError_Count = 3,
    };

    // size 0x14
    struct AsyncNetworkAcceptResult
    {
        AsyncNetworkSocket* pNewSocket; // 0x00
        NetworkAddress remoteAddress; // 0x04
        NetworkAddress localAddress; // 0x0C
    };

    // size 0x4
    struct AsyncNetworkIoResult
    {
        uint32 bytesTransferred; // 0x00
    };

    // size 0x24
    struct NetworkOperationResult
    {
        AsyncNetworkOperationType type; // 0x00
        AsyncNetworkIoError error; // 0x04
        uint32 errorCode; // 0x08
        union
        {
            AsyncNetworkAcceptResult acceptData; // 0x00
            AsyncNetworkIoResult ioData; // 0x00
        } data; // 0x0C
        void* pUserData; // 0x20
    };

    namespace network
    {
        NetworkSystem* createNetworkSystem(MemoryAllocator* pAllocator); // 0x00C87380 (networksystem_posix.cpp:560)
        void destroyNetworkSystem(NetworkSystem* pNetworkSystem); // 0x00C88C90 (networksystem_posix.cpp:586)
        AsyncNetworkSocket* createAsyncNetworkSocket(NetworkSystem* pNetworkSystem, NetworkSocketType socketType, const NetworkAddress* pListenAddress, uint32 maxClientCount); // 0x00C874C0 (networksystem_posix.cpp:626)
        void destroyAsyncNetworkSocket(AsyncNetworkSocket* pSocket); // 0x00C84AE0 (networksystem_posix.cpp:668)
        bool startAsyncAccept(AsyncNetworkSocket* pListenSocket, void* pUserData); // 0x00C87630 (networksystem_posix.cpp:705)
        bool startAsyncConnect(AsyncNetworkSocket* pSocket, const NetworkAddress& address, void* pUserData); // 0x00C87770 (networksystem_posix.cpp:747)
        bool startAsyncRead(AsyncNetworkSocket* pSocket, void* pTarget, uint32 size, void* pUserData); // 0x00C87870 (networksystem_posix.cpp:787)
        bool startAsyncWrite(AsyncNetworkSocket* pSocket, const void* pSource, uint32 size, void* pUserData); // 0x00C87970 (networksystem_posix.cpp:831)
        bool pushAsyncUserEvent(NetworkSystem* pNetworkSystem, void* pUserData); // 0x00C87A70 (networksystem_posix.cpp:875)
        bool waitForAsyncOperationResult(NetworkOperationResult* pResult, NetworkSystem* pNetworkSystem, uint32 timeOut); // 0x00C87B10 (networksystem_posix.cpp:908)
        bool getLocalSocketAddress(NetworkAddress* pAddress, const AsyncNetworkSocket* pSocket); // 0x00C81CB0 (networksystem_posix.cpp:1008)
        bool getPeerSocketAddress(NetworkAddress* pAddress, const AsyncNetworkSocket* pSocket); // 0x00C81D40 (networksystem_posix.cpp:1034)
    }
}
