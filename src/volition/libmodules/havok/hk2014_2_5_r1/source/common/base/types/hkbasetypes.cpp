// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the hkUFloat8 encoding, and the inline hkHalf functions of hkbasetypes.h that
// have out-of-line copies. hkHalf keeps the high 16 bits of a float (sign, exponent and 7 mantissa bits; the low
// bits are cut, not rounded).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"

#include <string.h>

// ---- hkUFloat8
// 0x00FB6BA0 (:80)
// The smallest code whose value is at least fv (binary search over the 256 codes); values below the first code's
// give 0. A NaN compares unordered: it passes the first test and ends at code 1.
hkUFloat8& hkUFloat8::operator=(const float& fv)
{
    if (decodeFloat(getEncodedFloat(1)) > fv)
    {
        m_value = 0;
        return *this;
    }
    int lo = 0;
    int mid = 128;
    int hi = 256;
    for (int i = 0; i < 7; i++)
    {
        if (decodeFloat(getEncodedFloat((unsigned char)mid)) < fv)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
        mid = (hi + lo) >> 1;
    }
    if (decodeFloat(getEncodedFloat((unsigned char)mid)) < fv && mid < 255)
    {
        mid++;
    }
    m_value = (unsigned char)mid;
    return *this;
}

// 0x00FB6CA0 (:75)
hkUFloat8& hkUFloat8::operator=(const double& fv)
{
    const float f = (float)fv;
    return *this = f;
}

// ---- hkHalf (hkbasetypes.h)
// 0x00F98C50
hkHalf::hkHalf()
{
}

// 0x00F8C0F0 (.h:295)
hkHalf& hkHalf::operator=(const float& f)
{
    unsigned int bits;
    memcpy(&bits, &f, sizeof(bits));
    m_value = (short)(bits >> 16);
    return *this;
}

// 0x00D184A0 (.h:213)
hkHalf& hkHalf::operator=(const hkHalf& other)
{
    m_value = other.m_value;
    return *this;
}

// 0x00B62A20 (.h:315)
void hkHalf::store(float* f) const
{
    const unsigned int bits = (unsigned int)(int)m_value << 16;
    memcpy(f, &bits, sizeof(bits));
}

// 0x0080C1A0 (.h:334)
float hkHalf::getFloat32() const
{
    float f;
    store(&f);
    return f;
}

// 0x00813F00 (.h:329)
hkHalf::operator float() const
{
    return getFloat32();
}

// 0x00D184B0
float hkHalf::getReal() const
{
    return getFloat32();
}

// 0x00E9A5B0
short hkHalf::getStorage() const
{
    return m_value;
}

// 0x00DA02F0
void hkHalf::setZero()
{
    m_value = 0;
}

// 0x00DA0300
// The high half of 1.0f (0x3F800000).
void hkHalf::setOne()
{
    m_value = 0x3F80;
}
