// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.cpp
//
// Hand-written (Havok 2014.2.5, havok_impl.md HK1): a string pointer that owns a heap copy of its string (bit 0 of
// the pointer marks the copy).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"

class hkFinishLoadedObjectFlag;

// size 0x4
class hkStringPtr
{
public:
    enum StringFlags
    {
        OWNED_FLAG = 1,
    };

    hkStringPtr(hkFinishLoadedObjectFlag f); // 0x00CF0AD0
    hkStringPtr(const hkStringPtr& strRef); // 0x00CF0C50
    hkStringPtr(const char* s, int len); // 0x00CF0C20
    hkStringPtr(const char* string); // 0x00CF0C00
    hkStringPtr(); // 0x00CF0AC0
    ~hkStringPtr(); // 0x00CF0C80
    hkStringPtr& operator=(const char* string); // 0x00CF0C90
    hkStringPtr& operator=(const hkStringPtr& string); // 0x00CF0CB0
    const char* cString() const; // 0x00814120 (hkstringptr.inl:10)

    // 0x00F450D0 (hkstringptr.inl): two null strings are equal, a null string equals nothing else
    unsigned int operator==(const char* cptr) const
    {
        const char* s = cString();
        if (s == nullptr)
        {
            return cptr == nullptr;
        }
        return cptr != nullptr && hkString::strCmp(s, cptr) == 0;
    }

    // 0x00F704C0 (hkstringptr.inl)
    unsigned int operator!=(const char* cptr) const
    {
        const char* s = cString();
        if (s == nullptr)
        {
            return cptr != nullptr;
        }
        return cptr == nullptr || hkString::strCmp(s, cptr) != 0;
    }
    int getLength() const; // 0x00CF0AE0
    void printf(const char*, ...); // 0x00CF0D20
    void set(const char* s, int len); // 0x00CF0CD0
    void setPointerAligned(const char* s); // 0x00CF0CF0
    unsigned int startsWith(const char*) const;
    unsigned int endsWith(const char*) const;
    unsigned int contains(const char*) const;
    int compareTo(const char* s) const; // 0x00F450A0

public:
    const char* m_stringAndFlag; // 0x00
};
