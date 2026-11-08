// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkdefaulterror.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

class hkBool;

// size 0x44
class hkDefaultError : public hkError
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x008119B0, new(unsigned int nbytes) 0x00CE67F0, delete(void* p, unsigned int nbytes) 0x00CF7D80
    hkDefaultError(void (*errorReportFunction)(const char*, void*), void* errorReportObject); // 0x00CF7890
    virtual int message(hkError::Message msg, int id, const char* description, const char* file, int line); // vtbl+0x10 0x00CF7670
    virtual void setEnabled(int id, hkBool enabled); // vtbl+0x14 0x00CF77A0
    virtual hkBool isEnabled(int id); // vtbl+0x18 0x00CF7800
    virtual void setMinimumMessageLevel(hkError::Message msg); // vtbl+0x1C 0x00CF74D0
    virtual hkError::Message getMinimumMessageLevel(); // vtbl+0x20 0x00CF74E0
    virtual void enableAll(); // vtbl+0x24 0x00CF7640
    virtual void sectionBegin(int id, const char* sectionName); // vtbl+0x28 0x00CF7840
    virtual void sectionEnd(); // vtbl+0x2C 0x00CF7780
public:
    virtual void showMessage(const char* what, int id, const char* desc, const char* file, int line, hkBool stackTrace); // vtbl+0x30 0x00CF74F0
public:
    virtual ~hkDefaultError(); // vtbl+0x00 0x00CF7DC0

    hkPointerMap<int,int,hkContainerHeapAllocator> m_disabledAssertIds; // 0x08
    hkArray<int,hkContainerHeapAllocator> m_sectionIds; // 0x14
    void (*m_errorFunction)(const char*, void*); // 0x20
    void* m_errorObject; // 0x24
    hkError::Message m_minimumMessageLevel; // 0x28
    hkCriticalSection m_lock; // 0x2C
};
