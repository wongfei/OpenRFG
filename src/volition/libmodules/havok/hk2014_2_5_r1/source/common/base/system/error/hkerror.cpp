// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the error handler singleton (hkBaseSystem::init installs an hkDefaultError;
// the game replaces it with its own, havok_memory_init) and the inline defaults of its interface.
//
// Not reimplemented: the messages of hkReferenceCountError and hkErrorMessage are built with hkErrStream (an hkOstream
// over a 512-byte buffer, S phase); here snprintf writes the same text into a buffer of that size.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/buffered/hkbufferedstreamwriter.h"

#include <intrin.h>
#include <stdio.h>

// ---- globals
template<> hkError* hkSingleton<hkError>::s_instance = nullptr; // HK_SINGLETON_IMPLEMENTATION(hkError)

// 0x00CF7A50
hkError::hkError()
{
}

// 0x00CF74C0
hkError::~hkError()
{
}

// 0x00CF7A60 (.h:52)
void hkError::setMinimumMessageLevel(Message msg)
{
}

// 0x00CF7A70 (.h:55)
hkError::Message hkError::getMinimumMessageLevel()
{
    return MESSAGE_ALL;
}

// 0x00CF7A80 (.h:61)
void hkError::sectionBegin(int id, const char* sectionName)
{
}

// 0x00CF7A90 (.h:64)
void hkError::sectionEnd()
{
}

// 0x00D16A60 (.h:67)
int hkError::messageReport(int id, const char* description, const char* file, int line)
{
    return getInstance().message(MESSAGE_REPORT, id, description, file, line);
}

// 0x00D0ADA0 (.h:68)
int hkError::messageWarning(int id, const char* description, const char* file, int line)
{
    return getInstance().message(MESSAGE_WARNING, id, description, file, line);
}

int hkError::messageAssert(int id, const char* description, const char* file, int line)
{
    return getInstance().message(MESSAGE_ASSERT, id, description, file, line);
}

// 0x0071B6A0 (.h:70)
int hkError::messageError(int id, const char* description, const char* file, int line)
{
    return getInstance().message(MESSAGE_ERROR, id, description, file, line);
}

// 0x00CE79E0 (:30)
// HK_ERROR(0x2c66f2d8) with the reference count error checklist.
void hkReferenceCountError(const hkReferencedObject* o, const char* why)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "Reference count error on object %p with ref count of %u in %s.\n"
             " * Are you calling delete instead of removeReference?\n"
             " * Have you called removeReference too many times?\n"
             " * In a multithreaded environment, what is the hkReferencedObject lock mode you use (see setLockMode())?\n"
             " * Is this a valid object?\n"
             " * Do you have more than 32768 references? (unlikely)\n",
             (const void*)o, (unsigned int)o->getReferenceCount(), why);
    if (hkErrorFwd::messageError(0x2C66F2D8, buffer, "System\\Error\\hkError.cpp", 38) != 0)
    {
        __debugbreak(); // HK_BREAKPOINT
    }
}

// 0x00CE7960 (stub_impl.md S4): the writer is part of the buffer, so it is dropped without a release
hkErrStream::~hkErrStream()
{
    *reinterpret_cast<hkStreamWriter**>(&m_writer) = nullptr; // the pointer word cleared without a release
}

// 0x00CE7980 (stub_impl.md S4): the writer is built in the buffer's last 32 bytes over the rest (a string block)
hkErrStream::hkErrStream(void* buf, int bufSize)
    : hkOstream((hkStreamWriter*)nullptr)
{
    const int writerBytes = 0x20; // sizeof(hkBufferedStreamWriter) rounded to 32
    hkBufferedStreamWriter* w = new ((char*)buf + bufSize - writerBytes) hkBufferedStreamWriter(buf, bufSize - writerBytes, true);
    m_writer.setAndDontIncrementRefCount(w);
}

// 0x00CE7AC0 (:44)
void hkErrorMessage(const char* c)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s", c);
    if (hkErrorFwd::messageError(0x2636FE25, buffer, "System\\Error\\hkError.cpp", 45) != 0)
    {
        __debugbreak(); // HK_BREAKPOINT
    }
}
