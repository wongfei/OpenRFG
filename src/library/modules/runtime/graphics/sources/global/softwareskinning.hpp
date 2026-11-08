// Original: library/modules/runtime/graphics/sources/global/softwareskinning.hpp
// Implementation: library/modules/runtime/graphics/sources/global/softwareskinning.cpp
//
// Software (CPU) skinning and morphing of serialized geometry (SoftwareSkinGeometry, SoftwareMorphData) with SIMD
// helpers. Placement of the types inferred.
#pragma once

#include "keen/base/types.hpp"
#include <xmmintrin.h>
#include "keen/base/blob.hpp"

namespace keen
{
    struct Matrix43;

    // size 0x20
    struct SoftwareSkinningBoneInfo
    {
        enum
        {
            MaxWeightCount = 4,
        };

        SoftwareSkinningBoneInfo();

        uint32 index[4]; // 0x00
        float weight[4]; // 0x10
    };

    // size 0x14
    struct SoftwareSkinGeometry : public Blob
    {
        BlobMember<SoftwareSkinningBoneInfo> boneInfos; // 0x04
        BlobMember<float3> positions; // 0x08
        BlobMember<float3> normals; // 0x0C
        BlobMember<float4> tangents; // 0x10
    };

    // size 0xC
    struct SoftwareMorphData : public Blob
    {
        BlobMember<float3> positions; // 0x04
        BlobMember<float3> normals; // 0x08
    };

    // size 0x30
    struct SkinningSimdMatrix33
    {
        __m128 x; // 0x00
        __m128 y; // 0x10
        __m128 z; // 0x20
    };

    // size 0x40
    struct SkinningSimdMatrix43
    {
        SkinningSimdMatrix33 rot; // 0x00
        __m128 pos; // 0x30
    };

    // size 0x4000
    struct SoftwareSkinningJointMatrixData
    {
        SkinningSimdMatrix43 jointMatrices[256]; // 0x00
    };

    // size 0x30
    struct SkinningSimdVector3
    {
        __m128 x; // 0x00
        __m128 y; // 0x10
        __m128 z; // 0x20
    };

    void createSkinningSimdMatrix43(SkinningSimdMatrix43* pMat, const Matrix43& mtx); // 0x00C60180 (:33)

    namespace graphics
    {
        void prepareCpuSkinningMatrices(SoftwareSkinningJointMatrixData* pJointMatrices, const Matrix43* pSourceMatrices, uint32 matrixCount); // 0x00C69380 (:78)
        void skinGeometryCpu(uint8* pTargetVertex, const SoftwareSkinGeometry* pSkinData, const SkinningSimdMatrix43* pSimdMatrices, uint32 vertexCount); // 0x00C693F0 (:87)
    }
}
