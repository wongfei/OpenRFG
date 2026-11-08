// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); s_textLut and the inline
// getFloatFromType added by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkStringBuf;
struct hkSubString;

// size 0x1: the special float values as MSVC prints them ("1.#INF00", ...), for writing and reading XML
class hkFloatParseUtil
{
public:
    enum Type
    {
        TYPE_NEG_INF = 0,
        TYPE_INF = 1,
        TYPE_NEG_NAN = 2,
        TYPE_NAN = 3,
        TYPE_IND = 4,
        TYPE_FINITE = 5,
        TYPE_COUNT_OF = 6,
    };

    static Type getFloatType(double f); // 0x00F72FC0
    static Type getFloatType(float f); // 0x00F72F70
    static bool equals(double a, double b); // 0x00F73080
    static bool equals(float a, float b); // 0x00F73030
    static void calcFloatText(double a, hkStringBuf& buf); // 0x00F73120
    static void calcFloatText(float a, hkStringBuf& buf); // 0x00F730D0
    static void calcFloatTextWithPoint(double a, hkStringBuf& buf); // 0x00F731E0
    static void calcFloatTextWithPoint(float a, hkStringBuf& buf); // 0x00F73170
    static hkResult parseFloat(const hkSubString& str, float& valueOut); // 0x00F73250
    static float getFloatFromType(Type type); // 0x00F72F60

public:
    static const unsigned int s_valuesLut[]; // 0x013745BC
    static const char* const s_textLut[]; // 0x013745A4
};
