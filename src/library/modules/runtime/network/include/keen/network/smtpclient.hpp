// Original: library/modules/runtime/network/include/keen/network/smtpclient.hpp
// Implementation: library/modules/runtime/network/sources/global/smtpclient.cpp
//
// A minimal SMTP client for sending mails (crash and bug reports).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AsyncNetworkSocket;
    class MemoryAllocator;
    struct NetworkSystem;

    namespace network
    {
        enum SmtpState
        {
            SmtpState_Disconnected = 0,
            SmtpState_Connected = 1,
            SmtpState_Error = 2,
        };

    }

    // size 0x420
    struct SmtpClient
    {
        NetworkSystem* pSystem; // 0x00
        AsyncNetworkSocket* pSocket; // 0x04
        uint32 timeOut; // 0x08
        network::SmtpState state; // 0x0C
        uint32 result; // 0x10
        char readBuffer[1024]; // 0x14
        uint32 readBufferSize; // 0x414
        char* pMessageBuffer; // 0x418
        uint32 messageBufferSize; // 0x41C
    };

    namespace network
    {
        SmtpClient* createSmtpClient(MemoryAllocator* pAllocator, uint32* pError, const char* pUserName, const char* pSmtpServerIP, uint16 serverPort, uint32 timeOut, uint32 maxMessageSize); // 0x00C88FC0 (:269)
        void destroySmtpClient(MemoryAllocator* pAllocator, SmtpClient* pClient); // 0x00C88F10 (:372)
        bool sendMail(SmtpClient* pClient, uint32* pError, const char* pSubject, const char* pMessage, const char* pFrom, const char* pTo); // 0x00C88F80 (:397)
        bool sendMail(SmtpClient* pClient, uint32* pError, const char* pSubject, const char* pMessage, const char* pFrom, const char** pToList, uint32 toCount, const char** pCCList, uint32 ccCount, const char** pBCCList, uint32 bccCount); // 0x00C88930 (:405)
    }
}
