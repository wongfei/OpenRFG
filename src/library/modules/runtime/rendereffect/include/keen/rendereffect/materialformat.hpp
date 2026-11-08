// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/materialformat.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/materialfactory.cpp
//
// MaterialFormat, the serialized material format (MaterialInitializationData points at the material data).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct BaseMaterialData;

    namespace MaterialFormat
    {
        // size 0x4
        struct MaterialInitializationData
        {
            const BaseMaterialData* pMaterialData; // 0x00
        };

    }
}
