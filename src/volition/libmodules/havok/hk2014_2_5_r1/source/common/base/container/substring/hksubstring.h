// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/substring/hksubstring.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members from the 2014.1 SDK
// (hkSubString.h/.inl), checked against rfg.exe (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkOstream;

// size 0x8
struct hkSubString
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSubString);
    // 0x00F53610 (hksubstring.inl:40)
    hkSubString() : m_start(nullptr), m_end(nullptr) {}
    hkSubString(const hkSubString& rhs) : m_start(rhs.m_start), m_end(rhs.m_end) {}
    // 0x00F535F0 (hksubstring.inl:25)
    hkSubString(const char* start, int len) : m_start(start), m_end(start + len) {}
    // 0x00F535D0 (hksubstring.inl:19)
    explicit hkSubString(const char* start, const char* end) : m_start(start), m_end(end) {}
    // 0x00F535B0 (hksubstring.inl:11)
    explicit hkSubString(const char* cur) : m_start(cur), m_end(cur + hkString::strLen(cur)) {}
    void set(const char* cur, const char* end) { m_start = cur; m_end = end; }
    void set(const char* cur, int len) { m_start = cur; m_end = cur + len; }
    void operator=(const char* rhs); // 0x00FC2810
    // 0x00F71450 (hksubstring.inl:56)
    void operator=(const hkSubString& rhs) { m_start = rhs.m_start; m_end = rhs.m_end; }
    int getInt() const; // 0x00FC2770
    hkBool isEmpty() const { return m_end == m_start; }
    void empty() { m_end = m_start = nullptr; }
    static unsigned int isValid(const hkSubString& key) { return key.isValid(); }
    hkBool isValid() const { return m_start <= m_end; }
    void setInvalid() { m_start = m_end + 1; }
    int compareTo(const hkSubString& rhs) const; // 0x00FC2900
    hkBool operator==(const hkSubString& rhs) const; // 0x00FC2840
    hkBool operator==(const char* rhs) const; // 0x00FC28B0
    // 0x00F53580 (hksubstring.h:67)
    hkBool operator!=(const char* rhs) const { return !(*this == rhs); }
    hkBool operator!=(const hkSubString& rhs) const { return !(*this == rhs); }
    // 0x00F535A0 (hksubstring.h:70)
    int length() const { return int(m_end - m_start); }
    unsigned int calcHash() const; // 0x00FC27E0
    static unsigned int hash(const hkSubString& key, unsigned int mod) { return key.calcHash() & mod; }
    static void invalidate(hkSubString& key) { key.setInvalid(); }
    static unsigned int equal(const hkSubString& key0, const hkSubString& key1) { return key0 == key1; }

    const char* m_start; // 0x00
    const char* m_end; // 0x04
};

hkOstream& operator<<(hkOstream& stream, const hkSubString& text); // 0x00FC27C0
