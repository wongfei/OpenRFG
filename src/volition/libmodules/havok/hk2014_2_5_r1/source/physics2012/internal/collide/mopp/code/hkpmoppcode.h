// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkInternalClassMember;

// size 0x30
class hkpMoppCode : public hkReferencedObject
{
public:
    enum
    {
        MIN_PROPERTY_VALUE = 0,
        MAX_PROPERTY_VALUE = -1,
    };

    enum
    {
        MAX_PRIMITIVE_PROPERTIES = 1,
    };

    enum BuildType
    {
        BUILT_WITH_CHUNK_SUBDIVISION = 0,
        BUILT_WITHOUT_CHUNK_SUBDIVISION = 1,
        BUILD_NOT_SET = 2,
    };

    // size 0x10
    struct CodeInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CodeInfo);
        static const hkClass& staticClass(); // 0x00E33EB0
        float getScale() const; // 0x00D9A5A0
        void setScale(float);

        static const hkInternalClassMember Members[];
        hkVector4f m_offset; // 0x00
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00E33EA0, new(unsigned int nbytes) 0x00815FC0, delete(void* p, unsigned int nbytes) 0x008319F0
    static const hkClass& staticClass(); // 0x00E33ED0
    hkpMoppCode(const CodeInfo&, const unsigned char*, int, BuildType);
    hkpMoppCode(hkFinishLoadedObjectFlag f); // 0x00E33F30
    hkpMoppCode();
    virtual ~hkpMoppCode(); // vtbl+0x00 0x00831A30 (.h:108)
    int getCodeSize() const; // 0x00D7C860
    void initialize(const CodeInfo& info, const unsigned char* moppData, int moppBytes, BuildType buildType); // 0x00D9A610

    static const hkInternalClassMember Members[];
    alignas(16) CodeInfo m_info; // 0x10
    hkArray<unsigned char,hkContainerHeapAllocator> m_data; // 0x20
    hkEnum<enum BuildType,signed char> m_buildType; // 0x2C
};
