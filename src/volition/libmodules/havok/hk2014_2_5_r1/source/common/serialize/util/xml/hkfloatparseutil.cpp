// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): float text for the XML formats, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/substring/hksubstring.h"

#include <string.h>

// 0x013745A4: indexed by Type; none for TYPE_FINITE
const char* const hkFloatParseUtil::s_textLut[] =
{
    "-1.#INF00",
    "1.#INF00",
    "-1.#QNAN0",
    "1.#QNAN0",
    "-1.#IND00",
    nullptr,
};

// 0x013745BC: the bits of each Type's value
const unsigned int hkFloatParseUtil::s_valuesLut[] =
{
    0xFF800000, // -inf
    0x7F800000, // inf
    0xFF800001, // negative signalling NaN
    0x7F800001, // signalling NaN
    0xFFC00000, // the x87 indefinite
    0x00000000,
};

namespace
{
    // rfg.exe moves these values through the x87 stack (fld/fstp), which quiets a signalling NaN: the two NaN
    // entries arrive as 0xFFC00001 and 0x7FC00001.
    float loadThroughX87(unsigned int bits)
    {
        if ((bits & 0x7F800000) == 0x7F800000 && (bits & 0x007FFFFF) != 0)
        {
            bits |= 0x00400000;
        }
        float f;
        memcpy(&f, &bits, sizeof(f));
        return f;
    }

    unsigned int floatBits(float f)
    {
        unsigned int bits;
        memcpy(&bits, &f, sizeof(bits));
        return bits;
    }
}

// 0x00F72F60 (hkfloatparseutil.h:53)
float hkFloatParseUtil::getFloatFromType(Type type)
{
    return loadThroughX87(s_valuesLut[type]);
}

// 0x00F72F70 (:47): by bit pattern; any other NaN is NAN or NEG_NAN by its sign
hkFloatParseUtil::Type hkFloatParseUtil::getFloatType(float f)
{
    const unsigned int bits = floatBits(f);
    if ((bits & 0x7F800000) != 0x7F800000)
    {
        return TYPE_FINITE;
    }
    if (bits == 0x7F800000)
    {
        return TYPE_INF;
    }
    if (bits == 0xFF800000)
    {
        return TYPE_NEG_INF;
    }
    if (bits == 0xFFC00000)
    {
        return TYPE_IND;
    }
    return (bits >> 31) ? TYPE_NEG_NAN : TYPE_NAN;
}

// 0x00F72FC0 (:73)
hkFloatParseUtil::Type hkFloatParseUtil::getFloatType(double f)
{
    uint64_t bits;
    memcpy(&bits, &f, sizeof(bits));
    const unsigned int hi = (unsigned int)(bits >> 32);
    const unsigned int lo = (unsigned int)bits;
    if ((hi & 0x7FF00000) != 0x7FF00000)
    {
        return TYPE_FINITE;
    }
    if (lo == 0)
    {
        if (hi == 0x7FF00000)
        {
            return TYPE_INF;
        }
        if (hi == 0xFFF00000)
        {
            return TYPE_NEG_INF;
        }
        if (hi == 0xFFF80000)
        {
            return TYPE_IND;
        }
    }
    return (bits >> 63) ? TYPE_NEG_NAN : TYPE_NAN;
}

// 0x00F73030 (:99): the same type, and the same value when finite
bool hkFloatParseUtil::equals(float a, float b)
{
    const Type ta = getFloatType(a);
    if (ta != getFloatType(b))
    {
        return false;
    }
    return ta != TYPE_FINITE || a == b;
}

// 0x00F73080 (:114)
bool hkFloatParseUtil::equals(double a, double b)
{
    const Type ta = getFloatType(a);
    if (ta != getFloatType(b))
    {
        return false;
    }
    return ta != TYPE_FINITE || a == b;
}

// 0x00F730D0 (:129)
void hkFloatParseUtil::calcFloatText(float a, hkStringBuf& buf)
{
    const Type t = getFloatType(a);
    if (t == TYPE_FINITE)
    {
        buf.printf("%g", (double)a);
        return;
    }
    buf = s_textLut[t];
}

// 0x00F73120 (:142)
void hkFloatParseUtil::calcFloatText(double a, hkStringBuf& buf)
{
    const Type t = getFloatType(a);
    if (t == TYPE_FINITE)
    {
        buf.printf("%g", a);
        return;
    }
    buf = s_textLut[t];
}

// 0x00F73170 (:155): ".0" appended when %g printed no point (also after an exponent, "1e+010.0")
void hkFloatParseUtil::calcFloatTextWithPoint(float a, hkStringBuf& buf)
{
    const Type t = getFloatType(a);
    if (t == TYPE_FINITE)
    {
        buf.printf("%g", (double)a);
        if (buf.indexOf('.', 0, 0x7FFFFFFF) == -1)
        {
            buf += ".0";
        }
        return;
    }
    buf = s_textLut[t];
}

// 0x00F731E0 (:170)
void hkFloatParseUtil::calcFloatTextWithPoint(double a, hkStringBuf& buf)
{
    const Type t = getFloatType(a);
    if (t == TYPE_FINITE)
    {
        buf.printf("%g", a);
        if (buf.indexOf('.', 0, 0x7FFFFFFF) == -1)
        {
            buf += ".0";
        }
        return;
    }
    buf = s_textLut[t];
}

// 0x00F73250 (:185): at most 64 characters; text with a '#' must be one of the special spellings. Original bug kept:
// the lookup runs over all 6 entries, so a '#' text matching none of the five reaches the null TYPE_FINITE entry
// and dereferences it.
hkResult hkFloatParseUtil::parseFloat(const hkSubString& str, float& valueOut)
{
    const int len = str.length();
    if (len > 64)
    {
        return HK_FAILURE;
    }
    for (const char* p = str.m_start; p < str.m_end; p++)
    {
        if (*p == '#')
        {
            for (int i = 0; i < TYPE_COUNT_OF; i++)
            {
                if (str == s_textLut[i])
                {
                    valueOut = loadThroughX87(s_valuesLut[i]);
                    return HK_SUCCESS;
                }
            }
            return HK_FAILURE;
        }
    }
    char buf[68];
    hkString::strNcpy(buf, str.m_start, len);
    buf[len] = 0;
    valueOut = hkString::atof(buf);
    return HK_SUCCESS;
}
