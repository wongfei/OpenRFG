// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"

class hkBool;
class hkClass;
class hkClassNameRegistry;
struct hkContainerHeapAllocator;
class hkHalf;
class hkTypeInfoRegistry;
struct hkVariant;
class hkVtableClassRegistry;

// size 0x14
struct hkStridedBasicArray
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStridedBasicArray);

    hkClassMember::Type m_type; // 0x00
    int m_tupleSize; // 0x04
    void* m_data; // 0x08
    int m_size; // 0x0C
    int m_stride; // 0x10
};

// size 0x1
struct hkVariantDataUtil
{
    static float getReal(hkClassMember::Type type, const void* data); // 0x00D18500
    static void setReal(hkClassMember::Type type, void* data, float value); // 0x00D19830
    static void setHalf(hkClassMember::Type type, void* data, hkHalf value); // 0x00D18F80
    static int64_t getInt(hkClassMember::Type type, hkClassMember::Type subType, const void* data); // 0x00D185E0
    static int64_t getInt(hkClassMember::Type type, const void* data); // 0x00D18530
    static void setInt(hkClassMember::Type type, hkClassMember::Type subType, void* data, int64_t value); // 0x00D18700
    static void setInt(hkClassMember::Type type, void* data, int64_t value); // 0x00D18610
    static const char* getString(hkClassMember::Type type, const void* data); // 0x00D19870
    static void setString(hkClassMember::Type type, void* data, const char* value); // 0x00D18750
    static hkBool needsConstruction(const hkTypeInfoRegistry* infoReg, hkClassMember& mem); // 0x00D18FB0
    static void newInstance(const hkClass& cls, void* data); // 0x00D19040
    static void deleteInstance(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data); // 0x00D18800
    static void finishObject(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data); // 0x00D18780
    static void finishObjectWithoutTracker(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* data); // 0x00D187C0
    static void newArray(const hkTypeInfoRegistry* infoReg, hkClassMember::Type type, const hkClass* cls, void* array, int size, int stride); // 0x00D190A0
    static void newArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size, int stride); // 0x00D18840
    static void finishObjectArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size, int stride); // 0x00D188A0
    static void deleteArray(const hkTypeInfoRegistry* infoReg, hkClassMember::Type type, const hkClass* cls, void* array, int size, int stride); // 0x00D19DD0
    static void deleteArray(const hkTypeInfoRegistry* infoReg, const hkClass& cls, void* array, int size, int stride); // 0x00D188F0
    static void clearArray(const hkTypeInfoRegistry* infoReg, void* arrayIn, hkClassMember::Type type, const hkClass* cls); // 0x00D19E20
    static void* setArraySize(const hkTypeInfoRegistry* infoReg, void* arrayIn, hkClassMember::Type type, const hkClass* cls, int size); // 0x00D19E60
    static int getArraySize(void* arrayIn); // 0x00D19170
    static void* reserveArray(void* arrayIn, hkClassMember::Type type, const hkClass* cls, int size); // 0x00D19180
    static unsigned int calcElementSize(hkClassMember::Type type, hkClassMember::Type subType, const hkClass* klass, int tupleSize); // 0x00D18BA0
    static int calcElementSize(hkClassMember::Type type, const hkClass* cls); // 0x00D184E0
    static int calcNumReals(hkClassMember::Type type, int tupleSize); // 0x00D18940
    static void setReals(hkClassMember::Type type, int tupleSize, const float* src, void* data, int numRealsIn); // 0x00D198A0
    static const float* getReals(hkClassMember::Type type, int tupleSize, const void* data, hkArray<unsigned char,hkContainerHeapAllocator>& buffer); // 0x00D19F10
    static void setObject(const hkVariant& var, hkClassMember::Type type, void* data); // 0x00D189B0
    static hkVariant getObject(hkClassMember::Type type, const hkClass* cls, const hkVtableClassRegistry* vtableReg, const hkClassNameRegistry* classReg, const void* data); // 0x00D18B20
    static const hkClass* findMostDerivedClass(const void* object, const hkVtableClassRegistry* vtable, const hkClassNameRegistry* classReg); // 0x00D189E0
    static hkVariant getVariantWithMostDerivedClass(const hkClass* klass, const void* object, const hkVtableClassRegistry* vtable, const hkClassNameRegistry* classReg); // 0x00D18A10
    static void setPointer(const hkClass& cls, void* obj, void** dst, hkBool isReferenced); // 0x00D191C0
    static unsigned int calcBasicElementSize(hkClassMember::Type type, const hkClass* klass); // 0x00D18B80
    static void convertTypeToInt32Array(hkClassMember::Type srcType, const void* srcIn, int* dst, int size); // 0x00D19220
    static void convertInt32ToTypeArray(const int* src, hkClassMember::Type dstType, void* dstIn, int size); // 0x00D19370
    static void convertUint32ToTypeArray(const unsigned int* src, hkClassMember::Type dstType, void* dstIn, int size); // 0x00D19500
    static void convertInt64ToTypeArray(const int64_t* src, hkClassMember::Type dstType, void* dstIn, int size); // 0x00D18E10
    static void convertTypeToBoolArray(hkClassMember::Type srcType, const void* srcIn, hkBool* dst, int size); // 0x00D18BF0
    static void convertBoolToTypeArray(const hkBool* src, hkClassMember::Type dstType, void* dstIn, int size); // 0x00D18D00
    static void convertTypeToTypeArray(hkClassMember::Type srcType, const void* src, hkClassMember::Type dstType, void* dst, int size); // 0x00D19A00
    static void stridedCopy(const void* src, int srcStride, void* dst, int dstStride, int eleSize, int size); // 0x00D196A0
    static void convertArray(const hkStridedBasicArray& src, const hkStridedBasicArray& dst); // 0x00D19FF0

};
