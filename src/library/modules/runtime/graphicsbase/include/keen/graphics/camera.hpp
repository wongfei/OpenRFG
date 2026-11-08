// Original: library/modules/runtime/graphicsbase/include/keen/graphics/camera.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/camera.cpp
//
// Camera, a world matrix and a projection with the view matrix and frustum derived lazily (dirty flags).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/frustum.hpp"
#include "keen/math/matrix43.hpp"
#include "keen/math/projection.hpp"

namespace keen
{
    // size 0xE4
    class Camera
    {
    public:
        Camera(); // 0x00CE5790 (:8)
        void setProjection(const Projection& projection); // 0x00CE2C40 (:16)
        void setWorldMatrix(const Matrix43& matrix); // 0x00CE2C80 (:22)
        const Matrix43& getViewMatrix() const; // 0x00CE4450 (:29)
        const Matrix43& getWorldMatrix() const; // 0x00C5C980 (.h:30)
        const Projection& getProjection() const; // 0x00C5C990 (.h:31)
        const Frustum& getFrustum() const; // 0x00CE4480 (:36)
    private:
        void updateView() const; // 0x00CE2CD0 (:43)
        void updateFrustum() const; // 0x00CE2CF0 (:56)

        Matrix43 m_worldMatrix; // 0x00
        Projection m_projection; // 0x30
        Matrix43 m_viewMatrix; // 0x50
        Frustum m_frustum; // 0x80
        bool m_viewDirty; // 0xE0
        bool m_frustumDirty; // 0xE1
    };
}
