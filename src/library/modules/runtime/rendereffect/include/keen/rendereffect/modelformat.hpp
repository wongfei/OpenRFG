// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/modelformat.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/modelfactory.cpp
//
// ModelFormat, the serialized model format (material meshes, hierarchy).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct MaterialHandleType;
    struct Matrix43;
    struct ModelHierarchy;
    struct ModelNamedBoneLookup;
    struct ModelPose;
    struct VertexAttributeDescription;

    namespace ModelFormat
    {
        // size 0x2C
        struct MaterialMeshInitializationData
        {
            const MaterialHandleType* material; // 0x00
            const VertexAttributeDescription* pVertexAttributes; // 0x04
            const void* pVertexData; // 0x08
            const void* pIndexData; // 0x0C
            const void* pSkinningData; // 0x10
            const void* pPlatformData; // 0x14
            const void* pMorphData; // 0x18
            uint32 vertexAttributeCount; // 0x1C
            uint32 vertexCount; // 0x20
            uint32 indexCount; // 0x24
            uint8 indexFormat; // 0x28
            uint8 primitiveType; // 0x29
        };

        // size 0x44
        struct ModelInitializationData
        {
            const ModelHierarchy* pHierarchy; // 0x00
            const ModelPose* pDefaultPose; // 0x04
            const ModelNamedBoneLookup* pNamedBoneLookup; // 0x08
            const Matrix43* pHelperMatrices; // 0x0C
            const uint32* pHelperMatrixKeys; // 0x10
            const MaterialMeshInitializationData* pMeshInitData; // 0x14
            float3 boundingBoxMin; // 0x18
            float3 boundingBoxMax; // 0x24
            uint32 helperMatrixCount; // 0x30
            uint32 materialMeshCount; // 0x34
            uint8* pMaterialMeshData; // 0x38
            uint32 materialMeshDataSize; // 0x3C
            uint32 morphTargetCount; // 0x40
        };

    }
}
