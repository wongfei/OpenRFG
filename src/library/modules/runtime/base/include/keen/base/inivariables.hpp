// Original: library/modules/runtime/base/include/keen/base/inivariables.hpp
// Implementation: library/modules/runtime/base/sources/global/inivariables.cpp
//
// Debug/tuning variables that can be set from ini files and the debug console. Every IniVariableBase registers itself in
// a global intrusive list (IniVariables::addVariable); the typed variables (bool, int, float, string, and Vector3Variable
// and ColorVariable in the math and graphicsbase modules) parse and print their value.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class EnumVariableBase;
    struct FileSystem;
    class ReadStream;
    class WriteStream;

    // size 0x1C
    class IniVariableBase
    {
    public:
        virtual ~IniVariableBase(); // vtbl+0x00 0x00BF8A90 (:437)
        const char* getName() const; // 0x00BED3B0 (.h:34)
        const char* getDescription() const;
        const char* getOwner() const;
        bool hasChanged() const; // 0x00BED3C0 (.h:37)
        void resetChangeFlag();
        virtual bool setValueFromString(const char*) = 0; // vtbl+0x04
        virtual void toString(char*, uint32) const = 0; // vtbl+0x08
        virtual void setCurrentValueAsDefaultValue() = 0; // vtbl+0x0C
        virtual void reset() = 0; // vtbl+0x10
        virtual void destroy(); // vtbl+0x14 0x00BF8AA0 (.h:45)
        virtual void getDefinitionString(char*, uint32) const = 0; // vtbl+0x18
        virtual EnumVariableBase* castToEnumVariable(); // vtbl+0x1C 0x00BF8AB0 (.h:50)
    protected:
        IniVariableBase(const IniVariableBase&);
        IniVariableBase(const char* pName, const char* pDescription, const char* pOwner); // 0x00BFE760 (:418)
        IniVariableBase& operator=(const IniVariableBase&);

        friend class IniVariableList;

        // 0x00 vfptr
        const char* m_pName; // 0x04
        const char* m_pDescription; // 0x08
        const char* m_pOwner; // 0x0C
        bool m_hasChanged; // 0x10
        IniVariableBase* m_pNext; // 0x14
        IniVariableBase* m_pPrev; // 0x18
    };

    // size 0x20
    class BoolVariable : public IniVariableBase
    {
    public:
        typedef bool ValueType;

        BoolVariable(const char* pName, bool value, const char* pDescription, const char* pOwner); // 0x00BFE7E0 (:442)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00BF8AC0 (:448)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00C064D0 (:468)
        virtual void reset(); // vtbl+0x10 0x00BF8B50 (:478)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00BF0940 (:473)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00C06500 (:483)
        void setValue(bool value); // 0x00BF0950 (:494)
        bool getValue() const;
        operator bool() const;
        BoolVariable& operator=(bool);
        virtual ~BoolVariable(); // vtbl+0x00 0x00BFE840

    private:
        bool m_value; // 0x1C
        bool m_defaultValue; // 0x1D
    };

    // size 0x30
    class IntVariable : public IniVariableBase
    {
    public:
        typedef int ValueType;

        IntVariable(const char* pName, int value, int minValue, int maxValue, const char* pDescription, const char* pOwner); // 0x00BFE920 (:511)
        IntVariable(const char* pName, int value, const char* pDescription, const char* pOwner); // 0x00BFE880 (:504)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00BFE990 (:522)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00C06550 (:530)
        virtual void reset(); // vtbl+0x10 0x00BFE9F0 (:540)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00BF0980 (:535)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00C06570 (:545)
        int getValue() const;
        int getMaxValue() const;
        int getMinValue() const;
        bool getClampValue() const;
        operator int() const;
        IntVariable& operator=(int);
    private:
        void setValue(int value); // 0x00BF8B80 (:569)
    public:
        virtual ~IntVariable(); // vtbl+0x00 0x00BFE8E0

    private:
        int m_value; // 0x1C
        int m_defaultValue; // 0x20
        int m_minValue; // 0x24
        int m_maxValue; // 0x28
        bool m_clampValue; // 0x2C
    };

    // size 0x30
    class FloatVariable : public IniVariableBase
    {
    public:
        typedef float ValueType;

        FloatVariable(const char* pName, float value, float minValue, float maxValue, const char* pDescription, const char* pOwner); // 0x00BFEAD0 (:596)
        FloatVariable(const char* pName, float value, const char* pDescription, const char* pOwner); // 0x00BFEA30 (:589)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00BFEB50 (:607)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00C065E0 (:615)
        virtual void reset(); // vtbl+0x10 0x00BFEBE0 (:625)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00BF0990 (:620)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00C06610 (:630)
        float getValue() const;
        float getMaxValue() const;
        float getMinValue() const;
        bool getClampValue() const;
        operator float() const;
        FloatVariable& operator=(float);
    private:
        void setValue(float value); // 0x00BF8BD0 (:654)
    public:
        virtual ~FloatVariable(); // vtbl+0x00 0x00BFEA90

    private:
        float m_value; // 0x1C
        float m_defaultValue; // 0x20
        float m_minValue; // 0x24
        float m_maxValue; // 0x28
        bool m_clampValue; // 0x2C
    };

    // size 0x28
    class StringVariable : public IniVariableBase
    {
    public:
        typedef const char* ValueType;

    private:
        StringVariable(const StringVariable&);
    public:
        StringVariable(const char* pName, const char* pValue, const char* pDescription, const char* pOwner); // 0x00BFEC60 (:675)
        virtual ~StringVariable(); // vtbl+0x00 0x00BF8C50 (:683)
        virtual bool setValueFromString(const char* pValueString); // vtbl+0x04 0x00C03E30 (:687)
        virtual void toString(char* pBuffer, uint32 capacity) const; // vtbl+0x08 0x00BF8C60 (:696)
        virtual void reset(); // vtbl+0x10 0x00C03EA0 (:717)
        virtual void setCurrentValueAsDefaultValue(); // vtbl+0x0C 0x00BFECF0 (:701)
        virtual void destroy(); // vtbl+0x14 0x00BF09A0 (:722)
        virtual void getDefinitionString(char* pString, uint32 capacity) const; // vtbl+0x18 0x00C066C0 (:740)
        const char* getValue() const;
        operator const char*() const;
    private:
        StringVariable& operator=(const StringVariable&);
        void setValue(const char* pValue); // 0x00BFED40 (:747)

        bool m_deleteValue; // 0x1C
        bool m_deleteDefaultValue; // 0x1D
        const char* m_pValue; // 0x20
        const char* m_pDefaultValue; // 0x24
    };

    namespace IniVariables
    {
        void initialize(); // 0x00BF08D0 (:58)
        void shutdown(); // 0x00BFE6C0 (:62)
        void addVariable(IniVariableBase* pVariable); // 0x00BFE6D0 (:67)
        void removeVariable(IniVariableBase* pVariable); // 0x00BF85C0 (:89)
        uint32 getVariableCount(); // 0x00BF8630 (:96)
        void getVariables(IniVariableBase** ppVariables, uint32 capacity); // 0x00BF8670 (:101)
        void destroyAllVariables(); // 0x00BF86F0 (:117)
        IniVariableBase* findVariable(const char* pName); // 0x00BF87A0 (:131)
        bool setVariableValue(const char* pName, const char* pValueString); // 0x00BF8830 (:149)
        bool setVariableValue(IniVariableBase* pVariable, const char* pValueString); // 0x00BF08E0 (:166)
        bool getBoolVariableValue(const char* pName, bool defaultValue); // 0x00BF8860 (:171)
        int getIntVariableValue(const char* pName, int defaultValue); // 0x00BF88F0 (:194)
        bool readVariablesFromFile(FileSystem* pFileSystem, const char* pFileName); // 0x00C0CB90 (:232)
        bool readVariablesFromStream(ReadStream* pStream); // 0x00C0BA70 (:247)
        bool readVariablesFromFile(char* pUnknownVariables, uint32 unknownVariablesCapacity, FileSystem* pFileSystem, const char* pFileName); // 0x00C0BB80 (:267)
        void setModificationCallback(void (*pCallback)(IniVariableBase*, void*), void* pUserData); // 0x00BF0900 (:304)
        void invokeModificationCallback(IniVariableBase* pVariable); // 0x00BF0920 (:310)
        bool writeVariablesToFile(FileSystem* pFileSystem, const char* pFileName); // 0x00C0BDB0 (:318)
        bool writeVariablesToStream(WriteStream* pStream); // 0x00C084C0 (:329)
        bool writeVariablesToFile(FileSystem* pFileSystem, const char* pFileName, const char* pUnknownVariables); // 0x00C085A0 (:354)
        void traceVariables(); // 0x00BF8960 (:385)
        void resetVariables(); // 0x00BF8A10 (:406)
    }
}
