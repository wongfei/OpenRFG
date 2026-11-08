// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"

class hkBool;

// size 0x8
class hkError : public hkReferencedObject, public hkSingleton<hkError>
{
public:
    enum Message
    {
        MESSAGE_ALL = 0,
        MESSAGE_REPORT = 0,
        MESSAGE_WARNING = 1,
        MESSAGE_ASSERT = 2,
        MESSAGE_ERROR = 3,
        MESSAGE_NONE = 4,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00CF7AA0
    virtual ~hkError(); // vtbl+0x00 0x00CF74C0
    virtual int message(Message, int, const char*, const char*, int) = 0; // vtbl+0x10
    virtual void setEnabled(int, hkBool) = 0; // vtbl+0x14
    virtual hkBool isEnabled(int) = 0; // vtbl+0x18
    virtual void setMinimumMessageLevel(Message msg); // vtbl+0x1C 0x00CF7A60
    virtual Message getMinimumMessageLevel(); // vtbl+0x20 0x00CF7A70
    virtual void enableAll() = 0; // vtbl+0x24
    virtual void sectionBegin(int id, const char* sectionName); // vtbl+0x28 0x00CF7A80
    static int messageReport(int id, const char* description, const char* file, int line); // 0x00D16A60
    static int messageWarning(int id, const char* description, const char* file, int line); // 0x00D0ADA0
    static int messageAssert(int, const char*, const char*, int);
    static int messageError(int id, const char* description, const char* file, int line); // 0x0071B6A0 (.h:70)
    hkError(); // 0x00CF7A50
    virtual void sectionEnd(); // vtbl+0x2C 0x00CF7A90

};

// size 0xC
// An output stream into a caller's buffer (error message formatting): its writer lives in the buffer's last 32 bytes.
class hkErrStream : public hkOstream
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00CE7B20
    hkErrStream(void* buf, int bufSize); // 0x00CE7980
    virtual ~hkErrStream(); // vtbl+0x00 0x00CE7960

};

void hkReferenceCountError(const hkReferencedObject* o, const char* why); // 0x00CE79E0
void hkErrorMessage(const char* c); // 0x00CE7AC0

namespace hkErrorFwd
{
    int messageReport(int id, const char* description, const char* file, int line); // 0x00CE7880
    int messageWarning(int id, const char* description, const char* file, int line); // 0x00CE78B0
    int messageAssert(int id, const char* description, const char* file, int line); // 0x00CE78E0
    int messageError(int id, const char* description, const char* file, int line); // 0x00CE7910
}
