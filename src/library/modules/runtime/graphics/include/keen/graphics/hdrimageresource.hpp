// Original: library/modules/runtime/graphics/include/keen/graphics/hdrimageresource.hpp
// Implementation: library/modules/runtime/graphics/sources/global/hdrimageresource.cpp
//
// HdrImageResource, an HDR image (float pixels) loaded as a resource; HdrImageResourceData is its serialized form.
// Placement of the data and handle types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct HdrImageResourceInitializationData;

    struct HdrImageResourceHandleType; // no definition in PDB

    struct HdrImageResourceData; // no definition in PDB

    // size 0xC
    class HdrImageResource : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x49524448,
        };

        const HdrImageResourceData* getData() const;
    private:
        HdrImageResource(const HdrImageResource&);
    protected:
        HdrImageResource(uint32 key); // 0x00C5D150 (:7)
        bool initialize(const HdrImageResourceInitializationData* pInitData); // 0x00C5D170 (:12)
        void destroy(); // 0x00C5D180 (:19)
    private:
        HdrImageResource& operator=(const HdrImageResource&);
    public:
        ~HdrImageResource(); // 0x00C60C90

    private:
        const HdrImageResourceData* m_pData; // 0x08
    };
}
