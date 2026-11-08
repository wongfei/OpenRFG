// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the inline members of
// hkstringbuf.inl added by hand (nobuild). A string kept in an hkInplaceArray of 128 characters (temp allocator),
// always terminated: the size counts the terminator.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

class hkBool;
class hkStringPtr;

// size 0x8C
class hkStringBuf
{
public:
    enum ReplaceType
    {
        REPLACE_ONE = 0,
        REPLACE_ALL = 1,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStringBuf);
public:
    hkStringBuf(char*, int, int);
public:
    hkStringBuf(const hkStringBuf& s); // 0x00D14C10
    hkStringBuf(const char* b, int len); // 0x00D14BA0
    hkStringBuf(const char* s0, const char* s1, const char* s2, const char* s3, const char* s4, const char* s5); // 0x00D15730
    hkStringBuf(const hkStringPtr& s); // 0x00D15700
    hkStringBuf(const char* s); // 0x00D14B10
    // 0x00D0EA10
    hkStringBuf()
    {
        setLength(0);
    }

    hkStringBuf& operator=(const char* s); // 0x00D14C80
    hkStringBuf& operator=(const hkStringBuf& s); // 0x00D14990
public:
    // 0x00D0E9C0
    void setLength(int length)
    {
        m_string.setSize(length + 1);
        m_string[length] = 0;
    }
public:
    operator const char*() const { return m_string.begin(); }
    const char* cString() const { return m_string.begin(); } // 0x00CE89B0
    int getLength() const { return m_string.getSize() - 1; } // 0x00CE89C0
    char operator[](int i) const { return m_string[i]; } // 0x00CFD880
    int indexOf(const char* s, int startIndex, int endIndex) const; // 0x00D14440
    int indexOf(char c, int startIndex, int endIndex) const; // 0x00D145D0
    int indexOfCase(const char* needle) const; // 0x00D14470
    int lastIndexOf(const char* needle, int startIndex, int endIndex) const; // 0x00D14500
    int lastIndexOf(char c, int start, int end) const; // 0x00D14610
    int compareTo(const char* other) const; // 0x00D14650
    int compareToIgnoreCase(const char* other) const; // 0x00D14670
    hkBool operator<(const char* other) const; // 0x00D14690
    unsigned int operator==(const char* other) const; // 0x00D146B0
    unsigned int startsWith(const char* s) const; // 0x00D14540
    unsigned int startsWithCase(const char* s) const; // 0x00D14580
    unsigned int endsWith(const char* s) const; // 0x00D146D0
    unsigned int endsWithCase(const char* s) const; // 0x00D14720
    int split(int sep, hkArray<char const*,hkContainerTempAllocator>& bits); // 0x00D148E0
    void clear(); // 0x00D14D30
    void printf(const char*, ...); // 0x00D14D80
    void appendPrintf(const char*, ...); // 0x00D15780
    hkStringBuf& operator+=(const char* other); // 0x00D14EA0
    hkStringBuf& appendJoin(const char* s0, const char* s1, const char* s2, const char* s3, const char* s4, const char* s5); // 0x00D14ED0
    hkStringBuf& setJoin(const char* s0, const char* s1, const char* s2, const char* s3, const char* s4, const char* s5); // 0x00D14FE0
    void chompStart(int n); // 0x00D147A0
    void chompEnd(int n); // 0x00D15020
    void slice(int startOffset, int length); // 0x00D15080
    void set(const char* s, int len); // 0x00D150E0
    void append(const char* s, int len); // 0x00D15150
    void prepend(const char* s, int len); // 0x00D15940
    void insert(int pos, const char* s, int len); // 0x00D151E0
    void pathBasename(); // 0x00D147D0
    void pathDirname(); // 0x00D15220
    void pathChildname();
    void pathNormalize(); // 0x00D15270
    void pathExtension(); // 0x00D154C0
    hkStringBuf& pathAppend(const char* p0, const char* p1, const char* p2); // 0x00D14A10
    unsigned int replace(const char* from, const char* to, ReplaceType rtype); // 0x00D154F0
    unsigned int replace(char from, char to, ReplaceType rt); // 0x00D14810
    void lowerCase(); // 0x00D14860
    void upperCase(); // 0x00D148A0
    hkArray<char,hkContainerTempAllocator>& getArray(); // 0x00D14410
    ~hkStringBuf() {} // 0x00CFDB70 (the array frees a heap buffer)

public:
    hkInplaceArray<char,128,hkContainerTempAllocator> m_string; // 0x00
};
