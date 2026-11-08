// Original: library/modules/runtime/math/include/keen/math/vectorvariable.hpp
// Implementation: library/modules/runtime/math/sources/global/vectorvariable.cpp
//
// Vector3Variable, a debug ini variable holding a Vector3 (see keen/base/inivariables.hpp).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/inivariables.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x50
    class Vector3Variable : public IniVariableBase
    {
    public:
        Vector3Variable(const char* pName, const Vector3& value, const Vector3& minValue, const Vector3& maxValue, const char* pDescription, const char* pOwner); // 0x00C457E0 (:21)
        Vector3Variable(const char* pName, const Vector3& value, const char* pDescription, const char* pOwner); // 0x00C45780 (:13)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00C58950 (:32)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00C45860 (:50)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00C458B0 (:55)
        virtual void reset(); // vtbl+0x10 0x00C589C0 (:60)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00C458D0 (:65)
        const Vector3& getValue() const;
        const Vector3& getMinValue() const;
        const Vector3& getMaxValue() const;
        bool getClampValue() const;
        operator Vector3() const;
        Vector3 operator=(const Vector3&);
    private:
        void setValue(const Vector3& value); // 0x00C54880 (:89)
    public:
        virtual ~Vector3Variable(); // vtbl+0x00 0x00C457D0

    private:
        Vector3 m_value; // 0x1C
        Vector3 m_defaultValue; // 0x28
        Vector3 m_minValue; // 0x34
        Vector3 m_maxValue; // 0x40
        bool m_clampValue; // 0x4C
    };
}
