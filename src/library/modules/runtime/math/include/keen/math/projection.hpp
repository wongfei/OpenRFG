// Original: library/modules/runtime/math/include/keen/math/projection.hpp
// Implementation: library/modules/runtime/math/sources/global/projection.cpp
//
// Projection, a perspective or orthographic projection (a union of the two parameter sets plus the type) and its
// matrices. Placement of PerspectiveProjection, OrthographicProjection and ProjectionType inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Matrix43;
    struct Matrix44;
    struct ScreenRectangle;
    struct Vector3;

    enum ProjectionType
    {
        ProjectionType_Perspective = 0,
        ProjectionType_Orthographic = 1,
        ProjectionType_Count = 2,
    };

    // size 0x10
    struct PerspectiveProjection
    {
        float aspectRatio; // 0x00
        float nearClip; // 0x04
        float farClip; // 0x08
        float fovY; // 0x0C
    };

    // size 0x18
    struct OrthographicProjection
    {
        float x0; // 0x00
        float x1; // 0x04
        float y0; // 0x08
        float y1; // 0x0C
        float z0; // 0x10
        float z1; // 0x14
    };

    // size 0x20
    struct Projection
    {
        Projection(); // 0x00C53170 (:11)
        void setPerspective(float fovY, float aspectRatio, float nearClip, float farClip); // 0x00C43750 (:21)
        void setOrthographic(float x0, float x1, float y0, float y1, float z0, float z1); // 0x00C43790 (:31)
        void setNearPlaneDistance(float);
        void setFarPlaneDistance(float);
        float getNearPlaneDistance() const;
        float getFarPlaneDistance() const;
        void setRightHanded();

        union
        {
            PerspectiveProjection perspective; // 0x00
            OrthographicProjection orthographic; // 0x00
        } data; // 0x00
        ProjectionType type; // 0x18
        bool rightHanded; // 0x1C
    };

    void createProjectionMatrix(Matrix44* pTarget, const Projection& projection); // 0x00C437E0 (:47)
    bool projectSphereToScreen(ScreenRectangle* pRect, const Matrix44& projectionMatrix, const Matrix43& viewMatrix, float nearPlane, Vector3 sphereCenter, float sphereRadius); // 0x00C531B0 (:93)
    bool projectSphereToScreenInexact(ScreenRectangle* pRect, const Matrix44& viewProjectionMatrix, const Matrix43& viewMatrix, const Matrix43& worldMatrix, float nearPlane, Vector3 sphereCenter, float sphereRadius); // 0x00C536A0 (:178)
    bool projectCapsuleToScreenInexact(ScreenRectangle* pRect, const Matrix44& viewProjectionMatrix, const Matrix43& viewMatrix, const Matrix43& worldMatrix, float nearPlane, Vector3 position0, Vector3 position1, float radius); // 0x00C43BD0 (:215)
}
