// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkresource.h"

struct hkContainerHeapAllocator;
struct hkResult;
class hkTypeInfoRegistry;

namespace hkNativePackfileUtils
{
    hkResource* load(const void* packfileData, int dataSize, const hkTypeInfoRegistry* userRegistry); // 0x00F64B00
    hkResult validatePackfileHeader(const void* packfileData, const char** errOut); // 0x00F63D60
    int getRequiredBufferSize(const void* packfileData, int packfileSize); // 0x00F63F10
    void* load(const void* packfileData, int packfileSize, void* outBuffer, int outBufferSize, const hkTypeInfoRegistry* userRegistry); // 0x00F64490
    void* loadInPlace(void* packfileData, int dataSize, const hkTypeInfoRegistry* userRegistry, const char** errOut); // 0x00F64820
    const char* getContentsClassName(const void* packfileData, int dataSize); // 0x00F63F80
    void unload(void* buffer, int bufferSize); // 0x00F64C00
    void unloadInPlace(void* buffer, int bufferSize); // 0x00F64170
    void getImportsExports(const void* loadedBuffer, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut, hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut); // 0x00F64200
}
