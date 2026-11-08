// Original: library/modules/runtime/graphics/include/keen/graphics/shadowmap.hpp
// Implementation: library/modules/runtime/graphics/sources/global/shadowmap.cpp
//
// Shadow map helpers: ShadowMapParameters and the cascade/matrix computation. Placement of ShadowMapParameters inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/matrix43.hpp"
#include "keen/math/matrix44.hpp"
#include "keen/math/projection.hpp"
#include "keen/math/vector3.hpp"
#include "keen/math/vector4.hpp"

namespace keen
{
    class Camera;

    // size 0xE0
    struct ShadowMapParameters
    {
        ShadowMapParameters();

        Matrix43 worldMatrix; // 0x00
        Matrix43 viewMatrix; // 0x30
        Matrix44 worldToShadowMapMatrix; // 0x60
        Projection projection; // 0xA0
        Vector4 sizeOffsetInLightSpace; // 0xC0
        Vector3 boxScale; // 0xD0
        float boxZOffset; // 0xDC
    };

    namespace graphics
    {
        void createLightToWorldMatrix(Matrix43* pLightToWorldMatrix, const Vector3& toLightDir); // 0x00C67600 (:127)
        void computeFixedCascadesShadowMapParameters(ShadowMapParameters* pResults, uint32 cascadeCount, const float* pCascadeSplitDistances, const Camera& sceneCamera, float lightSpaceSceneMinDepth, float lightSpaceSceneMaxDepth, const Vector3& lightDirWs, float mapWidthResolution, float mapHeightResolution); // 0x00C67A60 (:171)
        void computePointShadowMapParameters(ShadowMapParameters* pResult, const Vector3& lightPositionWS, bool frontHemisphere, float lightRange); // 0x00C5F660 (:256)
        void computeSpotShadowMapParameters(ShadowMapParameters* pResult, const Vector3& lightPositionWS, const Vector3& lightDirWS, float lightConeLength, float lightConeAngle); // 0x00C68790 (:282)
    }
}
