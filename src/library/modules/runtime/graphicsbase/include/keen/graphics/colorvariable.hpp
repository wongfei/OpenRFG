// Original: library/modules/runtime/graphicsbase/include/keen/graphics/colorvariable.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/colorvariable.cpp
//
// ColorVariable, a debug ini variable holding an HDR color (Vector4).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/inivariables.hpp"
#include "keen/math/vector4.hpp"

namespace keen
{
    // size 0x3C
    class ColorVariable : public IniVariableBase
    {
    public:
        ColorVariable(const char* pName, const Vector4& value, const char* pDescription, const char* pOwner); // 0x00CE32C0 (:11)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00CE57F0 (:17)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00CE3320 (:40)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00CE3380 (:45)
        virtual void reset(); // vtbl+0x10 0x00CE5890 (:50)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00CE33A0 (:55)
        uint32 getLdrColorValue() const; // 0x00CE3440 (:65)
        const Vector4& getValue() const;
        operator Vector4() const;
        Vector4 operator=(const Vector4&);
    private:
        void setValue(const Vector4& value); // 0x00CE5160 (:74)
    public:
        virtual ~ColorVariable(); // vtbl+0x00 0x00CE3310

    private:
        Vector4 m_value; // 0x1C
        Vector4 m_defaultValue; // 0x2C
    };
}
