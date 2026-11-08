// Original: library/modules/runtime/base/include/keen/base/types.hpp
#pragma once

#include <stddef.h>

// The integer typedefs live in the global namespace (the PDB records them as ::uint32 etc.).
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;
typedef signed char         sint8;
typedef short               sint16;
typedef int                 sint32;
typedef long long           sint64;
typedef float               float32;

namespace keen
{
    // size 0x8
    struct float2
    {
        float x; // 0x00
        float y; // 0x04
    };

    // size 0xC
    struct float3
    {
        float x; // 0x00
        float y; // 0x04
        float z; // 0x08
    };

    // size 0x10
    struct float4
    {
        float x; // 0x00
        float y; // 0x04
        float z; // 0x08
        float w; // 0x0C
    };

    void setFloat2(float2* pValue, float x, float y); // 0x004B1780
    void setFloat3(float3* pValue, float x, float y, float z); // 0x004B1790
    void setFloat4(float4* pValue, float x, float y, float z, float w); // 0x004B17B0
    float2 createFloat2(float x, float y); // 0x004B17E0
    float3 createFloat3(float x, float y, float z); // 0x004B1800
    float4 createFloat4(float x, float y, float z, float w); // 0x004B1820
    float4 scaleFloat4(float4 value, float factor); // 0x004B1850

    // Non-owning character range.
    // size 0x8
    struct String
    {
        String(); // 0x00BED2C0
        String(const char* pInStart, const char* pInEnd); // 0x00BED2D0
        bool isEmpty() const; // 0x00BED2F0
        uint32 getLength() const;

        const char* pStart; // 0x00
        const char* pEnd; // 0x04
    };
}
