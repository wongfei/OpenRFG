// Original: volition/render_lib/code/speculartweak.hpp
// Implementation: volition/render_lib/code/speculartweak.cpp
//
// SpecularParameters, per-material specular tweaks looked up by hash.
#pragma once

#include "keen/base/hashmap.hpp"
#include "keen/base/hashmaptraits.hpp"

// size 0xC
struct SpecularParameters
{
    SpecularParameters(float _scale, float _power, float _useSpecMap); // 0x004D42E0 (.h:14)
    SpecularParameters(); // 0x004D42C0 (.h:8)

    float scale; // 0x00
    float power; // 0x04
    float enableSpecMap; // 0x08
};

const char* trimStringLeft(const char* pStringStart, const char* pStringEnd); // 0x004D4E40 (:7)
const char* trimStringRight(const char* pStringStart, const char* pStringEnd); // 0x004D4E60 (:26)
const char* extractToken(char* pTargetBuffer, unsigned int targetBufferCapacity, const char* pStart, const char* pEnd, char separatorChar); // 0x004D4E80 (:45)
bool parseSpecularTweak(const char* pText, unsigned int size, keen::HashMap<unsigned int,SpecularParameters,keen::DefaultHashmapTraits<unsigned int,SpecularParameters> >& parameters); // 0x0054E280 (:72)
