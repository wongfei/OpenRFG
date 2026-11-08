// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkBool;
class hkClass;
struct hkInternalClassMember;

// size 0x1
class hkUFloat8
{
public:
    enum
    {
        MAX_VALUE = 256,
        ENCODED_EXPONENT_BITS = 5,
        ENCODED_MANTISSA_BITS = 11,
        ENCODED_EXPONENT_BIAS = 119,
        ENCODED_EXPONENT_MASK = 31,
        ENCODED_MANTISSA_MASK = 2047,
        FLOAT_MANTISSA_BITS = 23,
        FLOAT_MANTISSA_MASK = 0x7FFFFF,
        ENCODE_MANTISSA_SHIFT = 12,
        DECODE_EXPONENT_BIAS = 0x3B800,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkUFloat8);
    static const hkClass& staticClass(); // 0x00CE9FA0
    hkBool operator==(const hkUFloat8&) const;
    HK_DECLARE_POD_TYPE();
    hkUFloat8(const double);
    hkUFloat8(const float f); // 0x00D6C810
    hkUFloat8(); // 0x00D6C800
    bool isZero() const; // 0x00E2F4E0
    void setZero();
    bool isMax() const;
    void setMax();
    void setArithmeticMean(const hkUFloat8&, const hkUFloat8&);
    operator float() const; // 0x00DF2A00
    hkUFloat8& operator=(const double& fv); // 0x00FB6CA0 (hkbasetypes.cpp:75)
    hkUFloat8& operator=(const float& fv); // 0x00FB6BA0 (hkbasetypes.cpp:80)
    static float decodeFloat(const unsigned short i); // 0x00DF1200
public:
    static unsigned short getEncodedFloat(unsigned char index); // 0x00FBEE60

public:
    static const hkInternalClassMember Members[];
    unsigned char m_value; // 0x00
};
