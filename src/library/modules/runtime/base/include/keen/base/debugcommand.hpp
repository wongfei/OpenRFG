// Original: library/modules/runtime/base/include/keen/base/debugcommand.hpp
// Implementation: library/modules/runtime/base/sources/global/debugcommand.cpp
//
// The debug command system: handlers registered by name in the DebugCommandRegistry, the broker that dispatches
// commands from a remote debug connection, and per-frame update handlers.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;
    class NetworkReader;
    class NetworkWriter;

    enum DebugConnectionState
    {
        DebugConnectionState_Disconnected = 0,
        DebugConnectionState_Connected = 1,
    };

    enum DebugCommandResult; // no definition in PDB

    // size 0x1
    class DebugCommandHandler
    {
    };

    // size 0x1
    class DebugCommandSystemUpdateHandler
    {
    };

    // size 0x4
    class DebugCommandBroker
    {
    public:
        virtual ~DebugCommandBroker(); // vtbl+0x00 0x00BEF720 (:10)
        virtual NetworkWriter* openWriteCommand(uint32, uint32) = 0; // vtbl+0x04
        virtual uint32 closeWriteCommand() = 0; // vtbl+0x08
        virtual DebugCommandResult openResponse(NetworkReader**, uint32) = 0; // vtbl+0x0C
        virtual void closeResponse() = 0; // vtbl+0x10

        // 0x00 vfptr
    };

    namespace DebugCommandRegistry
    {
        void create(MemoryAllocator*); // 0x00BEF750 (:195)
        void destroy(); // 0x00BEF760 (:199)
        void registerCommand(uint32, DebugCommandHandler*); // 0x00BEF770 (:203)
        void registerCommand(uint32, uint32, DebugCommandHandler*); // 0x00BEF780 (:207)
        void unregisterCommand(uint32); // 0x00BEF790 (:211)
        void unregisterCommand(uint32, uint32); // 0x00BEF7A0 (:215)
        void registerUpdateHandler(DebugCommandSystemUpdateHandler*); // 0x00BEF7B0 (:219)
        void unregisterUpdateHandler(DebugCommandSystemUpdateHandler*); // 0x00BEF7C0 (:223)
        DebugCommandHandler* findCommandHandler(uint32, uint32); // 0x00BEF7D0 (:227)
        void updateHandlers(DebugCommandBroker*, float); // 0x00BEF7E0 (:232)
        void setConnectionState(DebugConnectionState, uint32); // 0x00BEF7F0 (:236)
    }
}
