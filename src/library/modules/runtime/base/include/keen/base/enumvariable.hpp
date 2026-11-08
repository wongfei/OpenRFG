// Original: library/modules/runtime/base/include/keen/base/enumvariable.hpp
//
// EnumVariableBase, an ini variable whose value is one of a set of named entries. Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/inivariables.hpp"

namespace keen
{
    struct EnumDescriptionEntry;

    // size 0x2C
    class EnumVariableBase : public IniVariableBase
    {
    public:
        // size 0x8
        struct Entry
        {
            int value; // 0x00
            const char* pName; // 0x04
        };

        uint32 getEnumValueCount() const;
        const char* getEnumValueString(uint32) const;
        int getEnumValue(uint32) const;
        uint32 getEnumValueIndexFromValue(int) const;
        const char* getStringFromValue(int, const char*) const;
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C
        bool getValueFromString(int*, const char*, bool) const;
        int getValue() const;
        void setValue(int);
        virtual EnumVariableBase* castToEnumVariable(); // vtbl+0x1C
    protected:
        EnumVariableBase(const char*, int, const char*, const EnumDescriptionEntry*, uint32, const char*);

        int m_value; // 0x1C
        int m_defaultValue; // 0x20
    private:
        const EnumDescriptionEntry* m_pEntries; // 0x24
        uint32 m_entryCount; // 0x28
    };
}
