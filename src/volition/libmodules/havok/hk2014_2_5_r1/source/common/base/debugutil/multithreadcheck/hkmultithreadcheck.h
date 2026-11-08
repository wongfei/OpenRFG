// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkClass;
class hkCriticalSection;
struct hkInternalClassMember;
class hkMemoryAllocator;

// size 0xC
class hkMultiThreadCheck
{
public:
    enum AccessType
    {
        HK_ACCESS_IGNORE = 0,
        HK_ACCESS_RO = 1,
        HK_ACCESS_RW = 2,
    };

    enum ReadMode
    {
        THIS_OBJECT_ONLY = 0,
        RECURSIVE = 1,
    };

    enum
    {
        MARKED_RO = -31,
        MARKED_RO_SELF_ONLY = -63,
        UNMARKED = -15,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMultiThreadCheck);
    static const hkClass& staticClass(); // 0x00CEB5D0
    hkMultiThreadCheck(); // 0x00CE7430
    void init();
    void markForRead(ReadMode mode); // 0x00DF8D10
    void markForWrite(); // 0x00DB0F60
    bool isMarkedForWrite(); // 0x00DE8B00
    bool isMarkedForReadRecursive(); // 0x00DE8B10
    void unmarkForRead(); // 0x00DF8D20
    void unmarkForWrite(); // 0x00DB0F70
    void disableChecks(); // 0x00CFE630
    void enableChecks(); // 0x00CFE640
    void reenableChecks(); // 0x00CFE670
    bool isCheckingEnabled() const; // 0x00CFE440
    void accessCheck(AccessType type) const; // 0x00CFE610
    static void accessCheckWithParent(const hkMultiThreadCheck* parentLock, AccessType parentType, const hkMultiThreadCheck& lock, AccessType type); // 0x00CFE620
    static void staticInit(hkMemoryAllocator* memoryAllocator); // 0x00CFE6A0
    static void staticQuit(); // 0x00CFE720
public:
    void markForReadImpl(ReadMode mode); // 0x00CFE470
    void markForWriteImpl(); // 0x00CFE4D0
    bool isMarkedForWriteImpl(); // 0x00CFE520
    void unmarkForReadImpl(); // 0x00CFE570
    void unmarkForWriteImpl(); // 0x00CFE5C0
public:
    static void globalCriticalSectionLock(); // 0x00CFE680
    static void globalCriticalSectionUnlock(); // 0x00CFE690

    static const hkInternalClassMember Members[]; // 0x0134AE28
    unsigned int m_threadId; // 0x00
    // static hkStackTracer s_stackTracer; // 0x03A27014 (debug stack tracer: S phase, not declared)
    // static hkStackTracer::CallTree* s_stackTree; // 0x03A2700C
    int m_stackTraceId; // 0x04
    unsigned short m_markCount; // 0x08
public:
    unsigned short m_markBitStack; // 0x0A
public:
    static hkCriticalSection* s_criticalSection; // 0x03A27010
};
