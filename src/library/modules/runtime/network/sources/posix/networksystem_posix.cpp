// Original: library/modules/runtime/network/sources/posix/networksystem_posix.cpp
//
// Recovery stage: internal functions, file-private types and file-static globals only (rfg.pdb, no public symbol).
// The external interface is declared in networksystem.hpp. Implementation pending (multiplayer.md MP2); this file is
// not compiled yet. The posix implementation is the one rfg.exe uses on win32 (winsock select loop).

#include "keen/network/networksystem.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/sizedarray.hpp"

#include <winsock2.h>

namespace keen
{
    // ---- file-private types (rfg.pdb; NetworkSystem and AsyncNetworkSocket are opaque in networksystem.hpp)

    // size 0x8
    struct AsyncNetworkOperationConnectData
    {
        NetworkAddress address; // 0x00
    };

    // size 0xC
    struct AsyncNetworkOperationAcceptData
    {
        AsyncNetworkSocket* pSocket; // 0x00
        NetworkAddress address; // 0x04
    };

    // size 0xC
    struct AsyncNetworkOperationReadData
    {
        uint8* pDest; // 0x00
        uint32 size; // 0x04
        uint32 read; // 0x08
    };

    // size 0xC
    struct AsyncNetworkOperationWriteData
    {
        const uint8* pSource; // 0x00
        uint32 size; // 0x04
        uint32 written; // 0x08
    };

    // size 0x2
    struct AsyncNetworkOperationUserData
    {
        bool written; // 0x00
        bool read; // 0x01
    };

    // size 0x1C
    struct AsyncNetworkOperationData
    {
        AsyncNetworkSocket* pSocket; // 0x00
        AsyncNetworkOperationType type; // 0x04
        union // size 0xC
        {
            AsyncNetworkOperationConnectData connectData; // 0x00
            AsyncNetworkOperationAcceptData acceptData; // 0x00
            AsyncNetworkOperationReadData read; // 0x00
            AsyncNetworkOperationWriteData write; // 0x00
            AsyncNetworkOperationUserData user; // 0x00
        } data; // 0x08
        void* pUserData; // 0x14
        uint32 startTime; // 0x18
    };

    // size 0x35C
    struct NetworkSystem
    {
        NetworkSystem(); // 0x00C84AB0
        ~NetworkSystem(); // 0x00C825D0

        MemoryAllocator* pAllocator; // 0x00
        Mutex mutex; // 0x04
        AsyncNetworkSocket* pUserEventReadSocket; // 0x3C, socket pair for pushAsyncUserEvent
        AsyncNetworkSocket* pUserEventWriteSocket; // 0x40
        fd_set readSet; // 0x44
        fd_set writeSet; // 0x148
        fd_set errorSet; // 0x24C
        SizedArray<AsyncNetworkOperationData> operations; // 0x350
    };

    // size 0xC
    struct AsyncNetworkSocket
    {
        AsyncNetworkSocket(); // 0x00C81780 (:28)

        NetworkSystem* pSystem; // 0x00
        uint32 socketHandle; // 0x04
        bool isConnected; // 0x08
    };

    namespace network
    {
        // ---- file-static globals
        // uint32 s_libraryRefCount (0x038BD650) is used here too (the inlined initializePlatform/shutdownPlatform);
        // placed in networkplatform_win32.cpp.

        // ---- internal functions
        static bool handleOperationErrors(NetworkSystem* pNetworkSystem, NetworkOperationResult* pResult); // 0x00C87010 (:133)
        // LTCG: arguments in EAX (not mapped yet).
        static void addReadSocket(NetworkSystem* pNetworkSystem, const AsyncNetworkSocket* pSocket); // 0x00C81790 (:236)
        // LTCG: arguments in EAX (not mapped yet).
        static void addWriteSocket(NetworkSystem* pNetworkSystem, const AsyncNetworkSocket* pSocket); // 0x00C817D0 (:241)
        // LTCG: arguments in EAX (not mapped yet).
        static void addErrorSocket(NetworkSystem* pNetworkSystem, const AsyncNetworkSocket* pSocket); // 0x00C81820 (:246)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static void setOperationResultError(NetworkOperationResult* pResult, const AsyncNetworkOperationData& operationData, uint32 errorCode); // 0x00C81870 (:251)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static void setOperationResultClosed(NetworkOperationResult* pResult, const AsyncNetworkOperationData& operationData); // 0x00C81890 (:259)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static void setOperationResultOk(NetworkOperationResult* pResult, const AsyncNetworkOperationData& operationData); // 0x00C818B0 (:267)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static bool hasSocketReadData(NetworkSystem* pNetworkSystem, const AsyncNetworkOperationData& operationData); // 0x00C818D0 (:275)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static bool hasSocketWriteData(NetworkSystem* pNetworkSystem, const AsyncNetworkOperationData& operationData); // 0x00C818F0 (:280)
        static bool hasSocketUserData(NetworkSystem* pNetworkSystem, const AsyncNetworkOperationData& operationData); // 0x00C81910 (:285)
        // LTCG: arguments in EAX,ECX (not mapped yet).
        static bool hasSocketErrorData(NetworkSystem* pNetworkSystem, const AsyncNetworkOperationData& operationData); // 0x00C81950 (:293)
        // LTCG: arguments in ECX (not mapped yet).
        static bool isSocketOperationAvailable(NetworkSystem* pNetworkSystem, AsyncNetworkSocket* pSocket, AsyncNetworkOperationType type); // 0x00C848A0 (:298)
        static bool updateConnect(NetworkSystem* pNetworkSystem, AsyncNetworkOperationData* pOperationData); // 0x00C81970 (:312)
        static bool updateAccept(NetworkSystem* pNetworkSystem, AsyncNetworkOperationData* pOperationData); // 0x00C81AA0 (:356)
        static bool updateWrite(NetworkSystem* pNetworkSystem, AsyncNetworkOperationData* pOperationData); // 0x00C848D0 (:392)
        static bool updateRead(NetworkSystem* pNetworkSystem, AsyncNetworkOperationData* pOperationData); // 0x00C84960 (:426)
        static bool updateUser(NetworkSystem* pNetworkSystem, AsyncNetworkOperationData* pOperationData); // 0x00C849E0 (:460)
        static bool isOperationFinished(NetworkSystem* pNetworkSystem, NetworkOperationResult* pResult, const AsyncNetworkOperationData& operationData); // 0x00C81B30 (:487)
    }
}
